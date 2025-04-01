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

## @package windows_script
#  Documentation for this module.
#
#  More details.

import os
import subprocess
import shutil

from pathlib import PureWindowsPath
from ini_file_functions import modify_ini_file

## Documentation for WindowsScript class.
#
#  Class for automating project setup and configuration on Windows systems.
#  Handles repository cloning, building, configuration and IDE setup.
#
class WindowsScript:
    ## Documentation for __init__ function.
    #
    #  Initializes WindowsScript with project configuration parameters.
    #
    #  Args:
    #      current_directory (str): Base directory for project setup.
    #      repo_urls (dict): URLs for repositories to clone.
    #      ide (str): Target IDE ("VSCode", "Platformio" or "Arduino").
    #      architecture (str): Target architecture ("Risc-V", "Lx6" or default).
    #      wifi_ssid (str): WiFi network name for configuration.
    #      wifi_pass (str): WiFi password for configuration.
    #
    def __init__(self, current_directory, repo_urls, ide, architecture, wifi_ssid, wifi_pass):
        self.current_directory = current_directory
        self.repo_urls = repo_urls
        self.ide = ide
        self.wifi_ssid = wifi_ssid
        self.wifi_pass = wifi_pass

        architecture_dir = "cmake"
        if architecture=="Risc-V":
            architecture_dir = "espressif_riscv"
        elif architecture=="Lx6":
            architecture_dir = "xtensa_lx6"

        ide_dir = "vscode"
        if ide == "Platformio":
            ide_dir = "platformio"

        # Library name
        self.library_name = "Aether"
        # Directories
        self.clone_directory_aether = os.path.join(current_directory,"Aether")
        self.clone_directory_arduino = os.path.join(current_directory,"Arduino")
        self.source_directory = os.path.join(current_directory,"Aether","projects","cmake")
        self.project_directory_aether = os.path.join(current_directory,"Aether","projects",architecture_dir,ide_dir,"aether-client-cpp")
        self.project_directory_arduino = os.path.join(current_directory,"Arduino","Examples","Registered")
        self.libraries_directory_arduino = os.path.expanduser("~/Documents/Arduino/libraries")
        # Build
        self.build_directory = "./build"
        self.release_directory = "./build/Release"
        self.registrator_executable = "./build/Release/aether-registrator.exe"
        # Files
        self.ini_file = os.path.join(current_directory,"Aether","examples","registered","config","registered_config.ini")
        self.ini_file_out = os.path.join("config","file_system_init.h")

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
            print(f"Directory for clone Aether is {self.clone_directory_aether}")
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
        script_path = os.path.join(self.clone_directory_aether, "git_init.ps1")

        # The command to run git_init.ps1
        git_init_command = ["powershell.exe",
                            "-NoProfile",
                            "-File",
                            script_path]
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
    def cmake_registrator(self):
        print("Setting up CMake...")
        if os.path.exists(self.build_directory):
            shutil.rmtree(self.build_directory)
        os.makedirs(self.build_directory)

        # The command to run CMake
        cmake_command = ["cmake",
                         "-G",
                         "Visual Studio 17 2022",
                         "-A",
                         "x64",
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
    def compile_registrator(self):
        print("Building project...")
        # The command to build a project using MSBuild
        msbuild_command = [
            "msbuild.exe",
            "ALL_BUILD.vcxproj",  # Building the entire project
            "-p:Configuration=Release"  # We specify the configuration (Release или Debug)
        ]

        try:
            # We specify the configuration (Release or Debug)
            subprocess.run(msbuild_command, cwd=self.build_directory, check=True)

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
        source_ini_file = os.path.normpath(self.ini_file_out)
        if os.path.sep == '\\':
            source_ini_file = PureWindowsPath(source_ini_file).as_posix()
        print(source_ini_file)
        # The command to run CMake
        register_command = [self.registrator_executable,
                            self.ini_file,
                            source_ini_file]

        print(register_command)
        try:
            # We run CMake in the specified build directory
            subprocess.run(register_command, cwd=self.release_directory, check=False)
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
    def copy_header_file(self):
        source_ini_file = os.path.join(self.release_directory, self.ini_file_out)
        source_ini_file = os.path.normpath(source_ini_file)
        if os.path.sep == '\\':
            source_ini_file = PureWindowsPath(source_ini_file).as_posix()

        destination_ini_file = os.path.join(self.clone_directory_aether, self.ini_file_out)
        if self.ide == "Arduino":
            destination_ini_file = os.path.join(self.clone_directory_arduino, "src", self.ini_file_out)

        print("Source ini file is {}".format(source_ini_file))
        print("Destination ini file is {}".format(destination_ini_file))

        try:
            shutil.copy(source_ini_file, destination_ini_file)
        except PermissionError:
            raise NameError(f"Permission denied!")
        except OSError as e:
            raise NameError(f"Error occurred: {e}")

    ## Documentation for install_arduino_library function.
    #
    #  Installs Arduino library if Arduino IDE is selected.
    #
    def install_arduino_library(self):
        if self.ide == "Arduino":
            print("Installing Arduino Library")
            # The path to the folder where the library will be unpacked
            library_source_directory = self.clone_directory_arduino
            library_destination_directory = os.path.join(self.libraries_directory_arduino, self.library_name)

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
    def open_ide(self):
        if self.ide == "VSCode" or self.ide == "Platformio":
            # Checking if the specified folder exists
            if not os.path.isdir(self.project_directory_aether):
                print(f"Folder '{self.project_directory_aether}' does not exist.")
                return

            # The command to run VS Code and open the folder
            # The 'code' command should be available in the PATH
            vscode_path = "Code.exe"
            command = [vscode_path, self.project_directory_aether]

            try:
                # Launching VS Code
                subprocess.run(command, check=True)
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

            # The command to run Arduino and open the folder
            # The 'code' command should be available in the PATH
            arduino_path = "Arduino IDE.exe"
            command = [arduino_path, self.project_directory_arduino]
            print(command)
            try:
                # Launching Arduino
                subprocess.run(command, check=True)
                print("Arduino is running and opened the folder: {}".format(self.project_directory_arduino))
            except FileNotFoundError:
                raise NameError("Arduino was not found. Make sure that the 'Arduino IDE.exe' is available in the PATH.")
            except subprocess.CalledProcessError as e:
                raise NameError("Error when starting Arduino: {}".format(e))