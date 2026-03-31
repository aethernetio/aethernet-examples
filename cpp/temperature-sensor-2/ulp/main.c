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

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ulp_lp_core.h"
#include "ulp_lp_core_i2c.h"
#include "ulp_lp_core_utils.h"

#include "sensors/sensors.h"

// Constants for SHTC3
#define SHTC3_SLAVE_ADDR 0x70  // I2C address [4][8]
// Commands (16-bit, transmitted MSB first)
#define SHTC3_CMD_WAKEUP 0x3517   // Wake from sleep
#define SHTC3_CMD_SLEEP 0xB098    // Enter sleep [3][6]
#define SHTC3_CMD_MEASURE 0x7CA2  // High precision measurement [7]

// Constants for STCC4
#define STCC4_SLAVE_ADDR 0x65  // Typical address for Sensirion sensors
// Commands (16-bit, transmitted MSB first)
#define STCC4_CMD_MEASURE_SINGLE_SHOT \
  0x2C1F  // Command for single shot measurement
#define STCC4_CMD_MEASURE_CONTINUOUS \
  0x21B1                            // Command for continuous measurement
#define STCC4_CMD_READ_DATA 0xE000  // Command to read data
#define STCC4_CMD_SLEEP 0xB009      // Command to enter sleep

// Constants for SHT45
#define SHT45_SLAVE_ADDR 0x44  // I2C address
// Commands (8-bit)
#define SHT4X_CMD_MEASURE_HIGH_PRECISION 0xFD
#define SHT4X_CMD_MEASURE_MEDIUM_PRECISION 0xF6
#define SHT4X_CMD_MEASURE_LOW_PRECISION 0xE0
#define SHT4X_CMD_READ_SERIAL 0x89
#define SHT4X_CMD_SOFT_RESET 0x94
#define SHT4X_CMD_HEATER_OFF 0x00  // Not a direct command, but a flag

// Constants
#define LP_I2C_TRANS_TIMEOUT_CYCLES 5000
#define LP_I2C_TRANS_WAIT_FOREVER -1

#define nullptr ((void*)0)

// Variables in RTC memory (accessible from main.c)
volatile uint32_t wakeup_temp_threshold;  // Threshold: XX.XX°C
volatile uint32_t wakeup_co2_threshold;   // Threshold: XXX ppm
volatile uint16_t wakeup_gas_threshold;   // Threshold: Gas
// Variables to store latest values

uint32_t temperature;
uint32_t humidity;
uint32_t pressure;
uint32_t co2;
uint32_t gas_resistance;

volatile uint32_t can_start = 0;
// Local variables
static bool should_wakeup = false;

// I2C Buffers
static uint8_t data_wr[2];
static uint8_t data_rd[6];

// Helper function to send a 16-bit command
static void send_command_16bit(uint16_t cmd, uint8_t slave_addr) {
  data_wr[0] = (cmd >> 8) & 0xFF;  // High byte
  data_wr[1] = cmd & 0xFF;         // Low byte
  esp_err_t ret = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, slave_addr,
                                                     data_wr, sizeof(data_wr),
                                                     LP_I2C_TRANS_WAIT_FOREVER);
  if (ret != ESP_OK) {
    // Bail and try again
    return;
  }
}

// Helper function to send an 8-bit command
static void send_command_8bit(uint8_t cmd, uint8_t slave_addr) {
  data_wr[0] = cmd;
  lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, slave_addr, data_wr, 1,
                                     LP_I2C_TRANS_WAIT_FOREVER);
}

int main(void) {
  while (can_start == 0) {
    asm("nop");
  }  // Waiting main CPU

  ReadSensors(&temperature, &humidity, &pressure, &co2, &gas_resistance);
  if (temperature > wakeup_temp_threshold) {
    should_wakeup = true;
  }
  if (co2 > wakeup_co2_threshold) {
    should_wakeup = true;
  }
  if (gas_resistance > wakeup_gas_threshold) {
    should_wakeup = true;
  }

  esp_err_t ret;
#if BOARD_HAS_SHT45 == 1
  // SHT45 does not require a separate wakeup command

  // Send measurement command
  send_command_8bit(SHT4X_CMD_MEASURE_HIGH_PRECISION, SHT45_SLAVE_ADDR);

  // Measurement time for high precision
  ulp_lp_core_delay_us(10000);  // 10 ms

  // Read 6 bytes: temperature and humidity with CRC
  ret = lp_core_i2c_master_read_from_device(
      LP_I2C_NUM_0, SHT45_SLAVE_ADDR, data_rd, 6, LP_I2C_TRANS_TIMEOUT_CYCLES);

  if (ret == ESP_OK) {
    uint16_t raw_temp = (data_rd[0] << 8) | data_rd[1];
    uint16_t raw_hum = (data_rd[3] << 8) | data_rd[4];

    // Formulas for SHT45
    // Temperature: T = -45 + 175 * raw_temp / 65535
    // Humidity: RH = 100 * raw_hum / 65535
    uint32_t temp_x100 = (17500ULL * raw_temp) / 65535 - 4500;
    uint32_t hum_x100 = (10000ULL * raw_hum) / 65535;

    last_sht45_temperature = temp_x100;
    last_sht45_humidity = hum_x100;
  }

  // Decision to wake up the HP core
  if (last_sht45_temperature > wakeup_temp_threshold) {
    should_wakeup = true;
  }
#endif
#if BOARD_HAS_SHTC3 == 1
  // Wake up the sensor
  // SHTC3 sleeps after power-up. Wakeup command requires ~240 µs.
  send_command_16bit(SHTC3_CMD_WAKEUP, SHTC3_SLAVE_ADDR);
  ulp_lp_core_delay_us(300);  // Small margin

  // Start measurement (single-shot mode)
  send_command_16bit(SHTC3_CMD_MEASURE, SHTC3_SLAVE_ADDR);

  // Wait for measurement completion. The sensor uses clock stretching,
  // but for simplicity we add a fixed delay.
  // Maximum measurement time for high precision ~12.1 ms [7]
  ulp_lp_core_delay_us(12500);  // 12.5 ms

  // Read 6 bytes: [TempMSB, TempLSB, TempCRC, HumMSB, HumLSB, HumCRC]
  // In this example, we ignore CRC for simplicity.
  ret = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, SHTC3_SLAVE_ADDR,
                                            data_rd, sizeof(data_rd),
                                            LP_I2C_TRANS_TIMEOUT_CYCLES);
  if (ret == ESP_OK) {
    // 5. Raw values
    uint16_t raw_temp = (data_rd[0] << 8) | data_rd[1];
    uint16_t raw_hum = (data_rd[3] << 8) | data_rd[4];

    // Convert to physical quantities according to datasheet
    // Temperature: T (°C) = -45 + 175 * raw_temp / 2^16
    // Store as integer * 100 (to avoid float in ULP)
    uint32_t temp_x100 = (17500ULL * raw_temp) / 65536 - 4500;  // *100
    uint32_t hum_x100 = (10000ULL * raw_hum) / 65536;           // *100

    last_shtc3_temperature = temp_x100;
    last_shtc3_humidity = hum_x100;
  }

  // Put sensor back to sleep (power saving)
  send_command_16bit(SHTC3_CMD_SLEEP, SHTC3_SLAVE_ADDR);

  // Decision to wake up the HP core
  if (last_shtc3_temperature > wakeup_temp_threshold) {
    should_wakeup = true;
  }
#endif
#if BOARD_HAS_STCC4 == 1
  send_command_16bit(STCC4_CMD_MEASURE_SINGLE_SHOT, STCC4_SLAVE_ADDR);

  // Wait for measurement completion (according to STCC4 datasheet ~500-720ms)
  ulp_lp_core_delay_us(500000);  // 500 ms

  // Read 3 bytes: CO2 with CRC
  ret = lp_core_i2c_master_read_from_device(
      LP_I2C_NUM_0, STCC4_SLAVE_ADDR, data_rd, 3, LP_I2C_TRANS_TIMEOUT_CYCLES);

  if (ret == ESP_OK) {
    // Convert to CO2 value (format depends on sensor, typically 16 bits)
    uint16_t co2_ppm = (data_rd[0] << 8) | data_rd[1];

    last_stcc4_co2 = co2_ppm;
  }

  // Decision to wake up the HP core
  if (last_stcc4_co2 > wakeup_co2_threshold) {
    should_wakeup = true;
  }
#endif
#if BOARD_HAS_BME68X == 1
  // BME68X Code there
  static struct bme68x_dev bme;
  static struct bme68x_conf conf;
  static struct bme68x_heatr_conf heater_conf;
  static uint8_t dev_addr = BME68X_I2C_ADDR_LOW;
  struct bme68x_data data;
  bool bme68x_initialized = true;
  i2c_port_t lp_i2c = LP_I2C_NUM_0;

  // Static Initialization Block (Runs once)
  // Initialize BME Sensor
  bme.read = bme_i2c_read;
  bme.write = bme_i2c_write;
  bme.intf = BME68X_I2C_INTF;
  bme.delay_us = bme_delay_us;
  bme.intf_ptr = &lp_i2c;
  bme.amb_temp = 25;

  if (bme68x_init(&bme) != BME68X_OK) {
    dev_addr = BME68X_I2C_ADDR_HIGH;  // Try alternate address
    if (bme68x_init(&bme) != BME68X_OK) {
      bme68x_initialized = false;
    }
  }

#  if USE_BME68X_HEATER == 1
  // Setting up the gas sensor (optional)
  heater_conf.enable = BME68X_ENABLE,
  heater_conf.heatr_temp = 300;  // Температура нагревателя в градусах Цельсия
  heater_conf.heatr_dur = 100;   // Длительность нагрева в миллисекундах
  heater_conf.heatr_temp_prof = nullptr;
  heater_conf.heatr_dur_prof = nullptr;
  heater_conf.profile_len = 0;
  heater_conf.shared_heatr_dur = 100;

  bme68x_set_heatr_conf(BME68X_PARALLEL_MODE, &heater_conf, &bme);
#  endif

  if (bme68x_initialized) {
    // 3. Configure Sensor
    conf.filter = BME68X_FILTER_OFF;
    conf.odr = BME68X_ODR_NONE;
    conf.os_hum = BME68X_OS_NONE;
    conf.os_pres = BME68X_OS_NONE;
    conf.os_temp = BME68X_OS_2X;
    bme68x_set_conf(&conf, &bme);

    if (bme68x_set_op_mode(BME68X_FORCED_MODE, &bme) != BME68X_OK)
      bme68x_initialized = false;

    // Wait for measurement
    uint32_t del_period = bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf, &bme);
    bme.delay_us(del_period, bme.intf_ptr);

    // Read Data
    struct bme68x_data data;
    uint8_t n_fields;
    if (!(bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &bme) ==
              BME68X_OK &&
          n_fields > 0)) {
      bme68x_initialized = false;
    }
  }

  if (bme68x_initialized) {
    // Trigger measurement
    if (bme68x_set_op_mode(BME68X_FORCED_MODE, &bme) != BME68X_OK)
      bme68x_initialized = false;

    // Wait for measurement
    uint32_t del_period = bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf, &bme);
    bme.delay_us(del_period, bme.intf_ptr);

    // Read Data
    uint8_t n_fields;
    if (bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &bme) ==
            BME68X_OK &&
        n_fields == 0) {
      bme68x_initialized = false;
    }
  }

  // Save latest values
  last_bme68x_temperature = data.temperature * 100;
  last_bme68x_pressure = data.pressure * 100;
  last_bme68x_humidity = data.humidity * 1000;
  last_bme68x_gas_resistance = data.gas_resistance;

  // Decision to wake up the HP core based on temperature or gas
  if (bme68x_initialized) {
    if (last_bme68x_temperature > wakeup_temp_threshold ||
        last_bme68x_gas_resistance < wakeup_gas_threshold) {
      should_wakeup = 1;
    }
  }
#endif
  if (should_wakeup) {
    ulp_lp_core_wakeup_main_processor();
  }

  // Analogue of "deep sleep" for the LP core itself
  // ulp_lp_core_halt();
  // ulp_lp_core_stop_lp_core();

  return 0;
}
