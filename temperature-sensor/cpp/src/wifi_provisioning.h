/*
 * Copyright 2025 Aethernet Inc.
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

#ifndef WIFI_PROVISIONING_H_
#define WIFI_PROVISIONING_H_

// configs used for wifi provisioning

// Wifi access point name
#if not defined WP_APP_NAME
#  define WP_APP_NAME "AetherTempSensor"
#endif

// Redirect URL redirect after credentials entered
#if not defined WP_REDIR_URL
#  define WP_REDIR_URL "https://aethernet.io"
// TODO: add this url by config
// "https://aethernet.io/temp_test_plain.html?id=%s";
#endif

#if not defined STATUS_LED_PIN
#  define STATUS_LED_PIN GPIO_NUM_35
#endif
#if not defined RESET_BUTTON_PIN
#  define RESET_BUTTON_PIN GPIO_NUM_0
#endif

/**
 * \brief Wifi provisioning is used for wifi configuration during runtime.
 * Wifi credentials are saved to the flash memory.
 */
bool WifiProvisioning();

#endif  // WIFI_PROVISIONING_H_
