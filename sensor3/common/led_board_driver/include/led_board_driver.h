#pragma once

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // typedef void (*esp_temp_sensor_callback_t)(float temperature);

    esp_err_t led_board_driver_init(uint16_t pin);
    void led_on();
    void led_off();

#ifdef __cplusplus
} // extern "C"
#endif
