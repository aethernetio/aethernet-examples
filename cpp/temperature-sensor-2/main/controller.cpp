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
#include "sensors/sensors.h"
#include "sleeping/sleeping.h"

#ifdef ESP_PLATFORM
#  include <freertos/FreeRTOS.h>
#  include <freertos/task.h>
#if BOARD_HAS_ULP == 1
#  include <ulp_lp_core.h>
#  include <lp_core_i2c.h>
#  include <esp_sleep.h>
#endif
#endif

#if BOARD_HAS_ULP == 1
extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[] asm("_binary_ulp_main_bin_end");

static void lp_core_init(void);
static void lp_i2c_init(void);
static void lp_goto_sleep(void);
#endif

#if BOARD_HAS_ULP == 1
static esp_sleep_wakeup_cause_t cause{ESP_SLEEP_WAKEUP_UNDEFINED};
#endif

// timeouts
// kMaxWaitTime is used to limit the wait time to prevent blocking other tasks
static constexpr auto kMaxWaitTime = std::chrono::seconds{1};

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
void UpdateSensors();
// Message from aether service received
void MessageReceived(ae::DataBuffer const& buffer);
// Send the message value to the aether service
void SendValue(std::uint16_t temperature);
// Go to sleep method
void GoToSleep(ae::Uap::Timer uap_timer);

static ae::RcPtr<ae::AetherApp> aether_app;
static ae::RcPtr<ae::P2pStream> message_stream;

void setup() {
#if BOARD_HAS_ULP == 1
  cause = esp_sleep_get_wakeup_cause();
  std::cout << ae::Format(R"(Cause {})", cause) << std::endl;
#endif

  aether_app = ae::AetherApp::Construct(
      ae::AetherAppContext{}
#if AE_DISTILLATION
#  ifdef ESP_PLATFORM
          // For esp32 wifi adapter configured with wifi ssid and password
          // required
          .AddAdapterFactory([&](ae::AetherAppContext const& context) {
            return ae::WifiAdapter::ptr::Create(
                ae::CreateWith{context.domain()}.with_id(
                    ae::GlobalId::kWiFiAdapter),
                context.aether(), context.poller(), context.dns_resolver(),
                kWifiInit);
          })
#  endif
          .UapFactory([](ae::AetherAppContext const& context) {
            auto uap = context.aether()->uap;
            if (uap.is_valid()) {
              return uap;
            }
            // configure uap
            // 60secs for send/receive
            // then 2 times by 30 seconds for send only
            return ae::Uap::ptr::Create(
                ae::CreateWith{context.domain()}.with_id(ae::GlobalId::kUap),
                context.aether(),
                std::initializer_list{
                    ae::Interval{.type = ae::IntervalType::kSendReceive,
                                 .duration = std::chrono::seconds{60},
                                 .window = std::chrono::seconds{10}},
                    ae::Interval{.type = ae::IntervalType::kSendOnly,
                                 .duration = std::chrono::seconds{30}},
                    ae::Interval{.type = ae::IntervalType::kSendOnly,
                                 .duration = std::chrono::seconds{30}}});
          })
#endif
  );

  // setup sleep on uap event
  aether_app->aether()->uap->sleep_event().Subscribe(GoToSleep);

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

          // measure temperature and send updated value
          UpdateSensors();
        });
      }},
      ae::OnError{[]() {
        std::cerr << " !!! Client selection error";
        aether_app->Exit(1);
      }},
  });
}

void loop() {
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

// implemented in sensors/
void UpdateSensors() {
  std::uint16_t temperature = {};
  ReadSensors(&temperature, nullptr, nullptr, nullptr, nullptr);
  SendValue(temperature);
}

void MessageReceived(ae::DataBuffer const& buffer) {
  // TODO: add handle serivice's requests
  std::cout << ae::Format(" >>> Received message from service: [{}]\n", buffer);
}

void SendValue(std::uint16_t temperature) {
  // The stream is not initialized yet
  if (!message_stream) {
    return;
  }

  // the value in range -100 to 100 is mapped to 0 to 20000 (100 units per
  // degree)
  // remap it on -30 to 50 - 0 to 8000
  // encode temperature value in range 0 to 255
  // to decode use (v/3-30)
  auto encoded_value = static_cast<std::uint8_t>(
      (std::clamp(temperature, std::uint16_t{7000}, std::uint16_t{15000}) -
       7000) *
      3U / 100U);

  auto message = ae::DataBuffer{};
  message.reserve(2);
  {
    auto writer = ae::VectorWriter<>{message};
    auto stream = ae::omstream{writer};
    // write message code and encoded value
    stream << std::uint8_t{0x03} << encoded_value;
  }

  auto write_action = message_stream->Write(std::move(message));
  write_action->StatusEvent().Subscribe([](auto) {
    // with any result ready to sleep
    aether_app->aether()->uap->SleepReady();
  });
}

void GoToSleep(ae::Uap::Timer uap_timer) {
  std::cout << " >>> Going to sleep...\n";

  if (!aether_app) {
    return;
  }

#if BOARD_HAS_SLEEP_MANAGER == 1 && BOARD_HAS_ULP == 0
  // get the interval with the specified offset
  // offset is required to account the Save operation
  auto interval = uap_timer.interval(std::chrono::seconds{10});
  // save current aether state
  aether_app->aether().Save();
  // Go to sleep
  auto sleep_until = interval.until();
  std::cout << ae::Format(" >>> Sleep until {:%Y-%m-%d %H:%M:%S}...\n",
                          sleep_until);
  DeepSleep(interval.until());
#elif BOARD_HAS_ULP == 1
  std::cout << ae::Format("ULP Core sleep.\n");
  // save current aether state
  aether_app->aether().Save();
  // Go to sleep
  lp_goto_sleep();
#endif
}

#if BOARD_HAS_ULP == 1
static void lp_core_init(void) {
  esp_err_t ret = ESP_OK;

  ulp_lp_core_cfg_t cfg = {.wakeup_source = ULP_LP_CORE_WAKEUP_SOURCE_LP_TIMER,
                           .lp_timer_sleep_duration_us = 1000000};

  ret = ulp_lp_core_load_binary(ulp_main_bin_start,
                                (ulp_main_bin_end - ulp_main_bin_start));
  if (ret != ESP_OK) {
    std::cout << ae::Format("LP Core load failed!\n");
    abort();
  }

  ret = ulp_lp_core_run(&cfg);
  if (ret != ESP_OK) {
    std::cout << ae::Format("LP Core run failed!\n");
    abort();
  }

  std::cout << ae::Format("LP core loaded with firmware successfully!\n");
}

#ifndef LP_I2C_SDA_IO
    #define LP_I2C_SDA_IO  GPIO_NUM_6
#endif

#ifndef LP_I2C_SCL_IO
    #define LP_I2C_SCL_IO  GPIO_NUM_7
#endif

static void lp_i2c_init(void) {
  esp_err_t ret = ESP_OK;

  /* Initialize LP I2C with default configuration */
  lp_core_i2c_cfg_t i2c_cfg{};
  lp_core_i2c_timing_cfg_t i2c_timing_cfg{};

  i2c_timing_cfg.clk_speed_hz = 100000;

  i2c_cfg.i2c_pin_cfg.sda_io_num = LP_I2C_SDA_IO;
  i2c_cfg.i2c_pin_cfg.scl_io_num = LP_I2C_SCL_IO;
  i2c_cfg.i2c_pin_cfg.sda_pullup_en = true;
  i2c_cfg.i2c_pin_cfg.scl_pullup_en = true;
  i2c_cfg.i2c_timing_cfg = i2c_timing_cfg;
  i2c_cfg.i2c_src_clk = LP_I2C_SCLK_LP_FAST;

  ret = lp_core_i2c_master_init(LP_I2C_NUM_0, &i2c_cfg);
  if (ret != ESP_OK) {
    std::cout << ae::Format("LP I2C init failed!\n");
    abort();
  }

  std::cout << ae::Format("LP I2C initialized successfully!\n");
}

static void lp_goto_sleep(void) {
  /* Initialize LP_I2C from the main processor */
  lp_i2c_init();
  /* Load LP Core binary and start the coprocessor */
  lp_core_init();
  
  vTaskDelay(pdMS_TO_TICKS(1));
  
  //ulp_wakeup_temp_threshold = 2000;  // Threshold: 20.00°C
  //ulp_can_start = 1;
  
  esp_sleep_enable_ulp_wakeup();
  esp_deep_sleep_start();
  }
#endif