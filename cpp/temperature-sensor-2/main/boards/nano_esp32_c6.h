/*
 * Copyright 2026 Aethernet Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32C6 != 1
#  error "Illegal CPU! It must be an ESP32C6."
#endif

#include "hal/i2c_types.h"
#include "soc/gpio_num.h"

#ifndef BOARD_HAS_ULP
#  define BOARD_HAS_ULP 1
#endif
#ifndef BOARD_HAS_LED
#  define BOARD_HAS_LED 1
#endif
#ifndef STATUS_LED_PIN
#  define STATUS_LED_PIN GPIO_NUM_8
#endif
#ifndef RESET_BUTTON_PIN
#  define RESET_BUTTON_PIN GPIO_NUM_9
#endif
// --- Sensors ---
#define BOARD_HAS_SHTC3 0
#define BOARD_HAS_SHT45 1
#define BOARD_HAS_STCC4 0
#define BOARD_HAS_BME688 0
// --- Hardware Settings ---
#define SENSOR_SDA_PIN GPIO_NUM_6
#define SENSOR_SCL_PIN GPIO_NUM_7
// FIX 1: Use a Fixed Buffer instead of VLA (Variable Length Array) to prevent
// stack smash
#define MAX_I2C_BUFFER 64
