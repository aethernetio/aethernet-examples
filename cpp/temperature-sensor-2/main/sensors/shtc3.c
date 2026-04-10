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

#if BOARD_HAS_SHTC3 == 1
#if ((ULP_COMP == 1) && (BOARD_HAS_ULP == 1)) || ((ULP_COMP == 0) && (BOARD_HAS_ULP == 0))
#pragma message("SHTC3 is enabled")

#  include <stdint.h>
#  include <stdbool.h>

#  include "sensors/sensors.h"
#  include "sensors/utils.h"

// Constants for SHTC3
#  define SHTC3_SLAVE_ADDR 0x70  // I2C address [4][8]
// Commands (16-bit, transmitted MSB first)
#  define SHTC3_CMD_WAKEUP 0x3517   // Wake from sleep
#  define SHTC3_CMD_SLEEP 0xB098    // Enter sleep [3][6]
#  define SHTC3_CMD_MEASURE 0x7CA2  // High precision measurement [7]

// Constants
#  define LP_I2C_TRANS_TIMEOUT_CYCLES 5000
#  define LP_I2C_TRANS_WAIT_FOREVER -1

// I2C Buffers
static uint8_t data_wr[2];
static uint8_t data_rd[6];

#ifdef IS_ULP_COCPU
#  define SHTC3_I2C_NUM_0 LP_I2C_NUM_0
#else
#  define SHTC3_I2C_NUM_0 I2C_NUM_0
#endif

bool initialized = false;

// Helper function to send a 16-bit command
static void send_command_16bit(uint16_t cmd, uint8_t slave_addr) {
  data_wr[0] = (cmd >> 8) & 0xFF;  // High byte
  data_wr[1] = cmd & 0xFF;         // Low byte
  esp_err_t ret = i2c_write(SHTC3_I2C_NUM_0, slave_addr, data_wr,
                            sizeof(data_wr), LP_I2C_TRANS_WAIT_FOREVER);
  if (ret != ESP_OK) {
    // Bail and try again
    return;
  }
}

// Helper function to send an 8-bit command
static void send_command_8bit(uint8_t cmd, uint8_t slave_addr) {
  data_wr[0] = cmd;
  i2c_write(SHTC3_I2C_NUM_0, slave_addr, data_wr, 1, LP_I2C_TRANS_WAIT_FOREVER);
}

bool Init() {
  // 1. INSTALL I2C DRIVER
  if (i2c_init(SHTC3_I2C_NUM_0, SENSOR_SDA_PIN, SENSOR_SCL_PIN) != ESP_OK) {
    return false;
  }
return true;
}

void ReadSensors(int16_t* temperature, uint32_t* humidity, uint32_t* pressure,
                 uint32_t* co2, uint32_t* gas_resistance) {
  esp_err_t ret;
  // Wake up the sensor
  // SHTC3 sleeps after power-up. Wakeup command requires ~240 µs.
  if (!initialized) {
    initialized = Init();
  }
  
  send_command_16bit(SHTC3_CMD_WAKEUP, SHTC3_SLAVE_ADDR);
  wait_for(300);  // Small margin

  // Start measurement (single-shot mode)
  send_command_16bit(SHTC3_CMD_MEASURE, SHTC3_SLAVE_ADDR);

  // Wait for measurement completion. The sensor uses clock stretching,
  // but for simplicity we add a fixed delay.
  // Maximum measurement time for high precision ~12.1 ms [7]
  wait_for(12500);  // 12.5 ms

  // Read 6 bytes: [TempMSB, TempLSB, TempCRC, HumMSB, HumLSB, HumCRC]
  // In this example, we ignore CRC for simplicity.
  ret = i2c_write_read(SHTC3_I2C_NUM_0, SHTC3_SLAVE_ADDR, data_rd,
                       sizeof(data_rd), LP_I2C_TRANS_TIMEOUT_CYCLES);
  if (ret == ESP_OK) {
    // 5. Raw values
    uint16_t raw_temp = (data_rd[0] << 8) | data_rd[1];
    uint16_t raw_hum = (data_rd[3] << 8) | data_rd[4];

    // Convert to physical quantities according to datasheet
    // Temperature: T (°C) = -45 + 175 * raw_temp / 2^16
    // Store as integer * 100 (to avoid float in ULP)
    int32_t temp_x100 = (17500ULL * raw_temp) / 65536 - 4500;  // *100
    uint32_t hum_x100 = (10000ULL * raw_hum) / 65536;           // *100

    if (temperature) {
      *temperature = (int16_t)temp_x100;
    }
    if (humidity) {
      *humidity = hum_x100;
    }
  }

  // Put sensor back to sleep (power saving)
  send_command_16bit(SHTC3_CMD_SLEEP, SHTC3_SLAVE_ADDR);
}

#endif
#endif
