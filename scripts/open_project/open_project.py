#!/usr/bin/env python
#
# Copyright 2024-2025 Aethernet Inc.
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

import os
import platform
import argparse

from windows_script import WindowsScript
from linux_script import LinuxScript
from macos_script import MacosScript

import sys

repo_urls = {"Aether":"https://github.com/aethernetio/aether-client-cpp.git",
             "Arduino":"https://github.com/aethernetio/aether-client-arduino-library.git"}


## Documentation for run_library_script function.
#
#  Runs a library script with the given parameters, detecting the OS and executing the appropriate setup.
#
#  Args:
#      script_name (str): Name of the script (for logging purposes).
#      ide (str): Target IDE (e.g., "VSCode", "Arduino", "Platformio").
#      architecture (str): CPU architecture (e.g., "x86", "arm64").
#      wifi_ssid (str): Wi-Fi network name (if required).
#      wifi_pass (str): Wi-Fi password (if required).
#
#  Returns:
#      None: This function does not return anything; it executes OS-specific scripts.
#
#  Raises:
#      NotImplementedError: If the OS is not Windows, Linux, or macOS (Darwin).
#
#  Notes:
#      - Uses platform-specific classes: `WindowsScript`, `LinuxScript`, `MacosScript`.
#      - Prints debug information about the OS and parameters.
#
#  Example:
#      >>> run_library_script("setup_env", "VSCode", "Risc-V", "MyWiFi", "secret")
#      "run script setup_env with parameters: [VSCode], [Risc-V], [MyWiFi], [secret]!"
#      "Script runs on Windows"  # (if OS is Windows)
#
def run_library_script(script_name, ide, architecture, wifi_ssid, wifi_pass):
    print("run script {} with parameters: [{}], [{}], [{}], [{}]!".format(script_name, ide, architecture, wifi_ssid,
                                                                          wifi_pass))

    # Get info about OS
    os_info = platform.system()
    current_directory = os.path.dirname(os.path.realpath(__file__))

    if os_info == 'Windows':
        print("Script runs on Windows")
        win_script = WindowsScript(current_directory, repo_urls, ide, architecture, wifi_ssid, wifi_pass)
        win_script.run()
    elif os_info == 'Linux':
        print("Script runs on Linux")
        lin_script = LinuxScript(current_directory, repo_urls, ide, architecture, wifi_ssid, wifi_pass)
        lin_script.run()
    elif os_info == 'Darwin':
        print("Script runs on macOS")
        mac_script = MacosScript(current_directory, repo_urls, ide, architecture, wifi_ssid, wifi_pass)
        mac_script.run()
    else:
        print(f"Unknown OS: {os_info}")


##
# @mainpage Aether Client Setup
# @section intro Introduction
# Configuration script for Aether clients with support for multiple IDEs and architectures.
#
# @section usage Usage
# Run from the command line with required arguments:
# @code{.sh}
# python script.py IDE ARCH SSID PASS
# @endcode
#
# @section args Command Line Arguments
# @arg IDE - development environment type (Arduino, VSCode, Platformio)
# @arg ARCH - target architecture (ARM, Risc-V, Lx6)
# @arg SSID - WiFi network identifier (SSID)
# @arg PASS - WiFi network password (PASS)
#
# @section example Example
# @code{.sh}
# python setup.py VSCode ARM MyWiFiSSID MyWiFiPASS
# @endcode
if __name__ == "__main__":
    ## @var main_script_name
    # Name of the main executable script
    ## @var main_script_name
    # Name of the main executable script
    main_script_name = sys.argv[0]

    # Command line arguments parsing
    parser = argparse.ArgumentParser(
        description='Register Aetger clients and open IDE with project.'
    )

    ## @var IDE
    # @brief Type of development environment (Arduino/VSCode/Platformio)
    parser.add_argument('IDE', type=str, help='IDE type (Arduino, VSCode, Platformio)')

    ## @var ARCH
    # @brief Target architecture type (ARM/Risc-V/Lx6)
    parser.add_argument('ARCH', type=str, help='ARCHITECTURE type (ARM, Risc-V, Lx6)')

    ## @var SSID
    # @brief WiFi network identifier (SSID)
    parser.add_argument('SSID', type=str, help='Your WiFi SSID')

    ## @var PASS
    # @brief WiFi network password
    parser.add_argument('PASS', type=str, help='Your WiFi PASS')

    args = parser.parse_args()

    # Execute library main script
    run_library_script(
        main_script_name,
        args.IDE,
        args.ARCH,
        args.SSID,
        args.PASS
    )
    