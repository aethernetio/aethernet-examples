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

#ifndef TEMPERATURE_ESP_TEMP_SENSOR_H_
#define TEMPERATURE_ESP_TEMP_SENSOR_H_

#if defined ESP_PLATFORM

#  include "aether/all.h"
#  include "driver/temperature_sensor.h"

#  include "idevice.h"
#  include "api/types.h"

namespace ae {
class EspTempSensor : public IDevice {
 public:
  EspTempSensor(ActionContext action_context,
                temperature_sensor_config_t temp_sensor_config);
  ~EspTempSensor() override;

  void SetLocalId(int id) override;

  HardwareDevice description() const override;

  ActionPtr<DeviceStateAction> GetState() override;

  ActionPtr<DeviceStateAction> Execute(VariantData const& command) override;

  float GetTemperature();

 private:
  void StartSensor();
  void StopSensor();

  ActionContext action_context_;
  int local_id_{};
  temperature_sensor_handle_t temp_sensor_ = nullptr;
  temperature_sensor_config_t temp_sensor_config_ =
      TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
};

}  // namespace ae
#endif
#endif  // TEMPERATURE_ESP_TEMP_SENSOR_H_
