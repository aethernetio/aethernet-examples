/*
 * Copyright 2026 Aethernet Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdio>
#include <cstdlib>

#include "api/legacy_reply.h"
#include "commutator_api_impl.h"

#define CHECK(condition)                                           \
  do {                                                             \
    if (!(condition)) {                                            \
      std::fprintf(stderr, "line %d: %s\n", __LINE__, #condition); \
      std::exit(1);                                                \
    }                                                              \
  } while (false)

namespace {
using namespace ae;

// Fixed bytes derived from the pre-push SDK 345e556b (2025-11-28), not by
// round-tripping this implementation. See README.md for the original sources.
// Original mstream writes arithmetic values as little-endian native bytes on
// supported targets, bool as 00/01, and collection sizes as PackedSize.
template <typename T>
DataBuffer Pack(T value) {
  DataBuffer buffer;
  ApiPacker packer{buffer};
  packer.Pack(99, GenericMessage<T>{std::move(value)});
  return buffer;
}

void GoldenTypes() {
  CHECK(Pack(VariantData{VariantBool{true}}) == DataBuffer({99, 1, 1}));
  CHECK(Pack(VariantData{VariantBool{false}}) == DataBuffer({99, 1, 0}));
  CHECK(Pack(VariantData{VariantLong{0x0102030405060708ULL}}) ==
        DataBuffer({99, 2, 8, 7, 6, 5, 4, 3, 2, 1}));
  CHECK(Pack(VariantData{VariantDouble{1.5}}) ==
        DataBuffer({99, 3, 0, 0, 0, 0, 0, 0, 0xf8, 0x3f}));
  CHECK(Pack(VariantData{VariantString{"abc"}}) ==
        DataBuffer({99, 4, 3, 'a', 'b', 'c'}));
  CHECK(Pack(VariantData{VariantBytes{{0xde, 0xad, 0xbe}}}) ==
        DataBuffer({99, 5, 3, 0xde, 0xad, 0xbe}));

  HardwareSensor sensor;
  sensor.local_id = 7;
  sensor.descriptor = "S";
  sensor.unit = "C";
  HardwareActor actor;
  actor.local_id = 8;
  actor.descriptor = "A";
  std::vector<HardwareDevice> devices{HardwareDevice{sensor},
                                      HardwareDevice{actor}};
  CHECK(smart_home_wire::Result(RequestId{0x12345678}, devices) ==
        DataBuffer({0, 0x78, 0x56, 0x34, 0x12, 2, 1, 0, 7, 0, 0,  0,
                    1, 'S',  1,    'C',  2,    8, 0, 0, 0, 1, 'A'}));
  sensor.unit.reset();
  CHECK(Pack(HardwareDevice{sensor}) ==
        DataBuffer({99, 1, 4, 7, 0, 0, 0, 1, 'S'}));

  // Freeze the first size-encoding transition, including collections and UTF-8.
  auto string250 = Pack(VariantData{VariantString{std::string(250, 'x')}});
  CHECK(string250.size() == 253 && string250[2] == 0xfa);
  auto string251 = Pack(VariantData{VariantString{std::string(251, 'x')}});
  CHECK(string251.size() == 255 && string251[2] == 0xfb && string251[3] == 0);
  CHECK(Pack(VariantData{VariantString{"\xc2\xb0"}}) ==
        DataBuffer({99, 4, 2, 0xc2, 0xb0}));
}

class DelayedState final : public DeviceStateAction {
 public:
  void Resolve() {
    Complete({VariantData{VariantBool{true}}, 0x0102030405060708LL});
  }
};
class TestDevice final : public IDevice {
 public:
  void SetLocalId(int) override {}
  HardwareDevice description() const override {
    HardwareActor actor;
    actor.local_id = 0;
    actor.descriptor = "A";
    return HardwareDevice{actor};
  }
  std::shared_ptr<DeviceStateAction> GetState() override {
    ++reads;
    return operation = std::make_shared<DelayedState>();
  }
  std::shared_ptr<DeviceStateAction> Execute(
      VariantData const& command) override {
    CHECK(command.Index() == 4);
    CHECK(command.Get<VariantString>().value == "go");
    ++executes;
    return operation = std::make_shared<DelayedState>();
  }
  int reads{}, executes{};
  std::shared_ptr<DelayedState> operation;
};

struct TestContext {
  TaskScheduler scheduler_;
  EventSystem events_;
  TaskScheduler& scheduler() const {
    return const_cast<TaskScheduler&>(scheduler_);
  }
  EventSystem& event_system() const {
    return const_cast<EventSystem&>(events_);
  }
};

void ScheduledCompletion() {
  TestContext context;
  int completed = 0;
  auto operation = std::make_shared<ScheduledDeviceState>(context, 21.5F);
  operation->Then([&](DeviceStateData const& state) {
    ++completed;
    CHECK(state.payload.Get<VariantDouble>().value == 21.5);
  });
  CHECK(completed == 0 && !operation->finished());
  context.scheduler().Update();
  CHECK(completed == 1 && operation->finished());
  context.scheduler().Update();
  CHECK(completed == 1);
  {
    auto canceled = std::make_shared<ScheduledDeviceState>(context, 99.F);
    canceled->Then([&](DeviceStateData const&) { ++completed; });
  }
  context.scheduler().Update();
  CHECK(completed == 1);
}

void GoldenDispatch() {
  TestContext context;
  ProtocolContext protocol{context};
  std::vector<std::unique_ptr<IDevice>> devices;
  auto device = std::make_unique<TestDevice>();
  auto* observed = device.get();
  devices.push_back(std::move(device));
  std::vector<std::shared_ptr<DeviceStateAction>> pending;
  std::vector<DataBuffer> replies;
  int query_all = 0;
  auto dispatch = [&](DataBuffer const& bytes) {
    CommutatorApiImpl api{
        devices, pending,
        [&](DataBuffer data) { replies.push_back(std::move(data)); },
        [&] { ++query_all; }};
    ApiParser parser{protocol, bytes};
    CHECK(parser.Parse(api));
  };

  dispatch({10, 0x78, 0x56, 0x34, 0x12});
  CHECK(replies.back() ==
        DataBuffer({0, 0x78, 0x56, 0x34, 0x12, 1, 2, 0, 0, 0, 0, 1, 'A'}));
  auto count = replies.size();
  dispatch({5, 0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0});
  CHECK(observed->reads == 1 && replies.size() == count);
  // Parser and handler have already died; delayed completion remains valid.
  observed->operation->Resolve();
  CHECK(replies.back() ==
        DataBuffer({0, 0x78, 0x56, 0x34, 0x12, 1, 1, 8, 7, 6, 5, 4, 3, 2, 1}));
  dispatch({4, 0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0, 4, 2, 'g', 'o'});
  CHECK(observed->executes == 1 && pending.size() == 1);
  observed->operation->Resolve();

  // Negative and out-of-range IDs must reply exactly once without device
  // access.
  count = replies.size();
  dispatch({5, 0x78, 0x56, 0x34, 0x12, 0xff, 0xff, 0xff, 0xff});
  CHECK(replies.size() == count + 1 && observed->reads == 1);
  CHECK(replies.back() ==
        DataBuffer({1, 0x78, 0x56, 0x34, 0x12, 2, 1, 0, 0, 0}));
  dispatch({4, 0x78, 0x56, 0x34, 0x12, 1, 0, 0, 0, 1, 0});
  CHECK(observed->executes == 1);
  CHECK(replies.back() ==
        DataBuffer({1, 0x78, 0x56, 0x34, 0x12, 1, 1, 0, 0, 0}));
  dispatch({6});
  CHECK(query_all == 1);

  SmartHomeClientApi api;
  ApiContext call{api};
  call->device_state_updated(
      -2, {VariantData{VariantDouble{1.5}}, 0x0102030405060708LL});
  CHECK(std::move(call).Pack() ==
        DataBuffer({3, 0xfe, 0xff, 0xff, 0xff, 3, 0, 0, 0, 0, 0,
                    0, 0xf8, 0x3f, 8,    7,    6, 5, 4, 3, 2, 1}));
}
}  // namespace

int main() {
  static_assert(sizeof(int) == 4);
  GoldenTypes();
  ScheduledCompletion();
  GoldenDispatch();
  std::puts("SmartHome legacy wire and deferred dispatch checks passed");
}
