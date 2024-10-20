/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Zigbee HA_on_off_light Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#include "esp_zigbee_core.h"
#include "esp_idf_version.h"

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE false /* enable the install code policy for security */
#define ED_AGING_TIMEOUT ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE 3000                                               /* 3000 millisecond */
#define HA_ESP_GALILEO_SENSOR_ENDPOINT 10                                         /* esp light bulb device endpoint, used to process light controlling commands */
#define HA_VIN_ENDPOINT 11     

#define SENSOR_ENDPOINT 10                                               // per adesso uguale a HA_ESP_GALILEO_SENSOR_ENDPOINT
#define ESP_ZB_PRIMARY_CHANNEL_MASK ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK /* Zigbee primary channel mask use in the example */

#define ESP_ZB_ZED_CONFIG()                               \
    {                                                     \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,         \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE, \
        .nwk_cfg.zed_cfg = {                              \
            .ed_timeout = ED_AGING_TIMEOUT,               \
            .keep_alive = ED_KEEP_ALIVE,                  \
        },                                                \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()       \
    {                                       \
        .radio_mode = ZB_RADIO_MODE_NATIVE, \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                          \
    {                                                         \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE, \
    }

#define MANUFACTURER_NAME "\x07" \
                          "Galileo" // numero caratteri
#define MODEL_IDENTIFIER "\x0E" \
                         "galileo.sensor" // numero caratteri

#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32H2 || CONFIG_IDF_TARGET_ESP32C6
#define BOOT_BUTTON_NUM 9
#else
#define BOOT_BUTTON_NUM 0
#endif

#define BUTTON_ACTIVE_LEVEL 0

const char *button_event_table[] = {
    "BUTTON_PRESS_DOWN",
    "BUTTON_PRESS_UP",
    "BUTTON_PRESS_REPEAT",
    "BUTTON_PRESS_REPEAT_DONE",
    "BUTTON_SINGLE_CLICK",
    "BUTTON_DOUBLE_CLICK",
    "BUTTON_MULTIPLE_CLICK",
    "BUTTON_LONG_PRESS_START",
    "BUTTON_LONG_PRESS_HOLD",
    "BUTTON_LONG_PRESS_UP",
};

#define LED 15

// In the SDK, there is a special function to create custom clusters (not described in the documentation), but there is a restriction they should be the type of 0xFxxx other variants in the SDK prohibited.
#define CLUSTER_ID_CUSTOM 0xFFF2 /* Custom cluster used because standard cluster not working */
#define CO2_CUSTOM_CLUSTER              0xFFF2


#define ESP_TEMP_SENSOR_UPDATE_INTERVAL (10)     /* Local sensor update interval (second) */
#define ESP_TEMP_SENSOR_MIN_VALUE       (-10)   /* Local sensor min measured value (degree Celsius) */
#define ESP_TEMP_SENSOR_MAX_VALUE       (80)    /* Local sensor max measured value (degree Celsius) */

