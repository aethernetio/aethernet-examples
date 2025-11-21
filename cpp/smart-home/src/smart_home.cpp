/*
 * Copyright 2024 Aethernet Inc.
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

#include <string_view>

#include "aether/all.h"

#include "commutator.h"
#include "temperature/temperature_factory.h"

#if defined ESP_PLATFORM
#  define ESP_WIFI 1
#else
#  define ETHERNET 1
#endif

// IWYU pragma: begin_keeps
#include "aether_construct_esp_wifi.h"
#include "aether_construct_ethernet.h"
// IWYU pragma: end_keeps

static constexpr auto kParentUid =
    ae::Uid::FromString("3ac93165-3d37-4970-87a6-fa4ee27744e4");

int SmartHomeMain() {
  /**
   * Construct a main aether application class.
   * It's include a Domain and Aether instances accessible by getter methods.
   * It has Update, WaitUntil, Exit, IsExit, ExitCode methods to integrate it in
   * your update loop.
   * Also it has action context protocol implementation \see Action.
   * To configure its creation \see AetherAppContext.
   */
  auto aether_app = ae::construct_aether_app();

  std::unique_ptr<ae::Commutator> commutator;

  // load or register new client for smart home
  aether_app->aether()
      ->SelectClient(kParentUid, 0)
      ->StatusEvent()
      .Subscribe(ae::ActionHandler{
          ae::OnError{[&]() { aether_app->Exit(1); }},
          ae::OnResult{[&](auto const &action) {
            auto smart_home_client = action.client();
            std::cout << ae::Format(
                R"(

>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 REGISTERED CLIENT'S UID: {}
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

                )",
                smart_home_client->uid());
            commutator = std::make_unique<ae::Commutator>(smart_home_client);
    // add sensors to commutator
#if defined ESP_PLATFORM
            auto temp_sensor_config =
                ae::EspTempSensorConfig{ae::TempSensorType::kEspTempSensor, {}};
            temp_sensor_config.config =
                TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
#else
            auto temp_sensor_config =
                ae::TempSensorConfig{ae::TempSensorType::kFakeTempSensor};
#endif
            commutator->AddDevice(ae::TemperatureFactory::CreateDevice(
                *aether_app, &temp_sensor_config));
          }},
      });

  /**
   * Application loop.
   * All the asynchronous actions are updated on this loop.
   * WaitUntil either waits until the next selected time or some action
   * triggers new event.
   */
  while (!aether_app->IsExited()) {
    // Wait for next event or timeout
    auto current_time = ae::Now();
    auto next_time = aether_app->Update(current_time);
    aether_app->WaitUntil(
        std::min(next_time, current_time + std::chrono::seconds{5}));
  }

  return aether_app->ExitCode();
}
