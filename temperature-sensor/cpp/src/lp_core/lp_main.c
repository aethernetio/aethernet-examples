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
#include "ulp_lp_core.h"
#include "ulp_lp_core_i2c.h"
#include "ulp_lp_core_utils.h"
#include "../config.h"

// Constants for SHTC3
#define SHTC3_SLAVE_ADDR                   0x70   // I2C address [4][8]
// Commands (16-bit, transmitted MSB first)
#define SHTC3_CMD_WAKEUP                   0x3517 // Wake from sleep
#define SHTC3_CMD_SLEEP                     0xB098 // Enter sleep [3][6]
#define SHTC3_CMD_MEASURE                  0x7CA2 // High precision measurement [7]


// Constants for STCC4
#define STCC4_SLAVE_ADDR                   0x65   // Typical address for Sensirion sensors
// Commands (16-bit, transmitted MSB first)
#define STCC4_CMD_MEASURE_SINGLE_SHOT      0x2C1F // Command for single shot measurement
#define STCC4_CMD_MEASURE_CONTINUOUS       0x21B1 // Command for continuous measurement
#define STCC4_CMD_READ_DATA                0xE000 // Command to read data
#define STCC4_CMD_SLEEP                     0xB009 // Command to enter sleep

// Constants for SHT45
#define SHT45_SLAVE_ADDR                   0x44 // I2C address
// Commands (8-bit)
#define SHT4X_CMD_MEASURE_HIGH_PRECISION   0xFD
#define SHT4X_CMD_MEASURE_MEDIUM_PRECISION 0xF6
#define SHT4X_CMD_MEASURE_LOW_PRECISION    0xE0
#define SHT4X_CMD_READ_SERIAL              0x89
#define SHT4X_CMD_SOFT_RESET               0x94
#define SHT4X_CMD_HEATER_OFF               0x00 // Not a direct command, but a flag

// BME68X Registers
#define BME68X_CHIP_ID                     0x61
#define BME68X_SLAVE_ADDR                  0x77  // Or 0x76, depending on connection
// Commands (8-bit)
#define BME68X_REG_CHIP_ID                 0xD0
#define BME68X_REG_RESET                   0xE0
#define BME68X_REG_CTRL_MEAS               0xF4
#define BME68X_REG_CTRL_HUM                0xF2
#define BME68X_REG_CONFIG                  0xF5
#define BME68X_REG_DATA_START              0xF7
#define BME68X_REG_GAS_CONFIG              0x71
#define BME68X_REG_GAS_R                    0x72

// Constants
#define LP_I2C_TRANS_TIMEOUT_CYCLES 5000
#define LP_I2C_TRANS_WAIT_FOREVER   -1

// Variables in RTC memory (accessible from main.c)
uint32_t wakeup_temp_threshold;    // Threshold: XX.XX°C
uint32_t wakeup_co2_threshold;     // Threshold: XXX ppm
uint16_t wakeup_gas_threshold;     // Threshold: Gas
// Variables to store latest values
uint32_t last_sht45_temperature;   // Temperature in degrees Celsius * 100 (int)
uint32_t last_sht45_humidity;      // Humidity in %RH * 100 (int)
uint32_t last_shtc3_temperature;   // Temperature in degrees Celsius * 100 (int)
uint32_t last_shtc3_humidity;      // Humidity in %RH * 100 (int)
uint32_t last_stcc4_co2;           // CO2 ppm
int32_t last_bme68x_temperature;
int32_t last_bme68x_pressure;
uint32_t last_bme68x_humidity;
uint16_t last_bme68x_gas_resistance;
// Local variables
static uint32_t should_wakeup;

// I2C Buffers
static uint8_t data_wr[2];
static uint8_t data_rd[6];

// Helper function to send a 16-bit command
static void send_command_16bit(uint16_t cmd, uint8_t slave_addr) {
    data_wr[0] = (cmd >> 8) & 0xFF; // High byte
    data_wr[1] = cmd & 0xFF;        // Low byte
    esp_err_t ret = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, slave_addr, data_wr, sizeof(data_wr), LP_I2C_TRANS_WAIT_FOREVER);
    if (ret != ESP_OK) {
      // Bail and try again
      return;
    }
}

// Helper function to send an 8-bit command
static void send_command_8bit(uint8_t cmd, uint8_t slave_addr) {
    data_wr[0] = cmd;
    lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, slave_addr, data_wr, 1, LP_I2C_TRANS_WAIT_FOREVER);
}

// Structure for BME688 calibration data
typedef struct {
    // Humidity  
    uint8_t par_h1;
    int16_t par_h2;
    uint8_t par_h3;
    int8_t par_h4;
    int8_t par_h5;
    uint8_t par_h6;
    int8_t par_h7;
    
    // Temperature
    uint16_t par_t1;
    int16_t par_t2;
    int16_t par_t3;
    
    // Pressure
    uint16_t par_p1;
    int16_t par_p2;
    int16_t par_p3;
    int16_t par_p4;
    int16_t par_p5;
    int16_t par_p6;
    int16_t par_p7;
    int16_t par_p8;
    int16_t par_p9;
    uint8_t par_p10;
    
    /*! Variable to store the intermediate temperature coefficient */
    int32_t t_fine;

    /*! Heater resistance range coefficient */
    uint8_t res_heat_range;

    /*! Heater resistance value coefficient */
    int8_t res_heat_val;

    /*! Gas resistance range switching error coefficient */
    int8_t range_sw_err;
} bme688_calib_t;

// Variable to store calibration data
static bme688_calib_t calib;

// Read calibration data
static int read_calibration_data(uint8_t *calib_data, uint8_t size) {
    uint8_t reg = 0x8A;  // Start address of calibration data
    
    // Write register address for reading
    int ret = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, BME68X_SLAVE_ADDR, 
                                                &reg, 1, LP_I2C_TRANS_TIMEOUT_CYCLES);
    if (ret) return ret;
    
    // Read calibration data
    return lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, BME68X_SLAVE_ADDR, 
                                              &calib_data[0], size - 16, 
                                              LP_I2C_TRANS_TIMEOUT_CYCLES);
    
    reg = 0xE1;  // Start address of calibration data
    
    // Write register address for reading
    ret = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, BME68X_SLAVE_ADDR, 
                                                &reg, 1, LP_I2C_TRANS_TIMEOUT_CYCLES);
    if (ret) return ret;
    
    // Read calibration data
    return lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, BME68X_SLAVE_ADDR, 
                                              &calib_data[26], 16, 
                                              LP_I2C_TRANS_TIMEOUT_CYCLES);
}

// Function to parse calibration data
void parse_calibration_data(uint8_t *calib_data) {
    // Temperature
    calib.par_t1 = (uint16_t)((calib_data[1] << 8) | calib_data[0]);
    calib.par_t2 = (int16_t)((calib_data[3] << 8) | calib_data[2]);
    calib.par_t3 = (int16_t)((calib_data[5] << 8) | calib_data[4]);
    
    // Pressure
    calib.par_p1 = (uint16_t)((calib_data[7] << 8) | calib_data[6]);
    calib.par_p2 = (int16_t)((calib_data[9] << 8) | calib_data[8]);
    calib.par_p3 = (int16_t)((calib_data[11] << 8) | calib_data[10]);
    calib.par_p4 = (int16_t)((calib_data[13] << 8) | calib_data[12]);
    calib.par_p5 = (int16_t)((calib_data[15] << 8) | calib_data[14]);
    calib.par_p6 = (int16_t)((calib_data[17] << 8) | calib_data[16]);
    calib.par_p7 = (int16_t)((calib_data[19] << 8) | calib_data[18]);
    calib.par_p8 = (int16_t)((calib_data[21] << 8) | calib_data[20]);
    calib.par_p9 = (uint8_t)((calib_data[23] << 8) | calib_data[22]);
    
    // Humidity
    calib.par_h1 = (uint8_t)(calib_data[25]);
    calib.par_h2 = (int16_t)((calib_data[26] << 8) | calib_data[27]);
    calib.par_h3 = (uint8_t)(calib_data[28]);
    calib.par_h4 = (int8_t)((calib_data[29] << 4) | (calib_data[30] & 0x0F));
    calib.par_h5 = (int8_t)((calib_data[31] << 4) | ((calib_data[30] >> 4) & 0x0F));
    calib.par_h6 = (uint8_t)(calib_data[32]);
    calib.par_h7 = (int8_t)(calib_data[33]);
}

// Temperature compensation
/* @brief This internal API is used to calculate the temperature value. */
static int16_t compensate_temperature(uint32_t temp_adc)
{
    int64_t var1;
    int64_t var2;
    int64_t var3;
    int16_t calc_temp;

    /*lint -save -e701 -e702 -e704 */
    var1 = ((int32_t)temp_adc >> 3) - ((int32_t)calib.par_t1 << 1);
    var2 = (var1 * (int32_t)calib.par_t2) >> 11;
    var3 = ((var1 >> 1) * (var1 >> 1)) >> 12;
    var3 = ((var3) * ((int32_t)calib.par_t3 << 4)) >> 14;
    calib.t_fine = (int32_t)(var2 + var3);
    calc_temp = (int16_t)(((calib.t_fine * 5) + 128) >> 8);

    /*lint -restore */
    return calc_temp;
}

// Pressure compensation
uint32_t compensate_pressure(uint32_t pres_adc) {
    int32_t var1;
    int32_t var2;
    int32_t var3;
    int32_t pressure_comp;

    /* This value is used to check precedence to multiplication or division
     * in the pressure compensation equation to achieve least loss of precision and
     * avoiding overflows.
     * i.e Comparing value, pres_ovf_check = (1 << 31) >> 1
     */
    const int32_t pres_ovf_check = INT32_C(0x40000000);

    /*lint -save -e701 -e702 -e713 */
    var1 = (((int32_t)calib.t_fine) >> 1) - 64000;
    var2 = ((((var1 >> 2) * (var1 >> 2)) >> 11) * (int32_t)calib.par_p6) >> 2;
    var2 = var2 + ((var1 * (int32_t)calib.par_p5) << 1);
    var2 = (var2 >> 2) + ((int32_t)calib.par_p4 << 16);
    var1 = (((((var1 >> 2) * (var1 >> 2)) >> 13) * ((int32_t)calib.par_p3 << 5)) >> 3) +
           (((int32_t)calib.par_p2 * var1) >> 1);
    var1 = var1 >> 18;
    var1 = ((32768 + var1) * (int32_t)calib.par_p1) >> 15;
    pressure_comp = 1048576 - pres_adc;
    pressure_comp = (int32_t)((pressure_comp - (var2 >> 12)) * ((uint32_t)3125));
    if (pressure_comp >= pres_ovf_check)
    {
        pressure_comp = ((pressure_comp / var1) << 1);
    }
    else
    {
        pressure_comp = ((pressure_comp << 1) / var1);
    }

    var1 = ((int32_t)calib.par_p9 * (int32_t)(((pressure_comp >> 3) * (pressure_comp >> 3)) >> 13)) >> 12;
    var2 = ((int32_t)(pressure_comp >> 2) * (int32_t)calib.par_p8) >> 13;
    var3 =
        ((int32_t)(pressure_comp >> 8) * (int32_t)(pressure_comp >> 8) * (int32_t)(pressure_comp >> 8) *
         (int32_t)calib.par_p10) >> 17;
    pressure_comp = (int32_t)(pressure_comp) + ((var1 + var2 + var3 + ((int32_t)calib.par_p7 << 7)) >> 4);

    /*lint -restore */
    return (uint32_t)pressure_comp;
}

// Humidity compensation
uint32_t compensate_humidity(uint32_t hum_adc) {
    int32_t var1;
    int32_t var2;
    int32_t var3;
    int32_t var4;
    int32_t var5;
    int32_t var6;
    int32_t temp_scaled;
    int32_t calc_hum;

    /*lint -save -e702 -e704 */
    temp_scaled = (((int32_t)calib.t_fine * 5) + 128) >> 8;
    var1 = (int32_t)(hum_adc - ((int32_t)((int32_t)calib.par_h1 * 16))) -
           (((temp_scaled * (int32_t)calib.par_h3) / ((int32_t)100)) >> 1);
    var2 =
        ((int32_t)calib.par_h2 *
         (((temp_scaled * (int32_t)calib.par_h4) / ((int32_t)100)) +
          (((temp_scaled * ((temp_scaled * (int32_t)calib.par_h5) / ((int32_t)100))) >> 6) / ((int32_t)100)) +
          (int32_t)(1 << 14))) >> 10;
    var3 = var1 * var2;
    var4 = (int32_t)calib.par_h6 << 7;
    var4 = ((var4) + ((temp_scaled * (int32_t)calib.par_h7) / ((int32_t)100))) >> 4;
    var5 = ((var3 >> 14) * (var3 >> 14)) >> 10;
    var6 = (var4 * var5) >> 1;
    calc_hum = (((var3 + var6) >> 10) * ((int32_t)1000)) >> 12;
    if (calc_hum > 100000) /* Cap at 100%rH */
    {
        calc_hum = 100000;
    }
    else if (calc_hum < 0)
    {
        calc_hum = 0;
    }

    /*lint -restore */
    return (uint32_t)calc_hum;
}

// Gas resistance compensation
uint32_t compensate_gas(uint16_t raw_gas, uint8_t gas_range) {
    int64_t var1;
    uint64_t var2;
    int64_t var3;
    uint32_t calc_gas_res;
    uint32_t lookup_table1[16] = {
        UINT32_C(2147483647), UINT32_C(2147483647), UINT32_C(2147483647), UINT32_C(2147483647), UINT32_C(2147483647),
        UINT32_C(2126008810), UINT32_C(2147483647), UINT32_C(2130303777), UINT32_C(2147483647), UINT32_C(2147483647),
        UINT32_C(2143188679), UINT32_C(2136746228), UINT32_C(2147483647), UINT32_C(2126008810), UINT32_C(2147483647),
        UINT32_C(2147483647)
    };
    uint32_t lookup_table2[16] = {
        UINT32_C(4096000000), UINT32_C(2048000000), UINT32_C(1024000000), UINT32_C(512000000), UINT32_C(255744255),
        UINT32_C(127110228), UINT32_C(64000000), UINT32_C(32258064), UINT32_C(16016016), UINT32_C(8000000), UINT32_C(
            4000000), UINT32_C(2000000), UINT32_C(1000000), UINT32_C(500000), UINT32_C(250000), UINT32_C(125000)
    };

    /*lint -save -e704 */
    var1 = (int64_t)((1340 + (5 * (int64_t)calib.range_sw_err)) * ((int64_t)lookup_table1[gas_range])) >> 16;
    var2 = (((int64_t)((int64_t)raw_gas << 15) - (int64_t)(16777216)) + var1);
    var3 = (((int64_t)lookup_table2[gas_range] * (int64_t)var1) >> 9);
    calc_gas_res = (uint32_t)((var3 + ((int64_t)var2 >> 1)) / (int64_t)var2);

    /*lint -restore */
    return calc_gas_res;
}

int main(void) {
    esp_err_t ret;

    should_wakeup = 0;
#if BOARD_HAS_SHT45 == 1
    // SHT45 does not require a separate wakeup command
    
    // Send measurement command
    send_command_8bit(SHT4X_CMD_MEASURE_HIGH_PRECISION, SHT45_SLAVE_ADDR);
    
    // Measurement time for high precision
    ulp_lp_core_delay_us(10000); // 10 ms

    // Read 6 bytes: temperature and humidity with CRC
    ret = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, SHT45_SLAVE_ADDR, data_rd, 6, LP_I2C_TRANS_TIMEOUT_CYCLES);
    
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
        should_wakeup = 1;
    }
#endif
#if BOARD_HAS_SHTC3 == 1
    // Wake up the sensor
    // SHTC3 sleeps after power-up. Wakeup command requires ~240 µs.
    send_command_16bit(SHTC3_CMD_WAKEUP, SHTC3_SLAVE_ADDR);
    ulp_lp_core_delay_us(300); // Small margin

    // Start measurement (single-shot mode)
    send_command_16bit(SHTC3_CMD_MEASURE, SHTC3_SLAVE_ADDR);

    // Wait for measurement completion. The sensor uses clock stretching,
    // but for simplicity we add a fixed delay.
    // Maximum measurement time for high precision ~12.1 ms [7]
    ulp_lp_core_delay_us(12500); // 12.5 ms

    // Read 6 bytes: [TempMSB, TempLSB, TempCRC, HumMSB, HumLSB, HumCRC]
    // In this example, we ignore CRC for simplicity.
    ret = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, SHTC3_SLAVE_ADDR, data_rd, sizeof(data_rd), LP_I2C_TRANS_TIMEOUT_CYCLES);
    if (ret == ESP_OK) {
    // 5. Raw values
    uint16_t raw_temp = (data_rd[0] << 8) | data_rd[1];
    uint16_t raw_hum  = (data_rd[3] << 8) | data_rd[4];

    // Convert to physical quantities according to datasheet
    // Temperature: T (°C) = -45 + 175 * raw_temp / 2^16
    // Store as integer * 100 (to avoid float in ULP)
    uint32_t temp_x100 = (17500ULL * raw_temp) / 65536 - 4500; // *100
    uint32_t hum_x100  = (10000ULL * raw_hum) / 65536;        // *100

    last_shtc3_temperature = temp_x100;
    last_shtc3_humidity = hum_x100;
    }

    // Put sensor back to sleep (power saving)
    send_command_16bit(SHTC3_CMD_SLEEP, SHTC3_SLAVE_ADDR);

    // Decision to wake up the HP core
    if (last_shtc3_temperature > wakeup_temp_threshold) {
        should_wakeup = 1;
    }  
#endif
#if BOARD_HAS_STCC4 == 1
    send_command_16bit(STCC4_CMD_MEASURE_SINGLE_SHOT, STCC4_SLAVE_ADDR);
        
    // Wait for measurement completion (according to STCC4 datasheet ~500-720ms)
    ulp_lp_core_delay_us(500000); // 500 ms
    
    // Read 3 bytes: CO2 with CRC
    ret = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, STCC4_SLAVE_ADDR, data_rd, 3, LP_I2C_TRANS_TIMEOUT_CYCLES);
    
    if (ret == ESP_OK) {
        // Convert to CO2 value (format depends on sensor, typically 16 bits)
        uint16_t co2_ppm = (data_rd[0] << 8) | data_rd[1];
        
        last_stcc4_co2 = co2_ppm;
    }

    // Decision to wake up the HP core
    if (last_stcc4_co2 > wakeup_co2_threshold) {
        should_wakeup = 1;        
    }  
#endif
#if BOARD_HAS_BME68X == 1
    // BME68X requires initialization at first start
    static uint8_t bme68x_initialized = 0;
    
    if (!bme68x_initialized) {
        // Check Chip ID
        uint8_t reg = BME68X_REG_CHIP_ID;
        ret = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, BME68X_SLAVE_ADDR, &reg, 1, LP_I2C_TRANS_TIMEOUT_CYCLES);
        if (ret == ESP_OK) {
            uint8_t chip_id;
            ret = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, BME68X_SLAVE_ADDR, &chip_id, 1, LP_I2C_TRANS_TIMEOUT_CYCLES);
            if (ret == ESP_OK && chip_id == BME68X_CHIP_ID) {
                // Reset sensor
                uint8_t reset_cmd[2] = {BME68X_REG_RESET, 0xB6};
                ret = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, BME68X_SLAVE_ADDR, reset_cmd, 2, LP_I2C_TRANS_TIMEOUT_CYCLES);
                
                // Delay after reset
                ulp_lp_core_delay_us(10000); // 10 ms
                
                // Configure operating mode
                // Gas sensor configuration
                uint8_t gas_config[2] = {BME68X_REG_GAS_CONFIG, 0x00}; // Default gas settings
                ret = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, BME68X_SLAVE_ADDR, gas_config, 2, LP_I2C_TRANS_TIMEOUT_CYCLES);
                
                // Humidity configuration (oversampling x1)
                uint8_t ctrl_hum[2] = {BME68X_REG_CTRL_HUM, 0x01};
                ret = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, BME68X_SLAVE_ADDR, ctrl_hum, 2, LP_I2C_TRANS_TIMEOUT_CYCLES);
                
                bme68x_initialized = 1;
            }
        }
    }
    
    if (bme68x_initialized) {
        // Start measurement in forced mode
        uint8_t meas_cmd[2] = {BME68X_REG_CTRL_MEAS, 0x25}; // 0b00100101: oversampling temp x1, press x1, forced mode
        ret = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, BME68X_SLAVE_ADDR, meas_cmd, 2, LP_I2C_TRANS_TIMEOUT_CYCLES);
        
        // Measurement waiting time (~10 ms for oversampling x1)
        ulp_lp_core_delay_us(15000); // 15 ms
        
        // Read data: temperature (3 bytes), pressure (3 bytes), humidity (2 bytes)
        uint8_t data_reg = BME68X_REG_DATA_START;
        ret = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, BME68X_SLAVE_ADDR, &data_reg, 1, LP_I2C_TRANS_TIMEOUT_CYCLES);
        
        uint8_t data_rd[8];
        ret = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, BME68X_SLAVE_ADDR, data_rd, sizeof(data_rd), LP_I2C_TRANS_TIMEOUT_CYCLES);
        
        if (ret == ESP_OK) {
            // Raw data
            uint32_t raw_temp = ((uint32_t)data_rd[3] << 12) | ((uint32_t)data_rd[4] << 4) | ((uint32_t)data_rd[5] >> 4);
            uint32_t raw_press = ((uint32_t)data_rd[0] << 12) | ((uint32_t)data_rd[1] << 4) | ((uint32_t)data_rd[2] >> 4);
            uint32_t raw_hum = ((uint32_t)data_rd[6] << 8) | data_rd[7];
            
            uint8_t calib_data[42];
            ret = read_calibration_data(calib_data, 42);
            parse_calibration_data(calib_data);
            
            if (ret == ESP_OK) {
                // Compensation using calibration data
                int32_t temp_x100 = compensate_temperature(raw_temp);
                uint32_t press_x100 = compensate_pressure(raw_press);
                uint32_t hum_x1000 = compensate_humidity(raw_hum);

                // Save latest values
                last_bme68x_temperature = temp_x100;
                last_bme68x_pressure = press_x100;
                last_bme68x_humidity = hum_x1000;
            }

            // Read gas sensor (if available)
            uint8_t gas_reg = BME68X_REG_GAS_R;
            ret = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, BME68X_SLAVE_ADDR, &gas_reg, 1, LP_I2C_TRANS_TIMEOUT_CYCLES);
            
            uint8_t gas_data[2];
            ret = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, BME68X_SLAVE_ADDR, gas_data, 2, LP_I2C_TRANS_TIMEOUT_CYCLES);
            
            if (ret == ESP_OK) {
                uint16_t gas_raw = (gas_data[0] << 2) | (gas_data[1] >> 6);
                uint8_t gas_range = gas_data[1] & 0x0F;
                
                uint32_t gas_resistance = compensate_gas(gas_raw, gas_range);
                
                last_bme68x_gas_resistance = gas_raw * 1000;
            }
        }
    }

    // Decision to wake up the HP core based on temperature or gas
    if (bme68x_initialized) {
        if (last_bme68x_temperature > wakeup_temp_threshold || 
            last_bme68x_gas_resistance < wakeup_gas_threshold) {
            should_wakeup = 1;
        }
    }
#endif
    if(should_wakeup == 1){
      ulp_lp_core_wakeup_main_processor();
    }

    // Analogue of "deep sleep" for the LP core itself
    //ulp_lp_core_halt();
    //ulp_lp_core_stop_lp_core();

    return 0;
}