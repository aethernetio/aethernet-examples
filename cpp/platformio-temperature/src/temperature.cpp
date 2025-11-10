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

#include <iostream>
#include <string_view>

#include "aether/all.h"
#include "temp_sensor.h"

#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_task_wdt.h>

namespace ae::temperature_sensor {
static constexpr int kWaitTime = 1;
static constexpr int kWaitUntil = 5;

static constexpr std::string_view kWifiSsid = "Test1234";
static constexpr std::string_view kWifiPass = "Test1234";
static constexpr std::string_view kTag = "Temperature";

static constexpr auto kFromUid =
    ae::Uid::FromString("3ac93165-3d37-4970-87a6-fa4ee27744e4");
static constexpr auto kToUid =    
    ae::Uid::FromString("3ac93165-3d37-4970-87a6-fa4ee27744e5");

constexpr ae::SafeStreamConfig kSafeStreamConfig{
    std::numeric_limits<std::uint16_t>::max(),                // buffer_capacity
    (std::numeric_limits<std::uint16_t>::max() / 2) - 1,      // window_size
    (std::numeric_limits<std::uint16_t>::max() / 2) - 1 - 1,  // max_data_size
    10,                              // max_repeat_count
    std::chrono::milliseconds{600},  // wait_confirm_timeout
    {},                              // send_confirm_timeout
    std::chrono::milliseconds{400},  // send_repeat_timeout
};

ae::RcPtr<AetherApp> construct_aether_app() {
  return AetherApp::Construct(
      AetherAppContext{}.AdaptersFactory([](AetherAppContext const &context) {
        auto adapter_registry =
            context.domain().CreateObj<ae::AdapterRegistry>();
        adapter_registry->Add(context.domain().CreateObj<ae::WifiAdapter>(
            ae::GlobalId::kWiFiAdapter, context.aether(), context.poller(),
            context.dns_resolver(), std::string(kWifiSsid),
            std::string(kWifiPass)));
        return adapter_registry;
      }));
}

} // namespace ae::temperature_sensor


extern "C" void app_main();
int AetherTemperatureExample();
ae::RcPtr<ae::AetherApp> aether_app{};
ae::RcPtr<ae::P2pSafeStream> sender_stream{};

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
  if (err != 0)
    ESP_LOGE(std::string(ae::temperature_sensor::kTag).c_str(), "Reconfigure WDT is failed!");

  AetherTemperatureExample();
}

int AetherTemperatureExample() {
  temperature_sensor_config_t temp_sensor_config_ =
      TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);

  ae::TemperatureSensor temp_sensor{temp_sensor_config_};
  
  auto aether_app = ae::AetherApp::Construct(
      ae::AetherAppContext{}
#if defined AE_DISTILLATION
          .AdaptersFactory([](ae::AetherAppContext const& context) {
            auto adapter_registry =
                context.domain().CreateObj<ae::AdapterRegistry>();
#  if defined ESP32_WIFI_ADAPTER_ENABLED
            adapter_registry->Add(context.domain().CreateObj<ae::WifiAdapter>(
                ae::GlobalId::kWiFiAdapter, context.aether(), context.poller(),
                context.dns_resolver(), std::string(ae::temperature_sensor::kWifiSsid),
                std::string(ae::temperature_sensor::kWifiPass)));
#  else
            adapter_registry->Add(
                context.domain().CreateObj<ae::EthernetAdapter>(
                    ae::GlobalId::kEthernetAdapter, context.aether(),
                    context.poller(), context.dns_resolver()));
#  endif
            return adapter_registry;
          })
#endif
  );

  ae::Client::ptr client_temperature;

  auto select_client_temperature = aether_app->aether()->SelectClient(
      ae::temperature_sensor::kFromUid, 0);

  select_client_temperature->StatusEvent().Subscribe(ae::ActionHandler{
      ae::OnResult{[&](auto const &action) { client_temperature = action.client(); }},
      ae::OnError{[&]() { aether_app->Exit(1); }}});

  aether_app->WaitActions(select_client_temperature);

  // clients must be selected
  assert(client_temperature);

  // Make clients messages exchange.
  int confirmed_count = 0;
  sender_stream= ae::MakeRcPtr<ae::P2pSafeStream>(
      *aether_app, ae::temperature_sensor::kSafeStreamConfig,
      ae::MakeRcPtr<ae::P2pStream>(*aether_app, client_temperature, 
      ae::temperature_sensor::kToUid));

  sender_stream->out_data_event().Subscribe([&](auto const &data) {
    auto str_response =
        std::string(reinterpret_cast<const char *>(data.data()), data.size());
    AE_TELED_DEBUG("Received a response [{}], confirm_count {}", str_response,
                   confirmed_count);
    confirmed_count++;
  });

  auto repeat_count = 10;
  auto request_timeout = std::chrono::minutes{5};

  auto repeatable = ae::ActionPtr<ae::RepeatableTask>{*aether_app, [&sender_stream, &temp_sensor](){
  auto temp = temp_sensor.GetTemperature();
  AE_TELED_DEBUG("Temperature is [{}]", temp);
  auto msg = std::to_string(temp);  
  sender_stream->Write(ae::DataBuffer{std::begin(msg), std::end(msg)});
  }, request_timeout, repeat_count};
  
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
