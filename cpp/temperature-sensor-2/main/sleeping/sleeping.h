/*
 * Copyright 2026 Aethernet Inc.
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

#ifndef SLEEPING_SLEEPING_H_
#define SLEEPING_SLEEPING_H_

#include <chrono>
#include <cstdint>

using time_point = std::chrono::time_point<std::chrono::system_clock>;

int DeepSleep(time_point soft_sleep_tp, time_point hard_sleep_tp,
              std::int16_t temperature_threshold);
#endif  // SLEEPING_SLEEPING_H_
