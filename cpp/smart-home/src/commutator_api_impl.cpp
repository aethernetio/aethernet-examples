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

#include "idevice.h"
#include "commutator.h"

namespace ae {
CommutatorApiImpl::CommutatorApiImpl(Commutator& commutator,
                                     RcPtr<P2pStream> stream)
    : SmartHomeCommutatorApi{commutator.protocol_context_},
      commutator_{&commutator},
      stream_{std::move(stream)} {}

void CommutatorApiImpl::GetSystemStructure(
    PromiseResult<std::vector<HardwareDevice>> result) {
  std::vector<HardwareDevice> hw_devices;
  hw_devices.reserve(commutator_->devices_.size());
  for (auto& d : commutator_->devices_) {
    hw_devices.emplace_back(d->description());
  }

  ReturnResultApi ret{protocol_context()};
  auto api_call = ApiCallAdapter{ApiContext{ret}, *stream_};

  api_call->SendResult(result.request_id, std::move(hw_devices));
  api_call.Flush();
}

void CommutatorApiImpl::ExecuteActorCommand(
    PromiseResult<DeviceStateData> result, int local_actor_id,
    VariantData command) {
  auto dev_id = static_cast<std::size_t>(local_actor_id);

  if (dev_id >= commutator_->devices_.size()) {
    // index out of range
    ReturnResultApi ret{protocol_context()};
    auto api_call = ApiCallAdapter{ApiContext{ret}, *stream_};
    api_call->SendError(result.request_id, 1, 1);
  }
  auto& device = commutator_->devices_[dev_id];
  auto state_action = device->Execute(command);
  state_action->StatusEvent().Subscribe(OnResult{
      [pc{&protocol_context()}, stream{stream_}, result](auto const& action) {
        ReturnResultApi ret{*pc};
        auto api_call = ApiCallAdapter{ApiContext{ret}, *stream};
        api_call->SendResult(result.request_id, action.state_data());
        api_call.Flush();
      }});
}

void CommutatorApiImpl::QueryState(PromiseResult<DeviceStateData> result,
                                   int local_device_id) {
  auto dev_id = static_cast<std::size_t>(local_device_id);

  if (dev_id >= commutator_->devices_.size()) {
    // index out of range
    ReturnResultApi ret{protocol_context()};
    auto api_call = ApiCallAdapter{ApiContext{ret}, *stream_};
    api_call->SendError(result.request_id, 2, 1);
  }
  auto& device = commutator_->devices_[dev_id];
  auto state_action = device->GetState();
  state_action->StatusEvent().Subscribe(OnResult{
      [pc{&protocol_context()}, stream{stream_}, result](auto const& action) {
        ReturnResultApi ret{*pc};
        auto api_call = ApiCallAdapter{ApiContext{ret}, *stream};
        api_call->SendResult(result.request_id, action.state_data());
        api_call.Flush();
      }});
}

void CommutatorApiImpl::QueryAllSensorStates() {
  commutator_->SendSensorsState(stream_);
}

}  // namespace ae
