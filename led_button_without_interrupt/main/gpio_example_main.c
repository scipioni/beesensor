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

int led_state = 0; // Stato del LED (0 = spento, 1 = acceso)
int last_button_state = 1; // Stato precedente del pulsante

static void configure_button() {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;  // Disabilita l'interrupt
    io_conf.pin_bit_mask = (1ULL << BUTTON);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;  // Abilita la pull-up interna
    gpio_config(&io_conf);
}

void app_main() {
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);  // Configura il LED come output
    configure_button();  // Configura il pulsante
    gpio_set_level(LED, led_state);  // Imposta lo stato iniziale del LED

    while (1) {
        int button_state = gpio_get_level(BUTTON);
        if (button_state == 0 && last_button_state == 1) {  // Rileva la caduta del segnale
            led_state = !led_state;  // Inverti lo stato del LED
            gpio_set_level(LED, led_state);
        }
        last_button_state = button_state;
        vTaskDelay(10 / portTICK_PERIOD_MS);  // Aggiungi un piccolo ritardo per il debounce
    }
}
