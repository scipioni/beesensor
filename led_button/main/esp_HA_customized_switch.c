#include "esp_HA_customized_switch.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "switch_driver.h"

#define LED 15

int led_state = 0; // Stato del LED (0 = spento, 1 = acceso)

// Handler per l'interrupt del pulsante
static void IRAM_ATTR gpio_isr_handler(void* arg) {
    led_state = !led_state;  // Inverti lo stato del LED
    gpio_set_level(LED, led_state);
}

static void configure_button() {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;  // Interrompi alla caduta del segnale
    io_conf.pin_bit_mask = (1ULL << GPIO_INPUT_IO_TOGGLE_SWITCH);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;  // Abilita la pull-up interna
    gpio_config(&io_conf);

    // Installa l'handler dell'interrupt per il pulsante
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_TOGGLE_SWITCH, gpio_isr_handler, (void*) GPIO_INPUT_IO_TOGGLE_SWITCH);
}

void app_main() {
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);  // Configura il LED come output
    configure_button();  // Configura il pulsante
    gpio_set_level(LED, led_state);  // Imposta lo stato iniziale del LED
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);  // Mantieni il task principale attivo
    }
}
