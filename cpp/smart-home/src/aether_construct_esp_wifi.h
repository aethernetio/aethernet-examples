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

#ifndef CLOUD_AETHER_CONSTRUCT_ESP_WIFI_H_
#define CLOUD_AETHER_CONSTRUCT_ESP_WIFI_H_

#include "aether_construct.h"

#if defined ESP_PLATFORM
#if ESP_WIFI

namespace ae {
std::unique_ptr<AetherApp> construct_aether_app() {
  return AetherApp::Construct(AetherAppContext{}.AddAdapterFactory(
      [](AetherAppContext const& context) -> ObjPtr<Adapter> {
        return WifiAdapter::ptr::Create(
            CreateWith{context.domain()}.with_id(GlobalId::kWiFiAdapter),
            context.aether(), context.poller(), context.dns_resolver(),
            WiFiInit{.wifi_ap = {WiFiAp{.creds = {.ssid = WIFI_SSID,
                                                  .password = WIFI_PASSWORD},
                                        .static_ip = {}}},
                     .psp = {}});
      }));
}

}  // namespace ae

#endif
#endif
#endif  // CLOUD_AETHER_CONSTRUCT_ESP_WIFI_H_
