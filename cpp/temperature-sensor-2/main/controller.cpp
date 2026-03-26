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
#include "sleep_manager.h"
#include "user_config.h"

#ifdef ESP_PLATFORM
#  include <freertos/freertos.h>
#  include <freertos/task.h>
#  ifndef WIFI_SSID
#    define WIFI_SSID "test_wifi"
#  endif
#  ifndef WIFI_PASSWORD
#    define WIFI_PASSWORD ""
#  endif
#endif

#if BOARD_HAS_ULP == 1
#  include <ulp_lp_core.h>
#  include <lp_core_i2c.h>
#  include <esp_sleep.h>
#  include "ulp_main.h"
#endif

#if BOARD_HAS_SLEEP_MANAGER == 1
static ae::SleepManager::WakeupSource cause{ae::SleepManager::WakeupSource::WAKEUP_UNDEFINED};
#elif BOARD_HAS_ULP == 1
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

static const auto kWifiCreds = ae::WifiCreds{
    /* .ssid*/ std::string{WIFI_SSID},
    /* .password*/ std::string{WIFI_PASSWORD},
};
static const auto kWifiInit = ae::WiFiInit{
    std::vector<ae::WiFiAp>{{kWifiCreds, {}}},
    ae::WiFiPowerSaveParam{},
};

// Update temperature sensor
void UpdateTemperature();
// Message from aether service received
void MessageReceived(ae::DataBuffer const& buffer);
// Send the message value to the aether service
void SendValue(float value);
// Go to sleep method
void GoToSleep(ae::Uap::Timer uap_timer);

static ae::RcPtr<ae::AetherApp> aether_app;
static ae::RcPtr<ae::P2pStream> message_stream;

#if BOARD_HAS_SLEEP_MANAGER == 1
static ae::SleepManager sleep_mngr;
#endif

#if BOARD_HAS_SLEEP_MANAGER == 1
#elif BOARD_HAS_ULP == 1
extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[] asm("_binary_ulp_main_bin_end");

static void lp_core_init(void);
static void lp_i2c_init(void);
static void lp_goto_sleep(void);
#endif

void setup() {
#if BOARD_HAS_SLEEP_MANAGER == 1
  cause = sleep_mngr.GetWakeupCause();
  std::cout << ae::Format(R"(Cause {})", cause) << std::endl;
#elif BOARD_HAS_ULP == 1
  cause = esp_sleep_get_wakeup_cause();
  std::cout << ae::Format(R"(Cause {})", cause) << std::endl;
#endif

  aether_app = ae::AetherApp::Construct(
      ae::AetherAppContext{}
#if AE_DISTILLATION
#  if defined ESP_PLATFORM
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
              std::cout << " >>>> Return loaded UAP\n";
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
  aether_app->aether()->uap->sleep_event().Subscribe(&GoToSleep);

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
          UpdateTemperature();
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

#if BOARD_HAS_BME688 == 1
#  include "driver/i2c.h"
#  include "BME68x_SensorAPI/bme68x.h"
// --- SAFER Interface Functions ---
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
    std::cout << ae::Format("Write length too big: %lu", len);
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

float GetTemperatureBME(void) {
  float value{0};

  static struct bme68x_dev bme;
  static struct bme68x_conf conf;
  static uint8_t dev_addr = BME68X_I2C_ADDR_HIGH;

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

    std::cout << ">> BME " << "\n";
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

  if (!initialized) {
    value = -1000.0f;
  } else {
    // Trigger measurement
    if (bme68x_set_op_mode(BME68X_FORCED_MODE, &bme) != BME68X_OK) {
      value = -1000.0f;
    } else {
      // Wait for measurement
      uint32_t del_period =
          bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf, &bme);
      bme.delay_us(del_period, bme.intf_ptr);

      // Read Data
      struct bme68x_data data;
      uint8_t n_fields;
      if (bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &bme) ==
              BME68X_OK &&
          n_fields > 0) {
        value = data.temperature;
      } else {
        return -1000.0f;
      }
    }
  }

  std::cout << ae::Format("\n >>> BME Temperature measured: {}°C\n\n", value);

  return value;
}
#elif BOARD_HAS_ULP == 1
float GetTemperatureULP(void) {
  float value{0};

  value = static_cast<float>(ulp_last_bme68x_temperature) / 100;
  std::cout << ae::Format("\n >>> ULP Temperature measured: {}°C\n\n", value);

  return value;
}
#else
float GetTemperatureRND(void) {
  // get random value as temperature
  static bool seed = (std::srand(std::time(nullptr)), true);
  (void)seed;

  static float last_value = 20.F;
  // get diff in range -2 to 2
  auto diff = (static_cast<float>(std::rand() % 40) / 10.F) - 2.F;
  auto value = last_value += diff;
  std::cout << ae::Format("\n >>> RND Temperature measured: {}°C\n\n", value);

  return value;
}
#endif

// TODO: add implementation for actual temperature sensor
void UpdateTemperature() {
#if BOARD_HAS_BME688 == 1
  auto value = GetTemperatureBME();
#elif BOARD_HAS_ULP == 1
  auto value = GetTemperatureULP();
#else
  auto value = GetTemperatureRND();
#endif

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
    aether_app->aether()->uap->SleepReady();
  });
}

void GoToSleep(ae::Uap::Timer uap_timer) {
  std::cout << " >>> Going to sleep...\n";

  if (!aether_app) {
    return;
  }
#if BOARD_HAS_SLEEP_MANAGER == 1
  // get the interval with the specified offset
  // offset is required to account the Save operation
  auto interval = uap_timer.interval(std::chrono::seconds{10});
#endif
  // save current aether state
  aether_app->aether().Save();
  // Go to sleep
#if BOARD_HAS_SLEEP_MANAGER == 1
  // calculate time for sleep duration
  auto duration = interval.remaining();
  std::cout << ae::Format(" >>> Sleep for {:%S}...\n", duration);

  sleep_mngr.EnableTimerWakeup(
      std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
  sleep_mngr.EnterSleep(ae::SleepManager::SleepMode::DEEP_SLEEP, true);
#elif BOARD_HAS_ULP == 1
  lp_goto_sleep();
#endif
}

#if BOARD_HAS_SLEEP_MANAGER == 1
#elif BOARD_HAS_ULP == 1
static void lp_core_init(void) {
  esp_err_t ret = ESP_OK;

  ulp_lp_core_cfg_t cfg = {.wakeup_source = ULP_LP_CORE_WAKEUP_SOURCE_LP_TIMER,
                           .lp_timer_sleep_duration_us = 1000000};

  ret = ulp_lp_core_load_binary(ulp_main_bin_start,
                                (ulp_main_bin_end - ulp_main_bin_start));
  if (ret != ESP_OK) {
    std::cout << ae::Format("LP Core load failed!");
    abort();
  }

  ret = ulp_lp_core_run(&cfg);
  if (ret != ESP_OK) {
    std::cout << ae::Format("LP Core run failed!");
    abort();
  }

  std::cout << ae::Format("LP core loaded with firmware successfully!");
}

#  ifndef LP_I2C_SDA_IO
#    define LP_I2C_SDA_IO GPIO_NUM_6
#  endif

#  ifndef LP_I2C_SCL_IO
#    define LP_I2C_SCL_IO GPIO_NUM_7
#  endif

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
    std::cout << ae::Format("LP I2C init failed!");
    abort();
  }

  std::cout << ae::Format("LP I2C initialized successfully!");
}

static void lp_goto_sleep(void) {
  /* Initialize LP_I2C from the main processor */
  lp_i2c_init();
  /* Load LP Core binary and start the coprocessor */
  lp_core_init();

  vTaskDelay(pdMS_TO_TICKS(1));

  ulp_wakeup_temp_threshold = 2000;  // Threshold: 20.00°C
  ulp_can_start = 1;

  esp_sleep_enable_ulp_wakeup();
  esp_deep_sleep_start();
}
#endif