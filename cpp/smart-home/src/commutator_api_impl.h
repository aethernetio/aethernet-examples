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

#ifndef COMMUTATOR_API_IMPL_H_
#define COMMUTATOR_API_IMPL_H_

#include "aether/all.h"

#include "api/api.h"

namespace ae {
class Commutator;
class CommutatorApiImpl : public SmartHomeCommutatorApi {
 public:
  CommutatorApiImpl(Commutator& commutator, RcPtr<P2pStream> stream);

  void GetSystemStructure(
      PromiseResult<std::vector<HardwareDevice>> result) override;

  void ExecuteActorCommand(PromiseResult<DeviceStateData> result,
                           int local_actor_id, VariantData command) override;

  void QueryState(PromiseResult<DeviceStateData> result,
                  int local_device_id) override;

  void QueryAllSensorStates() override;

 private:
  Commutator* commutator_;
  RcPtr<P2pStream> stream_;
};
}  // namespace ae

#endif  // COMMUTATOR_API_IMPL_H_
