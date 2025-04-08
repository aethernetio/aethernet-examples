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
  ae::Ptr<ae::AetherApp> aether_app;
  int send_success = 0;
  bool greeting_success = false;
  std::unique_ptr<ae::ByteIStream> bob_stream;
  std::unique_ptr<ae::ByteIStream> alice_stream;
  std::unique_ptr<ae::TimerAction> timer;
};

static TestContext* context{};

void setup() {
  context->aether_app = ae::AetherApp::Construct(ae::AetherAppConstructor{[]() {
    // TODO: return normal file system
    return ae::make_unique<ae::FileSystemHeaderFacility>(std::string(""));
  }});

  ae::Client::ptr alice_client = context->aether_app->aether()->clients()[0];
  ae::Client::ptr bob_client = context->aether_app->aether()->clients()[1];

  context->bob_stream = ae::make_unique<ae::P2pSafeStream>(
      *context->aether_app->aether()->action_processor, kSafeStreamConfig,
      ae::make_unique<ae::P2pStream>(
          *context->aether_app->aether()->action_processor, bob_client,
          alice_client->uid(), ae::StreamId{0}));
  auto bob_say = std::string_view{"Hello"};
  auto bob_send_message =
      context->bob_stream->Write({std::begin(bob_say), std::end(bob_say)});

  bob_send_message->ResultEvent().Subscribe([&](auto const&) {
    context->send_success += 1;
    context->aether_app->get_trigger().Trigger();
  });
  bob_send_message->ErrorEvent().Subscribe([&](auto const&) {
    Serial.println("Send error");
    context->aether_app->Exit(1);
  });

  context->bob_stream->out_data_event().Subscribe([&](auto const& data) {
    auto str =
        std::string{reinterpret_cast<char const*>(data.data()), data.size()};
    Serial.printf("Bob received %s\n", str.c_str());
    context->greeting_success = true;
    context->aether_app->get_trigger().Trigger();
  });

  alice_client->client_connection()->new_stream_event().Subscribe(
      [&, alice_client](auto uid, auto id, auto& stream) {
        context->alice_stream = ae::make_unique<ae::P2pSafeStream>(
            *context->aether_app->aether()->action_processor, kSafeStreamConfig,
            ae::make_unique<ae::P2pStream>(
                *context->aether_app->aether()->action_processor, alice_client,
                uid, id, stream));

        context->alice_stream->out_data_event().Subscribe(
            [&](auto const& data) {
              auto str = std::string{reinterpret_cast<char const*>(data.data()),
                                     data.size()};
              Serial.printf("Alice received %s\n", str.c_str());
              auto answear = std::string_view{"Hi"};
              auto alice_send_message = context->alice_stream->Write(
                  {std::begin(answear), std::end(answear)});
              alice_send_message->ResultEvent().Subscribe([&](auto const&) {
                context->send_success += 1;
                context->aether_app->get_trigger().Trigger();
              });
              alice_send_message->ErrorEvent().Subscribe([&](auto const&) {
                Serial.println("Send answear error");
                context->aether_app->Exit(2);
              });
            });
      });

  context->timer = ae::make_unique<ae::TimerAction>(
      *context->aether_app->aether()->action_processor,
      std::chrono::seconds{10});
  context->timer->ResultEvent().Subscribe([&](auto const&) {
    Serial.println("Test timeout");
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
