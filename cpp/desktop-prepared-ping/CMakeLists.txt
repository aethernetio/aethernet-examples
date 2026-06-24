# Copyright 2024 Aethernet Inc.
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

project(prepared-ping-example VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add user provided config wich will be included as regular .h file
# Add a user-provided config file, which will be included as a regular .h file
set(USER_CONFIG user_config.h CACHE PATH "" FORCE)
# ${USER_CONFIG} must be an absolute path or a path to something listed in include directories
include_directories(${CMAKE_CURRENT_LIST_DIR})

include(../cmake/CPM.cmake)
CPMAddPackage(URI "https://github.com/aethernetio/aether-client-cpp.git#main")

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE prepared-ping.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE aether)

set(AETHER_PREPARED_PACKET_DEP
    "${CMAKE_BINARY_DIR}/_deps/aether-client-cpp-src")

set(AETHER_PREPARED_PACKET_PATCH
    "${CMAKE_CURRENT_LIST_DIR}/aether-prepared-packet.patch")

add_custom_target(aether_prepared_packet_patch
    COMMAND "${CMAKE_COMMAND}"
            "-DAETHER_PREPARED_PACKET_DEP=${AETHER_PREPARED_PACKET_DEP}"
            "-DAETHER_PREPARED_PACKET_PATCH=${AETHER_PREPARED_PACKET_PATCH}"
            -P "${CMAKE_CURRENT_LIST_DIR}/apply_aether_prepared_packet_patch.cmake"
    COMMENT "Applying prepared-packet patch to aether-client-cpp")

if(TARGET aether)
  add_dependencies(aether aether_prepared_packet_patch)
endif()

if(TARGET prepared-ping-example)
  add_dependencies(prepared-ping-example aether_prepared_packet_patch)
endif()
