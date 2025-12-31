/*
 * Copyright 2025 Aethernet Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wifi_provisioning.h"

#if defined ESP_PLATFORM

#include <array>
#include <mutex>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <condition_variable>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "driver/gpio.h"

// --- CONFIGURATION ---

#ifdef ESP_M5STACK_ATOM_LITE
    #include "led_strip.h"
#endif

static const char* TAG = "AE_WP";

namespace wp_internal {

struct WifiCreds {
  char ssid[32];
  char password[64];
};

// Hardcoded device ID for wizard linking
static char device_uuid[37] = "58a0981f-5bad-4fa4-bb2a-cd2aa26e1b3a";

#ifdef ESP_M5STACK_ATOM_LITE
static led_strip_handle_t led_strip;
#endif

enum class LedMode : char {
    kProvisioning,     // Blue blinking: AP mode active
    kClientConnected,  // Yellow solid: Smartphone connected to AP
    kConnecting,       // White blinking: Connecting to router
    kOnline,           // Green double pulse: Connected to cloud
    kHoldSlow,         // Red slow: Hold 0-3s warning
    kHoldFast,         // Red fast: Hold 3-10s critical
    kResetting         // Red solid: Resetting device
};

static volatile LedMode current_led_mode = LedMode::kProvisioning;

// --- NVS HELPERS ---
bool SaveCreds(WifiCreds const& creds) {
  nvs_handle_t h;
  if (nvs_open("storage", NVS_READWRITE, &h) != ESP_OK) return false;
  nvs_set_str(h, "ssid", creds.ssid);
  nvs_set_str(h, "pass", creds.password);
  nvs_commit(h);
  nvs_close(h);
  return true;
}

std::optional<WifiCreds> GetSavedCredentials() {
  nvs_handle_t h;
  if (nvs_open("storage", NVS_READONLY, &h) != ESP_OK) return std::nullopt;

  WifiCreds creds{};
  std::size_t ssid_len = sizeof(creds.ssid);
  std::size_t password_len = sizeof(creds.password);
  
  esp_err_t err_s = nvs_get_str(h, "ssid", creds.ssid, &ssid_len);
  esp_err_t err_p = nvs_get_str(h, "pass", creds.password, &password_len);
  nvs_close(h);

  if (err_s != ESP_OK || err_p != ESP_OK) return std::nullopt;
  return creds;
}

// --- LED LOGIC ---
static void SetLed(bool on, uint32_t r, uint32_t g, uint32_t b) {
#ifdef ESP_M5STACK_ATOM_LITE
    if (!led_strip) return;
    if (on) {
        led_strip_set_pixel(led_strip, 0, r, g, b);
        led_strip_refresh(led_strip);
    } else {
        led_strip_clear(led_strip);
    }
#else
    gpio_set_level(STATUS_LED_PIN, on ? 1 : 0);
#endif
}

void LedTask(void* pvParameters) {
#ifdef ESP_M5STACK_ATOM_LITE
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = STATUS_LED_PIN;
    strip_config.max_leds = 1;
    
    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000;
    rmt_config.flags.with_dma = false;

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
#else
    gpio_reset_pin(STATUS_LED_PIN);
    gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT);
#endif

    while (1) {
        switch (current_led_mode) {
            case LedMode::kProvisioning: 
                SetLed(true, 0, 0, 255); vTaskDelay(pdMS_TO_TICKS(1000));
                SetLed(false, 0, 0, 0);  vTaskDelay(pdMS_TO_TICKS(1000));
                break;
            case LedMode::kClientConnected: 
                SetLed(true, 255, 165, 0); vTaskDelay(pdMS_TO_TICKS(100));
                break;
            case LedMode::kConnecting: 
                SetLed(true, 200, 200, 200); vTaskDelay(pdMS_TO_TICKS(100));
                SetLed(false, 0, 0, 0);      vTaskDelay(pdMS_TO_TICKS(100));
                break;
            case LedMode::kOnline: 
                SetLed(true, 0, 255, 0); vTaskDelay(pdMS_TO_TICKS(80));
                SetLed(false, 0, 0, 0);  vTaskDelay(pdMS_TO_TICKS(150));
                SetLed(true, 0, 255, 0); vTaskDelay(pdMS_TO_TICKS(80));
                SetLed(false, 0, 0, 0);  vTaskDelay(pdMS_TO_TICKS(4000));
                break;
            case LedMode::kHoldSlow: 
                SetLed(true, 255, 0, 0); vTaskDelay(pdMS_TO_TICKS(500));
                SetLed(false, 0, 0, 0);  vTaskDelay(pdMS_TO_TICKS(500));
                break;
            case LedMode::kHoldFast: 
                SetLed(true, 255, 0, 0); vTaskDelay(pdMS_TO_TICKS(100));
                SetLed(false, 0, 0, 0);  vTaskDelay(pdMS_TO_TICKS(100));
                break;
            case LedMode::kResetting: 
                SetLed(true, 255, 0, 0); vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
    }
}

// --- BUTTON MONITOR ---
void ResetMonitorTask(void* pv) {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << RESET_BUTTON_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    
    gpio_config(&io_conf);

    int hold_ms = 0;
    bool is_pressed = false;
    LedMode previous_mode = current_led_mode;

    while (1) {
        if (gpio_get_level(RESET_BUTTON_PIN) == 0) {
            if (!is_pressed) {
                is_pressed = true;
                previous_mode = current_led_mode;
                hold_ms = 0;
                current_led_mode = LedMode::kHoldSlow; 
            }
            hold_ms += 100;
            
            if (hold_ms >= 3000 && hold_ms < 10000) {
                 if(current_led_mode != LedMode::kHoldFast) current_led_mode = LedMode::kHoldFast;
            }
            if (hold_ms >= 10000) {
                current_led_mode = LedMode::kResetting;
                vTaskDelay(pdMS_TO_TICKS(500));
                nvs_flash_erase();
                esp_restart();
            }
        } else {
            if (is_pressed) {
                is_pressed = false;
                if (hold_ms < 10000) {
                    current_led_mode = previous_mode;
                }
                hold_ms = 0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// --- WIFI HELPERS ---
void WifiApStart() {
    char ssid_name[32];
    size_t len = strlen(device_uuid);
    const char* suffix = (len > 4) ? (device_uuid + len - 4) : "wifi";
    snprintf(ssid_name, sizeof(ssid_name), "aether_%s", suffix);

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    
    wifi_config_t ap_cfg = {};
    strcpy((char*)ap_cfg.ap.ssid, ssid_name);
    ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    ap_cfg.ap.max_connection = 4;
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();
}

// --- PROVISIONING SERVER ---
class ProvisioningServer {
public:
    ProvisioningServer() {
        WifiApStart();
        ScanForNetworks();
        RunServer();
    }
    ~ProvisioningServer() { if (server_) httpd_stop(server_); }

    std::optional<WifiCreds> WaitForCompletion() {
        auto lock = std::unique_lock{mutex_};
        cv_.wait(lock);
        return saved_creds_;
    }

private:
    static esp_err_t IndexHandler(httpd_req_t* req) {
        char html[1500];
        snprintf(html, sizeof(html),
            "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<style>body{background:#000;color:#fff;font-family:sans-serif;text-align:center;padding:20px;}"
            "input,select,button{width:100%%;padding:15px;margin:10px 0;border-radius:12px;font-size:16px;box-sizing:border-box;border:1px solid #333;background:#111;color:#fff;}"
            "button{background:#008cff;border:none;font-weight:bold;}</style></head>"
            "<body><h1>Step 3: WiFi</h1><p>Device ID: %s</p>"
            "<form action='/save' method='POST'><select name='s' id='s'><option>Scanning...</option></select>"
            "<input type='text' name='p' placeholder='WiFi Password' required><button type='submit'>CONNECT DEVICE</button></form>"
            "<script>fetch('/scan').then(r=>r.json()).then(d=>{let s=document.getElementById('s');s.innerHTML='';d.forEach(n=>{let o=document.createElement('option');o.value=n;o.text=n;s.appendChild(o);});});</script></body></html>",
            device_uuid);
        return httpd_resp_send(req, html, -1);
    }

    static esp_err_t ScanHandler(httpd_req_t* req) {
        auto* self = static_cast<ProvisioningServer*>(req->user_ctx);
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, self->cached_wifi_json_.data(), -1);
    }

    static esp_err_t SaveHandler(httpd_req_t* req) {
        auto* self = static_cast<ProvisioningServer*>(req->user_ctx);
        char buf[256];
        int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
        if (ret <= 0) return ESP_FAIL;
        buf[ret] = '\0';

        WifiCreds creds{};
        auto s_res = httpd_query_key_value(buf, "s", creds.ssid, sizeof(creds.ssid));
        auto p_res = httpd_query_key_value(buf, "p", creds.password, sizeof(creds.password));

        if (s_res != ESP_OK || p_res != ESP_OK) return ESP_FAIL;
        
        self->saved_creds_.emplace(creds);

        // Switch to connecting mode if not in reset state
        if (current_led_mode != LedMode::kHoldSlow && current_led_mode != LedMode::kHoldFast && current_led_mode != LedMode::kResetting) {
            current_led_mode = LedMode::kConnecting;
        }

        // Redirect back to cloud wizard
        char redir[160];
        snprintf(redir, sizeof(redir), "https://aethernet.io/wizard.html?id=%s&status=checking", device_uuid);
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", redir);
        httpd_resp_send(req, nullptr, 0);

        vTaskDelay(pdMS_TO_TICKS(1500));
        { std::lock_guard<std::mutex> lock(self->mutex_); self->cv_.notify_all(); }
        return ESP_OK;
    }

    void ScanForNetworks() {
        uint16_t ap_num = 15;
        wifi_ap_record_t records[15];
        esp_wifi_scan_start(nullptr, true);
        esp_wifi_scan_get_ap_records(&ap_num, records);
        int offset = snprintf(cached_wifi_json_.data(), 1024, "[");
        for (int i=0; i<ap_num; i++) {
            offset += snprintf(cached_wifi_json_.data()+offset, 1024-offset, "\"%s\"%s", (char*)records[i].ssid, (i==ap_num-1)?"":",");
        }
        snprintf(cached_wifi_json_.data()+offset, 1024-offset, "]");
    }

    void RunServer() {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.stack_size = 8192; 
        // Note: Header limits must be set in sdkconfig.defaults

        if (httpd_start(&server_, &config) == ESP_OK) {
            httpd_uri_t i={ "/", HTTP_GET, IndexHandler, nullptr }, sc={ "/scan", HTTP_GET, ScanHandler, this }, sa={ "/save", HTTP_POST, SaveHandler, this };
            httpd_register_uri_handler(server_, &i); 
            httpd_register_uri_handler(server_, &sc); 
            httpd_register_uri_handler(server_, &sa);
        }
    }

    httpd_handle_t server_ = nullptr;
    std::array<char, 1024> cached_wifi_json_;
    std::optional<WifiCreds> saved_creds_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

static void WifiEventHandler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    bool btn_active = (current_led_mode == LedMode::kHoldSlow || current_led_mode == LedMode::kHoldFast || current_led_mode == LedMode::kResetting);
    if (base == WIFI_EVENT && !btn_active) {
        if (id == WIFI_EVENT_AP_STACONNECTED) current_led_mode = LedMode::kClientConnected;
        if (id == WIFI_EVENT_AP_STADISCONNECTED) current_led_mode = LedMode::kProvisioning;
    }
}

} // namespace wp_internal

// --- ENTRY POINT ---
bool WifiProvisioning() {
    using namespace wp_internal;
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    esp_netif_init();
    esp_event_loop_create_default();
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiEventHandler, nullptr, nullptr);

    xTaskCreate(LedTask, "led", 4096, nullptr, 5, nullptr);
    xTaskCreate(ResetMonitorTask, "reset", 4096, nullptr, 10, nullptr);

    auto saved_creds = GetSavedCredentials();
    
    if (!saved_creds) {
        ProvisioningServer server;
        auto new_creds = server.WaitForCompletion();
        if (!new_creds) return false;
        if (!SaveCreds(*new_creds)) return false;
        saved_creds = new_creds;
    }

    if (current_led_mode != LedMode::kResetting) {
        current_led_mode = LedMode::kConnecting;
        
        wifi_config_t sta_cfg = {};
        strncpy((char*)sta_cfg.sta.ssid, saved_creds->ssid, 32);
        strncpy((char*)sta_cfg.sta.password, saved_creds->password, 64);
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
        esp_wifi_start();
        esp_wifi_connect();

        // Simulate cloud connection delay
        vTaskDelay(pdMS_TO_TICKS(5000));
        current_led_mode = LedMode::kOnline;
    }

    return true;
}

#endif