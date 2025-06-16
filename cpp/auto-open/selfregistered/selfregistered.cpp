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

#include <limits>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

#if defined ESP_PLATFORM
#  include <freertos/FreeRTOS.h>
#  include <esp_log.h>
#  include <esp_task_wdt.h>
#endif

#include "aether/all.h"

#include "project_config.h"

constexpr ae::SafeStreamConfig kSafeStreamConfig{
    std::numeric_limits<std::uint16_t>::max(),                // buffer_capacity
    (std::numeric_limits<std::uint16_t>::max() / 2) - 1,      // window_size
    (std::numeric_limits<std::uint16_t>::max() / 2) - 1 - 1,  // max_data_size
    10,                              // max_repeat_count
    std::chrono::milliseconds{600},  // wait_confirm_timeout
    {},                              // send_confirm_timeout
    std::chrono::milliseconds{400},  // send_repeat_timeout
};

struct TestContext {
  ae::Ptr<ae::AetherApp> aether_app;
  int send_success = 0;
  bool greeting_success = false;
  ae::CumulativeEvent<ae::Client::ptr, 2> clients_registered_event;
  std::unique_ptr<ae::ByteIStream> bob_stream;
  std::unique_ptr<ae::ByteIStream> alice_stream;
  std::unique_ptr<ae::TimerAction> timer;
};

static TestContext* context{};

void BobMeetAlice(ae::Client::ptr const& alice_client,
                  ae::Client::ptr const& bob_client) {
  context->bob_stream = ae::make_unique<ae::P2pSafeStream>(
      *context->aether_app, kSafeStreamConfig,
      ae::make_unique<ae::P2pStream>(*context->aether_app, bob_client,
                                     alice_client->uid()));

  auto bob_say = std::string_view{"Hello"};
  auto bob_send_message =
      context->bob_stream->Write({std::begin(bob_say), std::end(bob_say)});

  bob_send_message->ResultEvent().Subscribe(
      [&](auto const&) { context->send_success += 1; });
  bob_send_message->ErrorEvent().Subscribe([&](auto const&) {
    std::cerr << "Send error" << std::endl;
    context->aether_app->Exit(1);
  });

  context->bob_stream->out_data_event().Subscribe([&](auto const& data) {
    auto str = std::string_view{reinterpret_cast<char const*>(data.data()),
                                data.size()};
    std::cout << "Bob received " << str << std::endl;
    context->greeting_success = true;
  });

  context->alice_stream = ae::make_unique<ae::P2pSafeStream>(
      *context->aether_app, kSafeStreamConfig,
      ae::make_unique<ae::P2pStream>(*context->aether_app, alice_client,
                                     bob_client->uid()));

  context->alice_stream->out_data_event().Subscribe([&](auto const& data) {
    auto str = std::string_view{reinterpret_cast<char const*>(data.data()),
                                data.size()};
    std::cout << "Alice received " << str << std::endl;
    auto answear = std::string_view{"Hi"};
    auto alice_send_message =
        context->alice_stream->Write({std::begin(answear), std::end(answear)});
    alice_send_message->ResultEvent().Subscribe(
        [&](auto const&) { context->send_success += 1; });
    alice_send_message->ErrorEvent().Subscribe([&](auto const&) {
      std::cerr << "Send answear error" << std::endl;
      context->aether_app->Exit(2);
    });
  });
}

void setup() {
  context->aether_app = ae::AetherApp::Construct(ae::AetherAppContext{
      []() {
        return ae::make_unique<ae::RamDomainStorage>();
      }}.Adapter([](ae::Domain* domain, ae::Aether::ptr const& aether) {
#if defined ESP32_WIFI_ADAPTER_ENABLED
    return domain->CreateObj<ae::Esp32WifiAdapter>(
        aether, aether->poller, std::string{kWifiSsid}, std::string{kWifiPass});
#else
    return domain->CreateObj<ae::EthernetAdapter>(aether, aether->poller);
#endif
  }));

  auto alice_selector = context->aether_app->aether()->SelectClient(
      ae::Uid::FromString("3ac93165-3d37-4970-87a6-fa4ee27744e4"), 0);
  auto bob_selector = context->aether_app->aether()->SelectClient(
      ae::Uid::FromString("3ac93165-3d37-4970-87a6-fa4ee27744e4"), 1);

  context->clients_registered_event.Connect(
      [](auto& action) { return action.client(); },
      alice_selector->ResultEvent(), bob_selector->ResultEvent());

  alice_selector->ErrorEvent().Subscribe([&](auto const&) {
    std::cerr << "Alice register failed" << std::endl;
    context->aether_app->Exit(1);
  });

  bob_selector->ErrorEvent().Subscribe([&](auto const&) {
    std::cerr << "Bob register failed" << std::endl;
    context->aether_app->Exit(1);
  });

  context->clients_registered_event.Subscribe([&](auto const& event) {
    std::cerr << "Bob meet alice" << std::endl;
    BobMeetAlice(event[0], event[1]);
  });

  context->timer = ae::make_unique<ae::TimerAction>(
      *context->aether_app->aether()->action_processor,
      std::chrono::seconds{10});
  context->timer->ResultEvent().Subscribe([&](auto const&) {
    std::cerr << "Test timeout" << std::endl;
    context->aether_app->Exit(3);
  });
}

void loop() {
  auto current_time = ae::Now();
  auto next_time = context->aether_app->Update(current_time);
  context->aether_app->WaitUntil(
      std::min(next_time, current_time + std::chrono::seconds{1}));
  if (context->greeting_success && (context->send_success == 2)) {
    context->aether_app->Exit(0);
  }
}

#if defined ESP_PLATFORM
extern "C" void app_main();
void app_main(void) {
  /*If you are using WDT at a given time, you must disable it by updating the
  configuration, or simply deleting the WDT tasks for each processor core
  using the following code:
  esp_task_wdt_delete(xTaskGetIdleTaskHandleForCore(0));
  esp_task_wdt_delete(xTaskGetIdleTaskHandleForCore(1));
  In the future, WDT support will be included in the core code of the
  Aether library.*/

  esp_task_wdt_config_t config_wdt = {
      .timeout_ms = 60000,
      .idle_core_mask = 0,  // i.e. do not watch any idle task
      .trigger_panic = true};

  esp_err_t err = esp_task_wdt_reconfigure(&config_wdt);
  if (err != 0) {
    ESP_LOGE("Selfregistered", "Reconfigure WDT is failed!");
  }
  TestContext main_context;
  context = &main_context;

  setup();
  while (!context->aether_app->IsExited()) {
    loop();
  }
  if (context->aether_app->ExitCode() != 0) {
    std::cerr << "Exit with code " << context->aether_app->ExitCode()
              << std::endl;
    return;
  }
  std::cout << "Exit normally" << std::endl;
}
#else
int main() {
  TestContext main_context;
  context = &main_context;
  setup();
  while (!context->aether_app->IsExited()) {
    loop();
  }
  return context->aether_app->ExitCode();
}
#endif
