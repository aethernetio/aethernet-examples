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

#include <limits>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#include "aether_lib.h"

constexpr ae::SafeStreamConfig kSafeStreamConfig{
    std::numeric_limits<std::uint16_t>::max(),                // buffer_capacity
    (std::numeric_limits<std::uint16_t>::max() / 2) - 1,      // window_size
    (std::numeric_limits<std::uint16_t>::max() / 2) - 1 - 1,  // max_data_size
    10,                              // max_repeat_count
    std::chrono::milliseconds{600},  // wait_confirm_timeout
    {},                              // send_confirm_timeout
    std::chrono::milliseconds{400},  // send_repeat_timeout
};

struct TestContext {
  ae::RcPtr<ae::AetherApp> aether_app;
  int send_success = 0;
  bool greeting_success = false;
  ae::CumulativeEvent<ae::Client::ptr, 2> client_selection_event;
  std::unique_ptr<ae::ByteIStream> bob_stream;
  std::unique_ptr<ae::ByteIStream> alice_stream;
  ae::ActionPtr<ae::TimerAction> timer;
};

static TestContext* context{};

void BobMeetAlice(ae::Client::ptr const& alice_client,
                  ae::Client::ptr const& bob_client) {
  context->bob_stream = ae::make_unique<ae::P2pSafeStream>(
      *context->aether_app, kSafeStreamConfig,
      bob_client->message_stream_manager().CreateStream(alice_client->uid()));
  auto bob_say = std::string_view{"Hello"};
  auto bob_send_message =
      context->bob_stream->Write({std::begin(bob_say), std::end(bob_say)});

  bob_send_message->StatusEvent().Subscribe(ae::ActionHandler{
      ae::OnResult{[&]() { context->send_success += 1; }},
      ae::OnError{[&]() {
        std::cerr << "Send error" << std::endl;
        context->aether_app->Exit(1);
      }},
  });

  context->bob_stream->out_data_event().Subscribe([&](auto const& data) {
    auto str = std::string_view{reinterpret_cast<char const*>(data.data()),
                                data.size()};
    Serial.println("Bob received %s\n", str);
    context->greeting_success = true;
  });

  context->alice_stream = ae::make_unique<ae::P2pSafeStream>(
      *context->aether_app, kSafeStreamConfig,
      alice_client->message_stream_manager().CreateStream(bob_client->uid()));

  context->alice_stream->out_data_event().Subscribe([&](auto const& data) {
    auto str = std::string_view{reinterpret_cast<char const*>(data.data()),
                                data.size()};
    Serial.println("Alice received %s\n", str);
    auto answear = std::string_view{"Hi"};
    auto alice_send_message =
        context->alice_stream->Write({std::begin(answear), std::end(answear)});
    alice_send_message->StatusEvent().Subscribe(ae::ActionHandler{
        ae::OnResult{[&]() { context->send_success += 1; }},
        ae::OnError{[&] {
          Serial.println("Send answer error");
          context->aether_app->Exit(2);
        }},
    });
  });
}

void setup() {
  context->aether_app = ae::AetherApp::Construct(ae::AetherAppContext{});

  auto alice_selector = context->aether_app->aether()->SelectClient(
      ae::Uid::FromString("3ac93165-3d37-4970-87a6-fa4ee27744e4"), 0);
  auto bob_selector = context->aether_app->aether()->SelectClient(
      ae::Uid::FromString("3ac93165-3d37-4970-87a6-fa4ee27744e4"), 1);

  context->client_selection_event.Connect(
      [&](auto event, auto status) {
        status.OnResult([&](auto const& action) { event = action.client(); })
            .OnError([&]() { context->aether_app->Exit(1); });
      },
      alice_selector->StatusEvent(), bob_selector->StatusEvent());

  context->client_selection_event.Subscribe([&](auto const& event) {
    if (context->aether_app->IsExited()) {
      return;
    }
    BobMeetAlice(event[0], event[1]);
  });

  context->timer = ae::ActionPtr<ae::TimerAction>(
      *context->aether_app->aether()->action_processor,
      std::chrono::seconds{10});
  context->timer->StatusEvent().Subscribe([&](auto const&) {
    Serial.println("Test timeout\n");
    context->aether_app->Exit(3);
  });
}

void loop() {
  if (context->aether_app->IsExited()) {
    Serial.printf("Exit error code: %d\n", context->aether_app->ExitCode());
    Serial.println();
    return;
  }

  auto current_time = ae::Now();
  auto next_time = context->aether_app->Update(current_time);
  context->aether_app->WaitUntil(
      std::min(next_time, current_time + std::chrono::seconds{1}));
  if (context->greeting_success && (context->send_success == 2)) {
    context->aether_app->Exit(0);
  }
}
