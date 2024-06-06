#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LED GPIO_NUM_15

void app_main()
{
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);

    while (1)
    {
        gpio_set_level(LED, 0);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        gpio_set_level(LED, 1);
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}