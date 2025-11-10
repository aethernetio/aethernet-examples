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

#ifndef SRC_TEMP_SENSOR_H_
#define SRC_TEMP_SENSOR_H_

#include "driver/temperature_sensor.h"

namespace ae {
class TemperatureSensor {
 public:
  TemperatureSensor(temperature_sensor_config_t temp_sensor_config);
  ~TemperatureSensor();
  float GetTemperature();

 private:
  temperature_sensor_handle_t temp_sensor_ = NULL;
  temperature_sensor_config_t temp_sensor_config_ =
      TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);

  void StartSensor();
  void StopSensor();
};

}  // namespace ae
#endif  // SRC_TEMP_SENSOR_H_