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

static constexpr auto kParentUid =
    ae::Uid::FromString("3ac93165-3d37-4970-87a6-fa4ee27744e4");

constexpr ae::SafeStreamConfig kSafeStreamConfig{
    std::numeric_limits<std::uint16_t>::max(),                // buffer_capacity
    (std::numeric_limits<std::uint16_t>::max() / 2) - 1,      // window_size
    (std::numeric_limits<std::uint16_t>::max() / 2) - 1 - 1,  // max_data_size
    10,                              // max_repeat_count
    std::chrono::milliseconds{600},  // wait_confirm_timeout
    {},                              // send_confirm_timeout
    std::chrono::milliseconds{400},  // send_repeat_timeout
};

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

class TimeSynchronizer {
 public:
  TimeSynchronizer() = default;

  void SetPingSentTime(ae::TimePoint ping_sent_time);
  void SetPongSentTime(ae::TimePoint pong_sent_time);

  ae::Duration GetPingDuration() const;
  ae::Duration GetPongDuration() const;

 private:
  ae::TimePoint ping_sent_time_;
  ae::TimePoint pong_sent_time_;
};

// Alice sends "ping"s to Bob
class Alice {
  class IntervalSender : public ae::Action<IntervalSender> {
   public:
    IntervalSender(ae::ActionContext action_context,
                   TimeSynchronizer& time_synchronizer, ae::ByteIStream& stream,
                   ae::Duration interval);

    ae::TimePoint Update(ae::TimePoint current_time) override;

   private:
    void ResponseReceived(ae::DataBuffer const& data_buffer);

    ae::ByteIStream& stream_;
    TimeSynchronizer* time_synchronizer_;
    ae::Duration interval_;
    ae::Subscription response_subscription_;
    ae::TimePoint sent_time_;
    ae::MultiSubscription send_subscriptions_;
  };

 public:
  explicit Alice(ae::AetherApp& aether_app, ae::Client::ptr client_alice,
                 TimeSynchronizer& time_synchronizer, ae::Uid bobs_uid);

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
  explicit Bob(ae::AetherApp& aether_app, ae::Client::ptr client_bob,
               TimeSynchronizer& time_synchronizer);

 private:
  void OnNewStream(ae::Uid destination_uid,
                   std::unique_ptr<ae::ByteIStream> message_stream);
  void StreamCreated(ae::ByteIStream& stream);

  ae::Aether::ptr aether_;
  ae::Client::ptr client_bob_;
  TimeSynchronizer* time_synchronizer_;
  std::unique_ptr<ae::P2pSafeStream> p2pstream_;
  ae::Subscription new_stream_receive_subscription_;
  ae::Subscription message_receive_subscription_;
};

int main() {
  auto aether_app = ae::AetherApp::Construct(ae::AetherAppConstructor{});

  std::unique_ptr<Alice> alice;
  std::unique_ptr<Bob> bob;
  TimeSynchronizer time_synchronizer;

  auto client_register_action = ClientRegister{*aether_app};

  // Create a subscription to the Result event
  client_register_action.ResultEvent().Subscribe([&](auto const& action) {
    auto [client_alice, client_bob] = action.get_clients();
    alice = ae::make_unique<Alice>(*aether_app, std::move(client_alice),
                                   time_synchronizer, client_bob->uid());
    bob = ae::make_unique<Bob>(*aether_app, std::move(client_bob),
                               time_synchronizer);
    // Save the current aether state
    aether_app->domain().SaveRoot(aether_app->aether());
  });

  // Subscription to the Error event
  client_register_action.ErrorEvent().Subscribe(
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
      alice_register->ResultEvent().Subscribe([&](auto const& action) {
        alice_ = action.client();
        if (bob_) {
          state_ = State::kDone;
        }
      }),
      bob_register->ResultEvent().Subscribe([&](auto const& action) {
        bob_ = action.client();
        if (alice_) {
          state_ = State::kDone;
        }
      }),
      alice_register->ErrorEvent().Subscribe(
          [&](auto const&) { state_ = State::kError; }),
      bob_register->ErrorEvent().Subscribe(
          [&](auto const&) { state_ = State::kError; }));
}

void TimeSynchronizer::SetPingSentTime(ae::TimePoint ping_sent_time) {
  ping_sent_time_ = ping_sent_time;
}
void TimeSynchronizer::SetPongSentTime(ae::TimePoint pong_sent_time) {
  pong_sent_time_ = pong_sent_time;
}
ae::Duration TimeSynchronizer::GetPingDuration() const {
  return std::chrono::duration_cast<ae::Duration>(ae::Now() - ping_sent_time_);
}
ae::Duration TimeSynchronizer::GetPongDuration() const {
  return std::chrono::duration_cast<ae::Duration>(ae::Now() - pong_sent_time_);
}

Alice::Alice(ae::AetherApp& aether_app, ae::Client::ptr client_alice,
             TimeSynchronizer& time_synchronizer, ae::Uid bobs_uid)
    : aether_{aether_app.aether()},
      client_alice_{std::move(client_alice)},
      p2pstream_{ae::ActionContext{*aether_->action_processor},
                 kSafeStreamConfig,
                 ae::make_unique<ae::P2pStream>(
                     ae::ActionContext{*aether_->action_processor},
                     client_alice_, bobs_uid)},
      interval_sender_{ae::ActionContext{*aether_->action_processor},
                       time_synchronizer, p2pstream_,
                       std::chrono::milliseconds{5000}},
      interval_sender_subscription_{interval_sender_.ErrorEvent().Subscribe(
          [&](auto const&) { aether_app.Exit(1); })} {}

Alice::IntervalSender::IntervalSender(ae::ActionContext action_context,
                                      TimeSynchronizer& time_synchronizer,
                                      ae::ByteIStream& stream,
                                      ae::Duration interval)
    : ae::Action<IntervalSender>{action_context},
      stream_{stream},
      time_synchronizer_{&time_synchronizer},
      interval_{interval},
      response_subscription_{stream.out_data_event().Subscribe(
          *this, ae::MethodPtr<&IntervalSender::ResponseReceived>{})} {}

ae::TimePoint Alice::IntervalSender::Update(ae::TimePoint current_time) {
  if (sent_time_ + interval_ <= current_time) {
    constexpr std::string_view ping_message = "ping";

    time_synchronizer_->SetPingSentTime(current_time);

    std::cout << "send \"ping\"" << '\n';
    auto send_action =
        stream_.Write({std::begin(ping_message), std::end(ping_message)});

    // notify about error
    send_subscriptions_.Push(
        send_action->ErrorEvent().Subscribe([&](auto const&) {
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
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   time_synchronizer_->GetPongDuration())
                   .count()
            << " ms" << '\n';
}

Bob::Bob(ae::AetherApp& aether_app, ae::Client::ptr client_bob,
         TimeSynchronizer& time_synchronizer)
    : aether_{aether_app.aether()},
      client_bob_{std::move(client_bob)},
      time_synchronizer_{&time_synchronizer},
      new_stream_receive_subscription_{
          client_bob_->client_connection()->new_stream_event().Subscribe(
              *this, ae::MethodPtr<&Bob::OnNewStream>{})} {}

void Bob::OnNewStream(ae::Uid destination_uid,
                      std::unique_ptr<ae::ByteIStream> message_stream) {
  p2pstream_ = ae::make_unique<ae::P2pSafeStream>(
      ae::ActionContext{*aether_->action_processor}, kSafeStreamConfig,
      ae::make_unique<ae::P2pStream>(
          ae::ActionContext{*aether_->action_processor}, client_bob_,
          destination_uid, std::move(message_stream)));
  StreamCreated(*p2pstream_);
}

void Bob::StreamCreated(ae::ByteIStream& stream) {
  message_receive_subscription_ =
      stream.out_data_event().Subscribe([&](auto const& data_buffer) {
        auto ping_message =
            std::string_view{reinterpret_cast<char const*>(data_buffer.data()),
                             data_buffer.size()};
        std::cout << "received " << std::quoted(ping_message) << " within time "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         time_synchronizer_->GetPingDuration())
                         .count()
                  << " ms\n";

        time_synchronizer_->SetPongSentTime(ae::Now());
        constexpr std::string_view pong_message = "pong";
        std::cout << "send \"pong\"" << '\n';
        stream.Write({std::begin(pong_message), std::end(pong_message)});
      });
}
