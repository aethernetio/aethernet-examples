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

#if defined ESP_PLATFORM
#  include <freertos/FreeRTOS.h>
#  include <esp_log.h>
#  include <esp_task_wdt.h>
#endif

#include "aether/all.h"

extern int client_main(ae::AetherAppContext&& aether_app_context);

#if defined ESP_PLATFORM
extern "C" void app_main();
void app_main(void) {
  esp_task_wdt_config_t config_wdt = {
      .timeout_ms = 60000,
      .idle_core_mask = 0,  // i.e. do not watch any idle task
      .trigger_panic = true};

  esp_err_t err = esp_task_wdt_reconfigure(&config_wdt);
  if (err != 0) {
    ESP_LOGE("TempSensorClient", "Reconfigure WDT is failed!");
  }

  auto res =
      client_main(ae::AetherAppContext{}
#  ifdef AE_DISTILLATION
#    if not defined WIFI_SSID or not defined WIFI_PASS
#      error "WIFI_SSID and WIFI_PASS should be defined"
#    endif
                      .AdapterFactory([](ae::AetherAppContext const& context) {
                        return context.domain().CreateObj<ae::Esp32WifiAdapter>(
                            ae::GlobalId::kEsp32WiFiAdapter, context.aether(),
                            context.poller(), context.dns_resolver(),
                            std::string(WIFI_SSID), std::string(WIFI_PASS));
                      })
#  endif
      );

  if (res != 0) {
    std::cerr << "Exit with code " << res << std::endl;
    return;
  }
  std::cout << "Exit normally" << std::endl;
}
#else
int main() { return client_main(ae::AetherAppContext{}); }
#endif
