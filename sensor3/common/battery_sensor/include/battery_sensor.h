#pragma once

#include "esp_err.h"

#define BATTERY_POLL_MS 5000
#define ADC_ATTEN ADC_ATTEN_DB_12
#define ADC_CHANNEL ADC_CHANNEL_0

extern int adc_raw[2][10];
extern float battery_voltage;

#ifdef __cplusplus
extern "C"
{
#endif

    // typedef void (*esp_temp_sensor_callback_t)(float temperature);
    esp_err_t battery_sensor_init();

#ifdef __cplusplus
} // extern "C"
#endif
