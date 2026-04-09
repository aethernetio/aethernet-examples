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

#include "sleeping/sleeping.h"

#include <chrono>

#include "aether/all.h"

#if ESP_MAIN_SLEEP == 1

#  include <esp_sleep.h>
#  include <esp_timer.h>
#  include <esp_log.h>
#  include <hal/uart_types.h>
#  include <soc/gpio_num.h>

// Conditional includes based on chip
#  if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || \
      CONFIG_IDF_TARGET_ESP32S3
#    include <driver/rtc_io.h>
#    include <driver/gpio.h>
#  endif

#  if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || \
      CONFIG_IDF_TARGET_ESP32H2 || CONFIG_IDF_TARGET_ESP32C2
#    include <driver/rtc_io.h>
#    include <driver/gpio.h>
#  endif

#  if CONFIG_IDF_TARGET_ESP32P4
// P4 specific headers if needed
#  endif

static const char* TAG = "ESP_MAIN_SLEEP";

int DeepSleep(time_point soft_sleep_tp, time_point, std::int16_t) {
  auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(
                     soft_sleep_tp - std::chrono::system_clock::now())
                     .count();

  esp_sleep_enable_timer_wakeup(time_us);
  ESP_LOGI(TAG, "Timer wakeup enabled: %llu us", time_us);

  ESP_LOGI(TAG, "Entering deep sleep...");
  // preserve RTC memory
#  if SOC_PM_SUPPORT_RTC_SLOW_MEM_PD
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
#  endif
#  if SOC_PM_SUPPORT_RTC_FAST_MEM_PD
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);
#  endif

  // Enter deep sleep
  esp_err_t ret = esp_deep_sleep_try_to_start();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Deep sleep failed: %s", esp_err_to_name(ret));
  }

  return ret;  // Should be never reached
}
#endif
