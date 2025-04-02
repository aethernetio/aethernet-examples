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
import shutil

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
        self.apply_patches(None)
        self.cmake_registrator(None, None)
        self.compile_registrator(None, None)
        self.modify_settings()
        self.register_clients(None, None)
        self.copy_header_file(None, None)
        self.install_arduino_library(None)
        self.open_ide(None)

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
    def apply_patches(self, git_init_command):
        print("Apply patches!")
        try:
            subprocess.run(git_init_command, cwd=self.clone_directory_aether, check=True)
            print("Script git_init.ps1 has been successfully launched!")
        except subprocess.CalledProcessError as e:
            raise NameError("Error when launching Script git_init.ps1: {}".format(e))

    ## Documentation for cmake_registrator function.
    #
    #  Configures CMake build system for the Registrator project.
    #
    #  Raises:
    #      NameError: If CMake configuration fails.
    #
    def cmake_registrator(self, cmake_command, build_directory):
        print("Setting up CMake...")
        try:
            # We run CMake in the specified build directory
            subprocess.run(cmake_command, cwd=build_directory, check=True)
            print("CMake has been successfully launched!")
        except subprocess.CalledProcessError as e:
            raise NameError("Error when launching CMake: {}".format(e))

    ## Documentation for compile_registrator function.
    #
    #  Compiles the project using MSBuild.
    #
    #  Raises:
    #      NameError: If compilation fails.
    #
    def compile_registrator(self, build_command, build_directory):
        print("Building project...")
        try:
            # We specify the configuration (Release or Debug)
            subprocess.run(build_command, cwd=build_directory, check=True)

            print(f"The build has been completed successfully!")
        except subprocess.CalledProcessError as e:
            raise NameError("Error when building the project: {}".format(e))

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
    def register_clients(self, register_command, release_directory):
        print("Register clients...")
        print("The client registration command is {}".format(register_command))
        try:
            # We run CMake in the specified build directory
            subprocess.run(register_command, cwd=release_directory, check=False)
            print("Aether registrator has been successfully launched!")
        except subprocess.CalledProcessError as e:
            raise NameError("Error when launching Aether registrator: {}".format(e))

    ## Documentation for copy_header_file function.
    #
    #  Copies generated header file to appropriate locations.
    #
    #  Raises:
    #      NameError: If file copy operation fails.
    #
    def copy_header_file(self, source_ini_file, destination_ini_file):
        print("Copy header file...")
        print("Source ini file is {}".format(source_ini_file))
        print("Destination ini file is {}".format(destination_ini_file))

        try:
            shutil.copy(source_ini_file, destination_ini_file)
        except PermissionError:
            raise NameError("Permission denied!")
        except OSError as e:
            raise NameError("Error occurred: {e}")

    ## Documentation for install_arduino_library function.
    #
    #  Installs Arduino library if Arduino IDE is selected.
    #
    def install_arduino_library(self, directory_arduino):
        print("Install arduino library...")
        if self.ide == "Arduino":
            print("Installing Arduino Library")
            # The path to the folder where the library will be unpacked
            library_source_directory = self.clone_directory_arduino
            library_destination_directory = os.path.join(directory_arduino, self.library_name)

            if os.path.exists(library_destination_directory):
                shutil.rmtree(library_destination_directory)

            try:
                # Copy the src folder to the dst folder
                shutil.copytree(library_source_directory, library_destination_directory)
                print("Folder {} successfully copied to {}".format(library_source_directory, library_destination_directory))
            except FileExistsError:
                print("Folder {} is already exists. Delete it or choose a different name.".format(library_destination_directory))
            except Exception as e:
                print("Error occurred: {}".format(e))

    ## Documentation for open_ide function.
    #
    #  Opens project in selected IDE.
    #
    #  Raises:
    #      NameError: If IDE launch fails.
    #
    def open_ide(self, command):
        print("Open IDE...")
        print("Command is {}".format(command))
        if self.ide == "VSCode" or self.ide == "Platformio":
            # Checking if the specified folder exists
            if not os.path.isdir(self.project_directory_aether):
                print("Folder {} does not exist.".format(self.project_directory_aether))
                return

            try:
                # Launching VS Code
                subprocess.Popen(command, stdout=None, stderr=None, stdin=None, close_fds=True)
                print("VS Code is running and opened the folder: {}".format(self.project_directory_aether))
            except FileNotFoundError:
                raise NameError("VS Code was not found. Make sure that the 'Code.exe' is available in the PATH.")
            except subprocess.CalledProcessError as e:
                raise NameError("Error when starting VS Code: {}".format(e))
        elif self.ide == "Arduino":
            # Checking if the specified folder exists
            if not os.path.isdir(self.project_directory_arduino):
                print("Folder {} does not exist.".format(self.project_directory_arduino))
                return

            try:
                # Launching Arduino
                subprocess.Popen(command, stdout=None, stderr=None, stdin=None, close_fds=True)
                print("Arduino is running and opened the folder: {}".format(self.project_directory_arduino))
            except FileNotFoundError:
                raise NameError("Arduino was not found. Make sure that the 'Arduino IDE.exe' is available in the PATH.")
            except subprocess.CalledProcessError as e:
                raise NameError("Error when starting Arduino: {}".format(e))