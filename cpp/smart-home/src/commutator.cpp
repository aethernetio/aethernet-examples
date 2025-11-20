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

#include "commutator.h"
#include "commutator_api_impl.h"

namespace ae {
Commutator::Commutator(Client::ptr const& client)
    : client_{client}, client_api_{protocol_context_} {
  new_request_sub_ =
      client->message_stream_manager().new_stream_event().Subscribe(
          MethodPtr<&Commutator::OnNewStream>{this});
}

void Commutator::AddDevice(std::unique_ptr<IDevice>&& device) {
  assert(device && "Device cannot be null");
  devices_.push_back(std::move(device));
}

void Commutator::SendSensorsState(RcPtr<P2pStream> const& stream) {
  for (std::size_t i = 0; i < devices_.size(); ++i) {
    auto state_action = devices_[i]->GetState();
    state_action->StatusEvent().Subscribe(OnResult{[stream, this,
                                                    i](auto const& action) {
      auto api_call = ApiCallAdapter{ApiContext{client_api_}, *stream};
      api_call->device_state_updated(static_cast<int>(i), action.state_data());
      api_call.Flush();
    }});
  }
}

void Commutator::OnNewStream(RcPtr<P2pStream> stream) {
  // store the stream inside the subscription
  streams_[stream->destination()] = stream;
  new_message_subs_.Push(stream->out_data_event().Subscribe(
      [stream_view{RcPtrView{stream}}, this](DataBuffer const& data) {
        if (auto stream = stream_view.lock(); stream) {
          OnNewMessage(stream, data);
        }
      }));
}

void Commutator::OnNewMessage(RcPtr<P2pStream> stream, DataBuffer const& data) {
  auto api_impl = CommutatorApiImpl{*this, std::move(stream)};
  auto parser = ApiParser{protocol_context_, data};
  parser.Parse(api_impl);
}

}  // namespace ae
