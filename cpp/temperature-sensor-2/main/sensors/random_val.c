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

#include "sensors/sensors.h"

#include "user_config.h"

#ifdef TEMP_SENSOR_RANDOM
#  ifndef IS_ULP_COCPU
#    include <stdlib.h>
#    include <stdio.h>
#    include <time.h>
#  endif

void ReadSensors(uint16_t* temperature, uint32_t* humidity, uint32_t* pressure,
                 uint32_t* co2, uint32_t* gas_resistance) {
#  ifndef IS_ULP_COCPU
  // get random value as temperature
  srand(time(NULL));

  static uint32_t last_value = 12000;  // 20°C
  // get diff in range -2 to 2
  int32_t diff = (rand() % 400) - 200;
  uint32_t value = last_value += diff;
  printf("\n >>>  RND Temperature measured: %0.2f°C\n\n",
         (float)value / 100.0F - 100.0F);
#  else
  static uint32_t last_value = 12110;  // 21.1 °C
  uint32_t value = last_value += 110;
  if (last_value > 19000) {
    last_value = 9900;  // -1 °C
  }
#  endif

  if (temperature) {
    *temperature = value;
  }
}

#endif
