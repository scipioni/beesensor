#include "config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"
#include "esp_zigbee_core.h"
#include "iot_button.h"


bool button_state = false;
bool connected = false;

unsigned int led_time_ms = 500;
float_t analog_value = 1.0;
float_t temperature = 10.0;

#define ARRAY_LENTH(arr) (sizeof(arr) / sizeof(arr[0]))

static const char *TAG = "GALILEO_SENSOR";

static int16_t zb_temperature_to_s16(float temp)
{
    return (int16_t)(temp * 100);
}


static esp_err_t deferred_driver_init(void)
{
    // light_driver_init(LIGHT_DEFAULT_OFF);
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
                    led_time_ms = 100;
                }
                else
                {
                    led_time_ms = 500;
                }
            }
        }
    }
    return ret;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id)
    {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

static void reset_and_reboot(void *arg, void *data)
{
    // ESP_EARLY_LOGI
    ESP_LOGI(TAG, "Button event %s", button_event_table[(button_event_t)data]);
    esp_zb_factory_reset();
    led_time_ms = 50; // led intermittente per 2 secondi
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    esp_restart(); // poi reboot
}

// void my_memcpy(void* dest, const void* src, size_t size) {
//     uint8_t* dst_ptr = (uint8_t*)dest;
//     const uint8_t* src_ptr = (uint8_t*)src;

//     for (size_t i = 0; i < size; i++) {
//         dst_ptr[i] = src_ptr[i];
//     }
// }

/* Manual reporting atribute to coordinator */
// static void reportAttribute(uint8_t endpoint, uint16_t clusterID, uint16_t attributeID, void *value, uint8_t value_length)
// {
//     esp_zb_zcl_report_attr_cmd_t cmd = {
//         .zcl_basic_cmd = {
//             .dst_addr_u.addr_short = 0x0000,
//             .dst_endpoint = endpoint,
//             .src_endpoint = endpoint,
//         },
//         .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
//         .clusterID = clusterID,
//         .attributeID = attributeID,
//         .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
//     };
//     esp_zb_zcl_attr_t *value_r = esp_zb_zcl_get_attribute(endpoint, clusterID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attributeID);
//     //my_memcpy(value_r->data_p, value, value_length);
//     esp_zb_zcl_report_attr_cmd_req(&cmd);
// }

static void button_event_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Button event %s", button_event_table[(button_event_t)data]);
    // uint16_t co2_value = 99;

    // esp_zb_zcl_status_t state_co2 = esp_zb_zcl_set_attribute_val(SENSOR_ENDPOINT, VALUE_CUSTOM_CLUSTER, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, 0, &co2_value, false);

    // /* Check for error */
    // if (state_co2 != ESP_ZB_ZCL_STATUS_SUCCESS)
    // {
    //     ESP_LOGE(TAG, "Setting CO2_value attribute failed!");
    // }

    // analog_value = analog_value + 1.1;
    // esp_zb_zcl_status_t status = esp_zb_zcl_set_attribute_val(
    //     SENSOR_ENDPOINT,                    // DEVICE_ENDPOINT
    //     ESP_ZB_ZCL_CLUSTER_ID_ANALOG_VALUE, // TARGET_CLUSTER
    //     ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    //     ESP_ZB_ZCL_ATTR_ANALOG_VALUE_PRESENT_VALUE_ID, // TARGET_ATTRIBUTE
    //     &analog_value,
    //     false);

    // if (status != ESP_ZB_ZCL_STATUS_SUCCESS)
    // {
    //     ESP_LOGE(TAG, "Set attribute failed: %x", status);
    // }
    // else
    // {
    //     ESP_LOGI(TAG, "Attribute value changed to: %f", analog_value);
    // }

    temperature = temperature + 1.1;
    int16_t measured_value = zb_temperature_to_s16(temperature);
    /* Update temperature sensor measured value */
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_status_t status = esp_zb_zcl_set_attribute_val(SENSOR_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &measured_value, false);
    esp_zb_lock_release();
    if (status != ESP_ZB_ZCL_STATUS_SUCCESS)
    {
        ESP_LOGE(TAG, "Set temp failed: %x", status);
    }
    else
    {
        ESP_LOGI(TAG, "Attribute temp changed to: %f", temperature);
    }

    /* CO2 Cluster is custom and we must report it manually*/
    // reportAttribute(SENSOR_ENDPOINT, VALUE_CUSTOM_CLUSTER, 0, &analog_value, 2);
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
    // err |= iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, button_event_cb, (void *)BUTTON_DOUBLE_CLICK);
    // err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, button_event_cb, (void *)BUTTON_LONG_PRESS_START);
    // err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_HOLD, button_event_cb, (void *)BUTTON_LONG_PRESS_HOLD);
    err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, reset_and_reboot, (void *)BUTTON_LONG_PRESS_UP);
    ESP_ERROR_CHECK(err);
}

/* ---- custom package ---- */

// typedef struct esp_zb_data_pkg_cluster_cfg_s
// {
//     uint32_t data;
// } esp_zb_data_pkg_cluster_cfg_t;

// typedef struct esp_zb_package_cfg_s
// {
//     esp_zb_basic_cluster_cfg_t basic_cfg;
//     esp_zb_identify_cluster_cfg_t identify_cfg;
//     esp_zb_data_pkg_cluster_cfg_t data_cfg;
// } esp_zb_package_cfg_t;

// static esp_zb_cluster_list_t *custom_package_clusters_create(esp_zb_package_cfg_t *package)
// {
//     esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
//     esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&(package->basic_cfg));

//     ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
//     ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(&(package->identify_cfg)), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
//     ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));
//     ESP_ERROR_CHECK(esp_zb_cluster_list_add_analog_value_cluster(cluster_list, esp_zb_analog_value_cluster_create(&(package->data_cfg)), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

//     return cluster_list;
// }

// static esp_zb_ep_list_t *custom_package_ep_create(uint8_t endpoint_id, esp_zb_package_cfg_t *package)
// {
//     esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();

//     esp_zb_endpoint_config_t endpoint_config = {
//         .endpoint = endpoint_id,
//         .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
//         .app_device_id = ESP_ZB_HA_TEST_DEVICE_ID,
//         .app_device_version = 0};
//     esp_zb_ep_list_add_ep(ep_list, custom_package_clusters_create(package), endpoint_config);

//     return ep_list;
// }

// # define ESP_TX_ENDPOINT 11

// static void create_custom_endpoint() {

//     esp_zb_zcl_attr_location_info_t filter;
//     esp_zb_zcl_reporting_info_t *report;

//     filter.attr_id=0; //CUSTOM_ON_OFF_CLUSTER_ATTR_STATE_ID;
//     filter.cluster_id=ESP_ZB_ZCL_CLUSTER_ID_ANALOG_VALUE;
//     filter.endpoint_id=SENSOR_ENDPOINT;
//     filter.cluster_role=ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
//     filter.manuf_code=ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;
//     report=esp_zb_zcl_find_reporting_info(filter);
//     if(report!=NULL){
//         report->u.send_info.def_max_interval=0xFFFE;
//         report->u.send_info.def_min_interval=0x00;
//         report->u.send_info.max_interval=0xFFFE;
//         report->u.send_info.min_interval=0x00;
//         report->u.send_info.delta.u32=1;
//         if(esp_zb_zcl_update_reporting_info(report)!=ESP_OK) ESP_LOGE(TAG, "error conf update");
//     }else ESP_LOGE(TAG, "error filter");

// //     /* Create customized package tx endpoint */
// //     esp_zb_package_cfg_t package_cfg = ESP_ZB_DEFAULT_CONFIGURATION_TOOL_CONFIG();
// //     esp_zb_ep_list_t *esp_zb_tx_ep = custom_package_ep_create(ESP_TX_ENDPOINT, &package_cfg);

// //     /* Register the device */
// //     esp_zb_device_register(esp_zb_tx_ep);

// //     /* Config the reporting info  */
// //     esp_zb_zcl_reporting_info_t reporting_info = {
// //         .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
// //         .ep = ESP_TX_ENDPOINT,
// //         .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT,
// //         .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
// //         .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
// //         .u.send_info.min_interval = 1,
// //         .u.send_info.max_interval = 0,
// //         .u.send_info.def_min_interval = 1,
// //         .u.send_info.def_max_interval = 0,
// //         .u.send_info.delta.u16 = 1,
// //         .attr_id = ESP_ZB_ZCL_ATTR_CUSTOM_CIE_EP, //ESP_ZB_ZCL_ATTR_CUSTOM_PKG_VALUE_ID,
// //         .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
// //     };

// //     esp_zb_zcl_update_reporting_info(&reporting_info);
// }

/* -------------- */



static void esp_zb_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Entering ESP_ZB_TASK");
    // initialize Zigbee stack with Zigbee end-device config
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    uint8_t zcl_version, null_values;

    zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE;
    null_values = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE;

    // basic cluster create with fully customized
    esp_zb_attribute_list_t *basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &zcl_version);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &null_values);
    // esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, &modelid[0]);
    // esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, &manufname[0]);
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_IDENTIFIER));
    // identify cluster create with fully customized
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY);
    ESP_ERROR_CHECK(esp_zb_identify_cluster_add_attr(esp_zb_identify_cluster, ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID, &null_values));
    // group cluster create with fully customized
    esp_zb_attribute_list_t *esp_zb_groups_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_GROUPS);
    ESP_ERROR_CHECK(esp_zb_groups_cluster_add_attr(esp_zb_groups_cluster, ESP_ZB_ZCL_ATTR_GROUPS_NAME_SUPPORT_ID, &null_values));
    // scenes cluster create with standard cluster + customized
    esp_zb_attribute_list_t *esp_zb_scenes_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_SCENES);
    esp_zb_scenes_cluster_add_attr(esp_zb_scenes_cluster, ESP_ZB_ZCL_ATTR_SCENES_NAME_SUPPORT_ID, &null_values);
    esp_zb_scenes_cluster_add_attr(esp_zb_scenes_cluster, ESP_ZB_ZCL_ATTR_SCENES_CURRENT_GROUP_ID, &null_values);
    esp_zb_scenes_cluster_add_attr(esp_zb_scenes_cluster, ESP_ZB_ZCL_ATTR_SCENES_CURRENT_SCENE_ID, &null_values);
    esp_zb_scenes_cluster_add_attr(esp_zb_scenes_cluster, ESP_ZB_ZCL_ATTR_SCENES_SCENE_VALID_ID, &null_values);
    esp_zb_scenes_cluster_add_attr(esp_zb_scenes_cluster, ESP_ZB_ZCL_ATTR_SCENES_SCENE_COUNT_ID, &null_values);

    // create cluster lists for this endpoint
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

    // Analog input cluster (Currently used for flow sensor - should be replaced with flow measurement when ESP supports it)
    // from https://github.com/Bobstin/AutomatedBrewery/blob/b292b6e3d0344bba9a9dc35f3571ffdb034a176e/ESPZigbeeDemo/zigbee_cluster_demo/main/esp_zb_light.c
    // esp_zb_analog_input_cluster_cfg_t analog_input_cfg;
    // esp_zb_analog_value_cluster_cfg_t analog_value_cfg;
    // analog_value_cfg.out_of_service = 0;
    // analog_value_cfg.present_value = 0;
    // analog_value_cfg.status_flags = ESP_ZB_ZCL_ANALOG_VALUE_STATUS_FLAGS_DEFAULT_VALUE; // ESP_ZB_ZCL_ANALOG_INPUT_STATUS_FLAG_DEFAULT_VALUE;
    // esp_zb_attribute_list_t *esp_zb_analog_value_cluster = esp_zb_analog_value_cluster_create(&analog_value_cfg);
    // ESP_ERROR_CHECK(esp_zb_cluster_list_add_analog_value_cluster(cluster_list, esp_zb_analog_value_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    /* Custom cluster for CO2 ( standart cluster not working), solution only for HOMEd */
    // https://habr.com/ru/articles/759964/
    // https://github.com/lmahmutov/esp32_c6_co2_sensor/blob/main/main/esp_zigbee_co2.c
    // const uint16_t attr_id = 0;
    // const uint8_t attr_type = ESP_ZB_ZCL_ATTR_TYPE_U16;
    // const uint8_t attr_access = ESP_ZB_ZCL_ATTR_MANUF_SPEC | ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING;
    // esp_zb_attribute_list_t *custom_value_attributes_list = esp_zb_zcl_attr_list_create(VALUE_CUSTOM_CLUSTER);
    // esp_zb_custom_cluster_add_custom_attr(custom_value_attributes_list, attr_id, attr_type, attr_access, &null_values);
    // ESP_ERROR_CHECK(esp_zb_cluster_list_add_custom_cluster(cluster_list, custom_value_attributes_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    /* Create customized temperature sensor endpoint */
    esp_zb_temperature_sensor_cfg_t sensor_cfg = ESP_ZB_DEFAULT_TEMPERATURE_SENSOR_CONFIG();
    sensor_cfg.temp_meas_cfg.min_value = zb_temperature_to_s16(ESP_TEMP_SENSOR_MIN_VALUE);
    sensor_cfg.temp_meas_cfg.max_value = zb_temperature_to_s16(ESP_TEMP_SENSOR_MAX_VALUE);
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_temperature_meas_cluster(cluster_list, esp_zb_temperature_meas_cluster_create(&(sensor_cfg.temp_meas_cfg)), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    esp_zb_endpoint_config_t EPC = {
        .endpoint = HA_ESP_GALILEO_SENSOR_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, // ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID,
        .app_device_version = 1,
    };
    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
    // add created endpoint (cluster_list) to endpoint list
    esp_zb_ep_list_add_ep(esp_zb_ep_list, cluster_list, EPC);
    esp_zb_device_register(esp_zb_ep_list);

    // create_custom_endpoint();


    // esp_zb_zcl_reporting_info_t reporting_info = {
    //     .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
    //     .ep = HA_ESP_GALILEO_SENSOR_ENDPOINT,
    //     .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ANALOG_VALUE,
    //     .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    //     .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    //     .u.send_info.min_interval = 5,
    //     .u.send_info.max_interval = 10,
    //     //.u.send_info.def_min_interval = 1,
    //     //.u.send_info.def_max_interval = 0,
    //     //.u.send_info.delta.u32 = 1,
    //     .attr_id = ESP_ZB_ZCL_ATTR_ANALOG_VALUE_PRESENT_VALUE_ID, // ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
    //     .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
    // };
    // esp_zb_zcl_update_reporting_info(&reporting_info);

    // esp_zb_zcl_config_report_cmd_t report_cmd;
    // report_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    // report_cmd.zcl_basic_cmd.src_endpoint = HA_ESP_GALILEO_SENSOR_ENDPOINT;
    // report_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ANALOG_VALUE;

    // int16_t report_change = 1; /* report on each 1 changes */
    // esp_zb_zcl_config_report_record_t records[] = {
    //     {
    //         .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
    //         .attributeID = ESP_ZB_ZCL_ATTR_ANALOG_VALUE_PRESENT_VALUE_ID,
    //         .attrType = ESP_ZB_ZCL_ATTR_TYPE_S16,
    //         .min_interval = 0,
    //         .max_interval = 10,
    //         .reportable_change = &report_change,
    //     },
    // };
    // report_cmd.record_number = ARRAY_LENTH(records);
    // report_cmd.record_field = records;

    // esp_zb_lock_acquire(portMAX_DELAY);
    // esp_zb_zcl_config_report_cmd_req(&report_cmd);
    // esp_zb_lock_release();


    /* Config the reporting info  */
    // esp_zb_zcl_reporting_info_t reporting_info = {
    //     .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
    //     .ep = HA_ESP_GALILEO_SENSOR_ENDPOINT,
    //     .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
    //     .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    //     .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    //     .u.send_info.min_interval = 1,
    //     .u.send_info.max_interval = 0,
    //     .u.send_info.def_min_interval = 1,
    //     .u.send_info.def_max_interval = 0,
    //     .u.send_info.delta.u16 = 100,
    //     .attr_id = ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
    //     .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
    // };

    // esp_zb_zcl_update_reporting_info(&reporting_info);


    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);

    // Erase NVRAM before creating connection to new Coordinator
    // Comment out next line to erase NVRAM data if you are connecting to new Coordinator <----------------------------
    // esp_zb_nvram_erase_at_start(true);

    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
}

void update_attribute()
{
    uint16_t analog_value = 0;
    while (1)
    {
        if (1)
        {
            analog_value = analog_value + 1;
            ESP_LOGI(TAG, "analog_value %i", analog_value);
            // /* Write new temperature value */
            // esp_zb_zcl_status_t state_tmp = esp_zb_zcl_set_attribute_val(SENSOR_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &temperature, false);

            // /* Check for error */
            // if (state_tmp != ESP_ZB_ZCL_STATUS_SUCCESS)
            // {
            //     ESP_LOGE(TAG, "Setting temperature attribute failed!");
            // }

            // /* Write new humidity value */
            // esp_zb_zcl_status_t state_hum = esp_zb_zcl_set_attribute_val(SENSOR_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, &humidity, false);

            // /* Check for error */
            // if (state_hum != ESP_ZB_ZCL_STATUS_SUCCESS)
            // {
            //     ESP_LOGE(TAG, "Setting humidity attribute failed!");
            // }

            /* Write new analog value */
            // esp_zb_zcl_status_t state_analog = esp_zb_zcl_set_attribute_val(SENSOR_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_VALUE, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ANALOG_VALUE_PRESENT_VALUE_ID, &analog_value, false);

            // /* Check for error */
            // if (state_analog != ESP_ZB_ZCL_STATUS_SUCCESS)
            // {
            //     ESP_LOGE(TAG, "Setting analog attribute failed!");
            // }

            // if (CO2_value != 0)
            // {
            //     /* Write new CO2_value value */
            //     esp_zb_zcl_status_t state_co2 = esp_zb_zcl_set_attribute_val(SENSOR_ENDPOINT, CO2_CUSTOM_CLUSTER, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, 0, &CO2_value, false);

            //     /* Check for error */
            //     if (state_co2 != ESP_ZB_ZCL_STATUS_SUCCESS)
            //     {
            //         ESP_LOGE(TAG, "Setting CO2_value attribute failed!");
            //     }

            //     /* CO2 Cluster is custom and we must report it manually*/
            //     reportAttribute(SENSOR_ENDPOINT, CO2_CUSTOM_CLUSTER, 0, &CO2_value, 2);
            // }
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void led_task(void *pvParameters)
{
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);

    while (1)
    {
        gpio_set_level(LED, 0);
        vTaskDelay(led_time_ms / portTICK_PERIOD_MS);
        gpio_set_level(LED, 1);
        vTaskDelay(led_time_ms / portTICK_PERIOD_MS);
    }
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
    xTaskCreate(led_task, "LED", 4096, NULL, 5, NULL);
    // xTaskCreate(update_attribute, "update_attribute", 4096, NULL, 5, NULL);
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}