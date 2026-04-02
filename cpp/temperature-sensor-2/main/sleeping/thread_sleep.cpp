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

#include "sleeping/sleeping.h"

#include "aether/all.h"

#if THREAD_SLEEP == 1
#  include <thread>

extern void setup();

int DeepSleep(time_point sleep_until) {
  std::this_thread::sleep_until(sleep_until);
  // TODO: how to restart the app, use execv ?
  return 0;
}

#endif
