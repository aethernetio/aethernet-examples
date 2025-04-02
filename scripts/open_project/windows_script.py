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

from pathlib import PureWindowsPath
from parent_script import ParentScript

## Documentation for WindowsScript class.
#
#  Class for automating project setup and configuration on Windows systems.
#  Handles repository cloning, building, configuration and IDE setup.
#
class WindowsScript(ParentScript):
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
        super().__init__(current_directory, repo_urls, ide, architecture, wifi_ssid, wifi_pass)

        # Directories
        self.libraries_directory_arduino = os.path.expanduser("~/Documents/Arduino/libraries")
        # Build
        self.build_directory = "./build"
        self.release_directory = "./build/Release"
        self.registrator_executable = "./build/Release/aether-registrator.exe"

    ## Documentation for apply_patches function.
    #
    #  Applies initial patches using git_init.ps1 script.
    #
    #  Raises:
    #      NameError: If script execution fails.
    #
    def apply_patches(self, git_init_command):
        script_path = os.path.join(self.clone_directory_aether, "git_init.ps1")

        # The command to run git_init.ps1
        git_init_command = ["powershell.exe",
                            "-NoProfile",
                            "-File",
                            script_path]
        super().apply_patches(git_init_command)


    ## Documentation for cmake_registrator function.
    #
    #  Configures CMake build system for the Registrator project.
    #
    #  Raises:
    #      NameError: If CMake configuration fails.
    #
    def cmake_registrator(self, cmake_command, build_directory):
        build_directory = self.build_directory
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
        super().cmake_registrator(cmake_command, build_directory)



    ## Documentation for compile_registrator function.
    #
    #  Compiles the project using MSBuild.
    #
    #  Raises:
    #      NameError: If compilation fails.
    #
    def compile_registrator(self, build_command, build_directory):
        build_directory = self.build_directory
        # The command to build a project using MSBuild
        build_command = [
            "msbuild.exe",
            "ALL_BUILD.vcxproj",  # Building the entire project
            "-p:Configuration=Release"  # We specify the configuration (Release или Debug)
        ]
        super().compile_registrator(build_command, build_directory)

    ## Documentation for register_clients function.
    #
    #  Runs client registration process.
    #
    #  Raises:
    #      NameError: If registration fails.
    #
    def register_clients(self, register_command, release_directory):
        release_directory = self.release_directory
        source_ini_file = os.path.normpath(self.ini_file_out)
        if os.path.sep == '\\':
            source_ini_file = PureWindowsPath(source_ini_file).as_posix()
        print(source_ini_file)
        # The command to run CMake
        register_command = [self.registrator_executable,
                            self.ini_file,
                            source_ini_file]
        super().register_clients(register_command, release_directory)



    ## Documentation for copy_header_file function.
    #
    #  Copies generated header file to appropriate locations.
    #
    #  Raises:
    #      NameError: If file copy operation fails.
    #
    def copy_header_file(self, source_ini_file, destination_ini_file):
        source_ini_file = os.path.join(self.release_directory, self.ini_file_out)
        source_ini_file = os.path.normpath(source_ini_file)
        if os.path.sep == '\\':
            source_ini_file = PureWindowsPath(source_ini_file).as_posix()

        destination_ini_file = os.path.join(self.clone_directory_aether, self.ini_file_out)
        if self.ide == "Arduino":
            destination_ini_file = os.path.join(self.clone_directory_arduino, "src", self.ini_file_out)

        super().copy_header_file(source_ini_file, destination_ini_file)

    ## Documentation for install_arduino_library function.
    #
    #  Installs Arduino library if Arduino IDE is selected.
    #
    def install_arduino_library(self, directory_arduino):
        directory_arduino = self.libraries_directory_arduino
        super().install_arduino_library(directory_arduino)

    ## Documentation for open_ide function.
    #
    #  Opens project in selected IDE.
    #
    #  Raises:
    #      NameError: If IDE launch fails.
    #
    def open_ide(self, command):
        if self.ide == "VSCode" or self.ide == "Platformio":
            # The command to run VS Code and open the folder
            # The 'code' command should be available in the PATH
            vscode_path = "Code.exe"
            command = [vscode_path,
                       self.project_directory_aether]
        elif self.ide == "Arduino":
            # The command to run Arduino and open the folder
            # The 'code' command should be available in the PATH
            arduino_path = "Arduino IDE.exe"
            command = [arduino_path,
                       self.project_directory_arduino]
        super().open_ide(command)
