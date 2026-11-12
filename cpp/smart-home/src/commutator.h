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

#ifndef COMUTATOR_H_
#define COMUTATOR_H_

#include <map>
#include <memory>
#include <vector>

#include "aether/all.h"
#include "api/api.h"
#include "idevice.h"

namespace ae {
class Commutator {
  friend class CommutatorApiImpl;

 public:
  Commutator(AeContext const& context, Client::ptr const& client);

  void AddDevice(std::unique_ptr<IDevice>&& device);

 private:
  void OnNewPort(P2pPortHandle& port);
  void OnNewMessage(std::shared_ptr<P2pStream> stream, DataBuffer const& data);
  void SendSensorsState(std::shared_ptr<P2pStream> const& stream);

  AeContext context_;
  Client::ptr client_;
  ProtocolContext protocol_context_;
  SmartHomeClientApi client_api_;
  std::vector<std::unique_ptr<IDevice>> devices_;

  struct StreamSession {
    std::shared_ptr<P2pStream> stream;
    Subscription receive_sub;
  };
  // Replacing a peer session unsubscribes before releasing the old stream.
  std::map<Uid, std::unique_ptr<StreamSession>> streams_;
  Subscription new_request_sub_;
  std::vector<std::shared_ptr<DeviceStateAction>> pending_states_;
};
}  // namespace ae

#endif  // COMUTATOR_H_
