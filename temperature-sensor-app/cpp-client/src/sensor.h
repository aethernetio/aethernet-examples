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

#include "aether/all.h"

#include "sensor_api.h"

class Sensor {
  class SensorReader final : public ae::Action<SensorReader> {
   public:
    SensorReader(ae::ActionContext action_context, ae::Duration interval);

    ae::ActionResult Update(ae::TimePoint current_time);

    ae::EventSubscriber<void(float)> value_changed_event();

   private:
    void Read();

    ae::TimePoint last_read_;
    ae::Duration interval_;
    float old_value_;
    ae::Event<void(float value)> value_changed_event_;
  };

 public:
  Sensor(ae::Aether::ptr const& aether, ae::Client::ptr client,
         ae::Uid application_uid);

 private:
  void OnValueChanged(float value);

  ae::Client::ptr client_;
  ae::Uid application_uid_;

  ae::ProtocolContext protocol_context_;
  SensorApi sensor_api_;
  SensorReader sensor_reader_;

  std::unique_ptr<ae::ByteIStream> send_stream_;
  ae::Subscription value_changed_sub_;
};
