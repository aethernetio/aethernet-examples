#!/bin/bash
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

git submodule update --init --remote aether-client-cpp
cd aether-client-cpp
./git_init.sh
cd ../
mkdir build-example
cd build-example
cmake ..
cmake --build .

echo "Get a reference ping to the Aethernet infrastructure"
ping cloud.aethernet.io -c 5

echo "Run example"
./ping-pong-example
