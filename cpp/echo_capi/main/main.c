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

#include "stdio.h"
#include "stdlib.h"

#include "aether/capi.h"

#define TEST_UID "3ac93165-3d37-4970-87a6-fa4ee27744e4"

#define ECHO_SERVICE_UID "61BEF6C8-9680-47D2-8029-1A1D89E3F54C"

void MessageReceived(AetherClient* client, CUid sender, void const* data,
                     size_t size, void* user_data) {
  printf(" >>> Received %.*s\n", (int)size, (char const*)data);
  AetherExit(0);
}

char const* message = "Hello, Echo!";

int EchoExample() {
#ifdef ESP_PLATFORM
  AeWifiAdapterConf wifi_adapter_conf = {
      .type = AeWifiAdapter,
      .ssid = WIFI_SSID,
      .password = WIFI_PASSWORD,
  };
  AdapterBase* adapter_confs = (AdapterBase*)&wifi_adapter_conf;
  Adapters adapters = {
      .count = 1,
      .adapters = &adapter_confs,
  };
#endif

  ClientConfig client_config = {
      .id = "echo_client",
      .parent_uid = CUidFromString(TEST_UID),
      .message_received_cb = MessageReceived,
  };

  AetherConfig aether_config = {
#ifdef ESP_PLATFORM
      .adapters = &adapters,
#endif
      .default_client = &client_config,
  };

  AetherInit(&aether_config);
  printf(" >>> Send message %s\n", message);
  SendStr(CUidFromString(ECHO_SERVICE_UID), message, NULL, NULL);

  while (AetherExcited() == AE_NOK) {
    uint64_t time = AetherUpdate();
    AetherWait(time);
  }
  return AetherEnd();
}

#ifdef ESP_PLATFORM
#  include <freertos/FreeRTOS.h>
#  include <esp_log.h>
#  include <esp_task_wdt.h>

extern "C" void app_main() {
  esp_task_wdt_config_t config_wdt = {
      /*.timeout_ms = */ 60000,
      /*.idle_core_mask = */ 0,  // i.e. do not watch any idle task
      /*.trigger_panic = */ true};

  esp_err_t err = esp_task_wdt_reconfigure(&config_wdt);
  if (err != 0) {
    ESP_LOGE("ECHO", "Reconfigure WDT is failed!");
  }

  EchoExample();
}
#else
int main() { return EchoExample(); }
#endif
