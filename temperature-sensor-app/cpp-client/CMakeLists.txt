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

# use Release build by default
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")
endif()

project(temp-sensor-client VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# By default, distillation mode is off; turn it on for this example
if (NOT DEFINED AE_DISTILLATION)
  set(AE_DISTILLATION On)
endif()

# add user provided config wich will be included as regular .h file
# Add a user-provided config file, which will be included as a regular .h file
set(USER_CONFIG user_config.h CACHE PATH "" FORCE)
include_directories(${CMAKE_CURRENT_LIST_DIR})

set(APP_ID "" CACHE STRING "Application ID")

# Add the Aether library dependency
add_subdirectory(aether-client-cpp/aether aether)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE client-main.cpp sensor.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE aether)
target_compile_definitions(${PROJECT_NAME} PRIVATE APP_ID=\"${APP_ID}\")
