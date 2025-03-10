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

from windows_script import windowsScript

import sys

repo_url = "https://github.com/aethernetio/aether-client-cpp.git"

def run_library_script(script_name, ide, **kwargs):
    wifi_ssid = kwargs.get("SSID", "Unknown")
    wifi_pass = kwargs.get("PASS", "Unknown")
    print("run script %s with parameters %s, %s, %s!" % (script_name, ide, wifi_ssid, wifi_pass))
    # Get info about OS
    os_info = platform.system()
    path = os.path.dirname(os.path.realpath(__file__))
    # Paths components
    components = path.split(os.sep)
    script_cwd = os.sep.join(components[:-1])
    if os_info == 'Windows':
        print("Script runs on Windows")
        current_directory = os.path.dirname(os.path.realpath(__file__))
        win_script = windowsScript(current_directory, repo_url, ide, wifi_ssid, wifi_pass)
        win_script.run()
    elif os_info == 'Linux':
        print("Script runs on Linux")
    elif os_info == 'Darwin':
        print("Script runs on macOS")
    else:
        print(f"Unknown OS: {os_info}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Insufficient Arugments")
        print("Use format open_project.py IDE SSID=your_wifi_ssid PASS=your_wifi_pass")
    script_name = sys.argv[0]
    ide = sys.argv[1]
    run_library_script(script_name, ide, **dict(arg.split('=') for arg in sys.argv[2:]))
    