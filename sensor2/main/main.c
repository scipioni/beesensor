#include "config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "iot_button.h"
#include "esp_zigbee_core.h"
#include "esp_ieee802154.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "esp_sleep.h"
#include "string.h"

#include "temp_sensor_driver.h"
#include "led_board_driver.h"
#include "battery_sensor.h"

bool connected = false;

static const char *TAG = "GALILEO_SENSOR";
float_t counter = 1.0;

// typedef struct zbstring_s {
//     uint8_t len;
//     char data[];
// } ESP_ZB_PACKED_STRUCT
// zbstring_t;

static void esp_app_temp_sensor_handler(float temperature)
{
    int16_t measured_value = zb_temperature_to_s16(temperature);
    /* Update temperature sensor measured value */
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_set_attribute_val(HA_ESP_GALILEO_SENSOR_ENDPOINT,
                                 ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                 ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &measured_value, false);
    esp_zb_lock_release();
}

static esp_err_t deferred_driver_init(void)
{
    temperature_sensor_config_t temp_sensor_config =
        TEMPERATURE_SENSOR_CONFIG_DEFAULT(ESP_TEMP_SENSOR_MIN_VALUE, ESP_TEMP_SENSOR_MAX_VALUE);
    ESP_RETURN_ON_ERROR(temp_sensor_driver_init(&temp_sensor_config, ESP_TEMP_SENSOR_UPDATE_INTERVAL, esp_app_temp_sensor_handler), TAG,
                        "Failed to initialize temperature sensor");
    ESP_RETURN_ON_ERROR(led_board_driver_init(LED), TAG,
                        "Failed to initialize LED board");
    return ESP_OK;
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG, "Failed to start Zigbee commissioning");
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;

    ESP_LOGI(TAG, "signal handler - ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
             esp_err_to_name(err_status));
    switch (sig_type)
    {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK)
        {
            ESP_LOGI(TAG, "Deferred driver initialization %s", deferred_driver_init() ? "failed" : "successful");
            ESP_LOGI(TAG, "Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
            if (esp_zb_bdb_is_factory_new())
            {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            }
            else
            {
                ESP_LOGI(TAG, "Device rebooted");
            }
        }
        else
        {
            /* commissioning failed */
            ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK)
        {
            connected = true;
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            // https://github.com/espressif/esp-zigbee-sdk/issues/341
            // memcpy(&(coord.ieee_addr), extended_pan_id, sizeof(esp_zb_ieee_addr_t));
            // coord.endpoint = 1;
            // coord.short_addr = 0;
            // /* bind the reporting clusters to ep */
            // esp_zb_zdo_bind_req_param_t bind_req;
            // memcpy(&(bind_req.dst_address_u.addr_long), coord.ieee_addr, sizeof(esp_zb_ieee_addr_t));
            // bind_req.dst_endp = coord.endpoint;
            // bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
            // esp_zb_get_long_address(bind_req.src_address);
            // bind_req.req_dst_addr = esp_zb_get_short_address();
            // static zdo_info_user_ctx_t test_info_ctx;
            // test_info_ctx.short_addr = coord.short_addr;
            // for (auto reporting_info : zigbeeC->reporting_list)
            // {
            //     bind_req.cluster_id = reporting_info.cluster_id;
            //     bind_req.src_endp = reporting_info.ep;
            //     test_info_ctx.endpoint = reporting_info.ep;
            //     esp_zb_zdo_device_bind_req(&bind_req, bind_cb, (void *) & (test_info_ctx));
            // }
        }
        else
        {
            connected = false;
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    bool light_state = 0;
    bool deepsleep = 0;

    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)", message->info.dst_endpoint, message->info.cluster,
             message->attribute.id, message->attribute.data.size);
    if (message->info.dst_endpoint == HA_ESP_GALILEO_SENSOR_ENDPOINT)
    {
        if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF)
        {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL)
            {
                light_state = message->attribute.data.value ? *(bool *)message->attribute.data.value : light_state;
                ESP_LOGI(TAG, "Light sets to %s", light_state ? "On" : "Off");
                if (light_state)
                {
                    led_on();
                }
                else
                {
                    led_off();
                }
            }
        }
    }
    else if (message->info.dst_endpoint == HA_DEEPSLEEP_ENDPOINT)
    {
        if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF)
        {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL)
            {
                deepsleep = message->attribute.data.value ? *(bool *)message->attribute.data.value : deepsleep;
                ESP_LOGI(TAG, "Deepsleep %s", deepsleep ? "On" : "Off");
                if (deepsleep)
                {
                    // deep_sleep();
                    ESP_LOGI(TAG, "recieved command, entring deep sleep in 10 sec...");
                    vTaskDelay(10000 / portTICK_PERIOD_MS);
                    ESP_LOGI(TAG, "entring deep sleep...");
                    led_blink_and_off();
                    esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000);
                    esp_deep_sleep_start();
                    vTaskDelete(NULL);
                }
            }
        }
    }
    return ret;
}

// static void esp_app_zb_attribute_handler(uint16_t cluster_id, const esp_zb_zcl_attribute_t* attribute)
// {
//     /* Basic cluster attributes */
//     if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_BASIC) {
//         if (attribute->id == ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID &&
//             attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING &&
//             attribute->data.value) {
//             zbstring_t *zbstr = (zbstring_t *)attribute->data.value;
//             char *string = (char*)malloc(zbstr->len + 1);
//             memcpy(string, zbstr->data, zbstr->len);
//             string[zbstr->len] = '\0';
//             ESP_LOGI(TAG, "Peer Manufacturer is \"%s\"", string);
//             free(string);
//         }
//         if (attribute->id == ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID &&
//             attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING &&
//             attribute->data.value) {
//             zbstring_t *zbstr = (zbstring_t *)attribute->data.value;
//             char *string = (char*)malloc(zbstr->len + 1);
//             memcpy(string, zbstr->data, zbstr->len);
//             string[zbstr->len] = '\0';
//             ESP_LOGI(TAG, "Peer Model is \"%s\"", string);
//             free(string);
//         }
//     }

//     /* Temperature Measurement cluster attributes */
//     // if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT) {
//     //     if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID &&
//     //         attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_S16) {
//     //         int16_t value = attribute->data.value ? *(int16_t *)attribute->data.value : 0;
//     //         ESP_LOGI(TAG, "Measured Value is %.2f degrees Celsius", zb_s16_to_temperature(value));
//     //     }
//     //     if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID &&
//     //         attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_S16) {
//     //         int16_t min_value = attribute->data.value ? *(int16_t *)attribute->data.value : 0;
//     //         ESP_LOGI(TAG, "Min Measured Value is %.2f degrees Celsius", zb_s16_to_temperature(min_value));
//     //     }
//     //     if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID &&
//     //         attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_S16) {
//     //         int16_t max_value = attribute->data.value ? *(int16_t *)attribute->data.value : 0;
//     //         ESP_LOGI(TAG, "Max Measured Value is %.2f degrees Celsius", zb_s16_to_temperature(max_value));
//     //     }
//     //     if (attribute->id == ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_TOLERANCE_ID &&
//     //         attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
//     //         uint16_t tolerance = attribute->data.value ? *(uint16_t *)attribute->data.value : 0;
//     //         ESP_LOGI(TAG, "Tolerance is %.2f degrees Celsius", 1.0 * tolerance / 100);
//     //     }
//     // }
// }


// static esp_err_t zb_attribute_reporting_handler(const esp_zb_zcl_report_attr_message_t *message)
// {
//     ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
//     ESP_RETURN_ON_FALSE(message->status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
//                         message->status);
//     ESP_LOGI(TAG, "Received report from address(0x%x) src endpoint(%d) to dst endpoint(%d) cluster(0x%x)",
//              message->src_address.u.short_addr, message->src_endpoint,
//              message->dst_endpoint, message->cluster);
//     esp_app_zb_attribute_handler(message->cluster, &message->attribute);
//     return ESP_OK;
// }

// static esp_err_t zb_configure_report_resp_handler(const esp_zb_zcl_cmd_config_report_resp_message_t *message)
// {
//     ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
//     ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
//                         message->info.status);

//     esp_zb_zcl_config_report_resp_variable_t *variable = message->variables;
//     while (variable) {
//         ESP_LOGI(TAG, "Configure report response: status(%d), cluster(0x%x), direction(0x%x), attribute(0x%x)",
//                  variable->status, message->info.cluster, variable->direction, variable->attribute_id);
//         variable = variable->next;
//     }

//     return ESP_OK;
// }

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id)
    {
    // case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
    //     ret = zb_attribute_reporting_handler((esp_zb_zcl_report_attr_message_t *)message);
    //     break;
    // case ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID:
    //     ret = zb_configure_report_resp_handler((esp_zb_zcl_cmd_config_report_resp_message_t *)message);
    //     break;
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    // case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
    //     break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) default callback", callback_id);
        break;
    }
    return ret;
}

static void reset_and_reboot(void *arg, void *data)
{
    // ESP_EARLY_LOGI
    ESP_LOGI(TAG, "Button event %s: reset zigbee and reboot", button_event_table[(button_event_t)data]);
    led_blink_and_off();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    esp_zb_factory_reset();
    esp_restart(); // poi reboot
}

static void button_event_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Button event %s", button_event_table[(button_event_t)data]);
    led_blink_and_off();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_OFF * 1000000);
    esp_deep_sleep_start();
}

void broadcast_ping(void *pvParameters)
{
    ESP_LOGI(TAG, "broadcast ping start");
    esp_zb_zcl_on_off_cmd_t cmd_req;

    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = 0xFFFF; // Broadcast address
    cmd_req.zcl_basic_cmd.dst_endpoint = 255;             // Broadcast endpoint
    cmd_req.zcl_basic_cmd.src_endpoint = HA_ESP_GALILEO_SENSOR_ENDPOINT;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
    led_off();
    for (int i = 0; i < 10; i++)
    {
        if (esp_zb_lock_acquire(portMAX_DELAY))
        {
            esp_zb_zcl_on_off_cmd_req(&cmd_req);
            esp_zb_lock_release();
            // ESP_EARLY_LOGI(TAG, "Send 'on_off toggle' command to address(0x%x) endpoint(%d)", on_off_light.short_addr, on_off_light.endpoint);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "broadcast ping stop");
    // led_blink_slow();
    vTaskDelete(NULL);
}

void broadcast_ping_task(void *arg, void *data)
{
    xTaskCreate(broadcast_ping, "broacast_ping", 4096, NULL, 1, NULL);
}

void button_init(uint32_t button_num)
{
    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = button_num,
            .active_level = BUTTON_ACTIVE_LEVEL},
    };
    button_handle_t btn = iot_button_create(&btn_cfg);
    assert(btn);
    esp_err_t err = iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_event_cb, (void *)BUTTON_SINGLE_CLICK);
    // err |= iot_button_register_cb(btn, BUTTON_PRESS_DOWN, button_event_cb, (void *)BUTTON_PRESS_DOWN);
    // err |= iot_button_register_cb(btn, BUTTON_PRESS_UP, button_event_cb, (void *)BUTTON_PRESS_UP);
    // err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT, button_event_cb, (void *)BUTTON_PRESS_REPEAT);
    // err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT_DONE, button_event_cb, (void *)BUTTON_PRESS_REPEAT_DONE);
    err |= iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, broadcast_ping_task, (void *)BUTTON_DOUBLE_CLICK);
    // err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, button_event_cb, (void *)BUTTON_LONG_PRESS_START);
    // err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_HOLD, button_event_cb, (void *)BUTTON_LONG_PRESS_HOLD);
    err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, reset_and_reboot, (void *)BUTTON_LONG_PRESS_UP);
    ESP_ERROR_CHECK(err);
}

static void esp_zb_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Entering ESP_ZB_TASK");
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    uint8_t zcl_version, null_values;

    zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE;
    null_values = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE;

    // basic cluster create
    esp_zb_attribute_list_t *basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &zcl_version);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &null_values);
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_IDENTIFIER));
    // identify cluster
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY);
    ESP_ERROR_CHECK(esp_zb_identify_cluster_add_attr(esp_zb_identify_cluster, ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID, &null_values));
    // group cluster
    esp_zb_attribute_list_t *esp_zb_groups_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_GROUPS);
    ESP_ERROR_CHECK(esp_zb_groups_cluster_add_attr(esp_zb_groups_cluster, ESP_ZB_ZCL_ATTR_GROUPS_NAME_SUPPORT_ID, &null_values));
    // scenes cluster with standard cluster + customized
    esp_zb_attribute_list_t *esp_zb_scenes_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_SCENES);
    esp_zb_scenes_cluster_add_attr(esp_zb_scenes_cluster, ESP_ZB_ZCL_ATTR_SCENES_NAME_SUPPORT_ID, &null_values);
    esp_zb_scenes_cluster_add_attr(esp_zb_scenes_cluster, ESP_ZB_ZCL_ATTR_SCENES_CURRENT_GROUP_ID, &null_values);
    esp_zb_scenes_cluster_add_attr(esp_zb_scenes_cluster, ESP_ZB_ZCL_ATTR_SCENES_CURRENT_SCENE_ID, &null_values);
    esp_zb_scenes_cluster_add_attr(esp_zb_scenes_cluster, ESP_ZB_ZCL_ATTR_SCENES_SCENE_VALID_ID, &null_values);
    esp_zb_scenes_cluster_add_attr(esp_zb_scenes_cluster, ESP_ZB_ZCL_ATTR_SCENES_SCENE_COUNT_ID, &null_values);

    // cluster lists for this endpoint
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_groups_cluster(cluster_list, esp_zb_groups_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_scenes_cluster(cluster_list, esp_zb_scenes_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // on-off cluster create with standard cluster config
    esp_zb_on_off_cluster_cfg_t on_off_cfg;
    on_off_cfg.on_off = ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE;
    esp_zb_attribute_list_t *esp_zb_on_off_cluster = esp_zb_on_off_cluster_create(&on_off_cfg);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_on_off_cluster(cluster_list, esp_zb_on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // analog input cluster
    // from https://github.com/Bobstin/AutomatedBrewery/blob/b292b6e3d0344bba9a9dc35f3571ffdb034a176e/ESPZigbeeDemo/zigbee_cluster_demo/main/esp_zb_light.c
    // esp_zb_analog_value_ analog_value_cfg;
    esp_zb_analog_input_cluster_cfg_t analog_input_cfg;
    analog_input_cfg.out_of_service = 0;
    analog_input_cfg.present_value = 0;
    analog_input_cfg.status_flags = ESP_ZB_ZCL_ANALOG_INPUT_STATUS_FLAG_DEFAULT_VALUE;
    esp_zb_attribute_list_t *esp_zb_analog_input_cluster = esp_zb_analog_input_cluster_create(&analog_input_cfg);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_analog_input_cluster(cluster_list, esp_zb_analog_input_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // temperature sensor
    esp_zb_temperature_sensor_cfg_t sensor_cfg = ESP_ZB_DEFAULT_TEMPERATURE_SENSOR_CONFIG();
    sensor_cfg.temp_meas_cfg.min_value = zb_temperature_to_s16(ESP_TEMP_SENSOR_MIN_VALUE);
    sensor_cfg.temp_meas_cfg.max_value = zb_temperature_to_s16(ESP_TEMP_SENSOR_MAX_VALUE);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_temperature_meas_cluster(cluster_list, esp_zb_temperature_meas_cluster_create(&(sensor_cfg.temp_meas_cfg)), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // endpoint 10
    esp_zb_endpoint_config_t EPC = {
        .endpoint = HA_ESP_GALILEO_SENSOR_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, // ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID,
        .app_device_version = 1,
    };

    /* endpoint 11 with 2 attributes */
    esp_zb_cluster_list_t *cluster_list_vin = esp_zb_zcl_cluster_list_create();
    esp_zb_analog_input_cluster_cfg_t vin_cfg;
    vin_cfg.out_of_service = 0;
    vin_cfg.present_value = 0;
    vin_cfg.status_flags = ESP_ZB_ZCL_ANALOG_INPUT_STATUS_FLAG_DEFAULT_VALUE;
    esp_zb_attribute_list_t *vin_cluster = esp_zb_analog_input_cluster_create(&vin_cfg);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_analog_input_cluster(cluster_list_vin, vin_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    
    /* endpoint 11 second attribute  */
    // esp_zb_electrical_meas_cluster_cfg_t vin_type_cfg;
    // vin_type_cfg.measured_type = 0;
    // esp_zb_attribute_list_t *vin_type_cluster = esp_zb_electrical_meas_cluster_create(&vin_type_cfg);
    // ESP_ERROR_CHECK(esp_zb_cluster_list_add_electrical_meas_cluster(cluster_list_vin, vin_type_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
        


    esp_zb_endpoint_config_t EPC_VIN = {
        .endpoint = HA_VIN_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID,
        .app_device_version = 1,
    };
    /* ********  */

    /***** endpoint 12 *****/
    esp_zb_cluster_list_t *cluster_list_counter = esp_zb_zcl_cluster_list_create();
    esp_zb_analog_input_cluster_cfg_t counter_cfg;
    counter_cfg.out_of_service = 0;
    counter_cfg.present_value = 0;
    counter_cfg.status_flags = ESP_ZB_ZCL_ANALOG_INPUT_STATUS_FLAG_DEFAULT_VALUE;
    esp_zb_attribute_list_t *counter_cluster = esp_zb_analog_input_cluster_create(&counter_cfg);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_analog_input_cluster(cluster_list_counter, counter_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    esp_zb_endpoint_config_t EPC_COUNTER = {
        .endpoint = HA_COUNTER_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID,
        .app_device_version = 4,
    };

    // endpoint 13
    esp_zb_cluster_list_t *cluster_list_deepsleep = esp_zb_zcl_cluster_list_create();
    esp_zb_on_off_cluster_cfg_t deepsleep_cfg;
    deepsleep_cfg.on_off = ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE;
    esp_zb_attribute_list_t *esp_zb_deepsleep_cluster = esp_zb_on_off_cluster_create(&deepsleep_cfg);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_on_off_cluster(cluster_list_deepsleep, esp_zb_deepsleep_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    esp_zb_endpoint_config_t EPC_DEEPSLEEP = {
        .endpoint = HA_DEEPSLEEP_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, // ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID,
        .app_device_version = 1,
    };

    /* Config the reporting info (zigbee sdk examples temperature) */
    // https://github.com/espressif/esp-zigbee-sdk/issues/341   
    // esp_zb_zcl_report_config_cmd_req
    // esp_zb_zcl_reporting_info_t reporting_info_counter = {
    //     .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
    //     .ep = HA_COUNTER_ENDPOINT,
    //     .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT,
    //     .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    //     .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    //     .u.send_info.min_interval = 1,
    //     .u.send_info.max_interval = 0,
    //     .u.send_info.def_min_interval = 1,
    //     .u.send_info.def_max_interval = 0,
    //     .u.send_info.delta.u16 = 1,
    //     .attr_id = ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID,
    //     .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
    // };

    /***** end endpoint 12 *****/


    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
    esp_zb_ep_list_add_ep(esp_zb_ep_list, cluster_list, EPC);
    esp_zb_ep_list_add_ep(esp_zb_ep_list, cluster_list_vin, EPC_VIN);
    esp_zb_ep_list_add_ep(esp_zb_ep_list, cluster_list_counter, EPC_COUNTER);
    esp_zb_ep_list_add_ep(esp_zb_ep_list, cluster_list_deepsleep, EPC_DEEPSLEEP);


    esp_zb_device_register(esp_zb_ep_list);

    esp_zb_core_action_handler_register(zb_action_handler);
    //esp_zb_zcl_update_reporting_info(&reporting_info_counter);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);

    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
}

void update_attributes()
{
    int8_t rssi;
    for (;;)
    {
        if (esp_zb_lock_acquire(portMAX_DELAY))
        {
            counter = counter + 1.0;
            rssi = esp_ieee802154_get_recent_rssi();
            ESP_LOGI(TAG, "rssi=%i counter=%f battery=%0.2fmV adc_raw=%d", rssi, counter, battery_voltage, adc_raw[0][0]);

            esp_zb_zcl_status_t state_analog = esp_zb_zcl_set_attribute_val(
                SENSOR_ENDPOINT,
                ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT,
                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID,
                &battery_voltage,
                false);
            if (state_analog != ESP_ZB_ZCL_STATUS_SUCCESS)
            {
                ESP_LOGE(TAG, "Setting analog attribute failed!");
            }

            esp_zb_zcl_status_t state_analog_vin = esp_zb_zcl_set_attribute_val(
                HA_VIN_ENDPOINT,
                ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT,
                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID,
                &vin_voltage,
                false);

            if (state_analog_vin != ESP_ZB_ZCL_STATUS_SUCCESS)
            {
                ESP_LOGE(TAG, "Setting analog attribute failed!");
            }

            esp_zb_zcl_status_t state_analog_counter = esp_zb_zcl_set_attribute_val(
                HA_COUNTER_ENDPOINT,
                ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT,
                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID,
                &counter,
                false);

            if (state_analog_counter != ESP_ZB_ZCL_STATUS_SUCCESS)
            {
                ESP_LOGE(TAG, "Setting counter attribute failed!");
            }
            esp_zb_lock_release();
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void deep_sleep() {
    vTaskDelay(DEEP_SLEEP_ON * 1000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "entring deep sleep...");
    led_blink_and_off();
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_OFF * 1000000);
    esp_deep_sleep_start();
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    button_init(BOOT_BUTTON_NUM);
    battery_sensor_init();
    #ifdef DEEP_SLEEP_ACTIVE
        xTaskCreate(deep_sleep, "deep_sleep", 4096, NULL, 1, NULL);
    #endif
    xTaskCreate(update_attributes, "update_attributes", 4096, NULL, 5, NULL);
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
