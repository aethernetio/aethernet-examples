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
#include "idevice.h"

namespace ae {
class Commutator;
class CommutatorApiImpl : public SmartHomeCommutatorApi {
 public:
  using Reply = std::function<void(DataBuffer)>;
  CommutatorApiImpl(std::vector<std::unique_ptr<IDevice>>& devices,
                    std::vector<std::shared_ptr<DeviceStateAction>>& pending,
                    Reply reply, std::function<void()> query_all);

  void GetSystemStructure(RequestId request_id) override;

  void ExecuteActorCommand(RequestId request_id, int local_actor_id,
                           VariantData command) override;

  void QueryState(RequestId request_id, int local_device_id) override;

  void QueryAllSensorStates() override;

 private:
  void ReplyWhenReady(std::shared_ptr<DeviceStateAction> operation,
                      RequestId request_id);
  std::vector<std::unique_ptr<IDevice>>& devices_;
  std::vector<std::shared_ptr<DeviceStateAction>>& pending_;
  Reply reply_;
  std::function<void()> query_all_;
};
}  // namespace ae

#endif  // COMMUTATOR_API_IMPL_H_
