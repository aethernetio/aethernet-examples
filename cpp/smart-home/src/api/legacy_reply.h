/*
 * Copyright 2026 Aethernet Inc.
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

#ifndef SMART_HOME_API_LEGACY_REPLY_H_
#define SMART_HOME_API_LEGACY_REPLY_H_

#include "aether/api_protocol/details/api_packer.h"
#include "aether/api_protocol/details/request_id.h"
#include "api/types.h"

namespace ae::smart_home_wire {
// Original ReturnResultApi wire: ID 0, request ID, result; ID 1, request ID,
// uint8 error type, int32 error code. Do not replace these with new SDK
// promises.
template <typename T>
DataBuffer Result(RequestId request_id, T data) {
  DataBuffer buffer;
  ApiPacker packer{buffer};
  packer.Pack(0, GenericMessage<RequestId, T>{request_id, std::move(data)});
  return buffer;
}

inline DataBuffer Error(RequestId request_id, std::uint8_t type,
                        std::int32_t code) {
  DataBuffer buffer;
  ApiPacker packer{buffer};
  packer.Pack(1, GenericMessage<RequestId, std::uint8_t, std::int32_t>{
                     request_id, type, code});
  return buffer;
}
}  // namespace ae::smart_home_wire
#endif
