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

# PASS utm_source hash a first arg
UTM_ID=${1:-0}

# Get the number of processors for linux or mac or use 1
NPROC=$(echo "$(nproc 2> /dev/null || sysctl -n hw.logicalcpu 2> /dev/null || 1)")
echo "NRPOC count is ${NPROC}"

git submodule update --init --remote aether-client-cpp
cd aether-client-cpp
./git_init.sh
cd ../
mkdir build-example
cd build-example
cmake -DUTM_ID=${UTM_ID} ..
cmake --build . --parallel "${NPROC}" --config Release

echo "Get a reference ping to the Aethernet infrastructure"
ping cloud.aethernet.io -c 5
sleep 5

echo "Run example"
./ping-pong-example
