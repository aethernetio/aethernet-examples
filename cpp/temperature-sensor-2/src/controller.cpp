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

#include <chrono>
#include <cstdlib>

#include "aether/all.h"

// timeouts
// kMaxWaitTime is used to limit the wait time to prevent blocking other tasks
static constexpr auto kMaxWaitTime = std::chrono::seconds{1};
// Temperature measurement interval
static constexpr auto kTemperatureMeasureInterval = std::chrono::seconds{10};

/**
 * Standard uid for test application.
 * This is intended to use only for testing purposes due to its limitations.
 * For real applications you should register your own uid \see aethernet.io
 */
static constexpr auto kParentUid =
    ae::Uid::FromString("3ac93165-3d37-4970-87a6-fa4ee27744e4");
/**
 * \brief Uid of aether service for store the temperature values.
 * TODO: add actual uid
 */
static constexpr auto kServiceUid =
    ae::Uid::FromString("629bf907-293a-4b2b-bbc6-5e1bd6c89ffd");

#ifdef ESP_PLATFORM
#  ifndef WIFI_SSID
#    define WIFI_SSID "test_wifi"
#  endif
#  ifndef WIFI_PASSWORD
#    define WIFI_PASSWORD ""
#  endif

static const auto kWifiCreds = ae::WifiCreds{
    /* .ssid*/ std::string{WIFI_SSID},
    /* .password*/ std::string{WIFI_PASSWORD},
};
static const auto kWifiInit = ae::WiFiInit{
    std::vector<ae::WiFiAp>{{kWifiCreds, {}}},
    ae::WiFiPowerSaveParam{},
};
#endif

// Update temperature sensor
void UpdateTemperature();
// Message from aether service received
void MessageReceived(ae::DataBuffer const& buffer);
// Send the message value to the aether service
void SendValue(float value);
// Go to sleep method
void GoToSleep();

static ae::RcPtr<ae::AetherApp> aether_app;
static ae::RcPtr<ae::P2pStream> message_stream;
static ae::TimePoint last_temp_measure_time;

void setup() {
  aether_app = ae::AetherApp::Construct(
      ae::AetherAppContext{}
#if defined ESP_PLATFORM
  // For esp32 wifi adapter configured with wifi ssid and password required
#  if AE_DISTILLATION
          .AddAdapterFactory([&](ae::AetherAppContext const& context) {
            return ae::WifiAdapter::ptr::Create(
                ae::CreateWith{context.domain()}.with_id(
                    ae::GlobalId::kWiFiAdapter),
                context.aether(), context.poller(), context.dns_resolver(),
                kWifiInit);
          })
#  endif
#endif
  );

  // select controller's client
  auto select_client =
      aether_app->aether()->SelectClient(kParentUid, "Controller");

  select_client->StatusEvent().Subscribe(ae::ActionHandler{
      ae::OnResult{[](auto& action) {
        ae::Client::ptr client = action.client();
        client.WithLoaded([](auto const& c) {
          // open message stream to aether service client
          message_stream =
              c->message_stream_manager().CreateStream(kServiceUid);
          message_stream->out_data_event().Subscribe(MessageReceived);
        });
      }},
      ae::OnError{[]() {
        std::cerr << " !!! Client selection error";
        aether_app->Exit(1);
      }},
  });
}

void loop() {
  if ((ae::Now() - last_temp_measure_time) > kTemperatureMeasureInterval) {
    last_temp_measure_time = ae::Now();
    UpdateTemperature();
  }

  if (!aether_app) {
    return;
  }
  if (!aether_app->IsExited()) {
    // run aether update loop
    auto new_time = aether_app->Update(ae::Now());
    aether_app->WaitUntil(std::min(new_time, ae::Now() + kMaxWaitTime));
  } else {
    // cleanup resources
    message_stream.Reset();
    aether_app.Reset();
  }
}

// TODO: add implementation for actual temperature sensor
void UpdateTemperature() {
  // get random value as temperature
  static bool seed = (std::srand(std::time(nullptr)), true);
  (void)seed;

  static float last_value = 20.F;
  // get diff in range -2 to 2
  auto diff = (static_cast<float>(std::rand() % 40) / 10.F) - 2.F;
  auto value = last_value += diff;
  std::cout << ae::Format("\n >>> Temperature measured: {}°C\n\n", value);

  SendValue(value);
}

void MessageReceived(ae::DataBuffer const& buffer) {
  // TODO: add handle serivice's requests
  std::cout << ae::Format(" >>> Received message from service: [{}]\n", buffer);
}

void SendValue(float value) {
  // The stream is not initialized yet
  if (!message_stream) {
    return;
  }

  // encode temperature value in range 0 to 255 using equation: ev=(v+30)/3
  // use v=ev*3-30 to decode value
  auto encoded_value =
      static_cast<std::uint8_t>((std::clamp(value, -30.F, 50.F) + 30.F) * 3.F);

  auto message = ae::DataBuffer{};
  {
    auto writer = ae::VectorWriter<>{message};
    auto stream = ae::omstream{writer};
    // write message code and encoded value
    stream << std::uint8_t{0x03} << encoded_value;
  }

  auto write_action = message_stream->Write(std::move(message));
  write_action->StatusEvent().Subscribe([](auto) {
    // with any result ready to sleep
    GoToSleep();
  });
}

void GoToSleep() {
  std::cout << " >>> Going to sleep...\n";

  if (!aether_app) {
    return;
  }
  // save current aether state
  aether_app->aether().Save();
  // TODO: add implementation for actual sleep
}
