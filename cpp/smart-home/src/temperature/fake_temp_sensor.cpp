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

#include "temperature/fake_temp_sensor.h"

#include <cstdlib>
#include <algorithm>

namespace ae {
class FakeTempSensorDataStateAction : public DeviceStateAction {
 public:
  FakeTempSensorDataStateAction(ActionContext action_context, float value)
      : DeviceStateAction{action_context}, state_data_{} {
    state_data_.payload = VariantData{VariantDouble{value}};
    state_data_.timestamp = static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            Now().time_since_epoch())
            .count());
  }

  UpdateStatus Update() override { return UpdateStatus::Result(); }

  DeviceStateData state_data() const override { return state_data_; }

  DeviceStateData state_data_;
};

FakeTempSensor::FakeTempSensor(ActionContext action_context)
    : actio_context_{action_context} {}

void FakeTempSensor::SetLocalId(int id) { local_id_ = id; }

HardwareDevice FakeTempSensor::description() const {
  auto device_type = HardwareSensor{};
  device_type.local_id = local_id_;
  device_type.descriptor = "Fake temperature sensor";
  device_type.unit = "°C";
  return HardwareDevice{device_type};
}

ActionPtr<DeviceStateAction> FakeTempSensor::GetState() {
  return ActionPtr<FakeTempSensorDataStateAction>{actio_context_, Read()};
}

ActionPtr<DeviceStateAction> FakeTempSensor::Execute(VariantData const&) {
  return GetState();
}

float FakeTempSensor::Read() {
  // randomly generate a new value change
  static bool const seed =
      (std::srand(static_cast<unsigned int>(time(nullptr))), true);
  (void)seed;

  // value changes in range -2 to 2
  float value = (static_cast<float>(std::rand() % 4000) - 2000) / 1000;
  old_value_ += value;
  old_value_ = std::clamp(old_value_, -100.0F, 100.0F);
  return old_value_;
}
}  // namespace ae
