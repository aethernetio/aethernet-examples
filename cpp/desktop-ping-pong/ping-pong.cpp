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

#include "aether/all.h"

static constexpr auto kParentUid =
    ae::Uid::FromString("3ac93165-3d37-4970-87a6-fa4ee27744e4");

constexpr ae::SafeStreamConfig kSafeStreamConfig{
    std::numeric_limits<std::uint16_t>::max(),                // buffer_capacity
    (std::numeric_limits<std::uint16_t>::max() / 2) - 1,      // window_size
    (std::numeric_limits<std::uint16_t>::max() / 2) - 1 - 1,  // max_data_size
    10,                               // max_repeat_count
    std::chrono::milliseconds{1500},  // wait_confirm_timeout
    {},                               // send_confirm_timeout
    std::chrono::milliseconds{400},   // send_repeat_timeout
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
 public:
  explicit Alice(ae::AetherApp& aether_app, ae::Client::ptr client_alice,
                 TimeSynchronizer& time_synchronizer, ae::Uid bobs_uid);

 private:
  void SendMessage();
  void ResponseReceived(ae::DataBuffer const& data_buffer);

  ae::AetherApp* aether_app_;
  ae::Client::ptr client_alice_;
  TimeSynchronizer* time_synchronizer_;
  ae::P2pSafeStream p2pstream_;
  ae::OwnActionPtr<ae::RepeatableTask> interval_sender_;
  ae::Subscription receive_data_sub_;
  ae::MultiSubscription send_subs_;
};

// Bob answers "pong" to each "ping"
class Bob {
 public:
  explicit Bob(ae::AetherApp& aether_app, ae::Client::ptr client_bob,
               TimeSynchronizer& time_synchronizer);

 private:
  void OnNewStream(ae::Uid destination_uid,
                   std::unique_ptr<ae::ByteIStream> message_stream);
  void OnMessageReceived(ae::DataBuffer const& data_buffer);

  ae::AetherApp* aether_app_;
  ae::Client::ptr client_bob_;
  TimeSynchronizer* time_synchronizer_;
  std::unique_ptr<ae::P2pSafeStream> p2pstream_;
  ae::Subscription new_stream_receive_sub_;
  ae::Subscription message_receive_sub_;
};

int main() {
  auto aether_app = ae::AetherApp::Construct(ae::AetherAppContext{});

  std::unique_ptr<Alice> alice;
  std::unique_ptr<Bob> bob;
  TimeSynchronizer time_synchronizer;

  // register or load clients
  auto alice_client = aether_app->aether()->SelectClient(kParentUid, 0);
  auto bob_client = aether_app->aether()->SelectClient(kParentUid, 1);

  auto wait_clients = ae::CumulativeEvent<ae::Client::ptr, 2>{
      [&](auto event, auto status) {
        status.OnResult([&](auto& action) { event = action.client(); })
            .OnError([&]() { aether_app->Exit(1); });
      },
      alice_client->StatusEvent(), bob_client->StatusEvent()};

  // Create a subscription to the Result event
  wait_clients.Subscribe([&](auto const& event) {
    if (aether_app->IsExited()) {
      return;
    }
    auto client_alice = event[0];
    auto client_bob = event[1];
    alice = ae::make_unique<Alice>(*aether_app, std::move(client_alice),
                                   time_synchronizer, client_bob->uid());
    bob = ae::make_unique<Bob>(*aether_app, std::move(client_bob),
                               time_synchronizer);
    // Save the current aether state
    aether_app->domain().SaveRoot(aether_app->aether());
  });

  while (!aether_app->IsExited()) {
    auto next_time = aether_app->Update(ae::Now());
    aether_app->WaitUntil(next_time);
  }
  return aether_app->ExitCode();
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
    : aether_app_{&aether_app},
      client_alice_{std::move(client_alice)},
      time_synchronizer_{&time_synchronizer},
      p2pstream_{*aether_app_, kSafeStreamConfig,
                 ae::make_unique<ae::P2pStream>(*aether_app_, client_alice_,
                                                bobs_uid)},
      interval_sender_{*aether_app_, [this]() { SendMessage(); },
                       std::chrono::milliseconds{5000},
                       ae::RepeatableTask::kRepeatCountInfinite},
      receive_data_sub_{p2pstream_.out_data_event().Subscribe(
          *this, ae::MethodPtr<&Alice::ResponseReceived>{})} {}

void Alice::SendMessage() {
  auto current_time = ae::Now();
  constexpr std::string_view ping_message = "ping";

  time_synchronizer_->SetPingSentTime(current_time);

  std::cout << ae::Format("[{:%H:%M:%S}] Alice sends \"ping\"'\n", ae::Now());
  auto send_action =
      p2pstream_.Write({std::begin(ping_message), std::end(ping_message)});

  // notify about error
  send_subs_.Push(
      send_action->StatusEvent().Subscribe(ae::OnError{[&](auto const&) {
        std::cerr << "ping send error" << '\n';
        aether_app_->Exit(1);
      }}));
}

void Alice::ResponseReceived(ae::DataBuffer const& data_buffer) {
  auto pong_message = std::string_view{
      reinterpret_cast<char const*>(data_buffer.data()), data_buffer.size()};
  std::cout << ae::Format(
      "[{:%H:%M:%S}] Alice received \"{}\" within time {} ms\n", ae::Now(),
      pong_message,
      std::chrono::duration_cast<std::chrono::milliseconds>(
          time_synchronizer_->GetPongDuration())
          .count());
}

Bob::Bob(ae::AetherApp& aether_app, ae::Client::ptr client_bob,
         TimeSynchronizer& time_synchronizer)
    : aether_app_{&aether_app},
      client_bob_{std::move(client_bob)},
      time_synchronizer_{&time_synchronizer},
      new_stream_receive_sub_{
          client_bob_->client_connection()->new_stream_event().Subscribe(
              *this, ae::MethodPtr<&Bob::OnNewStream>{})} {}

void Bob::OnNewStream(ae::Uid destination_uid,
                      std::unique_ptr<ae::ByteIStream> message_stream) {
  p2pstream_ = ae::make_unique<ae::P2pSafeStream>(
      *aether_app_, kSafeStreamConfig,
      ae::make_unique<ae::P2pStream>(*aether_app_, client_bob_, destination_uid,
                                     std::move(message_stream)));
  message_receive_sub_ = p2pstream_->out_data_event().Subscribe(
      *this, ae::MethodPtr<&Bob::OnMessageReceived>{});
}

void Bob::OnMessageReceived(ae::DataBuffer const& data_buffer) {
  auto ping_message = std::string_view{
      reinterpret_cast<char const*>(data_buffer.data()), data_buffer.size()};
  std::cout << ae::Format(
      "[{:%H:%M:%S}] Bob received \"{}\" within time {} ms\n", ae::Now(),
      ping_message,
      std::chrono::duration_cast<std::chrono::milliseconds>(
          time_synchronizer_->GetPingDuration())
          .count());

  time_synchronizer_->SetPongSentTime(ae::Now());
  constexpr std::string_view pong_message = "pong";
  std::cout << ae::Format("[{:%H:%M:%S}] Bob sends \"pong\"\n", ae::Now());
  p2pstream_->Write({std::begin(pong_message), std::end(pong_message)});
}
