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

#include <algorithm>

#include "commutator_api_impl.h"

namespace ae {
Commutator::Commutator(AeContext const& context, Client::ptr const& client)
    : context_{context}, client_{client}, protocol_context_{context} {
  new_request_sub_ =
      client_->message_stream_manager().new_port_event().Subscribe(
          [this](P2pPortHandle& port) { OnNewPort(port); });
}

void Commutator::AddDevice(std::unique_ptr<IDevice>&& device) {
  assert(device && "Device cannot be null");
  device->SetLocalId(static_cast<int>(devices_.size()));
  devices_.push_back(std::move(device));
}

void Commutator::SendSensorsState(std::shared_ptr<P2pStream> const& stream) {
  std::erase_if(pending_states_, [](auto const& op) { return op->finished(); });
  for (std::size_t i = 0; i < devices_.size(); ++i) {
    auto operation = devices_[i]->GetState();
    pending_states_.push_back(operation);
    operation->Then([stream, i](auto const& state) {
      SmartHomeClientApi api;
      ApiContext call{api};
      call->device_state_updated(static_cast<int>(i), state);
      stream->Write(std::move(call).Pack());
    });
  }
}

void Commutator::OnNewPort(P2pPortHandle& port) {
  auto destination = port.destination();
  auto stream = std::make_shared<P2pStream>(context_, client_.Load(),
                                            destination, std::move(port));
  auto session = std::make_unique<StreamSession>();
  session->stream = stream;
  session->receive_sub = stream->out_data_event().Subscribe(
      [weak = std::weak_ptr<P2pStream>{stream}, this](DataBuffer const& data) {
        if (auto locked = weak.lock()) OnNewMessage(std::move(locked), data);
      });
  streams_[destination] = std::move(session);
}

void Commutator::OnNewMessage(std::shared_ptr<P2pStream> stream,
                              DataBuffer const& data) {
  CommutatorApiImpl api{
      devices_, pending_states_,
      [stream](DataBuffer reply) { stream->Write(std::move(reply)); },
      [this, stream] { SendSensorsState(stream); }};
  ApiParser parser{protocol_context_, data};
  parser.Parse(api);
}
}  // namespace ae
