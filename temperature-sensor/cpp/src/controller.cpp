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
#include "wifi_provisioning.h"

#if defined ESP_PLATFORM
#  include "soc/soc_caps.h"
#  include "driver/temperature_sensor.h"
#  include "sleep_manager.h"
#endif

// include Aether lib
#include "aether/all.h"
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
static SleepManager sleep_mngr{};

void setup() {
  auto cause = sleep_mngr.getWakeupCause();
  std::cout << ae::Format(R"(Cause {})", cause) << std::endl;
  sleep_mngr.enableTimerWakeup(15000000); // every 15 second
  
  // create an app
  context.aether_app = ae::AetherApp::Construct(ae::AetherAppContext{});

  // create a client and subscribe to new messages
  auto select_client_action =
      context.aether_app->aether()->SelectClient(kParentUid, "Controller");

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

        context.aether_app->aether().Save();
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
  auto next_update = context.last_update_time + kUpdateInterval;
  if (current_time >= next_update) {
    UpdateRead();
    context.last_update_time = current_time;
    next_update = context.last_update_time + kUpdateInterval;
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
    context.aether_app->WaitUntil(std::min(new_time, next_update));
  } else {
    context.streams.clear();
    context.aether_app.Reset();
  }
  
  context.aether_app->aether().Save();
  sleep_mngr.enterDeepSleep();
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
    case 3:  // 3,count:std::uint16_t request records
    {
      std::uint16_t count{};
      is >> count;
      assert((count > 0) && "Count should be > 0");
      auto records = RequestRecords(count);

      // serialize the answer
      std::vector<std::uint8_t> answer;
      auto writer = ae::VectorWriter<SizeType>{answer};
      auto os = ae::omstream{writer};

      // 3,records:std::vector<PackedRecord> answer
      os << std::uint8_t{3};
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
  }});
}

void UpdateRead() {
  auto time = ae::Now();
  auto delta = time - context.last_update_time;

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
#  ifndef ESP_M5STACK_ATOM_LITE
#    include "driver/i2c.h"
#    include "BME68x_SensorAPI/bme68x.h"
#    include <cstring>
#    include "esp_log.h"

// --- Hardware Settings ---
#    define BME_I2C_NUM I2C_NUM_0
#    define BME_SDA_PIN 2
#    define BME_SCL_PIN 1
#    define BME_ADDR 0x77

// --- SAFER Interface Functions ---

// FIX 1: Use a Fixed Buffer instead of VLA (Variable Length Array) to prevent
// stack smash
#    define MAX_I2C_BUFFER 64

static BME68X_INTF_RET_TYPE bme_i2c_read(uint8_t reg_addr, uint8_t* reg_data,
                                         uint32_t len, void* intf_ptr) {
  uint8_t dev_addr = *(uint8_t*)intf_ptr;
  esp_err_t err = i2c_master_write_read_device(
      BME_I2C_NUM, dev_addr, &reg_addr, 1, reg_data, len, pdMS_TO_TICKS(100));
  return (err == ESP_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
}

static BME68X_INTF_RET_TYPE bme_i2c_write(uint8_t reg_addr,
                                          const uint8_t* reg_data, uint32_t len,
                                          void* intf_ptr) {
  uint8_t dev_addr = *(uint8_t*)intf_ptr;

  // SAFETY CHECK: Prevent buffer overflow if driver requests too much data
  if (len > (MAX_I2C_BUFFER - 1)) {
    ESP_LOGE("BME688", "Write length too big: %lu", len);
    return BME68X_E_COM_FAIL;
  }

  uint8_t buffer[MAX_I2C_BUFFER];
  buffer[0] = reg_addr;
  // Safe copy
  memcpy(&buffer[1], reg_data, len);

  esp_err_t err = i2c_master_write_to_device(BME_I2C_NUM, dev_addr, buffer,
                                             len + 1, pdMS_TO_TICKS(100));
  return (err == ESP_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
}

static void bme_delay_us(uint32_t period, void* intf_ptr) {
  uint32_t ms = (period / 1000) + 1;
  // FIX 2: Ensure we don't delay for 0 ticks, but also don't block interrupts
  vTaskDelay(pdMS_TO_TICKS(ms));
}

// --- Main Reading Function ---
float ReadTemperature() {
  static struct bme68x_dev bme;
  static struct bme68x_conf conf;
  static uint8_t dev_addr = BME_ADDR;

  // Static Initialization Block (Runs once)
  static bool initialized = []() {
    // 1. INSTALL I2C DRIVER
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BME_SDA_PIN,
        .scl_io_num = BME_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {.clk_speed = 100000},
        .clk_flags = 0,
    };

    // Prevent re-install crash if I2C is already used elsewhere
    if (i2c_param_config(BME_I2C_NUM, &i2c_conf) != ESP_OK) return false;
    if (i2c_driver_install(BME_I2C_NUM, i2c_conf.mode, 0, 0, 0) != ESP_OK)
      return false;

    // 2. Initialize BME Sensor
    bme.read = bme_i2c_read;
    bme.write = bme_i2c_write;
    bme.intf = BME68X_I2C_INTF;
    bme.delay_us = bme_delay_us;
    bme.intf_ptr = &dev_addr;
    bme.amb_temp = 25;

    if (bme68x_init(&bme) != BME68X_OK) {
      dev_addr = 0x76;  // Try alternate address
      if (bme68x_init(&bme) != BME68X_OK) return false;
    }

    // 3. Configure Sensor
    conf.filter = BME68X_FILTER_OFF;
    conf.odr = BME68X_ODR_NONE;
    conf.os_hum = BME68X_OS_NONE;
    conf.os_pres = BME68X_OS_NONE;
    conf.os_temp = BME68X_OS_2X;
    bme68x_set_conf(&conf, &bme);

    return true;
  }();

  if (!initialized) return -1000.0f;

  // Trigger measurement
  if (bme68x_set_op_mode(BME68X_FORCED_MODE, &bme) != BME68X_OK)
    return -1000.0f;

  // Wait for measurement
  uint32_t del_period = bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf, &bme);
  bme.delay_us(del_period, bme.intf_ptr);

  // Read Data
  struct bme68x_data data;
  uint8_t n_fields;
  if (bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &bme) ==
          BME68X_OK &&
      n_fields > 0) {
    return data.temperature;
  }

  return -1000.0f;
}
#  else
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
  (void)initialized;

  float value = -1000;
  ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &value));
  return value;
}
#  endif  // ESP_M5STACK_ATOM_LITE
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
