# Copyright 2025 Aethernet Inc.
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

if (NOT DEFINED AE_DISTILLATION)
  set(AE_DISTILLATION On)
endif()

set(USER_CONFIG "${CMAKE_CURRENT_LIST_DIR}/user_config.h" CACHE PATH "" FORCE)

set(APP_ID "" CACHE STRING "Application ID")

list(APPEND src_list
  ${CMAKE_CURRENT_LIST_DIR}/main.cpp
  ${CMAKE_CURRENT_LIST_DIR}/client-main.cpp
  ${CMAKE_CURRENT_LIST_DIR}/sensor.cpp
)

if (NOT CM_PLATFORM)
  project(temp-sensor-client VERSION 1.0 LANGUAGES C CXX)
  set(TARGET_NAME "${PROJECT_NAME}")

  add_executable(${TARGET_NAME})
  target_sources(${TARGET_NAME} PRIVATE ${src_list})
  target_link_libraries(${TARGET_NAME} PRIVATE aether)
else()
  set(WIFI_SSID "" CACHE STRING "Wifi ssid for microcontrollers")
  set(WIFI_PASS "" CACHE STRING "Wifi password for microcontrollers")

  idf_component_register(SRCS ${src_list}
        INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/..
        PRIV_REQUIRES aether)

  set(TARGET_NAME "${COMPONENT_LIB}")

  if (WIFI_SSID)
    target_compile_definitions(${TARGET_NAME} PRIVATE WIFI_SSID=\"${WIFI_SSID}\")
  endif()
  if (WIFI_PASS)
    target_compile_definitions(${TARGET_NAME} PRIVATE WIFI_PASS=\"${WIFI_PASS}\")
  endif()
endif()

if (APP_ID)
  target_compile_definitions(${TARGET_NAME} PRIVATE APP_ID=\"${APP_ID}\")
endif()
