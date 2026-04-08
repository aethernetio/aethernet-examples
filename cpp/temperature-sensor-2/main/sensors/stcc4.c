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

#if BOARD_HAS_STTC4 == 1
#if ((ULP_COMP == 1) && (BOARD_HAS_ULP == 1)) || ((ULP_COMP == 0) && (BOARD_HAS_ULP == 0))
#pragma message("STTC4 is enabled")

#  include <stdint.h>
#  include <stdbool.h>

#  include "sensors/sensors.h"
#  include "sensors/utils.h"

// Constants for STCC4
#  define STCC4_SLAVE_ADDR 0x65  // Typical address for Sensirion sensors
// Commands (16-bit, transmitted MSB first)
#  define STCC4_CMD_MEASURE_SINGLE_SHOT \
    0x2C1F  // Command for single shot measurement
#  define STCC4_CMD_MEASURE_CONTINUOUS \
    0x21B1                            // Command for continuous measurement
#  define STCC4_CMD_READ_DATA 0xE000  // Command to read data
#  define STCC4_CMD_SLEEP 0xB009      // Command to enter sleep

// Constants
#  define LP_I2C_TRANS_TIMEOUT_CYCLES 5000
#  define LP_I2C_TRANS_WAIT_FOREVER -1

// I2C Buffers
static uint8_t data_wr[2];
static uint8_t data_rd[6];

#ifdef IS_ULP_COCPU
#  define STCC4_I2C_NUM_0 LP_I2C_NUM_0
#else
#  define STCC4_I2C_NUM_0 I2C_NUM_0
#endif

bool initialized = false;

// Helper function to send a 16-bit command
static void send_command_16bit(uint16_t cmd, uint8_t slave_addr) {
  data_wr[0] = (cmd >> 8) & 0xFF;  // High byte
  data_wr[1] = cmd & 0xFF;         // Low byte
  esp_err_t ret = i2c_write(STCC4_I2C_NUM_0, slave_addr, data_wr,
                            sizeof(data_wr), LP_I2C_TRANS_WAIT_FOREVER);
  if (ret != ESP_OK) {
    // Bail and try again
    return;
  }
}

// Helper function to send an 8-bit command
static void send_command_8bit(uint8_t cmd, uint8_t slave_addr) {
  data_wr[0] = cmd;
  i2c_write(STCC4_I2C_NUM_0, slave_addr, data_wr, 1, LP_I2C_TRANS_WAIT_FOREVER);
}

bool Init() {
  // 1. INSTALL I2C DRIVER
  if (i2c_init(STCC4_I2C_NUM_0, SENSOR_SDA_PIN, SENSOR_SCL_PIN) != ESP_OK) {
    return false;
  }
return true;
}

void ReadSensors(int16_t* temperature, uint32_t* humidity, uint32_t* pressure,
                 uint32_t* co2, uint32_t* gas_resistance) {
  esp_err_t ret;

  if (!initialized) {
    initialized = Init();
  }

  send_command_16bit(STCC4_CMD_MEASURE_SINGLE_SHOT, STCC4_SLAVE_ADDR);

  // Wait for measurement completion (according to STCC4 datasheet ~500-720ms)
  wait_for(500000);  // 500 ms

  // Read 3 bytes: CO2 with CRC
  ret = i2c_read(STCC4_I2C_NUM_0, STCC4_SLAVE_ADDR, data_rd, 3,
                 LP_I2C_TRANS_TIMEOUT_CYCLES);

  if (ret != ESP_OK) {
    return;
  }
  // Convert to CO2 value (format depends on sensor, typically 16 bits)
  uint16_t co2_ppm = (data_rd[0] << 8) | data_rd[1];

  if (co2) {
    *co2 = (uint32_t)co2_ppm;
  }
}

#endif
#endif
