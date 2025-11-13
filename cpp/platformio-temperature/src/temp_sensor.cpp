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

#include "src/temp_sensor.h"

namespace ae {
TemperatureSensor::TemperatureSensor(
    temperature_sensor_config_t temp_sensor_config)
    : temp_sensor_config_{temp_sensor_config} {
  StartSensor();
}

TemperatureSensor::~TemperatureSensor() { StopSensor(); }

float TemperatureSensor::GetTemperature() {
  float tsens_value = -1000;
  if (temp_sensor_ != nullptr) {
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor_, &tsens_value));
  }

  return tsens_value;
}

void TemperatureSensor::StartSensor() {
  ESP_ERROR_CHECK(
      temperature_sensor_install(&temp_sensor_config_, &temp_sensor_));
  ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor_));
}
void TemperatureSensor::StopSensor() {
  ESP_ERROR_CHECK(temperature_sensor_disable(temp_sensor_));
  ESP_ERROR_CHECK(
      temperature_sensor_uninstall(temp_sensor_));
}
}  // namespace ae
