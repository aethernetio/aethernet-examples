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

#include <cassert>
#include <functional>
#include <memory>
#include <utility>

#include "aether/actions/action_context.h"
#include "aether/clock.h"
#include "aether/tasks/manual_task_scheduler.h"
#include "api/types.h"

namespace ae {
class DeviceStateAction {
 public:
  using Completion = std::function<void(DeviceStateData const&)>;
  DeviceStateAction() = default;
  DeviceStateAction(DeviceStateAction const&) = delete;
  DeviceStateAction& operator=(DeviceStateAction const&) = delete;
  virtual ~DeviceStateAction() = default;
  void Then(Completion completion) {
    completion_ = std::move(completion);
    if (finished_) Deliver();
  }
  bool finished() const { return finished_; }

 protected:
  void Complete(DeviceStateData state) {
    assert(!finished_ && "A device operation completes once");
    if (finished_) return;
    state_ = std::move(state);
    finished_ = true;
    Deliver();
  }

 private:
  void Deliver() {
    if (completion_) {
      auto completion = std::exchange(completion_, {});
      completion(state_);
    }
  }
  bool finished_{};
  DeviceStateData state_{};
  Completion completion_;
};

// Ownership and the task subscription keep the operation alive until delivery.
// Completion happens on a later scheduler update, after Then() can be
// installed.
class ScheduledDeviceState final : public DeviceStateAction {
 public:
  ScheduledDeviceState(ActionContext auto const& context, float value) {
    state_.payload = VariantData{VariantDouble{value}};
    state_.timestamp = static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            Now().time_since_epoch())
            .count());
    task_ = context.scheduler().Task([this] { Complete(std::move(state_)); });
  }

 private:
  DeviceStateData state_{};
  TaskSubscription task_;
};

class IDevice {
 public:
  virtual ~IDevice() = default;

  virtual void SetLocalId(int id) = 0;
  virtual HardwareDevice description() const = 0;
  virtual std::shared_ptr<DeviceStateAction> GetState() = 0;
  virtual std::shared_ptr<DeviceStateAction> Execute(
      VariantData const& command) = 0;
};
}  // namespace ae

#endif  // IDEVICE_H_
