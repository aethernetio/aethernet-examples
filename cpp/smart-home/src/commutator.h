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
#include <vector>
#include <memory>

#include "aether/all.h"

#include "api/api.h"
#include "idevice.h"

namespace ae {
class Commutator {
  friend class CommutatorApiImpl;

 public:
  explicit Commutator(Client::ptr const& client);

  void AddDevice(std::unique_ptr<IDevice>&& device);

 private:
  void OnNewStream(RcPtr<P2pStream> stream);
  void OnNewMessage(RcPtr<P2pStream> stream, DataBuffer const& data);
  void SendSensorsState(RcPtr<P2pStream> const& stream);

  PtrView<Client> client_;
  ProtocolContext protocol_context_;
  SmartHomeClientApi client_api_;
  std::vector<std::unique_ptr<IDevice>> devices_;

  std::map<Uid, RcPtr<P2pStream>> streams_;
  Subscription new_request_sub_;
  MultiSubscription new_message_subs_;
};
}  // namespace ae

#endif  // COMUTATOR_H_
