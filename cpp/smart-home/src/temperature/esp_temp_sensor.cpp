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

#include "temperature/esp_temp_sensor.h"

#if defined ESP_PLATFORM

#  include <chrono>

namespace ae {
class EspTempDataStateAction : public DeviceStateAction {
 public:
  EspTempDataStateAction(ActionContext action_context, float value)
      : DeviceStateAction{action_context}, state_data_{} {
    state_data_.payload = VariantData{VariantDouble{value}};
    state_data_.timestamp = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            Now().time_since_epoch())
            .count());
  }

  UpdateStatus Update() override { return UpdateStatus::Result(); }

  DeviceStateData state_data() const override { return state_data_; }

  DeviceStateData state_data_;
};

EspTempSensor::EspTempSensor(ActionContext action_context,
                             temperature_sensor_config_t temp_sensor_config)
    : action_context_{action_context}, temp_sensor_config_{temp_sensor_config} {
  StartSensor();
}

EspTempSensor::~EspTempSensor() { StopSensor(); }

void EspTempSensor::SetLocalId(int id) { local_id_ = id; }

HardwareDevice EspTempSensor::description() const {
  auto device_type = HardwareSensor{};
  device_type.local_id = local_id_;
  device_type.descriptor = "Esp temperature sensor";
  device_type.unit = "°C";
  return HardwareDevice{device_type};
}

ActionPtr<DeviceStateAction> EspTempSensor::GetState() {
  return ActionPtr<EspTempDataStateAction>{action_context_, GetTemperature()};
}

ActionPtr<DeviceStateAction> EspTempSensor::Execute(VariantData const&) {
  return GetState();
}

float EspTempSensor::GetTemperature() {
  float tsens_value = -1000;
  if (temp_sensor_ != nullptr) {
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor_, &tsens_value));
  }

  return tsens_value;
}

void EspTempSensor::StartSensor() {
  ESP_ERROR_CHECK(
      temperature_sensor_install(&temp_sensor_config_, &temp_sensor_));
  ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor_));
}
void EspTempSensor::StopSensor() {
  ESP_ERROR_CHECK(temperature_sensor_disable(temp_sensor_));
  ESP_ERROR_CHECK(temperature_sensor_uninstall(temp_sensor_));
}
}  // namespace ae

#endif
