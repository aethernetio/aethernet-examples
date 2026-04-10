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

#if defined ESP_PLATFORM && !defined IS_ULP_COCPU
#  include <freertos/FreeRTOS.h>
#  include <freertos/task.h>

#  include "driver/i2c.h"
#  include "esp_log.h"

esp_err_t i2c_init(i2c_port_t port, int sda_pin, int scl_pin) {
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = sda_pin,
      .scl_io_num = scl_pin,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master = {.clk_speed = 100000},
      .clk_flags = 0,
  };
  if (i2c_param_config(port, &conf) != ESP_OK) {
    return ESP_ERR_INVALID_STATE;
  }
  return i2c_driver_install(port, conf.mode, 0, 0, 0);
}

esp_err_t i2c_write(i2c_port_t port, uint8_t address, uint8_t const* data,
                    uint8_t len, int32_t ms_dur) {
  return i2c_master_write_to_device(port, address, data, len,
                                    ms_dur);
}

esp_err_t i2c_read(i2c_port_t port, uint8_t address, uint8_t* data, uint8_t len,
                   int32_t ms_dur) {
  return i2c_master_read_from_device(port, address, data, len, ms_dur);
}

esp_err_t i2c_write_read(i2c_port_t port, uint8_t address,
                         uint8_t const* write_data, uint8_t write_len,
                         uint8_t* read_data, uint8_t read_len, int32_t ms_dur) {
  return i2c_master_write_read_device(port, address, write_data, write_len,
                                      read_data, read_len,
                                      ms_dur);
}

void wait_for(int32_t us_dur) {
  // wait min 1 tick
  static TickType_t one_tick_ms = pdTICKS_TO_MS(1);
  uint32_t ms_dur = us_dur / 1000U;
  if (ms_dur < one_tick_ms) {
    ms_dur = (uint32_t)one_tick_ms;
  }
  vTaskDelay(pdMS_TO_TICKS(ms_dur));
}
#endif
