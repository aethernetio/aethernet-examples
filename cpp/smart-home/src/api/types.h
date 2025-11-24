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

#ifndef API_TYPES_H_
#define API_TYPES_H_

#include <string>
#include <cstdint>
#include <optional>

#include "aether/all.h"

namespace ae {

struct VariantBool {
  AE_REFLECT_MEMBERS(value)
  bool value;
};

struct VariantLong {
  AE_REFLECT_MEMBERS(value)
  std::uint64_t value;
};

struct VariantDouble {
  AE_REFLECT_MEMBERS(value)
  double value;
};

struct VariantString {
  AE_REFLECT_MEMBERS(value)
  std::string value;
};

struct VariantBytes {
  AE_REFLECT_MEMBERS(value)
  DataBuffer value;
};

struct VariantData
    : public VariantType<std::uint8_t, VPair<1, VariantBool>,
                         VPair<2, VariantLong>, VPair<3, VariantDouble>,
                         VPair<4, VariantString>, VPair<5, VariantBytes>> {
  using VariantType::VariantType;
};

struct DeviceStateData {
  AE_REFLECT_MEMBERS(payload, timestamp)

  VariantData payload;
  std::int64_t timestamp;
};

struct HwDeviceBase {
  AE_REFLECT_MEMBERS(local_id, descriptor)

  int local_id;
  std::string descriptor;
};

struct HardwareSensor : HwDeviceBase, NullableType<HardwareSensor> {
  AE_REFLECT(AE_REF_BASE(HwDeviceBase), AE_MMBRS(unit))
  std::optional<std::string> unit;
};

struct HardwareActor : HwDeviceBase {
  AE_REFLECT(AE_REF_BASE(HwDeviceBase))
};

struct HardwareDevice
    : public VariantType<std::uint8_t, VPair<1, HardwareSensor>,
                         VPair<2, HardwareActor>> {
  using VariantType::VariantType;
};
}  // namespace ae

#endif  // API_TYPES_H_
