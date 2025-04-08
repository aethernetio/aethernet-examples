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

# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16.0)

if (NOT DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

idf_build_set_property(CM_PLATFORM "ESP32")

add_compile_definitions(CM_ESP32)

# custom user config
set(USER_CONFIG ${CMAKE_CURRENT_LIST_DIR}/../user_config.h CACHE PATH "" FORCE)
# preregistered saved state
set(FS_INIT ${CMAKE_CURRENT_LIST_DIR}/../registered_state.h CACHE PATH "" FORCE)

get_filename_component(configName "${CMAKE_BINARY_DIR}" NAME)

list(APPEND EXTRA_COMPONENT_DIRS
  "${CMAKE_CURRENT_LIST_DIR}/preregistered"
  "${CMAKE_CURRENT_LIST_DIR}/../../aether-client-cpp/aether"
)

project(preregistered LANGUAGES CXX C ASM)
