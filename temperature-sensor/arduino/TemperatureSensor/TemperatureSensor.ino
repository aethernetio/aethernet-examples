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

#include "soc/soc_caps.h"
#include "driver/temperature_sensor.h"

// include Aether lib
#include "aether_lib.h"

// use wifi for esp
static constexpr std::string_view kWifiSsid = "test_ssid";
static constexpr std::string_view kWifiPassword = "test_password";
/*
 * Maximum number of records to store.
 * Maximum amount should fit into 1K bytes of message.
 * 1 - byte for message code
 * 2 - byte for record count
 * 2 - byte each record size
 */
static constexpr std::uint16_t kMaxRecordCount = (1024 - 1 - 2) / 2;
/** temperature value update interval */
static constexpr ae::Duration kUpdateInterval = std::chrono::seconds{10};
/** Stream's time to live */
static constexpr ae::Duration kStreamRemoveTimeout = std::chrono::minutes{10};

/**
 * Standard uid for test application.
 * This is intended to use only for testing purposes due to its limitations.
 * For real applications you should register your own uid \see aethernet.io
 */
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
 * \brief Remove old streams.
 */
void RemoveStreams();
/**
 * \brief Pack requested count of recordse into vector.
 */
using PackedRecord = std::pair<std::uint8_t, std::uint8_t>;
std::vector<PackedRecord> RequestRecords(std::uint16_t count);
/**
 * \brief Message handler
 */
void OnMessage(ae::Uid const& from, std::vector<std::uint8_t> const& message);
/**
 * \brief Message send implementation
 */
void SendMessage(ae::Uid const& to, std::vector<std::uint8_t> message);

struct StreamStore {
  ae::RcPtr<ae::P2pStream> stream;
  ae::TimePoint remove_time;
};

struct Record {
  float temperature;
  std::chrono::seconds time_delta;
};

struct Context {
  ae::RcPtr<ae::AetherApp> aether_app;
  std::map<ae::Uid, StreamStore> streams;
  ae::TimePoint last_update_time;
  ae::TimePoint last_remove_time;
  std::deque<Record> records;
};

static Context context{};

void setup() {
  // create an app
  context.aether_app = ae::AetherApp::Construct(
      ae::AetherAppContext{}  // use wifi for esp
          .AdaptersFactory([](ae::AetherAppContext const& context) {
            auto adapter_registry =
                context.domain().CreateObj<ae::AdapterRegistry>();
            auto wifi_adapter = context.domain().CreateObj<ae::WifiAdapter>(
                context.aether(), context.poller(), context.dns_resolver(),
                std::string(kWifiSsid), std::string(kWifiPassword));
            adapter_registry->Add(std::move(wifi_adapter));
            return adapter_registry;
          }));

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
              auto [it, _] = context.streams.emplace(
                  stream->destination(),
                  StreamStore{std::move(stream),
                              ae::Now() + kStreamRemoveTimeout});
              it->second.stream->out_data_event().Subscribe(
                  [uid{it->first}](auto const& data) { OnMessage(uid, data); });
            });
      }},
      ae::OnError{[]() {
        std::cerr << "Register/Load client failed\n";
        context.aether_app->Exit(1);
      }},
  });

  context.last_update_time = ae::Now();
}

void loop() {
  auto current_time = ae::Now();
  // update sensor data
  if ((current_time - context.last_update_time) > kUpdateInterval) {
    UpdateRead();
    context.last_update_time = current_time;
  }

  // remove unused streams
  if ((current_time - context.last_remove_time) > kStreamRemoveTimeout) {
    RemoveStreams();
    context.last_remove_time = current_time;
  }

  if (!context.aether_app) {
    return;
  }
  if (!context.aether_app->IsExited()) {
    auto new_time = context.aether_app->Update(current_time);
    context.aether_app->WaitUntil(new_time);
  } else {
    context.streams.clear();
    context.aether_app.Reset();
  }
}

void OnMessage(ae::Uid const& from, std::vector<std::uint8_t> const& message) {
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
      assert((count > 0) && "Count should be > 0");
      auto records = RequestRecords(count);

      // serialize the answer
      std::vector<std::uint8_t> answer;
      auto writer = ae::VectorWriter<SizeType>{answer};
      auto os = ae::omstream{writer};
      os << std::uint8_t{3};  // the message code
      os << records;

      // send the answer to the client
      SendMessage(from, std::move(answer));
    } break;
    default:
      break;
  }
}

void SendMessage(ae::Uid const& to, std::vector<std::uint8_t> message) {
  auto it = context.streams.find(to);
  assert((it != context.streams.end()) && "Stream should exists");
  it->second.remove_time = ae::Now() + kStreamRemoveTimeout;

  auto const& stream = it->second.stream;
  stream->Write(std::move(message))->StatusEvent().Subscribe(ae::OnError{[]() {
    std::cerr << "Send message error\n";
    context.aether_app->Exit(3);
  }});
}

void UpdateRead() {
  auto time = ae::Now();
  auto delta = time - context.last_update_time;
  context.last_update_time = time;

  auto value = ReadTemperature();
  std::cout << ">> Temperature: " << value << "°C\n\n";
  // the last value is first value
  context.records.push_front(Record{
      value,
      std::chrono::duration_cast<std::chrono::seconds>(delta),
  });
  if (context.records.size() > kMaxRecordCount) {
    context.records.pop_back();
  }
}

void RemoveStreams() {
  auto current_time = ae::Now();
  for (auto it = context.streams.begin(); it != context.streams.end();) {
    if (current_time >= it->second.remove_time) {
      it = context.streams.erase(it);
    } else {
      ++it;
    }
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
  (void)seed;
  static float last_value = 20.F;
  // get diff in range -2 to 2
  auto diff = (static_cast<float>(std::rand() % 40) / 10.F) - 2.F;
  auto value = last_value += diff;
  return value;
}
#endif

std::vector<PackedRecord> RequestRecords(std::uint16_t count) {
  auto data_count =
      std::min(context.records.size(), static_cast<std::size_t>(count));
  // collect records in packed format
  // value represented in range -30 to 50 in one byte integer (T + 30) * 3
  // time represented in seconds between measures
  using PackedRecord = std::pair<std::uint8_t, std::uint8_t>;
  std::vector<PackedRecord> packed_data;
  packed_data.reserve(data_count);
  for (std::size_t i = 0; i < data_count; ++i) {
    auto const& record = context.records[i];
    auto rec = PackedRecord{
        /*.temperature = */ static_cast<std::uint8_t>(
            (std::clamp(record.temperature, -30.F, 50.F) + 30.F) * 3.F),
        /*.time_delta = */
        static_cast<std::uint8_t>(record.time_delta.count()),
    };
    packed_data.push_back(rec);
  }
  return packed_data;
}
