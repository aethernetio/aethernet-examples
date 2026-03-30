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

cmake_minimum_required(VERSION 3.16.0)

list(APPEND src_list
  "main.cpp"
  "controller.cpp"
)

list(APPEND esp_src_list
  "BME68x_SensorAPI/bme68x.c"
)

list(APPEND sleeping_src
  "sleeping/esp_main_sleep.cpp"
  "sleeping/thread_sleep.cpp"
  "sleeping/ulp_sleep.cpp"
)

list(APPEND sensors_src
  "sensors/ulp_utils.c"
  "sensors/esp_main_utils.c"
  "sensors/bme688.c"
  "sensors/random_val.c"
  "sensors/ulp_saved_val.c"
)

if(NOT CM_PLATFORM)
  project("temperature-sensor-app2" VERSION "1.0.0" LANGUAGES C CXX)

  add_executable(${PROJECT_NAME} ${src_list} ${sleeping_src} ${sensors_src})
  target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(${PROJECT_NAME} PRIVATE aether)
else()
  if(CM_PLATFORM STREQUAL "ESP32")
    set(WIFI_SSID "" CACHE STRING "WiFi SSID")
    set(WIFI_PASSWORD "" CACHE STRING "WiFi Password")

    #ESP32 CMake
    idf_component_register(SRCS ${src_list} ${esp_src_list} ${sleeping_src} ${sensors_src}
      INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
      REQUIRES soc ulp
      PRIV_REQUIRES
        aether
        esp_driver_tsens
        esp_driver_gpio
        esp_http_server
        esp_event
        esp_timer
        esp_netif
        esp_hw_support
        driver)

    #
    # ULP support additions to component CMakeLists.txt.
    #
    # 1. The LP Core app name must be unique (if multiple components use LP Core).
    set(ulp_app_name ulp_${COMPONENT_NAME})
    #
    # 2. Specify all C files.
    #    Files should be placed into a separate directory (in this case, ulp/),
    #    which should not be added to COMPONENT_SRCS.
    set(ulp_lp_core_sources ${ulp_src_list})

    #
    # 3. List all the component source files which include automatically
    #    generated LP Core export file, ${ulp_app_name}.h:
    set(ulp_exp_dep_srcs "main.cpp")

    #
    # 4. Call function to build ULP binary and embed in project using the argument
    #    values above.
    ulp_embed_binary(${ulp_app_name} "${ulp_lp_core_sources}" "${ulp_exp_dep_srcs}")

    set(TARGET_NAME "${COMPONENT_LIB}")
    if (NOT "${WIFI_SSID}" STREQUAL "")
      target_compile_definitions(${TARGET_NAME} PRIVATE "WIFI_SSID=\"${WIFI_SSID}\"")
    endif()
    if (NOT "${WIFI_PASSWORD}" STREQUAL "")
      target_compile_definitions(${TARGET_NAME} PRIVATE "WIFI_PASSWORD=\"${WIFI_PASSWORD}\"")
    endif()

  else()
    #Other platforms
    message(FATAL_ERROR "Platform ${CM_PLATFORM} is not supported")
  endif()
endif()
