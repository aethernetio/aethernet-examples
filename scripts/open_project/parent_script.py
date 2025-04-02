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
import subprocess

from ini_file_functions import modify_ini_file

class ParentScript:
    def __init__(self, current_directory, repo_urls, ide, architecture, wifi_ssid, wifi_pass):
        self.current_directory = current_directory
        self.repo_urls = repo_urls
        self.ide = ide
        self.wifi_ssid = wifi_ssid
        self.wifi_pass = wifi_pass

        self.architecture_dir = "cmake"
        if architecture=="Risc-V":
            self.architecture_dir = "espressif_riscv"
        elif architecture=="Lx6":
            self.architecture_dir = "xtensa_lx6"

        self.ide_dir = "vscode"
        if ide == "Platformio":
            self.ide_dir = "platformio"

        # Library name
        self.library_name = "Aether"
        # Directories
        self.clone_directory_aether = os.path.join(current_directory, "Aether")
        self.clone_directory_arduino = os.path.join(current_directory, "Arduino")
        self.source_directory = os.path.join(current_directory, "Aether", "projects", "cmake")
        self.project_directory_aether = os.path.join(current_directory, "Aether", "projects", self.architecture_dir,
                                                     self.ide_dir, "aether-client-cpp")
        self.project_directory_arduino = os.path.join(current_directory, "Arduino", "Examples", "Registered")
        # Files
        self.ini_file = os.path.join(current_directory, "Aether", "examples", "registered", "config",
                                     "registered_config.ini")
        self.ini_file_out = os.path.join("config", "file_system_init.h")

## Documentation for run function.
    #
    #  Executes complete project setup workflow.
    #
    #  Workflow:
    #      1. Clones repositories
    #      2. Applies patches
    #      3. Configures CMake
    #      4. Compiles project
    #      5. Modifies settings
    #      6. Registers clients
    #      7. Copies header files
    #      8. Installs Arduino library (if needed)
    #      9. Opens IDE
    #
    #  Raises:
    #      NameError: If any step in the workflow fails.
    #
    def run(self):
        self.clone_repository()
        self.apply_patches()
        self.cmake_registrator()
        self.compile_registrator()
        self.modify_settings()
        self.register_clients()
        self.copy_header_file()
        self.install_arduino_library()
        self.open_ide()

    ## Documentation for clone_repository function.
    #
    #  Clones required git repositories.
    #
    #  Raises:
    #      NameError: If git clone operation fails.
    #
    def clone_repository(self):
        if not os.path.exists(self.clone_directory_aether):
            print("Directory for clone Aether is {}".format(self.clone_directory_aether))
            # Execute git clone
            try:
                subprocess.run(["git", "clone", self.repo_urls["Aether"], self.clone_directory_aether], check=True)
                print("The repository has been successfully cloned in {}".format(self.clone_directory_aether))
            except subprocess.CalledProcessError as e:
                raise NameError("Error when cloning the repository: {}".format(e))

        if self.ide=="Arduino" and not os.path.exists(self.clone_directory_arduino):
            print("Directory for clone Aether is {}".format(self.clone_directory_arduino))
            # Execute git clone
            try:
                subprocess.run(["git", "clone", self.repo_urls["Arduino"], self.clone_directory_arduino], check=True)
                print("The repository has been successfully cloned in {}".format(self.clone_directory_arduino))
            except subprocess.CalledProcessError as e:
                raise NameError("Error when cloning the repository: {}".format(e))

    ## Documentation for apply_patches function.
    #
    #  Applies initial patches using git_init.ps1 script.
    #
    #  Raises:
    #      NameError: If script execution fails.
    #
    def apply_patches(self):
        print("Apply patches!")

    ## Documentation for cmake_registrator function.
    #
    #  Configures CMake build system for the Registrator project.
    #
    #  Raises:
    #      NameError: If CMake configuration fails.
    #
    def cmake_registrator(self):
        print("Setting up CMake...")

    ## Documentation for compile_registrator function.
    #
    #  Compiles the project using MSBuild.
    #
    #  Raises:
    #      NameError: If compilation fails.
    #
    def compile_registrator(self):
        print("Building project...")

    ## Documentation for modify_settings function.
    #
    #  Updates WiFi settings in configuration file.
    #
    #  Raises:
    #      NameError: If configuration modification fails.
    #
    def modify_settings(self):
        print("Modify settings...")
        section = "Aether"
        try:
            parameter = "wifiSsid"
            new_value = self.wifi_ssid
            modify_ini_file(self.ini_file, section, parameter, new_value)
        except ValueError as e:
            raise NameError("Error in the settings modification: {}".format(e))

        try:
            parameter = "wifiPass"
            new_value = self.wifi_pass
            modify_ini_file(self.ini_file, section, parameter, new_value)
        except ValueError as e:
            raise NameError("Error in the settings modification: {}".format(e))

    ## Documentation for register_clients function.
    #
    #  Runs client registration process.
    #
    #  Raises:
    #      NameError: If registration fails.
    #
    def register_clients(self):
        print("Register clients...")

    ## Documentation for copy_header_file function.
    #
    #  Copies generated header file to appropriate locations.
    #
    #  Raises:
    #      NameError: If file copy operation fails.
    #
    def copy_header_file(self):
        print("Copy header file...")

    ## Documentation for install_arduino_library function.
    #
    #  Installs Arduino library if Arduino IDE is selected.
    #
    def install_arduino_library(self):
        print("Install arduino library...")

    ## Documentation for open_ide function.
    #
    #  Opens project in selected IDE.
    #
    #  Raises:
    #      NameError: If IDE launch fails.
    #
    def open_ide(self):
        print("Open IDE...")