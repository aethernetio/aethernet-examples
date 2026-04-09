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

#if defined ESP_PLATFORM

#  include <esp_err.h>
#  include <driver/i2c.h>
#  include <esp_log.h>

#  ifdef __cplusplus
extern "C" {
#  endif

esp_err_t i2c_init(i2c_port_t port, int sda_pin, int scl_pin);

esp_err_t i2c_write(i2c_port_t port, uint8_t address, uint8_t const* data,
                    uint8_t len, int32_t ms_dur);
esp_err_t i2c_read(i2c_port_t port, uint8_t address, uint8_t* data, uint8_t len,
                   int32_t ms_dur);
esp_err_t i2c_write_read(i2c_port_t port, uint8_t address,
                         uint8_t const* write_data, uint8_t write_len,
                         uint8_t* read_data, uint8_t read_len, int32_t ms_dur);
void wait_for(int32_t us_dur);

#  ifdef __cplusplus
}
#  endif
#endif
