
#include "led_board_driver.h"

#include "esp_err.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* temperatuer sensor instance handle */
// static temperature_sensor_handle_t temp_sensor;
// /* call back function pointer */
// static esp_temp_sensor_callback_t func_ptr;
// /* update interval in seconds */
static uint16_t gpio_pin = 15;
static uint16_t led_time_ms = 500;

static const char *TAG = "ESP_LED_DRIVER";

static void led_update(void *arg)
{
    for (;;)
    {
        gpio_set_level(gpio_pin, 0);
        vTaskDelay(led_time_ms / portTICK_PERIOD_MS);
        gpio_set_level(gpio_pin, 1);
        vTaskDelay(led_time_ms / portTICK_PERIOD_MS);
    }
}

void led_on() {
    led_time_ms = 100;
}

void led_off() {
    led_time_ms = 500;
}

// static esp_err_t _init()
// {
//     ESP_RETURN_ON_ERROR(
//         gpio_set_direction(LED, GPIO_MODE_OUTPUT)
//     )

//     return (xTaskCreate(led_update, "sensor_update", 2048, NULL, 10, NULL) == pdTRUE) ? ESP_OK : ESP_FAIL;
// }

esp_err_t led_board_driver_init(uint16_t pin)
{
    ESP_RETURN_ON_ERROR(
        gpio_set_direction(gpio_pin, GPIO_MODE_OUTPUT),
        TAG, "Fail to install on-chip temperature sensor");
    // if (ESP_OK != temp_sensor_driver_sensor_init(config)) {
    //     return ESP_FAIL;
    // }
    gpio_pin = pin;
    // return ESP_OK;
    return (xTaskCreate(led_update, "led_update", 2048, NULL, 10, NULL) == pdTRUE) ? ESP_OK : ESP_FAIL;
}
