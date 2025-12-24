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

#include "wifi_provisioning.h"

#if defined ESP_PLATFORM
#  include <array>
#  include <mutex>
#  include <cstdio>
#  include <cstdint>
#  include <cstring>
#  include <optional>
#  include <string_view>
#  include <condition_variable>

#  include "freertos/FreeRTOS.h"
#  include "freertos/task.h"
#  include "esp_wifi.h"
#  include "esp_event.h"
#  include "esp_log.h"
#  include "nvs_flash.h"
#  include "nvs.h"
#  include "esp_http_server.h"
#  include "esp_netif.h"
#  include "driver/gpio.h"

static const char* TAG = "AETHER_FINAL";

static constexpr std::string_view kDeviceId =
    "2de26d15-ccad-4fb1-a88a-baa2f45327ce";
static constexpr std::string_view kApSsid = "Aether_72ce";

#  define STATUS_LED_PIN GPIO_NUM_35
#  define RESET_BUTTON_PIN GPIO_NUM_0

namespace wp_internal {

enum class LedMode : char {
  kProvisioning,     // Slow blink (1s) - Searching for phone
  kClientConnected,  // Solid ON - Phone joined
  kConnecting,       // Fast blink (100ms) - Joining router
  kOnline,           // Heartbeat - Connected to internet
  kResetting         // Rapid flicker - Resetting NVS
};

static volatile LedMode current_led_mode = LedMode::kProvisioning;

// --- LED TASK ---
void LedTask(void* pvParameters) {
  gpio_reset_pin(STATUS_LED_PIN);
  gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT);

  static constexpr auto provisioning = [] {
    gpio_set_level(STATUS_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(STATUS_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
  };
  static constexpr auto client_connected = [] {
    gpio_set_level(STATUS_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
  };
  static constexpr auto connecting = [] {
    gpio_set_level(STATUS_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(STATUS_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
  };
  static constexpr auto online = [] {
    gpio_set_level(STATUS_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(15));
    gpio_set_level(STATUS_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(STATUS_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(15));
    gpio_set_level(STATUS_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(4985));
  };
  static constexpr auto resetting = [] {
    gpio_set_level(STATUS_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(STATUS_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
  };

  while (1) {
    switch (current_led_mode) {
      case LedMode::kProvisioning:
        provisioning();
        break;
      case LedMode::kClientConnected:
        client_connected();
        break;
      case LedMode::kConnecting:
        connecting();
        break;
      case LedMode::kOnline:
        online();
        break;
      case LedMode::kResetting:
        resetting();
        break;
    }
  }
}

// --- BUTTON MONITOR ---
void ResetMonitorTask(void* pv) {
  gpio_reset_pin(RESET_BUTTON_PIN);
  gpio_set_direction(RESET_BUTTON_PIN, GPIO_MODE_INPUT);
  int hold = 0;
  while (1) {
    if (gpio_get_level(RESET_BUTTON_PIN) == 0) {
      ESP_LOGI(TAG, "Reset pressed");
      hold += 100;
      current_led_mode = LedMode::kResetting;
      if (hold >= 3000) {
        nvs_flash_erase();
        esp_restart();
      }
    } else {
      if (hold > 0) {
        hold = 0;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// --- WIFI EVENT HANDLER ---
static void WifiEventHandler(void* arg, esp_event_base_t base, int32_t id,
                             void* data) {
  if (base != WIFI_EVENT) {
    return;
  }
  switch (id) {
    case WIFI_EVENT_AP_STACONNECTED: {
      current_led_mode = LedMode::kClientConnected;
      ESP_LOGI(TAG, "Device connected to AP");
      break;
    }
    case WIFI_EVENT_AP_STADISCONNECTED: {
      // Only return to slow blink if we are not currently resetting
      if (current_led_mode != LedMode::kResetting) {
        current_led_mode = LedMode::kProvisioning;
      }
      ESP_LOGI(TAG, "Device disconnected from AP");
      break;
    }
  }
}

struct WifiCreds {
  char ssid[32];
  char password[64];
};

bool SaveCreds(WifiCreds const& creds) {
  nvs_handle_t h;
  if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK) {
    return false;
  }
  nvs_set_str(h, "ssid", creds.ssid);
  nvs_set_str(h, "pass", creds.password);
  nvs_commit(h);
  nvs_close(h);

  return true;
}

std::optional<WifiCreds> GetSavedCredentials() {
  nvs_handle_t h;
  if (nvs_open("storage", NVS_READONLY, &h) != ESP_OK) {
    return std::nullopt;
  }

  struct defer_close {
    ~defer_close() { nvs_close(h_); }
    nvs_handle_t h_;
  } _dc{h};

  WifiCreds creds{};
  std::size_t ssid_len = sizeof(creds.ssid);
  std::size_t password_len = sizeof(creds.password);

  if (nvs_get_str(h, "ssid", creds.ssid, &ssid_len) != ESP_OK) {
    return std::nullopt;
  }
  if (nvs_get_str(h, "pass", creds.password, &password_len) != ESP_OK) {
    return std::nullopt;
  }
  return creds;
}

class ProvisioningServer {
 public:
  ProvisioningServer() {
    WifiApStart();
    ScanForNetworks();
    RunServer();
  }

  ~ProvisioningServer() {
    if (server_ != nullptr) {
      httpd_stop(server_);
    }
  }

  std::optional<WifiCreds> WaitForCompletion() {
    auto lock = std::unique_lock{mutex_};
    cv_.wait(lock);
    return saved_creds_;
  }

 private:
  // --- SERVER HANDLERS ---
  static esp_err_t IndexHandler(httpd_req_t* req) {
    static constexpr char kHtml[] =
        "<html><head><meta name='viewport' "
        "content='width=device-width,initial-scale=1'><style>body{background:#"
        "000;color:#fff;font-family:sans-serif;text-align:center;padding:20px;}"
        "select,input,button{width:100%;padding:15px;margin:10px "
        "0;border-radius:10px;font-size:16px;box-sizing:border-box;}button{"
        "background:#008cff;color:#fff;border:none;font-weight:bold;cursor:"
        "pointer;}</style></head><body><h1>Setup</h1><form action='/save' "
        "method='POST'><select name='s' "
        "id='s'><option>Scanning...</option></select><input type='text' "
        "name='p' "
        "placeholder='Password'><button "
        "type='submit'>CONNECT</button></form><script>fetch('/"
        "scan').then(r=>r.json()).then(d=>{let "
        "s=document.getElementById('s');s.innerHTML='';d.forEach(n=>{let "
        "o=document.createElement('option');o.value=n;o.text=n;s.appendChild(o)"
        ";}"
        ");});</script></body></html>";
    return httpd_resp_send(req, kHtml, -1);
  }

  static esp_err_t ScanHandler(httpd_req_t* req) {
    ESP_LOGI(TAG, "Open scan handler");
    auto* self = static_cast<ProvisioningServer*>(req->user_ctx);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, self->cached_wifi_json_.data(), -1);
  }

  static esp_err_t SaveHandler(httpd_req_t* req) {
    auto* self = static_cast<ProvisioningServer*>(req->user_ctx);

    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
      return ESP_FAIL;
    }
    buf[ret] = '\0';
    auto creds = WifiCreds{};
    auto s_res =
        httpd_query_key_value(buf, "s", creds.ssid, sizeof(creds.ssid));
    auto p_res =
        httpd_query_key_value(buf, "p", creds.password, sizeof(creds.password));

    if ((s_res != ESP_OK) || (p_res != ESP_OK)) {
      return ESP_FAIL;
    }

    self->saved_creds_.emplace(creds);

    current_led_mode = LedMode::kConnecting;
    constexpr char redir_url_format[] =
        "https://aethernet.io/temp_test_plain.html?id=%s";
    char redirect_url[sizeof(redir_url_format) + kDeviceId.size()];
    snprintf(redirect_url, sizeof(redirect_url), redir_url_format,
             kDeviceId.data());

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", redirect_url);
    httpd_resp_send(req, NULL, 0);

    // wait till all requests are finished
    vTaskDelay(pdMS_TO_TICKS(2000));

    // notify all work is done
    {
      auto lock = std::scoped_lock{self->mutex_};
      self->cv_.notify_all();
    }
    return ESP_OK;
  }

  void WifiApStart() {
    // start Wifi in AP mode
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t ap_cfg = {};
    strcpy((char*)ap_cfg.ap.ssid, kApSsid.data());
    ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    ap_cfg.ap.max_connection = 10;  // INCREASED TO PREVENT DEAUTH

    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();
  }

  void ScanForNetworks() {
    // Instant scan for available networks
    uint16_t ap_num = 15;
    wifi_ap_record_t records[15];
    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_records(&ap_num, records);
    cached_wifi_json_[0] = '[';
    int offset = 1;
    for (std::uint16_t i = 0; i < ap_num; i++) {
      auto remaining = cached_wifi_json_.size() - offset - 1;
      if (remaining <= 0) {
        break;
      }

      if (i < (ap_num - 1)) {
        offset += snprintf(cached_wifi_json_.data() + offset, remaining,
                           "\"%s\",", reinterpret_cast<char*>(records[i].ssid));
      } else {
        offset += snprintf(cached_wifi_json_.data() + offset, remaining,
                           "\"%s\"", reinterpret_cast<char*>(records[i].ssid));
      }
    }
    cached_wifi_json_[offset] = ']';
    cached_wifi_json_[offset + 1] = '\0';
    ESP_LOGI(TAG, "found networks %s", cached_wifi_json_.data());
  }

  void RunServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server_, &config) == ESP_OK) {
      httpd_uri_t i = {
          .uri = "/",
          .method = HTTP_GET,
          .handler = IndexHandler,
          .user_ctx = nullptr,
      };
      httpd_uri_t sc = {
          .uri = "/scan",
          .method = HTTP_GET,
          .handler = ScanHandler,
          .user_ctx = this,
      };
      httpd_uri_t sa = {
          .uri = "/save",
          .method = HTTP_POST,
          .handler = SaveHandler,
          .user_ctx = this,
      };
      httpd_register_uri_handler(server_, &i);
      httpd_register_uri_handler(server_, &sc);
      httpd_register_uri_handler(server_, &sa);
    }
  }

  httpd_handle_t server_ = NULL;
  std::array<char, 1024> cached_wifi_json_;
  std::optional<WifiCreds> saved_creds_;
  std::mutex mutex_;
  std::condition_variable cv_;
};

void StartWifi(WifiCreds creds) {
  esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  wifi_config_t sta_cfg = {};
  strncpy((char*)sta_cfg.sta.ssid, creds.ssid, 32);
  strncpy((char*)sta_cfg.sta.password, creds.password, 64);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
  esp_wifi_start();
  esp_wifi_connect();
  vTaskDelay(pdMS_TO_TICKS(5000));
}

}  // namespace wp_internal

bool WifiProvisioning() {
  wp_internal::current_led_mode = wp_internal::LedMode::kProvisioning;

  esp_err_t ret = nvs_flash_init();
  if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) ||
      (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  esp_netif_init();
  esp_event_loop_create_default();
  esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wp_internal::WifiEventHandler, NULL, NULL);

  xTaskCreate(wp_internal::LedTask, "led", 1024, NULL, 5, NULL);
  xTaskCreate(wp_internal::ResetMonitorTask, "reset", 2048, NULL, 10, NULL);

  auto saved_creds = wp_internal::GetSavedCredentials();
  if (!saved_creds) {
    auto provisioning = wp_internal::ProvisioningServer{};
    auto new_creds = provisioning.WaitForCompletion();
    if (!new_creds) {
      return false;
    }
    if (!wp_internal::SaveCreds(*new_creds)) {
      return false;
    }
    saved_creds = new_creds;
  }

  wp_internal::current_led_mode = wp_internal::LedMode::kConnecting;
  wp_internal::StartWifi(*saved_creds);

  wp_internal::current_led_mode = wp_internal::LedMode::kOnline;
  return true;
}
#endif
