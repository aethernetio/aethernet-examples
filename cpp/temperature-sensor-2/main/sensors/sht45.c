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

#include "user_config.h"

#if BOARD_HAS_SHT45 == 1
#if ((ULP_COMP == 1) && (BOARD_HAS_ULP == 1)) || ((ULP_COMP == 0) && (BOARD_HAS_ULP == 0))
#pragma message("SHT45 is enabled")

#  include <stdint.h>
#  include <stdbool.h>

#  include "sensors/sensors.h"
#  include "sensors/utils.h"

// Constants for SHT45
#  define SHT45_SLAVE_ADDR 0x44  // I2C address
// Commands (8-bit)
#  define SHT4X_CMD_MEASURE_HIGH_PRECISION 0xFD
#  define SHT4X_CMD_MEASURE_MEDIUM_PRECISION 0xF6
#  define SHT4X_CMD_MEASURE_LOW_PRECISION 0xE0
#  define SHT4X_CMD_READ_SERIAL 0x89
#  define SHT4X_CMD_SOFT_RESET 0x94
#  define SHT4X_CMD_HEATER_OFF 0x00  // Not a direct command, but a flag

// Constants
#  define LP_I2C_TRANS_TIMEOUT_CYCLES 5000
#  define LP_I2C_TRANS_WAIT_FOREVER -1

// I2C Buffers
static uint8_t data_wr[2];
static uint8_t data_rd[6];

#if ULP_COMP == 1
#  define SHT45_I2C_NUM_0 LP_I2C_NUM_0
#else
#  define SHT45_I2C_NUM_0 I2C_NUM_0
#endif

bool initialized = false;

// Helper function to send an 8-bit command
static void send_command_8bit(uint8_t cmd, uint8_t slave_addr) {
  data_wr[0] = cmd;
  i2c_write(SHT45_I2C_NUM_0, slave_addr, data_wr, 1, LP_I2C_TRANS_WAIT_FOREVER);
}

bool Init() {
  // 1. INSTALL I2C DRIVER
  if (i2c_init(SHT45_I2C_NUM_0, SENSOR_SDA_PIN, SENSOR_SCL_PIN) != ESP_OK) {
    return false;
  }
return true;
}

void ReadSensors(int16_t* temperature, uint32_t* humidity, uint32_t* pressure,
                 uint32_t* co2, uint32_t* gas_resistance) {
  esp_err_t ret;
  
  // SHT45 does not require a separate wakeup command
  // Static Initialization Block (Runs once)
  if (!initialized) {
    initialized = Init();
  }
  
  // Send measurement command
  send_command_8bit(SHT4X_CMD_MEASURE_HIGH_PRECISION, SHT45_SLAVE_ADDR);

  // Measurement time for high precision
  wait_for(1100000);  // 1.1 S

  // Read 6 bytes: temperature and humidity with CRC
  ret = i2c_read(SHT45_I2C_NUM_0, SHT45_SLAVE_ADDR, data_rd, 6,
                 LP_I2C_TRANS_TIMEOUT_CYCLES);

  if (ret == ESP_OK) {
    uint16_t raw_temp = (data_rd[0] << 8) | data_rd[1];
    uint16_t raw_hum = (data_rd[3] << 8) | data_rd[4];

    // Formulas for SHT45
    // Temperature: T = -45 + 175 * raw_temp / 65535
    // Humidity: RH = 100 * raw_hum / 65535
    int32_t temp_x100 = (17500ULL * raw_temp) / 65535 - 4500;
    uint32_t hum_x100 = (10000ULL * raw_hum) / 65535;

    if (temperature) {
      *temperature = (int16_t)temp_x100;
    }
    if (humidity) {
      *humidity = hum_x100;
    }
  }
}

#endif
#endif
