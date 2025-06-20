#
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
#
cmake_minimum_required(VERSION 3.16)

list(APPEND src_list ../preregistered.cpp)

set(AE_DISTILLATION Off)

# custom user config
set(USER_CONFIG ${CMAKE_CURRENT_LIST_DIR}/../user_config.h CACHE PATH "" FORCE)
# preregistered saved state
set(FS_INIT ${CMAKE_CURRENT_LIST_DIR}/../registered_state.h CACHE PATH "" FORCE)

project(preregistered VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../aether-client-cpp/aether aether)

add_executable(${PROJECT_NAME} ${src_list})
target_link_libraries(${PROJECT_NAME} PRIVATE aether)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/..)
