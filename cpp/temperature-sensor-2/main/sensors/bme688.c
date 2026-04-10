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

#if BOARD_HAS_BME688 == 1
#  if ((ULP_COMP == 1) && (BOARD_HAS_ULP == 1)) || \
      ((ULP_COMP == 0) && (BOARD_HAS_ULP == 0))
#    pragma message("BME688 is enabled")

#    include <stdlib.h>
#    include <string.h>
#    include <stdbool.h>

#    include <freertos/FreeRTOS.h>
#    include <freertos/task.h>

#    include "BME68x_SensorAPI/bme68x.h"

#    include "sensors/sensors.h"
#    include "sensors/utils.h"

#    ifdef IS_ULP_COCPU
#      define BME_I2C_NUM LP_I2C_NUM_0
#    else
#      define BME_I2C_NUM I2C_NUM_0
#    endif

// Constants
#define I2C_TRANS_TIMEOUT_CYCLES 5000
#define I2C_TRANS_WAIT_FOREVER   -1

// --- SAFER Interface Functions ---
static BME68X_INTF_RET_TYPE bme_i2c_read(uint8_t reg_addr, uint8_t* reg_data,
                                         uint32_t len, void* intf_ptr) {
  uint8_t dev_addr = *(uint8_t*)intf_ptr;
  esp_err_t err =
      i2c_write_read(BME_I2C_NUM, dev_addr, &reg_addr, 1, reg_data, len, I2C_TRANS_TIMEOUT_CYCLES);
  return (err == ESP_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
}

static BME68X_INTF_RET_TYPE bme_i2c_write(uint8_t reg_addr,
                                          const uint8_t* reg_data, uint32_t len,
                                          void* intf_ptr) {
  uint8_t dev_addr = *(uint8_t*)intf_ptr;

  // SAFETY CHECK: Prevent buffer overflow if driver requests too much data
  if (len > (MAX_I2C_BUFFER - 1)) {
    return BME68X_E_COM_FAIL;
  }

  uint8_t buffer[MAX_I2C_BUFFER];
  buffer[0] = reg_addr;
  // Safe copy
  memcpy(&buffer[1], reg_data, len);

  esp_err_t err =
      i2c_write(BME_I2C_NUM, dev_addr, buffer, len + 1, I2C_TRANS_WAIT_FOREVER);
  return (err == ESP_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
}

static void bme_delay_us(uint32_t period, void* intf_ptr) {
  // FIX 2: Ensure we don't delay for 0 ticks, but also don't block interrupts
  wait_for(period);
}

bool initialized = false;
struct bme68x_dev bme;
struct bme68x_conf conf;
uint8_t dev_addr = BME68X_I2C_ADDR_LOW;

bool Init() {
  // 1. INSTALL I2C DRIVER
  if (i2c_init(BME_I2C_NUM, SENSOR_SDA_PIN, SENSOR_SCL_PIN) != ESP_OK) {
    return false;
  }

  // 2. Initialize BME Sensor
  bme.read = bme_i2c_read;
  bme.write = bme_i2c_write;
  bme.intf = BME68X_I2C_INTF;
  bme.delay_us = bme_delay_us;
  bme.intf_ptr = &dev_addr;
  bme.amb_temp = 25;

  if (bme68x_init(&bme) != BME68X_OK) {
    dev_addr = BME68X_I2C_ADDR_HIGH;  // Try alternate address
    if (bme68x_init(&bme) != BME68X_OK) {
      return false;
    }
  }

  // 3. Configure Sensor
  conf.filter = BME68X_FILTER_OFF;
  conf.odr = BME68X_ODR_NONE;
  conf.os_hum = BME68X_OS_NONE;
  conf.os_pres = BME68X_OS_NONE;
  conf.os_temp = BME68X_OS_2X;
  bme68x_set_conf(&conf, &bme);

  return true;
}

void ReadSensors(int16_t* temperature, uint32_t* humidity, uint32_t* pressure,
                 uint32_t* co2, uint32_t* gas_resistance) {
  // Static Initialization Block (Runs once)
  if (!initialized) {
    initialized = Init();
  }

  if (!initialized) {
    return;
  } else {
    // Trigger measurement
    if (bme68x_set_op_mode(BME68X_FORCED_MODE, &bme) != BME68X_OK) {
      return;
    } else {
      // Wait for measurement
      uint32_t del_period =
          bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf, &bme);
      bme.delay_us(del_period, bme.intf_ptr);

      // Read Data
      struct bme68x_data data;
      uint8_t n_fields;
      if (bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &bme) ==
              BME68X_OK &&
          n_fields > 0) {
#    ifndef BME68X_USE_FPU
        if (temperature) {
          *temperature = (int16_t)(data.temperature);
        }
        if (humidity) {
          *humidity = (uint32_t)data.humidity;
        }
        if (pressure) {
          *pressure = (uint32_t)data.pressure;
        }
        if (gas_resistance) {
          *gas_resistance = (uint32_t)data.gas_resistance;
        }
#    else
        if (temperature) {
          *temperature = (int16_t)(data.temperature * 100.F);
        }
        if (humidity) {
          *humidity = (uint32_t)(data.humidity * 1000);
        }
        if (pressure) {
          *pressure = (uint32_t)(data.pressure * 1000);
        }
        if (gas_resistance) {
          *gas_resistance = (uint32_t)(data.gas_resistance * 1000);
        }
#    endif
      }
    }
  }

  return;
}
#  endif
#endif
