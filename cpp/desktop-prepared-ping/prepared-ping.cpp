#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>
#include <optional>
#include <cstring>
#include <cerrno>
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
#include "aether/prepared_packet/packet_encoder.h"

static constexpr auto kParentUid =
    ae::Uid::FromString("3ac93165-3d37-4970-87a6-fa4ee27744e4");

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
  ae::RcPtr<ae::P2pStream> p2pstream_;
  ae::RepeatableTask<ae::AeContext> interval_sender_;
  ae::Subscription receive_data_sub_;
  ae::MultiSubscription send_subs_;
  std::optional<ae::prepared_packet::PreparedSendMessageBlock> prepared_block_;
};

// Bob answers "pong" to each "ping"
class Bob {
 public:
  explicit Bob(ae::AetherApp& aether_app, ae::Client::ptr client_bob,
               TimeSynchronizer& time_synchronizer);

 private:
  void OnNewStream(ae::RcPtr<ae::P2pStream> message_stream);
  void OnMessageReceived(ae::DataBuffer const& data_buffer);

  ae::AetherApp* aether_app_;
  ae::Client::ptr client_bob_;
  TimeSynchronizer* time_synchronizer_;
  ae::RcPtr<ae::P2pStream> p2pstream_;
  ae::Subscription new_stream_receive_sub_;
  ae::Subscription message_receive_sub_;
};

int main() {
  auto aether_app = ae::AetherApp::Construct(ae::AetherAppContext{});

  std::unique_ptr<Alice> alice;
  std::unique_ptr<Bob> bob;
  TimeSynchronizer time_synchronizer;

  // register or load clients
  auto& bob_select = aether_app->aether()->SelectClient(kParentUid, "Bob");
  bob_select.result_event().Subscribe([&](auto const& bob_res) {
    if (bob_res) {
      bob =
          ae::make_unique<Bob>(*aether_app, bob_res.value(), time_synchronizer);
      auto& alice_select =
          aether_app->aether()->SelectClient(kParentUid, "Alice");
      alice_select.result_event().Subscribe(
          [&, uid = bob_res.value()->uid()](auto const& alice_res) {
            if (alice_res) {
              alice = ae::make_unique<Alice>(*aether_app, alice_res.value(),
                                             time_synchronizer, uid);
              // Save the current aether state
              aether_app->aether().Save();
            } else {
              aether_app->Exit(1);
            }
          });
    } else {
      aether_app->Exit(1);
    }
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
      p2pstream_{
          client_alice_->message_stream_manager().CreateStream(bobs_uid)},
      interval_sender_{*aether_app_, [this]() { SendMessage(); },
                       std::chrono::seconds{5}},
      receive_data_sub_{p2pstream_->out_data_event().Subscribe(
          ae::MethodPtr<&Alice::ResponseReceived>{this})} {}


namespace {

std::string PreparedPacketEndpointTextV0(
    ae::prepared_packet::PreparedUdpEndpoint const& endpoint) {
  char buf[INET6_ADDRSTRLEN]{};

  if (endpoint.version == ae::prepared_packet::PreparedIpVersion::kIpV4) {
    if (::inet_ntop(AF_INET, endpoint.ip.data(), buf, sizeof(buf)) == nullptr) {
      return {};
    }
  } else {
    if (::inet_ntop(AF_INET6, endpoint.ip.data(), buf, sizeof(buf)) == nullptr) {
      return {};
    }
  }

  std::ostringstream ss;
  ss << buf << ":" << endpoint.port;
  return ss.str();
}

bool PreparedPacketSendUdpDatagramV0(
    ae::prepared_packet::PreparedUdpEndpoint const& endpoint,
    ae::DataBuffer const& packet) {
  sockaddr_storage addr{};
  socklen_t addr_len = 0;
  int fd = -1;

  auto endpoint_text = PreparedPacketEndpointTextV0(endpoint);
  if (endpoint_text.empty()) {
    std::cerr << "[prepared-packet] invalid prepared IP endpoint\n";
    return false;
  }

  if (endpoint.version == ae::prepared_packet::PreparedIpVersion::kIpV4) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(endpoint.port);
    std::memcpy(&a.sin_addr, endpoint.ip.data(), 4);

    fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    std::memcpy(&addr, &a, sizeof(a));
    addr_len = sizeof(a);
  } else {
    sockaddr_in6 a{};
    a.sin6_family = AF_INET6;
    a.sin6_port = htons(endpoint.port);
    std::memcpy(&a.sin6_addr, endpoint.ip.data(), 16);

    fd = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    std::memcpy(&addr, &a, sizeof(a));
    addr_len = sizeof(a);
  }

  if (fd < 0) {
    std::cerr << "[prepared-packet] socket() failed: "
              << std::strerror(errno) << "\n";
    return false;
  }

  auto sent = ::sendto(fd, packet.data(), packet.size(), 0,
                       reinterpret_cast<sockaddr const*>(&addr), addr_len);
  auto saved_errno = errno;
  ::close(fd);

  if (sent >= 0 && static_cast<std::size_t>(sent) == packet.size()) {
    std::cout << "[prepared-packet] UDP datagram sent "
              << packet.size() << " bytes to "
              << endpoint_text << "\n";
    return true;
  }

  if (sent < 0) {
    std::cerr << "[prepared-packet] sendto failed: "
              << std::strerror(saved_errno) << "\n";
  } else {
    std::cerr << "[prepared-packet] partial UDP send: "
              << sent << " of " << packet.size() << "\n";
  }

  return false;
}

}  // namespace

void Alice::SendMessage() {
  auto current_time = ae::Now();
  constexpr std::string_view ping_message = "ping";

  time_synchronizer_->SetPingSentTime(current_time);

  if (!prepared_block_) {
    std::cout << ae::Format(
        "[{:%H:%M:%S}] Alice tries to export PreparedSendMessageBlock\n",
        ae::Now());

    if (p2pstream_) {
      prepared_block_ = p2pstream_->ExportPreparedSendMessageBlock(1024);
    }

    if (!prepared_block_) {
      std::cout << ae::Format(
          "[{:%H:%M:%S}] Prepared block is not ready; using legacy Aether path\n",
          ae::Now());

      p2pstream_->Write({std::begin(ping_message), std::end(ping_message)});
      return;
    }

    std::cout << ae::Format(
        "[{:%H:%M:%S}] Alice exported PreparedSendMessageBlock; "
        "dropping full Alice send path\n",
        ae::Now());

    send_subs_.Reset();
    p2pstream_.Reset();
    client_alice_ = {};
  }

  ae::DataBuffer payload{std::begin(ping_message), std::end(ping_message)};
  ae::DataBuffer packet;

  auto result = ae::prepared_packet::EncodePacket(
      *prepared_block_, payload, packet);

  if (!result) {
    if (result.error == ae::prepared_packet::EncodePacketError::kNonceExhausted) {
      std::cerr << "[prepared-packet] nonce range exhausted; "
                << "desktop-v0 needs full Aether reprepare path\n";
    } else {
      std::cerr << "[prepared-packet] EncodePacket failed: "
                << ae::prepared_packet::ToString(result.error)
                << "; desktop-v0 needs legacy Aether fallback\n";
    }
    return;
  }

  std::cout << ae::Format(
      "[{:%H:%M:%S}] Alice sends prepared packet \"ping\"\n",
      ae::Now());

  PreparedPacketSendUdpDatagramV0(prepared_block_->endpoint, packet);
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
          client_bob_->message_stream_manager().new_stream_event().Subscribe(
              ae::MethodPtr<&Bob::OnNewStream>{this})} {}

void Bob::OnNewStream(ae::RcPtr<ae::P2pStream> message_stream) {
  p2pstream_ = std::move(message_stream);
  message_receive_sub_ = p2pstream_->out_data_event().Subscribe(
      ae::MethodPtr<&Bob::OnMessageReceived>{this});
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
}
