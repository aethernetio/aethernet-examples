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

#if (BOARD_HAS_ULP == 1) && !defined IS_ULP_COCPU
#  include <stdio.h>

#  include "ulp_main.h"

void ReadSensors(uint16_t* temperature, uint32_t* humidity, uint32_t* pressure,
                 uint32_t* co2, uint32_t* gas_resistance) {
  printf("\n >>> ULP Temperature measured: %.2f°C\n\n",
         (float)ulp_temperature / 1000.0F - 30.0F);

  if (temperature) {
    *temperature = ulp_temperature;
  }
  if (humidity) {
    *humidity = ulp_humidity;
  }
  if (pressure) {
    *pressure = ulp_pressure;
  }
  if (co2) {
    *co2 = ulp_co2;
  }
  if (gas_resistance) {
    *gas_resistance = ulp_gas_resistance;
  }
}
#endif
