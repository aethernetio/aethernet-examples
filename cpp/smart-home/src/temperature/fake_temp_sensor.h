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

#ifndef TEMPERATURE_FAKE_TEMP_SENSOR_H_
#define TEMPERATURE_FAKE_TEMP_SENSOR_H_

#include "aether/all.h"

#include "idevice.h"
#include "api/types.h"

namespace ae {
class FakeTempSensor : public IDevice {
 public:
  explicit FakeTempSensor(ActionContext action_context);

  void SetLocalId(int id) override;
  HardwareDevice description() const override;
  ActionPtr<DeviceStateAction> GetState() override;
  ActionPtr<DeviceStateAction> Execute(VariantData const& command) override;

 private:
  float Read();

  ActionContext actio_context_;
  int local_id_{};
  float old_value_{18.F};
};
}  // namespace ae

#endif  // TEMPERATURE_FAKE_TEMP_SENSOR_H_
