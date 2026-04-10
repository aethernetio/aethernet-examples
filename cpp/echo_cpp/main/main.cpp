/*
 * Copyright 2026 Aethernet Inc.
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

#define TEST_UID "3ac93165-3d37-4970-87a6-fa4ee27744e4"

constexpr auto kParentUid = ae::Uid::FromString(TEST_UID);
constexpr auto kEchoServiceUid =
    ae::Uid::FromString("61BEF6C8-9680-47D2-8029-1A1D89E3F54C");

ae::ObjPtr<ae::Adapter> MakeAdapter(ae::AetherAppContext const&);

void ReceiveMessage(ae::DataBuffer const& data) {
  std::cout << ae::Format(
      " >>> Received {}\n",
      std::string_view{reinterpret_cast<char const*>(data.data()),
                       data.size()});
}

void SendMessage(ae::RcPtr<ae::P2pStream> const& message_stream) {
  constexpr auto message = std::string_view{"Hello echo!"};
  std::cout << ae::Format(" >>> Send message {}\n", message);
  message_stream->Write(ae::DataBuffer{message.begin(), message.end()});
}

int EchoExample() {
  auto aether_app = ae::AetherApp::Construct(
      ae::AetherAppContext{}.AddAdapterFactory(&MakeAdapter));

  auto select_client =
      aether_app->aether()->SelectClient(kParentUid, "echo_client");
  select_client->StatusEvent().Subscribe(ae::OnResult{[&](auto const& action) {
    ae::Client::ptr c = action.client();
    auto ms = c->message_stream_manager().CreateStream(kEchoServiceUid);
    ms->out_data_event().Subscribe([&, ms](auto const& data) {
      ReceiveMessage(data);
      aether_app->Exit(0);
    });
    SendMessage(ms);
  }});

  // Wait/Update loop to run aether internal actions
  while (!aether_app->IsExited()) {
    aether_app->WaitUntil(aether_app->Update(ae::Now()));
  }
  return aether_app->ExitCode();
}

#ifdef ESP_PLATFORM
ae::ObjPtr<ae::Adapter> MakeAdapter(ae::AetherAppContext const& context) {
#  ifndef WIFI_SSID
#    error "Provide WIFI_SSID by -DWIFI_SSID="
#  endif
#  ifndef WIFI_PASSWORD
#    error "Provide WIFI_PASSWORD by -DWIFI_PASSWORD="
#  endif

  return ae::WifiAdapter::ptr::Create(
      context.domain(), context.aether(), context.poller(),
      context.dns_resolver(),
      ae::WiFiInit{
          .wifi_ap = {ae::WiFiAp{
              .creds = {.ssid = WIFI_SSID, .password = WIFI_PASSWORD},
              .static_ip = {}}},
          .psp = {},
      });
}
#else
ae::ObjPtr<ae::Adapter> MakeAdapter(ae::AetherAppContext const& context) {
  return ae::EthernetAdapter::ptr::Create(context.domain(), context.aether(),
                                          context.poller(),
                                          context.dns_resolver());
}
#endif

#ifdef ESP_PLATFORM
#  include <freertos/FreeRTOS.h>
#  include <esp_log.h>
#  include <esp_task_wdt.h>

extern "C" void app_main() {
  esp_task_wdt_config_t config_wdt = {
      /*.timeout_ms = */ 60000,
      /*.idle_core_mask = */ 0,  // i.e. do not watch any idle task
      /*.trigger_panic = */ true};

  esp_err_t err = esp_task_wdt_reconfigure(&config_wdt);
  if (err != 0) {
    ESP_LOGE("ECHO", "Reconfigure WDT is failed!");
  }

  EchoExample();
}
#else
int main() { return EchoExample(); }
#endif
