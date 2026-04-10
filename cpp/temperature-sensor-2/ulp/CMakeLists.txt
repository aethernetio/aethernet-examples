# Copyright 2026 Aethernet Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.16)

set(ULP_APP_NAME ulp_main)

project(${ULP_APP_NAME})

list(APPEND main_src
  "main.c"
)

list(APPEND sensors_src
  "${CMAKE_CURRENT_LIST_DIR}/../main/BME68x_SensorAPI/bme68x.c"
  "${CMAKE_CURRENT_LIST_DIR}/../main/sensors/ulp_utils.c"
  "${CMAKE_CURRENT_LIST_DIR}/../main/sensors/random_val.c"
  "${CMAKE_CURRENT_LIST_DIR}/../main/sensors/bme688.c"
  "${CMAKE_CURRENT_LIST_DIR}/../main/sensors/shtc3.c"
  "${CMAKE_CURRENT_LIST_DIR}/../main/sensors/stcc4.c"
  "${CMAKE_CURRENT_LIST_DIR}/../main/sensors/sht45.c"
)

add_executable(${ULP_APP_NAME} ${main_src} ${sensors_src})

add_compile_definitions(ESP_PLATFORM)
add_compile_definitions(BME68X_DO_NOT_USE_FPU)

# Import the ULP project helper functions
include(IDFULPProject)

# Apply default compile options
ulp_apply_default_options(${ULP_APP_NAME})

# Apply default sources provided by the IDF ULP component
ulp_apply_default_sources(${ULP_APP_NAME})

# Add targets for building the binary, as well as the linkerscript which exports ULP shared variables to the main app
ulp_add_build_binary_targets(${ULP_APP_NAME})

target_include_directories(${ULP_APP_NAME} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../main" "${CMAKE_CURRENT_LIST_DIR}/../aether-client-cpp/")

target_compile_definitions(${ULP_APP_NAME} PRIVATE "ULP_MODE=1")
