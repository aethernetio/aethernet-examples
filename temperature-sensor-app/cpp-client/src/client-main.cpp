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

#include "aether/aether_app.h"

#include "sensor.h"

#ifndef APP_ID
#  error APP_ID must be defined
#endif

int client_main(ae::AetherAppConstructor&& aether_app_constructor) {
  auto aether_app = ae::AetherApp::Construct(std::move(aether_app_constructor));

  std::unique_ptr<Sensor> sensor;
  auto app_id = ae::Uid::FromString(APP_ID);

  ae::Event<void(ae::Client::ptr client)> client_registered;
  ae::EventSubscriber{client_registered}.Subscribe([&](auto client) {
    aether_app->domain().SaveRoot(aether_app->aether());
    sensor = ae::make_unique<Sensor>(aether_app->aether(), client, app_id);
    client->client_connection()->SendTelemetry();
  });

  // get client
  if (!aether_app->aether()->clients().empty()) {
    // from saved state
    client_registered.Emit(aether_app->aether()->clients()[0]);
  } else {
    // register new one
    auto reg_action = aether_app->aether()->RegisterClient(app_id);
    reg_action->ResultEvent().Subscribe([&](auto const& reg_action) {
      client_registered.Emit(reg_action.client());
    });
  }

  while (!aether_app->IsExited()) {
    auto next_time = aether_app->Update(ae::Now());
    aether_app->WaitUntil(next_time);
  }

  return aether_app->ExitCode();
}
