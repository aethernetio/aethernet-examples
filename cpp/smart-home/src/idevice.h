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

#ifndef IDEVICE_H_
#define IDEVICE_H_

#include "aether/all.h"

#include "api/types.h"

namespace ae {
class DeviceStateAction : public Action<DeviceStateAction> {
 public:
  using Action::Action;

  virtual UpdateStatus Update() = 0;
  virtual DeviceStateData state_data() const = 0;
};

class IDevice {
 public:
  virtual ~IDevice() = default;

  virtual void SetLocalId(int id) = 0;
  virtual HardwareDevice description() const = 0;
  virtual ActionPtr<DeviceStateAction> GetState() = 0;
  virtual ActionPtr<DeviceStateAction> Execute(VariantData const& command) = 0;
};
}  // namespace ae

#endif  // IDEVICE_H_
