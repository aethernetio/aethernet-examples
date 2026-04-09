
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
#include "user_config.h"

#include "aether/all.h"

#if ULP_SLEEP == 1

#  include <freertos/FreeRTOS.h>
#  include <freertos/task.h>

#  include <esp_log.h>
#  include <esp_sleep.h>
#  include <esp_timer.h>

#  include <ulp_lp_core.h>
#  include <lp_core_i2c.h>
#  include "ulp_main.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[] asm("_binary_ulp_main_bin_end");

static void lp_core_init(void) {
  esp_err_t ret = ESP_OK;

  ulp_lp_core_cfg_t cfg = {.wakeup_source = ULP_LP_CORE_WAKEUP_SOURCE_LP_TIMER,
                           .lp_timer_sleep_duration_us = 1000000};

  ret = ulp_lp_core_load_binary(ulp_main_bin_start,
                                (ulp_main_bin_end - ulp_main_bin_start));
  if (ret != ESP_OK) {
    std::cout << ae::Format("LP Core load failed!\n");
    abort();
  }

  ret = ulp_lp_core_run(&cfg);
  if (ret != ESP_OK) {
    std::cout << ae::Format("LP Core run failed!\n");
    abort();
  }

  std::cout << ae::Format("LP core loaded with firmware successfully!\n");
}

#  ifndef LP_I2C_SDA_IO
#    define LP_I2C_SDA_IO SENSOR_SDA_PIN
#  endif

#  ifndef LP_I2C_SCL_IO
#    define LP_I2C_SCL_IO SENSOR_SCL_PIN
#  endif

static void lp_i2c_init(void) {
  esp_err_t ret = ESP_OK;

  /* Initialize LP I2C with default configuration */
  lp_core_i2c_cfg_t i2c_cfg{};
  lp_core_i2c_timing_cfg_t i2c_timing_cfg{};

  i2c_timing_cfg.clk_speed_hz = 100000;

  i2c_cfg.i2c_pin_cfg.sda_io_num = LP_I2C_SDA_IO;
  i2c_cfg.i2c_pin_cfg.scl_io_num = LP_I2C_SCL_IO;
  i2c_cfg.i2c_pin_cfg.sda_pullup_en = true;
  i2c_cfg.i2c_pin_cfg.scl_pullup_en = true;
  i2c_cfg.i2c_timing_cfg = i2c_timing_cfg;
  i2c_cfg.i2c_src_clk = LP_I2C_SCLK_LP_FAST;

  ret = lp_core_i2c_master_init(LP_I2C_NUM_0, &i2c_cfg);
  if (ret != ESP_OK) {
    std::cout << ae::Format("LP I2C init failed!\n");
    abort();
  }

  std::cout << ae::Format("LP I2C initialized successfully!\n");
}

int DeepSleep(time_point, time_point hard_sleep_tp,
              std::int16_t temperature_threshold) {
  /* Initialize LP_I2C from the main processor */
  lp_i2c_init();
  /* Load LP Core binary and start the coprocessor */
  lp_core_init();

  vTaskDelay(pdMS_TO_TICKS(1));

  ulp_wakeup_temp_threshold = static_cast<uint32_t>(temperature_threshold);
  ulp_can_start = 1;

  // sleep either until hard sleep or ulp wakeup
  auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(
                     hard_sleep_tp - std::chrono::system_clock::now())
                     .count();
  esp_sleep_enable_timer_wakeup(time_us);
  esp_sleep_enable_ulp_wakeup();

  std::cout << ae::Format("Timer wakeup enabled: {} us\n", time_us);

  std::cout << ae::Format("Entering deep sleep...\n");
  // preserve RTC memory
#  if SOC_PM_SUPPORT_RTC_SLOW_MEM_PD
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
#  endif
#  if SOC_PM_SUPPORT_RTC_FAST_MEM_PD
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);
#  endif

  esp_deep_sleep_start();

  return 0;
}
#endif
