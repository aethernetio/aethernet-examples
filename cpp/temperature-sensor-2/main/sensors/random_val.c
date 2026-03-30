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

#ifdef TEMP_SENSOR_RANDON
#  ifndef IS_ULP_COCPU
#    include <stdlib.h>
#    include <stdio.h>
#    include <time.h>
#  endif

void ReadSensors(uint32_t* temperature, uint32_t* humidity, uint32_t* pressure,
                 uint32_t* co2, uint32_t* gas_resistance) {
#  ifndef IS_ULP_COCPU
  // get random value as temperature
  srand(time(NULL));

  static uint32_t last_value = 50000;  // value *1000
  // get diff in range -2 to 2
  int32_t diff = (rand() % 4000) - 2000;
  uint32_t value = last_value += diff;
  printf("\n >>>  RND Temperature measured: %d°C/1000 - 30 \n\n", value);
#  else
  static uint32_t last_value = 51100;  // 21.1 °C
  uint32_t value = last_value += 1100;
  if (last_value > 79000) {
    last_value = 100;  // -29.9 °C
  }
#  endif

  if (temperature) {
    *temperature = value;
  }
}

#endif
