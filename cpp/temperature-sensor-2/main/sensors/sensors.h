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

#ifndef SENSORS_SENSORS_H_
#define SENSORS_SENSORS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Read the sensor values.
 * The values are optional, you may specify only those you are interested in.
 * Also not any sensor support all values.
 * \param temperature Pointer to store the temperature value (optional).
 * temperature in range -100.0 to 100.0 x100 (-10000 to 10000)
 * \param humidity Pointer to store the humidity value (optional).
 * \param pressure Pointer to store the pressure value (optional).
 * \param co2 Pointer to store the CO2 value (optional).
 * \param gas_resistance Pointer to store the gas resistance value (optional).
 */
void ReadSensors(int16_t* temperature, uint32_t* humidity, uint32_t* pressure,
                 uint32_t* co2, uint32_t* gas_resistance);

#ifdef __cplusplus
}
#endif
#endif  // SENSORS_SENSORS_H_
