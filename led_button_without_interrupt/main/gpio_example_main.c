#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LED 15
#define BUTTON GPIO_NUM_9

int led_state = 0; //Led State
int last_button_state = 1; // previous button state

static void configure_button() {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;  // Disable interrupt
    io_conf.pin_bit_mask = (1ULL << BUTTON);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;  // enable internal pull-up 
    gpio_config(&io_conf);
}

void app_main() {
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);  // configure  LED pin as output
    configure_button();  // Configure button
    gpio_set_level(LED, led_state);  // set initial led state

    while (1) {
        int button_state = gpio_get_level(BUTTON);
        if (button_state == 0 && last_button_state == 1) {  // get the falling state of the button
            led_state = !led_state;  // change the led status
            gpio_set_level(LED, led_state);
        }
        last_button_state = button_state;
        vTaskDelay(10 / portTICK_PERIOD_MS);  // small debounce delay
    }
}
