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


## Documentation for a function.
#
#  More details.
class LinuxScript:
    def __init__(self, current_directory, repo_urls, ide, architecture, wifi_ssid, wifi_pass):
        self.current_directory = current_directory
        self.repo_urls = repo_urls
        self.ide = ide
        self.wifi_ssid = wifi_ssid
        self.wifi_pass = wifi_pass

        arch_dir = "cmake"
        if architecture=="Risc-V":
            arch_dir = "espressif_riscv"
        elif architecture=="Lx6":
            arch_dir = "xtensa_lx6"

        ide_dir = "vscode"
        if ide == "Platformio":
            ide_dir = "platformio"

        # Library name
        self.library_name = "Aether"
        # Directories
        self.clone_directory_aether = os.path.join(current_directory,"Aether")
        self.clone_directory_arduino = os.path.join(current_directory,"Arduino")
        self.source_directory = os.path.join(current_directory,"Aether","projects","cmake")
        self.project_directory_aether = os.path.join(current_directory,"Aether","projects",arch_dir,ide_dir,"aether-client-cpp")
        self.project_directory_arduino = os.path.join(current_directory,"Arduino","Examples","Registered")
        self.libraries_directory_arduino = os.path.expanduser("~/Arduino/libraries")
        # Build
        self.build_directory = os.path.join(current_directory,"build")
        self.release_directory = os.path.join(current_directory,"build")
        self.registrator_executable = os.path.join(current_directory,"build","aether-registrator")
        # Files
        self.ini_file = os.path.join(current_directory,"Aether","examples","registered","config","registered_config.ini")
        self.ini_file_out = os.path.join("config","file_system_init.h")

    ## Documentation for a function.
    #
    #  More details.
    def run(self):
        self.clone_repository()
        self.apply_patches()
        self.cmake_registrator()
        self.compile_registrator()
        self.modifi_settings()
        self.register_clients()
        self.copy_header_file()
        self.install_arduino_library()
        self.open_ide()

    ## Documentation for a function.
    #
    #  More details.
    def clone_repository(self):
        if not os.path.exists(self.clone_directory_aether):
            print(f"Directory for clone Aether is {self.clone_directory_aether}")
            # Execute git clone
            try:
                subprocess.run(["git", "clone", self.repo_urls["Aether"], self.clone_directory_aether], check=True)
                print(f"The repository has been successfully cloned in {self.clone_directory_aether}")
            except subprocess.CalledProcessError as e:
                raise NameError(f"Error when cloning the repository: {e}")

        if self.ide=="Arduino" and not os.path.exists(self.clone_directory_arduino):
            print(f"Directory for clone Aether is {self.clone_directory_arduino}")
            # Execute git clone
            try:
                subprocess.run(["git", "clone", self.repo_urls["Arduino"], self.clone_directory_arduino], check=True)
                print(f"The repository has been successfully cloned in {self.clone_directory_arduino}")
            except subprocess.CalledProcessError as e:
                raise NameError(f"Error when cloning the repository: {e}")

    ## Documentation for a function.
    #
    #  More details.
    def apply_patches(self):
        script_path = os.path.join(self.clone_directory_aether, "git_init.sh")

        # The command to run git_init.ps1
        git_init_command = ["sh",
                            script_path]
        try:
            subprocess.run(git_init_command, cwd=self.clone_directory_aether, check=True)
            print(f"Script git_init.sh has been successfully launched!")
        except subprocess.CalledProcessError as e:
            raise NameError(f"Error when launching Script git_init.sh: {e}")

    ## Documentation for a function.
    #
    #  More details.
    def cmake_registrator(self):
        print("Setting up CMake...")
        if os.path.exists(self.build_directory):
            shutil.rmtree(self.build_directory)
        os.makedirs(self.build_directory)

        # The command to run CMake
        cmake_command = ["cmake",
                         "-G",
                         "Ninja",
                         "-DCMAKE_CXX_STANDARD=17",
                         "-DCMAKE_CXX_STANDARD_REQUIRED=ON",
                         "-DUSER_CONFIG=../config/user_config_hydrogen.h",
                         "-DFS_INIT=../../../../config/file_system_init.h",
                         "-DAE_DISTILLATION=On",
                         "-DCMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES=\""+self.clone_directory_aether+"\"",
                         self.source_directory]

        try:
            # We run CMake in the specified build directory
            subprocess.run(cmake_command, cwd=self.build_directory, check=True)
            print(f"CMake has been successfully launched!")
        except subprocess.CalledProcessError as e:
            raise NameError(f"Error when launching CMake: {e}")

    ## Documentation for a function.
    #
    #  More details.
    def compile_registrator(self):
        print("Building project...")
        # The command to build a project using Linux make
        linux_command = [
            "ninja"
        ]

        try:
            # We specify the configuration (Release or Debug)
            subprocess.run(linux_command, cwd=self.build_directory, check=True)

            print(f"The build has been completed successfully!")
        except subprocess.CalledProcessError as e:
            raise NameError(f"Error when building the project: {e}")

    ## Documentation for a function.
    #
    #  More details.
    def modifi_settings(self):
        section = "Aether"
        try:
            parameter = "wifiSsid"
            new_value = self.wifi_ssid
            modify_ini_file(self.ini_file, section, parameter, new_value)
        except ValueError as e:
            raise NameError(f"Error in the settings modification:", e)

        try:
            parameter = "wifiPass"
            new_value = self.wifi_pass
            modify_ini_file(self.ini_file, section, parameter, new_value)
        except ValueError as e:
            raise NameError(f"Error in the settings modification:", e)

    ## Documentation for a function.
    #
    #  More details.
    def register_clients(self):
        # The command to run CMake
        register_command = [self.registrator_executable,
                            self.ini_file,
                            self.ini_file_out]

        print(register_command)
        try:
            # We run CMake in the specified build directory
            subprocess.run(register_command, cwd=self.release_directory, check=False)
            print(f"Aether registrator has been successfully launched!")
        except subprocess.CalledProcessError as e:
            raise NameError(f"Error when launching Aether registrator: {e}")

    ## Documentation for a function.
    #
    #  More details.
    def copy_header_file(self):
        source_ini_file = os.path.join(self.release_directory, self.ini_file_out)
        destination_ini_file = os.path.join(self.clone_directory_aether, self.ini_file_out)
        if self.ide == "Arduino":
            destination_ini_file = os.path.join(self.clone_directory_arduino, "src", self.ini_file_out)

        try:
            shutil.copy(source_ini_file, destination_ini_file)
        except PermissionError:
            raise NameError(f"Permission denied!")
        except OSError as e:
            raise NameError(f"Error occurred: {e}")

    ## Documentation for a function.
    #
    #  More details.
    def install_arduino_library(self):
        if self.ide == "Arduino":
            print(f"Installing Arduino Library")
            # The path to the folder where the library will be unpacked
            library_source_directory = self.clone_directory_arduino
            library_destination_directory = os.path.join(self.libraries_directory_arduino, self.library_name)

            try:
                # Copy the src folder to the dst folder
                shutil.copytree(library_source_directory, library_destination_directory)
                print(f"Folder {library_source_directory} successfully copied to {library_destination_directory}")
            except FileExistsError:
                print(f"Folder {library_destination_directory} is already exists. Delete it or choose a different name.")
            except Exception as e:
                print(f"Error occurred: {e}")

    ## Documentation for a function.
    #
    #  More details.
    def open_ide(self):
        if self.ide == "VSCode" or self.ide == "Platformio":
            # Checking if the specified folder exists
            if not os.path.isdir(self.project_directory_aether):
                print(f"Folder '{self.project_directory_aether}' does not exist.")
                return

            # The command to run VS Code and open the folder
            # The 'code' command should be available in the PATH
            vscode_path = "code"
            command = [vscode_path,
                       "--no-sandbox",
                       self.project_directory_aether
            ]

            try:
                # Launching VS Code
                subprocess.run(command, check=True)
                print(f"VS Code is running and opened the folder: {self.project_directory_aether}")
            except FileNotFoundError:
                raise NameError("VS Code was not found. Make sure that the 'Code.exe' is available in the PATH.")
            except subprocess.CalledProcessError as e:
                raise NameError(f"Error when starting VS Code: {e}")
        elif self.ide == "Arduino":
            # Checking if the specified folder exists
            if not os.path.isdir(self.project_directory_arduino):
                print(f"Folder '{self.project_directory_arduino}' does not exist.")
                return

            # The command to run Arduino and open the folder
            # The 'code' command should be available in the PATH
            arduino_path = "Arduino"
            command = [arduino_path, self.project_directory_arduino]
            print(command)
            try:
                # Launching Arduino
                subprocess.run(command, check=True)
                print(f"Arduino is running and opened the folder: {self.project_directory_arduino}")
            except FileNotFoundError:
                raise NameError("Arduino was not found. Make sure that the 'Arduino IDE.exe' is available in the PATH.")
            except subprocess.CalledProcessError as e:
                raise NameError(f"Error when starting Arduino: {e}")