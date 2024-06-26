#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_adc/adc_oneshot.h"
#include "hal/adc_types.h"
#include "esp_adc/adc_cali.h"
#include "driver/gpio.h"

static const char *TAG = "ADC_BATTERY_EXAMPLE";

#define BATTERY_ADC_CHANNEL ADC_CHANNEL_0  // GPIO0 is connected to ADC1_CH0
#define BATTERY_GPIO_NUM GPIO_NUM_0       // GPIO0
#define MIN_BATTERY_VOLTAGE 1500 // Adjust this based on your battery's minimum voltage (1.5V)
#define MAX_BATTERY_VOLTAGE 3300 // Adjust this based on your battery's maximum voltage (3.3V)
#define BATTERY_DISCONNECT_THRESHOLD 100 // Voltage below this is considered disconnected

// Function to apply manual calibration
int apply_manual_calibration(int raw_value) {
    // Example linear calibration. Adjust these values based on your measurements
    const float scale = 1.0f;  // Adjust this if the voltage range seems off
    const int offset = 0;      // Adjust this if there's a consistent offset in the readings
    return (int)((float)raw_value * scale) + offset;
}

void app_main(void)
{
    esp_err_t ret;
    int adc_read_battery;
    int battery_voltage_mv;

    adc_oneshot_unit_handle_t handle = NULL;
    adc_cali_handle_t cali_handle = NULL;

    // Configure GPIO for input with pull-down
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BATTERY_GPIO_NUM),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    ESP_LOGI(TAG, "Initializing ADC unit...");
    ret = adc_oneshot_new_unit(&init_config1, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit. Error: %s", esp_err_to_name(ret));
        return;
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };

    ESP_LOGI(TAG, "Configuring ADC channel...");
    ret = adc_oneshot_config_channel(handle, ADC_CHANNEL_4, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel. Error: %s", esp_err_to_name(ret));
        return;
    }

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_LOGI(TAG, "Creating ADC calibration scheme...");
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC calibration scheme. Error: %s", esp_err_to_name(ret));
        return;
    }

    while(1)
    {
        ESP_LOGI(TAG, "Reading ADC...");
        ret = adc_oneshot_read(handle, ADC_CHANNEL_4, &adc_read_battery);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read ADC. Error: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Battery ADC raw read result: %d", adc_read_battery);
        
            ret = adc_cali_raw_to_voltage(cali_handle, adc_read_battery, &battery_voltage_mv);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to convert raw ADC to voltage. Error: %s", esp_err_to_name(ret));
            } else {
                // Apply manual calibration
                battery_voltage_mv = apply_manual_calibration(battery_voltage_mv);
                
                ESP_LOGI(TAG, "Battery voltage (after calibration): %d mV", battery_voltage_mv);

                if (battery_voltage_mv < BATTERY_DISCONNECT_THRESHOLD) {
                    ESP_LOGI(TAG, "Battery appears to be disconnected");
                } else {
                    // Calculate battery percentage
                    float battery_percentage = (battery_voltage_mv - MIN_BATTERY_VOLTAGE) / (float)(MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE) * 100;
                    if (battery_percentage > 100) battery_percentage = 100;
                    if (battery_percentage < 0) battery_percentage = 0;
            
                    ESP_LOGI(TAG, "Battery percentage: %.2f%%", battery_percentage);
                    
                    // Additional debug information
                    ESP_LOGI(TAG, "Debug: MIN_BATTERY_VOLTAGE: %d, MAX_BATTERY_VOLTAGE: %d", MIN_BATTERY_VOLTAGE, MAX_BATTERY_VOLTAGE);
                    ESP_LOGI(TAG, "Debug: Voltage difference from min: %d mV", battery_voltage_mv - MIN_BATTERY_VOLTAGE);
                    ESP_LOGI(TAG, "Debug: Voltage range: %d mV", MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(800)); // Read every 5 seconds
    }
}
