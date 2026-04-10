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

#include <Arduino.h>
#include <aether_lib.h>

#define TEST_UID "3ac93165-3d37-4970-87a6-fa4ee27744e4"

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

constexpr auto kParentUid = ae::Uid::FromString(TEST_UID);
constexpr auto kEchoServiceUid =
    ae::Uid::FromString("61BEF6C8-9680-47D2-8029-1A1D89E3F54C");

ae::ObjPtr<ae::Adapter> MakeAdapter(ae::AetherAppContext const& context) {
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

void ReceiveMessage(ae::DataBuffer const& data) {
  std::cout << ae::Format(" >>> Received {}\n", data);
}

void SendMessage(ae::RcPtr<ae::P2pStream> const& message_stream) {
  constexpr auto message = std::string_view{"Hello echo!"};
  std::cout << ae::Format(" >>> Send message {}\n", message);
  message_stream->Write(ae::DataBuffer{message.begin(), message.end()});
}

ae::RcPtr<ae::AetherApp> aether_app;
ae::RcPtr<ae::P2pStream> message_stream;

void setup() {
  aether_app = ae::AetherApp::Construct(
      ae::AetherAppContext{}.AddAdapterFactory(&MakeAdapter));

  auto select_client =
      aether_app->aether()->SelectClient(kParentUid, "echo_client");
  select_client->StatusEvent().Subscribe(ae::OnResult{[&](auto const& action) {
    ae::Client::ptr c = action.client();
    message_stream = c->message_stream_manager().CreateStream(kEchoServiceUid);
    message_stream->out_data_event().Subscribe([&](auto const& data) {
      ReceiveMessage(data);
      aether_app->Exit(0);
    });
    SendMessage(message_stream);
  }});
}

void loop() {
  // Wait/Update loop to run aether internal actions
  if (aether_app && !aether_app->IsExited()) {
    aether_app->WaitUntil(aether_app->Update(ae::Now()));
  }
}
