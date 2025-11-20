/*
 * Copyright 2025 Aethernet Inc.
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

#ifndef TEMPERATURE_TEMP_SENSOR_CONFIG_H_
#define TEMPERATURE_TEMP_SENSOR_CONFIG_H_

#if defined ESP_PLATFORM
#  include "driver/temperature_sensor.h"
#endif

namespace ae {
enum TempSensorType {
  kFakeTempSensor,
  kEspTempSensor,
};

struct TempSensorConfig {
  TempSensorType type;
};

#if defined ESP_PLATFORM
struct EspTempSensorConfig {
  TempSensorType type;
  temperature_sensor_config_t config;
};
#endif

}  // namespace ae

#endif  // TEMPERATURE_TEMP_SENSOR_CONFIG_H_
