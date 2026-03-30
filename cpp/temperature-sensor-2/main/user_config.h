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

#if ESP_PLATFORM
#  include "sdkconfig.h"
#endif
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

#if ESP_PLATFORM
// Select the board to build example for
#  define BOARD_AETHER_ESP32_C6 0
#  define BOARD_FIRE_BEETLE2_С6 1
#  define BOARD_NANO_ESP32_C6 2
#  define BOARD_WROVER_ESP32 3
#  define BOARD_M5STACK_ATOM_LITE 4

#  define BOARD BOARD_NANO_ESP32_C6

#  if BOARD == BOARD_AETHER_ESP32_C6
#    include "boards/aether_esp32_c6.h"
#  elif BOARD == BOARD_M5STACK_ATOM_LITE
#    include "boards/m5stack_atom_lite.h"
#  elif BOARD == BOARD_NANO_ESP32_C6
#    include "boards/nano_esp32_c6.h"
#  endif
#endif
// fallback to random sensor if no board has a sensor
#if (BOARD_HAS_ULP != 1 || IS_ULP_COCPU) && (BOARD_HAS_BME688 != 1)
#  define TEMP_SENSOR_RANDON 1
#endif

#if BOARD_HAS_ULP == 1
#  define ULP_SLEEP 1
#elif defined ESP_PLATFORM
#  define ESP_MAIN_SLEEP 1
#else
#  define THREAD_SLEEP 1
#endif

#endif  // USER_CONFIG_H_
