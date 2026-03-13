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

// Константы для SHTC3
#define SHTC3_SLAVE_ADDR                   0x70   // I2C адрес [4][8]
// Команды (16-битные, передаются старшим байтом вперед MSB first)
#define SHTC3_CMD_WAKEUP                   0x3517 // Вывод из сна
#define SHTC3_CMD_SLEEP                    0xB098 // Перевод в сон [3][6]
#define SHTC3_CMD_MEASURE                  0x7CA2 // Измерение с высокой точностью [7]


// Константы для STCC4
#define STCC4_SLAVE_ADDR                   0x65   // Типичный адрес для датчиков Sensirion
// Команды (16-битные, передаются старшим байтом вперед MSB first)
#define STCC4_CMD_MEASURE_SINGLE_SHOT      0x2C1F // Команда для однократного измерения
#define STCC4_CMD_MEASURE_CONTINUOUS       0x21B1 // Команда для непрерывного измерения
#define STCC4_CMD_READ_DATA                0xE000 // Команда для чтения данных
#define STCC4_CMD_SLEEP                    0xB009 // Команда для перехода в сон

// Константы для SHT45
#define SHT45_SLAVE_ADDR                   0x44 // I2C адрес
// Команды (8-битные)
#define SHT4X_CMD_MEASURE_HIGH_PRECISION   0xFD
#define SHT4X_CMD_MEASURE_MEDIUM_PRECISION 0xF6
#define SHT4X_CMD_MEASURE_LOW_PRECISION    0xE0
#define SHT4X_CMD_READ_SERIAL              0x89
#define SHT4X_CMD_SOFT_RESET               0x94
#define SHT4X_CMD_HEATER_OFF               0x00 // Не прямая команда, а флаг

#define LP_I2C_TRANS_TIMEOUT_CYCLES 5000
#define LP_I2C_TRANS_WAIT_FOREVER   -1

// Переменные в RTC-памяти (доступны из main.c)
uint32_t last_sht45_temperature;   // Температура в градусах Цельсия * 100 (int)
uint32_t last_sht45_humidity;      // Влажность в %RH * 100 (int)
uint32_t last_shtc3_temperature;   // Температура в градусах Цельсия * 100 (int)
uint32_t last_shtc3_humidity;      // Влажность в %RH * 100 (int)
uint32_t last_stcc4_co2;           // CO2 ppm
uint32_t wakeup_temp_threshold;    // Порог: XX.XX°C
uint32_t wakeup_co2_threshold;     // Порог: XXX ppm
uint32_t should_wakeup;

// Буферы для I2C
static uint8_t data_wr[2];
static uint8_t data_rd[6];

// Вспомогательная функция для отправки 16-битной команды
static void send_command_16bit(uint16_t cmd, uint8_t slave_addr) {
    data_wr[0] = (cmd >> 8) & 0xFF; // Старший байт
    data_wr[1] = cmd & 0xFF;        // Младший байт
    esp_err_t ret = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, slave_addr, data_wr, sizeof(data_wr), LP_I2C_TRANS_WAIT_FOREVER);
    if (ret != ESP_OK) {
      // Bail and try again
      return;
    }
}

// Вспомогательная функция для отправки 8-битной команды
static void send_command_8bit(uint8_t cmd, uint8_t slave_addr) {
    data_wr[0] = cmd;
    lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, slave_addr, data_wr, 1, LP_I2C_TRANS_WAIT_FOREVER);
}

int main(void) {
    esp_err_t ret;

    should_wakeup = 0;
#if BOARD_HAS_SHT45 == 1
    // SHT45 не требует отдельной команды пробуждения
    
    // Отправка команды измерения
    send_command_8bit(SHT4X_CMD_MEASURE_HIGH_PRECISION, SHT45_SLAVE_ADDR);
    
    // Время измерения для высокой точности
    ulp_lp_core_delay_us(10000); // 10 мс

    // Чтение 6 байт: температура и влажность с CRC
    ret = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, SHT45_SLAVE_ADDR, data_rd, 6, LP_I2C_TRANS_TIMEOUT_CYCLES);
    
    if (ret == ESP_OK) {
        uint16_t raw_temp = (data_rd[0] << 8) | data_rd[1];
        uint16_t raw_hum = (data_rd[3] << 8) | data_rd[4];
        
        // Формулы для SHT45
        // Температура: T = -45 + 175 * raw_temp / 65535
        // Влажность: RH = 100 * raw_hum / 65535
        uint32_t temp_x100 = (17500ULL * raw_temp) / 65535 - 4500;
        uint32_t hum_x100 = (10000ULL * raw_hum) / 65535;
        
        last_sht45_temperature = temp_x100;
        last_sht45_humidity = hum_x100;
    }

    // Принятие решения о пробуждении HP ядра
    if (last_sht45_temperature > wakeup_temp_threshold) {
        should_wakeup = 1;
    }
#endif
#if BOARD_HAS_SHTC3 == 1
    // Пробуждение датчика (Wakeup)
    // SHTC3 после подачи питания спит. Команда пробуждения требует времени ~240 мкс.
    send_command_16bit(SHTC3_CMD_WAKEUP, SHTC3_SLAVE_ADDR);
    ulp_lp_core_delay_us(300); // Небольшой запас

    // Запуск измерения (одноцикловый режим)
    send_command_16bit(SHTC3_CMD_MEASURE, SHTC3_SLAVE_ADDR);

    // Ожидание завершения измерения. Датчик использует clock stretching,
    // но для простоты добавим фиксированную задержку.
    // Максимальное время измерения для высокой точности ~12.1 мс [7]
    ulp_lp_core_delay_us(12500); // 12.5 мс

    // Чтение 6 байт: [TempMSB, TempLSB, TempCRC, HumMSB, HumLSB, HumCRC]
    // В этом примере мы игнорируем CRC для простоты.
    ret = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, SHTC3_SLAVE_ADDR, data_rd, sizeof(data_rd), LP_I2C_TRANS_TIMEOUT_CYCLES);
    if (ret == ESP_OK) {
    // 5. "Сырые" значения (raw)
    uint16_t raw_temp = (data_rd[0] << 8) | data_rd[1];
    uint16_t raw_hum  = (data_rd[3] << 8) | data_rd[4];

    // Перевод в физические величины согласно даташиту
    // Температура: T (°C) = -45 + 175 * raw_temp / 2^16
    // Храним как целое число * 100 (чтобы избежать float в ULP)
    uint32_t temp_x100 = (17500ULL * raw_temp) / 65536 - 4500; // *100
    uint32_t hum_x100  = (10000ULL * raw_hum) / 65536;        // *100

    last_shtc3_temperature = temp_x100;
    last_shtc3_humidity = hum_x100;
    }

    // Перевод датчика обратно в сон (экономия энергии)
    send_command_16bit(SHTC3_CMD_SLEEP, SHTC3_SLAVE_ADDR);

    // Принятие решения о пробуждении HP ядра
    if (last_shtc3_temperature > wakeup_temp_threshold) {
        should_wakeup = 1;
    }  
#endif
#if BOARD_HAS_STCC4 == 1
    send_command_16bit(STCC4_CMD_MEASURE_SINGLE_SHOT, STCC4_SLAVE_ADDR);
        
    // Ждем завершения измерения (согласно даташиту STCC4 ~500-720ms)
    ulp_lp_core_delay_us(500000); // 500 мс
    
    // Чтение 3 байт: CO2 с CRC
    ret = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, STCC4_SLAVE_ADDR, data_rd, 3, LP_I2C_TRANS_TIMEOUT_CYCLES);
    
    if (ret == ESP_OK) {
        // Преобразование в значение CO2 (формат зависит от датчика, обычно 16 бит)
        uint16_t co2_ppm = (data_rd[0] << 8) | data_rd[1];
        
        last_stcc4_co2 = co2_ppm;
    }

    // Принятие решения о пробуждении HP ядра
    if (last_stcc4_co2 > wakeup_co2_threshold) {
        should_wakeup = 1;        
    }  
#endif
    if(should_wakeup == 1){
      ulp_lp_core_wakeup_main_processor();
    }

    // Аналог "глубокого сна" для самого LP-ядра
    //ulp_lp_core_halt();
    //ulp_lp_core_stop_lp_core();

    return 0;
}