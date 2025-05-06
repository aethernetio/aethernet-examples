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

#include "sensor.h"

#include <random>
#include <iostream>
#include <algorithm>

#include "aether/client_messages/p2p_message_stream.h"

Sensor::SensorReader::SensorReader(ae::ActionContext action_context,
                                   ae::Duration interval)
    : ae::Action<SensorReader>{action_context},
      last_read_{},
      interval_{interval},
      old_value_{15.0F} {}

ae::TimePoint Sensor::SensorReader::Update(ae::TimePoint current_time) {
  if ((current_time - last_read_) >= interval_) {
    last_read_ = current_time;
    Read();
  }
  return last_read_ + interval_;
}

ae::EventSubscriber<void(float)> Sensor::SensorReader::value_changed_event() {
  return value_changed_event_;
}

void Sensor::SensorReader::Read() {
  // randomly generate a new value change
  static std::random_device dev;
  static std::mt19937 rng(dev());
  static std::uniform_int_distribution<std::mt19937::result_type> dist6(0,
                                                                        4000);

  // value changes in range -2 to 2
  float value = (static_cast<float>(dist6(rng)) - 2000) / 1000;
  old_value_ += value;
  old_value_ = std::clamp(old_value_, -100.0F, 100.0F);
  std::cout << "Sensor value read: " << old_value_ << std::endl;
  value_changed_event_.Emit(value);
}

Sensor::Sensor(ae::Aether::ptr const& aether, ae::Client::ptr client,
               ae::Uid application_uid)
    : client_{std::move(client)},
      application_uid_{application_uid},
      sensor_api_{protocol_context_},
      sensor_reader_{*aether->action_processor, std::chrono::seconds{10}},
      send_stream_{ae::make_unique<ae::P2pStream>(*aether->action_processor,
                                                  client_, application_uid_,
                                                  ae::StreamId{0})},
      value_changed_sub_{sensor_reader_.value_changed_event().Subscribe(
          *this, ae::MethodPtr<&Sensor::OnValueChanged>{})} {}

void Sensor::OnValueChanged(float value) {
  auto api_context = ae::ApiContext{protocol_context_, sensor_api_};
  api_context->temperature(value);
  send_stream_->Write(std::move(api_context));
}
