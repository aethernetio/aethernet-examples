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

#include <map>
#include <deque>
#include <string>
#include <chrono>
#include <vector>
#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <string_view>

#if defined ESP_PLATFORM
#  include "soc/soc_caps.h"
#  include "driver/temperature_sensor.h"
#endif

// include Aether lib
#include "aether_lib.h"

#if ESP_PLATFORM
// use wifi for esp
static constexpr std::string_view kWifiSsid = "test_ssid";
static constexpr std::string_view kWifiPassword = "test_password";
#endif
static constexpr std::uint16_t kMaxRecordCount = (1024 - 1 - 2) / 2;
static constexpr auto kParentUid =
    ae::Uid::FromString("3ac93165-3d37-4970-87a6-fa4ee27744e4");

/**
 * \brief Read sensor's current temperature
 */
float ReadTemperature();
/**
 * \brief Read new temperature value and store it in the context.
 */
void UpdateRead();
/**
 * \brief Message handler
 */
void OnMessage(ae::Uid const& from, std::vector<std::uint8_t> const& message);

struct Record {
  float temperature;
  std::chrono::seconds time_delta;
};

struct Context {
  ae::RcPtr<ae::AetherApp> aether_app;
  std::map<ae::Uid, ae::RcPtr<ae::P2pStream>> streams;
  ae::ActionPtr<ae::RepeatableTask> read_task;
  ae::TimePoint last_update_time;
  std::deque<Record> records;
};

static Context context{};

void setup() {
  // create an app
  context.aether_app = ae::AetherApp::Construct(
      ae::AetherAppContext{}
#if ESP_PLATFORM
          // use wifi for esp
          .AdaptersFactory([](ae::AetherAppContext const& context) {
            auto adapter_registry =
                context.domain().CreateObj<ae::AdapterRegistry>();
            auto wifi_adapter = context.domain().CreateObj<ae::WifiAdapter>(
                context.aether(), context.poller(), context.dns_resolver(),
                std::string(kWifiSsid), std::string(kWifiPassword));
            adapter_registry->Add(std::move(wifi_adapter));
            return adapter_registry;
          })
#else
// use default factory for desktop
#endif
  );

  // create a client and subscribe to new messages
  auto select_client_action =
      context.aether_app->aether()->SelectClient(kParentUid, 0);

  select_client_action->StatusEvent().Subscribe(ae::ActionHandler{
      ae::OnResult{[](auto const& action) {
        ae::Client::ptr client = action.client();
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
                         client->uid())
                  << std::endl;

        client->message_stream_manager().new_stream_event().Subscribe(
            [](ae::RcPtr<ae::P2pStream> stream) {
              // save stream to the storage and subscribe to messages
              auto [it, _] = context.streams.emplace(stream->destination(),
                                                     std::move(stream));
              it->second->out_data_event().Subscribe(
                  [uid{it->first}](auto const& data) { OnMessage(uid, data); });
            });
      }},
      ae::OnError{[]() {
        std::cerr << "Register/Load client failed\n";
        context.aether_app->Exit(1);
      }},
  });

  context.read_task = ae::ActionPtr<ae::RepeatableTask>{
      *context.aether_app, []() { UpdateRead(); }, std::chrono::seconds{10},
      /* infinite */};
  context.read_task->StatusEvent().Subscribe(ae::OnError{[]() {
    std::cerr << "Update read task failed\n";
    context.aether_app->Exit(2);
  }});

  context.last_update_time = ae::Now();
}

void loop() {
  if (!context.aether_app) {
    return;
  }
  if (!context.aether_app->IsExited()) {
    auto new_time = context.aether_app->Update(ae::Now());
    context.aether_app->WaitUntil(new_time);
  } else {
    context.streams.clear();
    context.aether_app.Reset();
  }
}

void OnMessage(ae::Uid const& from, std::vector<std::uint8_t> const& message) {
  auto it = context.streams.find(from);
  assert((it != context.streams.end()) && "Stream should exists");
  auto const& stream = it->second;

  // data {i,o}mstreams uses special type to save containers size
  using SizeType = ae::TieredInt<std::uint64_t, std::uint8_t, 250>;

  // parse the message
  auto reader = ae::VectorReader<SizeType>{message};
  auto is = ae::imstream{reader};
  // first must be the message code
  std::uint8_t code{};
  is >> code;
  switch (code) {
    case 3: {
      std::uint16_t count{};
      is >> count;
      assert((count > 0) && (count < kMaxRecordCount) &&
             "Count should in range (0 .. kMaxRecordCount)");
      auto data_count =
          std::min(context.records.size(), static_cast<std::size_t>(count));
      // collect records in packed format
      // value represented in range -30 to 50 in one byte integer (T + 30) * 3
      // time represented in seconds between measures
      std::vector<std::pair<std::uint8_t, std::uint8_t>> packed_data;
      packed_data.reserve(data_count);
      for (std::size_t i = 0; i < data_count; ++i) {
        auto const& record = context.records[i];
        auto encoded_temp = static_cast<std::uint8_t>(
            (std::clamp(record.temperature, -30.F, 50.F) + 30.F) * 3.F);
        auto encoded_time =
            static_cast<std::uint8_t>(record.time_delta.count());
        packed_data.emplace_back(encoded_temp, encoded_time);
      }

      // serialize the answer
      std::vector<std::uint8_t> answer;
      auto writer = ae::VectorWriter<SizeType>{answer};
      auto os = ae::omstream{writer};
      os << std::uint8_t{3};  // the message code
      os << packed_data;

      // send the answer to the client
      stream->Write(std::move(answer))
          ->StatusEvent()
          .Subscribe(ae::OnError{[]() {
            std::cerr << "Send message error\n";
            context.aether_app->Exit(3);
          }});
    } break;
    default:
      break;
  }
}

void UpdateRead() {
  auto time = ae::Now();
  auto delta = time - context.last_update_time;
  context.last_update_time = time;

  auto value = ReadTemperature();
  std::cout << "\n>> Temperature: " << value << "°C\n\n";
  // the last value is first value
  context.records.push_front(Record{
      value,
      std::chrono::duration_cast<std::chrono::seconds>(delta),
  });
  if (context.records.size() > kMaxRecordCount) {
    context.records.pop_back();
  }
}

#if defined ESP_PLATFORM && \
    (SOC_TEMPERATURE_SENSOR_INTR_SUPPORT || SOC_TEMP_SENSOR_SUPPORTED)
float ReadTemperature() {
  static temperature_sensor_handle_t temp_sensor;
  // initialize once
  static bool initialized = []() {
    temperature_sensor_config_t temp_sensor_config =
        TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    ESP_ERROR_CHECK(
        temperature_sensor_install(&temp_sensor_config, &temp_sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));
    return true;
  }();

  float value = -1000;
  ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &value));
  return value;
}
#else
float ReadTemperature() {
  // get random value as temperature
  static bool seed = (std::srand(std::time(nullptr)), true);
  static float last_value = 20.F;
  // get diff in range -2 to 2
  auto diff = (static_cast<float>(std::rand() % 40) / 10.F) - 2.F;
  auto value = last_value += diff;
  return value;
}
#endif
