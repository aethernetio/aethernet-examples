/*
 * Copyright 2024 Aethernet Inc.
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

#ifndef USER_CONFIG_H_
#define USER_CONFIG_H_

#include "sdkconfig.h"

#include "aether/config_consts.h"

#define AE_CRYPTO_ASYNC AE_HYDRO_CRYPTO_PK
#define AE_CRYPTO_SYNC AE_HYDRO_CRYPTO_SK
#define AE_SIGNATURE AE_HYDRO_SIGNATURE
#define AE_KDF AE_HYDRO_KDF

#if AE_DISTILLATION || AE_FILTRATION
#  define AE_SUPPORT_REGISTRATION 1
#  define AE_SUPPORT_CLOUD_DNS 1
#else
#  define AE_SUPPORT_REGISTRATION 0
#  define AE_SUPPORT_CLOUD_DNS 0
#endif

#define AE_SUPPORT_SPIFS_FS 1

// telemetry
#define AE_TELE_ENABLED 1
#define AE_TELE_LOG_CONSOLE 1

#define AE_STATISTICS_MAX_SIZE 1024

#define AE_TELE_DEBUG_MODULES AE_ALL

#define BOARD_AETHER_ESP32_C6    0
#define BOARD_FIRE_BEETLE2_С6    1
#define BOARD_NANO_ESP32_C6      2
#define BOARD_WROVER_ESP32       3
#define BOARD_M5STACK_ATOM_LITE  4

#define BOARD BOARD_AETHER_ESP32_C6

#if BOARD == BOARD_AETHER_ESP32_C6
#  if CONFIG_IDF_TARGET_ESP32C6 != 1
#    error "Illegal CPU! It must be an ESP32C6."
#  endif
#  if not defined BOARD_HAS_SLEEP_MANAGER
#    define BOARD_HAS_SLEEP_MANAGER 1
#  endif
#  if not defined BOARD_HAS_ULP
#    define BOARD_HAS_ULP 0
#  endif
#  if not defined BOARD_HAS_LED
#    define BOARD_HAS_LED 1
#  endif
#  if not defined STATUS_LED_PIN
#    define STATUS_LED_PIN GPIO_NUM_7
#  endif
#  if not defined RESET_BUTTON_PIN
#    define RESET_BUTTON_PIN GPIO_NUM_9
#  endif
// --- Sensors ---
#  define BOARD_HAS_SHTC3  0
#  define BOARD_HAS_SHT45  1
#  define BOARD_HAS_STCC4  1
#  define BOARD_HAS_BME688 0
// --- Hardware Settings ---
#  define BME_I2C_NUM I2C_NUM_0
#  define BME_SDA_PIN 19
#  define BME_SCL_PIN 18
// FIX 1: Use a Fixed Buffer instead of VLA (Variable Length Array) to prevent
// stack smash
#    define MAX_I2C_BUFFER 64
#endif

#if BOARD == BOARD_M5STACK_ATOM_LITE
#  if CONFIG_IDF_TARGET_ESP32C6 != 1
#    error "Illegal CPU! It must be an ESP32C6."
#  endif
#  if not defined BOARD_HAS_SLEEP_MANAGER
#    define BOARD_HAS_SLEEP_MANAGER 1
#  endif
#  if not defined BOARD_HAS_ULP
#    define BOARD_HAS_ULP 0
#  endif
#  if not defined BOARD_HAS_LED
#    define BOARD_HAS_LED 1
#  endif
#  if not defined STATUS_LED_PIN
#    define STATUS_LED_PIN GPIO_NUM_35
#  endif
#  if not defined RESET_BUTTON_PIN
#    define RESET_BUTTON_PIN GPIO_NUM_41
#  endif
// --- Sensors ---
#  define BOARD_HAS_SHTC3  0
#  define BOARD_HAS_SHT45  0
#  define BOARD_HAS_STCC4  0
#  define BOARD_HAS_BME688 1
// --- Hardware Settings ---
#  define BME_I2C_NUM I2C_NUM_0
#  define BME_SDA_PIN 2
#  define BME_SCL_PIN 1
// FIX 1: Use a Fixed Buffer instead of VLA (Variable Length Array) to prevent
// stack smash
#  define MAX_I2C_BUFFER 64
#endif

#if BOARD == BOARD_NANO_ESP32_C6
#  if CONFIG_IDF_TARGET_ESP32C6 != 1
#    error "Illegal CPU! It must be an ESP32C6."
#  endif
#  if not defined BOARD_HAS_SLEEP_MANAGER
#    define BOARD_HAS_SLEEP_MANAGER 1
#  endif
#  if not defined BOARD_HAS_ULP
#    define BOARD_HAS_ULP 1
#  endif
#  if not defined BOARD_HAS_LED
#    define BOARD_HAS_LED 1
#  endif
#  if not defined STATUS_LED_PIN
#    define STATUS_LED_PIN GPIO_NUM_8
#  endif
#  if not defined RESET_BUTTON_PIN
#    define RESET_BUTTON_PIN GPIO_NUM_9
#  endif
// --- Sensors ---
#  define BOARD_HAS_SHTC3  0
#  define BOARD_HAS_SHT45  0
#  define BOARD_HAS_STCC4  0
#  define BOARD_HAS_BME688 1
// --- Hardware Settings ---
#  define BME_I2C_NUM I2C_NUM_0
#  define BME_SDA_PIN 19
#  define BME_SCL_PIN 18
// FIX 1: Use a Fixed Buffer instead of VLA (Variable Length Array) to prevent
// stack smash
#  define MAX_I2C_BUFFER 64
#endif

#endif  // USER_CONFIG_H_
