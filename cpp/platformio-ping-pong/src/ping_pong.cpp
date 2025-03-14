/*
 * Copyright 2024 Aethernet Inc.
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

#include <iostream>
#include <string_view>

#include "aether/uid.h"
#include "aether/aether.h"
#include "aether/client.h"
#include "aether/literal_array.h"
#include "aether/state_machine.h"
#include "aether/actions/action.h"
#include "aether/events/multi_subscription.h"
#include "aether/client_messages/p2p_safe_message_stream.h"

#include "aether/aether_app.h"

#include "aether/adapters/ethernet.h"
#include "aether/adapters/esp32_wifi.h"

#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_task_wdt.h>

static constexpr std::string_view kWifiSsid = "Test123";
static constexpr std::string_view kWifiPass = "Test123";
static constexpr std::string_view kTag = "PingPong";

static constexpr auto kParentUid =
    ae::Uid{ae::MakeLiteralArray("3ac931653d37497087a6fa4ee27744e4")};

constexpr ae::SafeStreamConfig kSafeStreamConfig{
    std::numeric_limits<std::uint16_t>::max(),                // buffer_capacity
    (std::numeric_limits<std::uint16_t>::max() / 2) - 1,      // window_size
    (std::numeric_limits<std::uint16_t>::max() / 2) - 1 - 1,  // max_data_size
    10,                              // max_repeat_count
    std::chrono::milliseconds{600},  // wait_confirm_timeout
    {},                              // send_confirm_timeout
    std::chrono::milliseconds{400},  // send_repeat_timeout
};

extern "C" void app_main();
int AetherPingPongExample();

void app_main(void) {
  /*If you are using WDT at a given time, you must disable it by updating the
  configuration, or simply deleting the WDT tasks for each processor core
  using the following code:
  esp_task_wdt_delete(xTaskGetIdleTaskHandleForCore(0));
  esp_task_wdt_delete(xTaskGetIdleTaskHandleForCore(1));
  In the future, WDT support will be included in the core code of the
  Aether library.*/

  esp_task_wdt_config_t config_wdt = {
      .timeout_ms = 60000,
      .idle_core_mask = 0,  // i.e. do not watch any idle task
      .trigger_panic = true};

  esp_err_t err = esp_task_wdt_reconfigure(&config_wdt);
  if (err != 0)
    ESP_LOGE(std::string(kTag).c_str(), "Reconfigure WDT is failed!");

  AetherPingPongExample();
}

class ClientRegister : public ae::Action<ClientRegister> {
  enum class State : std::uint8_t { kRegistration, kDone, kError };

 public:
  explicit ClientRegister(ae::AetherApp& aether_app);

  ae::TimePoint Update(ae::TimePoint current_time) override;

  std::pair<ae::Client::ptr, ae::Client::ptr> get_clients() const {
    return std::make_pair(alice_, bob_);
  }

 private:
  void AliceAndBobRegister();

  ae::Aether::ptr aether_;
  ae::MultiSubscription register_subscriptions_;
  ae::Client::ptr alice_;
  ae::Client::ptr bob_;
  ae::StateMachine<State> state_;
};

// Alice sends "ping"s to Bob
class Alice {
  class IntervalSender : public ae::Action<IntervalSender> {
   public:
    IntervalSender(ae::ActionContext action_context, ae::ByteStream& stream,
                   ae::Duration interval);

    ae::TimePoint Update(ae::TimePoint current_time) override;

   private:
    void ResponseReceived(ae::DataBuffer const& data_buffer);

    ae::ByteStream& stream_;
    ae::Duration interval_;
    ae::Subscription response_subscription_;
    ae::TimePoint sent_time_;
    ae::MultiSubscription send_subscriptions_;
  };

 public:
  explicit Alice(ae::AetherApp& aether_app, ae::Client::ptr client_alice,
                 ae::Uid bobs_uid);

 private:
  ae::Aether::ptr aether_;
  ae::Client::ptr client_alice_;
  ae::P2pSafeStream p2pstream_;
  IntervalSender interval_sender_;
  ae::Subscription interval_sender_subscription_;
};

// Bob answers "pong" to each "ping"
class Bob {
 public:
  explicit Bob(ae::AetherApp& aether_app, ae::Client::ptr client_bob);

 private:
  void OnNewStream(ae::Uid destination_uid, ae::StreamId stream_id,
                   std::unique_ptr<ae::ByteStream> message_stream);
  void StreamCreated(ae::ByteStream& stream);

  ae::Aether::ptr aether_;
  ae::Client::ptr client_bob_;
  std::unique_ptr<ae::P2pSafeStream> p2pstream_;
  ae::Subscription new_stream_receive_subscription_;
  ae::Subscription message_receive_subscription_;
};

int AetherPingPongExample() {
  auto aether_app = ae::AetherApp::Construct(
      ae::AetherAppConstructor{
#if !AE_SUPPORT_REGISTRATION
          []() {
            auto fs =
                ae::make_unique<ae::FileSystemHeaderFacility>(std::string(""));
            return fs;
          }
#endif  // AE_SUPPORT_REGISTRATION
      }
#if defined AE_DISTILLATION
          .Adapter([](ae::Domain* domain,
                      ae::Aether::ptr const& aether) -> ae::Adapter::ptr {
#  if defined ESP32_WIFI_ADAPTER_ENABLED
            auto adapter = domain->CreateObj<ae::Esp32WifiAdapter>(
                ae::GlobalId::kEsp32WiFiAdapter, aether, aether->poller,
                std::string(kWifiSsid), std::string(kWifiPass));
#  else
            auto adapter = domain->CreateObj<ae::EthernetAdapter>(
                ae::GlobalId::kEthernetAdapter, aether, aether->poller);
#  endif
            return adapter;
          })
#endif
  );

  std::unique_ptr<Alice> alice;
  std::unique_ptr<Bob> bob;

  auto client_register_action = ClientRegister{*aether_app};

  // Create subscription to Result event
  auto on_registered =
      client_register_action.SubscribeOnResult([&](auto const& action) {
        auto [client_alice, client_bob] = action.get_clients();
        alice = ae::make_unique<Alice>(*aether_app, std::move(client_alice),
                                       client_bob->uid());
        bob = ae::make_unique<Bob>(*aether_app, std::move(client_bob));
        // Save current aether state
        aether_app->domain().SaveRoot(aether_app->aether());
      });

  // Subscription to Error event
  auto on_register_failed = client_register_action.SubscribeOnError(
      [&](auto const&) { aether_app->Exit(1); });

  while (!aether_app->IsExited()) {
    auto next_time = aether_app->Update(ae::Now());
    aether_app->WaitUntil(next_time);
  }
  return aether_app->ExitCode();
}

ClientRegister::ClientRegister(ae::AetherApp& aether_app)
    : ae::Action<ClientRegister>{aether_app},
      aether_{aether_app.aether()},
      state_{State::kRegistration} {}

ae::TimePoint ClientRegister::Update(ae::TimePoint current_time) {
  if (state_.changed()) {
    switch (state_.Acquire()) {
      case State::kRegistration:
        AliceAndBobRegister();
        break;
      case State::kDone:
        ae::Action<ClientRegister>::Result(*this);
        return current_time;
      case State::kError:
        ae::Action<ClientRegister>::Error(*this);
        return current_time;
    }
  }
  return current_time;
}

void ClientRegister::AliceAndBobRegister() {
  if (aether_->clients().size() == 2) {
    alice_ = aether_->clients()[0];
    bob_ = aether_->clients()[1];
    AE_TELED_INFO("Used already registered clients");
    state_ = State::kDone;
    return;
  }
  aether_->clients().clear();
  auto alice_register = aether_->RegisterClient(kParentUid);
  auto bob_register = aether_->RegisterClient(kParentUid);
  register_subscriptions_.Push(
      alice_register->SubscribeOnResult([&](auto const& action) {
        alice_ = action.client();
        if (bob_) {
          state_ = State::kDone;
        }
      }),
      bob_register->SubscribeOnResult([&](auto const& action) {
        bob_ = action.client();
        if (alice_) {
          state_ = State::kDone;
        }
      }),
      alice_register->SubscribeOnError(
          [&](auto const&) { state_ = State::kError; }),
      bob_register->SubscribeOnError(
          [&](auto const&) { state_ = State::kError; }));
}

Alice::Alice(ae::AetherApp& aether_app, ae::Client::ptr client_alice,
             ae::Uid bobs_uid)
    : aether_{aether_app.aether()},
      client_alice_{std::move(client_alice)},
      p2pstream_{ae::ActionContext{*aether_->action_processor},
                 kSafeStreamConfig,
                 ae::make_unique<ae::P2pStream>(
                     ae::ActionContext{*aether_->action_processor},
                     client_alice_, bobs_uid, ae::StreamId{0})},
      interval_sender_{ae::ActionContext{*aether_->action_processor},
                       p2pstream_, std::chrono::milliseconds{5000}},
      interval_sender_subscription_{interval_sender_.SubscribeOnError(
          [&](auto const&) { aether_app.Exit(1); })} {}

Alice::IntervalSender::IntervalSender(ae::ActionContext action_context,
                                      ae::ByteStream& stream,
                                      ae::Duration interval)
    : ae::Action<IntervalSender>{action_context},
      stream_{stream},
      interval_{interval},
      response_subscription_{stream.in().out_data_event().Subscribe(
          *this, ae::MethodPtr<&IntervalSender::ResponseReceived>{})} {}

ae::TimePoint Alice::IntervalSender::Update(ae::TimePoint current_time) {
  if (sent_time_ + interval_ <= current_time) {
    constexpr std::string_view ping_message = "ping";
    std::cout << "send \"ping\"" << '\n';
    auto send_action = stream_.in().Write(
        {std::begin(ping_message), std::end(ping_message)}, current_time);

    // notify about error
    send_subscriptions_.Push(send_action->SubscribeOnError([&](auto const&) {
      std::cerr << "ping send error" << '\n';
      ae::Action<IntervalSender>::Error(*this);
    }));

    sent_time_ = current_time;
  }

  return sent_time_ + interval_;
}

void Alice::IntervalSender::ResponseReceived(
    ae::DataBuffer const& data_buffer) {
  auto pong_message = std::string_view{
      reinterpret_cast<char const*>(data_buffer.data()), data_buffer.size()};
  std::cout << "received " << std::quoted(pong_message) << " within time "
            << std::chrono::duration_cast<std::chrono::milliseconds>(ae::Now() -
                                                                     sent_time_)
                   .count()
            << " ms" << '\n';
}

Bob::Bob(ae::AetherApp& aether_app, ae::Client::ptr client_bob)
    : aether_{aether_app.aether()},
      client_bob_{std::move(client_bob)},
      new_stream_receive_subscription_{
          client_bob_->client_connection()->new_stream_event().Subscribe(
              *this, ae::MethodPtr<&Bob::OnNewStream>{})} {}

void Bob::OnNewStream(ae::Uid destination_uid, ae::StreamId stream_id,
                      std::unique_ptr<ae::ByteStream> message_stream) {
  p2pstream_ = ae::make_unique<ae::P2pSafeStream>(
      ae::ActionContext{*aether_->action_processor}, kSafeStreamConfig,
      ae::make_unique<ae::P2pStream>(
          ae::ActionContext{*aether_->action_processor}, client_bob_,
          destination_uid, stream_id, std::move(message_stream)));
  StreamCreated(*p2pstream_);
}

void Bob::StreamCreated(ae::ByteStream& stream) {
  message_receive_subscription_ =
      stream.in().out_data_event().Subscribe([&](auto const& data_buffer) {
        auto ping_message =
            std::string_view{reinterpret_cast<char const*>(data_buffer.data()),
                             data_buffer.size()};
        std::cout << "received " << std::quoted(ping_message) << '\n';

        constexpr std::string_view pong_message = "pong";
        stream.in().Write({std::begin(pong_message), std::end(pong_message)},
                          ae::Now());
      });
}
