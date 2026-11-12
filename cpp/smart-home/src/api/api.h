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

#ifndef API_API_H_
#define API_API_H_

#include "aether/api_protocol/api_protocol.h"
#include "api/types.h"

namespace ae {
class SmartHomeCommutatorApi : public DeclareApi<SmartHomeCommutatorApi> {
 public:
  virtual ~SmartHomeCommutatorApi() = default;
  // The Java client uses the original request/reply protocol. The request ID
  // remains an explicit first wire argument now that the SDK uses push APIs.
  virtual void GetSystemStructure(RequestId request_id) = 0;
  virtual void ExecuteActorCommand(RequestId request_id, int local_actor_id,
                                   VariantData command) = 0;
  virtual void QueryState(RequestId request_id, int local_device_id) = 0;
  virtual void QueryAllSensorStates() = 0;
  API_LIST(METHOD(10, GetSystemStructure), METHOD(4, ExecuteActorCommand),
           METHOD(5, QueryState), METHOD(6, QueryAllSensorStates));
};

class SmartHomeClientApi : public DeclareApi<SmartHomeClientApi> {
 public:
  void device_state_updated(int local_device_id, DeviceStateData state) {
    ClientMethod<&SmartHomeClientApi::device_state_updated>(local_device_id,
                                                            std::move(state));
  }
  API_LIST(METHOD(3, device_state_updated));
};

}  // namespace ae

#endif  // API_API_H_
