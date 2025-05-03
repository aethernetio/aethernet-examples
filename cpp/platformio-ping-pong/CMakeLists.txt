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

# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16.0)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

idf_build_set_property(CM_PLATFORM "ESP32")

add_compile_definitions(CM_ESP32)

if (NOT FS_INIT)
  set(FS_INIT "file_system_init.h" CACHE PATH "")
endif()

# By default, distillation mode is off; turn it on for this example
if (NOT DEFINED AE_DISTILLATION)
  set(AE_DISTILLATION On)
endif()

# add user provided config wich will be included as regular .h file
# Add a user-provided config file, which will be included as a regular .h file
set(USER_CONFIG "user_config.h" CACHE PATH "" FORCE)
# ${USER_CONFIG} must be an absolute path or a path to something listed in include directories
include_directories(${CMAKE_CURRENT_LIST_DIR}/src)

get_filename_component(configName "${CMAKE_BINARY_DIR}" NAME)

list(APPEND EXTRA_COMPONENT_DIRS "./src"
                                 "${CMAKE_SOURCE_DIR}/.pio/libdeps/${configName}/Aether/aether"
)

include_directories("./src")

project(Example LANGUAGES CXX C ASM)
