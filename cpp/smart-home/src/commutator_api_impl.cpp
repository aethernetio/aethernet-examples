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

#include "commutator_api_impl.h"

#include <algorithm>

#include "api/legacy_reply.h"

namespace ae {
CommutatorApiImpl::CommutatorApiImpl(
    std::vector<std::unique_ptr<IDevice>>& devices,
    std::vector<std::shared_ptr<DeviceStateAction>>& pending, Reply reply,
    std::function<void()> query_all)
    : devices_{devices},
      pending_{pending},
      reply_{std::move(reply)},
      query_all_{std::move(query_all)} {}

void CommutatorApiImpl::GetSystemStructure(RequestId request_id) {
  std::vector<HardwareDevice> descriptions;
  descriptions.reserve(devices_.size());
  for (auto const& device : devices_)
    descriptions.push_back(device->description());
  reply_(smart_home_wire::Result(request_id, std::move(descriptions)));
}

void CommutatorApiImpl::ReplyWhenReady(
    std::shared_ptr<DeviceStateAction> operation, RequestId request_id) {
  std::erase_if(pending_, [](auto const& op) { return op->finished(); });
  pending_.push_back(operation);
  // The API parser handler is temporary. The completion owns its reply target,
  // and never captures the handler or its protocol context.
  operation->Then([reply = reply_, request_id](auto const& state) {
    reply(smart_home_wire::Result(request_id, state));
  });
}

void CommutatorApiImpl::ExecuteActorCommand(RequestId request_id,
                                            int local_actor_id,
                                            VariantData command) {
  if (local_actor_id < 0 ||
      static_cast<std::size_t>(local_actor_id) >= devices_.size()) {
    reply_(smart_home_wire::Error(request_id, 1, 1));
    return;
  }
  ReplyWhenReady(devices_[local_actor_id]->Execute(command), request_id);
}

void CommutatorApiImpl::QueryState(RequestId request_id, int local_device_id) {
  if (local_device_id < 0 ||
      static_cast<std::size_t>(local_device_id) >= devices_.size()) {
    reply_(smart_home_wire::Error(request_id, 2, 1));
    return;
  }
  ReplyWhenReady(devices_[local_device_id]->GetState(), request_id);
}

void CommutatorApiImpl::QueryAllSensorStates() { query_all_(); }
}  // namespace ae
