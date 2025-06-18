@echo off
REM Copyright 2025 Aethernet Inc.
REM
REM Licensed under the Apache License, Version 2.0 (the "License");
REM you may not use this file except in compliance with the License.
REM You may obtain a copy of the License at
REM
REM     http://www.apache.org/licenses/LICENSE-2.0
REM
REM Unless required by applicable law or agreed to in writing, software
REM distributed under the License is distributed on an "AS IS" BASIS,
REM WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
REM See the License for the specific language governing permissions and
REM limitations under the License.

REM Set UTM_ID to %1 if it provided
if "%~1" == "" if [%1] == [] goto :DefaultUtmId
set UTM_ID=%1
goto :StartBuild
:DefaultUtmId
set UTM_ID=0

:StartBuild

git submodule update --init --remote aether-client-cpp
mkdir build-example
cd build-example
cmake -DUTM_ID=%UTM_ID% ..
cmake --build . --parallel --config Release
cd Release

echo "Get a reference ping to the Aethernet infrastructure"
ping cloud.aethernet.io

echo "Run example"
ping-pong-example.exe
