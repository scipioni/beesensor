#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_sleep.h"

#define LED GPIO_NUM_15
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  10

void led()
{
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
    int a = 0;
    while(a < 8)
    /*
    abbiamo deciso di prolungare il tempo di wakeup per
    evitare problemmi di riprogrammazione della scheda
    */
    {
        gpio_set_level(LED, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(LED, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        a++;
    }
}

void deepslyp()
{
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP*uS_TO_S_FACTOR);
    esp_deep_sleep_start();
}

void app_main()
{
    led();
    deepslyp();
}
