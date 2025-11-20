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

#include "temperature/temperature_factory.h"

// IWYU pragma: begin_keeps
#include "temperature/esp_temp_sensor.h"
#include "temperature/fake_temp_sensor.h"
// IWYU pragma: end_keeps

namespace ae {
std::unique_ptr<IDevice> TemperatureFactory::CreateDevice(
    [[maybe_unused]] ActionContext action_context, TempSensorConfig* config) {
  switch (config->type) {
    case TempSensorType::kEspTempSensor:
#if defined ESP_PLATFORM
      auto* esp_config = reinterpret_cast<EspTempSensorConfig*>(config);
      return std::make_unique<EspTempSensor>(action_context,
                                             esp_config->config);
#else
      return nullptr;
#endif
    case TempSensorType::kFakeTempSensor:
      return std::make_unique<FakeTempSensor>(action_context);
    default:
      break;
  }
  return nullptr;
}

}  // namespace ae
