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

#include "temp_sensor_driver.h"
#include "led_board_driver.h"
#include "battery_sensor.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "led_strip.h"  
// to get  "led_strip.h": git clone https://github.com/espressif/idf-extra-components.git then comy the folder in flow_sensor_YF-B5/common/led_strip
// then edit flow_sensor_YF-B5/CMakeLists.txt to add the new folder ...
//  ${CMAKE_CURRENT_SOURCE_DIR}/common/led_strip

//https://www.seeedstudio.com/blog/2020/05/11/how-to-use-water-flow-sensor-with-arduino/

bool connected = false;
const int wakeup_time_sec = 600;

static const char *TAG = "GALILEO_SENSOR";
float_t counter = 1.0;
int sensor_flow = 0;
volatile int16_t liter_per_minute = 0;
volatile float liter_in_the_last_60_seconds = 0;
int16_t seconds_between_calls = 20;  //this should be set to 60 seconds to make the flow in liter per minute


// for onboard ESP32-C6-DevKitC-1-N8  rgb led
static uint8_t s_led_state = 0;
#define BLINK_GPIO GPIO_NUM_8
#define CONFIG_BLINK_LED_STRIP 1
#define CONFIG_BLINK_LED_STRIP_BACKEND_SPI 1



#ifdef CONFIG_BLINK_LED_STRIP

static led_strip_handle_t led_strip;

static void set_led(uint8_t led_status)
{
    /* If the addressable LED is enabled */
    if (led_status) {
        /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
        led_strip_set_pixel(led_strip, 0, 16, 16, 16);
        /* Refresh the strip to send data */
        led_strip_refresh(led_strip);
    } else {
        /* Set all LED off to clear all pixels */
        led_strip_clear(led_strip);
    }
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink addressable LED!");
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1, // at least one LED on board
    };
#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
#elif CONFIG_BLINK_LED_STRIP_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
#else
#error "unsupported LED strip backend"
#endif
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
}

#elif CONFIG_BLINK_LED_GPIO

static void blink_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

#else
#error "unsupported LED type"
#endif




// end of  onboard ESP32-C6-DevKitC-1-N8  rgb led













// interrupt on flow sensor pin part

int  FLOW_SENSOR_INPUT_PIN = 18;
volatile uint32_t number_of_sensor_clicks = 0;

// Declare gpio_evt_queue as a QueueHandle_t, not an int
static QueueHandle_t gpio_evt_queue = NULL;


// Interrupt service routine (ISR)
static void IRAM_ATTR button_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}


// Task to handle the flow sensor change status event
void button_task(void *arg) {
    while(1) {
        if(xQueueReceive(gpio_evt_queue, &FLOW_SENSOR_INPUT_PIN, portMAX_DELAY)) {
            // This is now safe to log outside the ISR
            number_of_sensor_clicks ++;
            ESP_LOGI(TAG, "Sensor state changed:%lu",number_of_sensor_clicks);
            set_led(s_led_state);
            /* Toggle the LED state */
            s_led_state = !s_led_state;
        }
    }
}



void timer_callback(void *param)  //this callback will be run every seconds_between_calls
{
  ESP_LOGI(TAG,"timer called!");
  liter_per_minute = (number_of_sensor_clicks*60)/6.6;   //Frequency: F=6.6*Q   (Q=L/MIN)    https://electronics.stackexchange.com/questions/701710/trying-to-use-a-flow-meter-to-measure-volume
  //since liter_per_minute is updated evvery 60 seconds, the number should be moltiplied by 60 to make the unit in hertz
 // liter_in_the_last_60_seconds = number_of_sensor_clicks / 396;
  ESP_LOGI(TAG,"number_of_sensor_clicks: %lu!",number_of_sensor_clicks);
  ESP_LOGI(TAG,"liter_per_minute: %u!",liter_per_minute);
  //ESP_LOGI(TAG,"liter_in_the_last_60_seconds: %f!",liter_in_the_last_60_seconds);

  number_of_sensor_clicks = 0;


}






// interrupt on flow sensor pin part end



static void esp_app_temp_sensor_handler(float temperature)
{
    int16_t measured_value = zb_temperature_to_s16(liter_per_minute);
//    int16_t measured_value = zb_temperature_to_s16(temperature);

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
                    //led_on();
                    set_led(1);
                    
                }
                else
                {
                    //led_off();
                    set_led(0);
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
    case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) default callback", callback_id);
        break;
    }
    return ret;
}

static void reset_and_reboot(void *arg, void *data)
{
    // ESP_EARLY_LOGI
    ESP_LOGI(TAG, "Button event %s", button_event_table[(button_event_t)data]);
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
    esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000);
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
    //led_off();
    //set_led(0);
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



    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
    esp_zb_ep_list_add_ep(esp_zb_ep_list, cluster_list, EPC);
    esp_zb_ep_list_add_ep(esp_zb_ep_list, cluster_list_vin, EPC_VIN);

    esp_zb_device_register(esp_zb_ep_list);

    esp_zb_core_action_handler_register(zb_action_handler);
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
            esp_zb_lock_release();
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
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
    battery_sensor_init();
    xTaskCreate(update_attributes, "update_attributes", 4096, NULL, 5, NULL);
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);

// timer creation part
    const esp_timer_create_args_t my_timer_args = {
        .callback = &timer_callback,
        .name = "My Timer"};
    esp_timer_handle_t timer_handler;
    ESP_ERROR_CHECK(esp_timer_create(&my_timer_args, &timer_handler)); 
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handler, seconds_between_calls*1000000));  // 1000000 is microseconds
//end timer creation part

/* Configure the peripheral according to the LED type */
    configure_led();  //rgb led setup

// flow sensor part


    // Configure the GPIO for the button
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,   // Trigger on falling edge (button press)
        .mode = GPIO_MODE_INPUT,          // Set as input mode
        .pin_bit_mask = (1ULL << FLOW_SENSOR_INPUT_PIN), // Bitmask for the GPIO pin
        .pull_up_en = GPIO_PULLUP_ENABLE, // Enable pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };

    // Apply the GPIO configuration
    gpio_config(&io_conf);

    // Create a queue to handle GPIO events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));  // Correctly declare the type as QueueHandle_t

    // Install the ISR service with default flag 0
    gpio_install_isr_service(0);

    // Attach the interrupt handler
    gpio_isr_handler_add(FLOW_SENSOR_INPUT_PIN, button_isr_handler, (void*) FLOW_SENSOR_INPUT_PIN);

    // Create a task to handle the button press event
    xTaskCreate(button_task, "button_task", 4048, NULL, 10, NULL);


// end of flow sensor part


}
