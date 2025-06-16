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

#include "aether/all.h"

#include "sensor.h"

#ifndef APP_ID
#  error APP_ID must be defined
#endif

int client_main(ae::AetherAppContext&& aether_app_context) {
  auto aether_app = ae::AetherApp::Construct(std::move(aether_app_context));

  std::unique_ptr<Sensor> sensor;
  constexpr auto app_id = ae::Uid::FromString(APP_ID);

  auto select_client = aether_app->aether()->SelectClient(app_id, 0);
  select_client->ResultEvent().Subscribe([&](auto& action) {
    aether_app->domain().SaveRoot(aether_app->aether());
    sensor = ae::make_unique<Sensor>(aether_app->aether(),
                                     std::move(action.client()), app_id);
  });

  while (!aether_app->IsExited()) {
    auto next_time = aether_app->Update(ae::Now());
    aether_app->WaitUntil(next_time);
  }

  return aether_app->ExitCode();
}
