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
#  if ESP_WIFI

namespace ae {
static constexpr auto wifi_init = WifiInit{
    {ae::WifiAp{
        ae::WifiCreds{WIFI_SSID, WIFI_PASSWORD},
        {},
    }},
    ae::WiFiPowerSaveParam{},
};

RcPtr<AetherApp> construct_aether_app() {
  return AetherApp::Construct(
      AetherAppContext{}
#    if AE_DISTILLATION
          .AdaptersFactory([](AetherAppContext const& context) {
            auto adapter_registry =
                AdapterRegistry::ptr::Create(context.domain());
            adapter_registry->Add(WifiAdapter::ptr::Create(
                CreateWith{context.domain()}.with_id(GlobalId::kWiFiAdapter),
                context.aether(), context.poller(), context.dns_resolver(),
                wifi_init));
            return adapter_registry;
          })
#    endif
  );
}

}  // namespace ae

#  endif
#endif
#endif  // CLOUD_AETHER_CONSTRUCT_ESP_WIFI_H_
