
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

#define SLOWTIME 500
static uint16_t gpio_pin = 15;
// static uint16_t led_time_ms = 500;
static uint16_t led_time_off_ms = SLOWTIME;
static uint16_t led_time_on_ms = SLOWTIME;

static const char *TAG = "ESP_LED_DRIVER";

static void led_update(void *arg)
{
    for (;;)
    {
        if (led_time_on_ms > 0)
        {
            gpio_set_level(gpio_pin, 1);
            vTaskDelay(led_time_on_ms / portTICK_PERIOD_MS);
        }
        if (led_time_off_ms > 0)
        {
            gpio_set_level(gpio_pin, 0);
            vTaskDelay(led_time_off_ms / portTICK_PERIOD_MS);
        }
        if (led_time_off_ms + led_time_on_ms == 0)
        {
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }
}

static void led_blink_and_off_task()
{
    led_blink_fast();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    led_off();
    vTaskDelete(NULL);
}

void led_blink_and_off()
{
    xTaskCreate(led_blink_and_off_task, "led_blink", 1024, NULL, 1, NULL);
}

void led_on()
{
    led_time_on_ms = SLOWTIME;
    led_time_off_ms = 0;
}

void led_off()
{
    led_time_on_ms = 0;
    led_time_off_ms = SLOWTIME;
}

void led_blink_fast()
{
    led_time_on_ms = 100;
    led_time_off_ms = 100;
}

void led_blink_slow()
{
    led_time_on_ms = SLOWTIME;
    led_time_off_ms = SLOWTIME;
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
    xTaskCreate(led_update, "led_update", 2048, NULL, 1, NULL);
    led_blink_and_off();
    return ESP_OK;
}
