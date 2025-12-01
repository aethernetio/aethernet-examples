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

#include "aether/all.h"

#include "api/types.h"

namespace ae {
class SmartHomeCommutatorApi : public ApiClassImpl<SmartHomeCommutatorApi> {
 public:
  using ApiClassImpl::ApiClassImpl;

  virtual ~SmartHomeCommutatorApi() = default;

  virtual void GetSystemStructure(
      PromiseResult<std::vector<HardwareDevice>> result) = 0;

  virtual void ExecuteActorCommand(PromiseResult<DeviceStateData> result,
                                   int local_actor_id, VariantData command) = 0;
  virtual void QueryState(PromiseResult<DeviceStateData> result,
                          int local_device_id) = 0;
  virtual void QueryAllSensorStates() = 0;

  AE_METHODS(RegMethod<10, &SmartHomeCommutatorApi::GetSystemStructure>,
             RegMethod<4, &SmartHomeCommutatorApi::ExecuteActorCommand>,
             RegMethod<5, &SmartHomeCommutatorApi::QueryState>,
             RegMethod<6, &SmartHomeCommutatorApi::QueryAllSensorStates>);
};

class SmartHomeClientApi : public ApiClass {
 public:
  SmartHomeClientApi(ProtocolContext& protocol_context);

  Method<3, void(int local_device_id, DeviceStateData state)>
      device_state_updated;
};

}  // namespace ae

#endif  // API_API_H_
