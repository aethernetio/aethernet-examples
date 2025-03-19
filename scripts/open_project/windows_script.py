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
import configparser

from typing import TYPE_CHECKING
if TYPE_CHECKING:
    from _typeshed import SupportsRead, SupportsWrite


def modify_ini_file(file_path, section, parameter, new_value):
    # Creating an object ConfigParser
    config = configparser.ConfigParser()
    config.optionxform = str

    # Reading a file
    config.read(file_path)

    # Check if there is a section and a parameter.
    if section in config and parameter in config[section]:
        # Changing the parameter value
        config[section][parameter] = new_value

        # Writing the changes back to the file
        with open(file_path, 'w') as configfile: # type: SupportsWrite[str]
            config.write(configfile)
        print(f"Parameter '{parameter}' in the section '[{section}]' changed to '{new_value}'.")
    else:
        #print(f"Section '[{section}]' or parameter '{parameter}' not found in the file.")
        raise NameError(f"Section '[{section}]' or parameter '{parameter}' not found in the file.")


class WindowsScript:
    def __init__(self, current_directory, repo_url, ide, architecture, wifi_ssid, wifi_pass):
        self.current_directory = current_directory
        self.repo_url = repo_url
        self.ide = ide
        self.wifi_ssid = wifi_ssid
        self.wifi_pass = wifi_pass

        arch_dir = "cmake"
        if architecture=="Risc-V":
            arch_dir = "espressif_riscv"
        elif architecture=="Lx6":
            arch_dir = "xtensa_lx6"

        self.clone_directory = os.path.join(current_directory,"Aether")
        self.source_directory = os.path.join(current_directory,"Aether","projects","cmake")
        self.project_folder = os.path.join(current_directory,"Aether","projects",arch_dir,"vscode","aether-client-cpp")
        self.build_directory = "./build"
        self.release_directory = "./build/Release"
        self.registrator_path = "./build/Release/aether-registrator.exe"
        self.ini_file_path = os.path.join(current_directory,"Aether","examples","registered","config","registered_config.ini")


    def run(self):
        if not os.path.exists(self.clone_directory):
            self.clone_repository()
            self.apply_patches()
        self.cmake_registrator()
        self.compile_registrator()
        self.modifi_settings()
        self.register_clients()
        self.copy_header_file()
        self.open_ide()

    def clone_repository(self):
        print(f"Directory for clone is {self.clone_directory}")
        # Execute git clone
        try:
            subprocess.run(["git", "clone", self.repo_url, self.clone_directory], check=True)
            print(f"The repository has been successfully cloned in {self.clone_directory}")
        except subprocess.CalledProcessError as e:
            raise NameError(f"Error when cloning the repository: {e}")

    def apply_patches(self):
        script_path = os.path.join(self.clone_directory, "git_init.ps1")

        # The command to run git_init.ps1
        git_init_command = ["powershell.exe",
                            "-NoProfile",
                            "-File",
                            script_path]
        try:
            subprocess.run(git_init_command, cwd=self.clone_directory, check=True)
            print(f"Script git_init.ps1 has been successfully launched!")
        except subprocess.CalledProcessError as e:
            raise NameError(f"Error when launching Script git_init.ps1: {e}")

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
                         "-DCMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES=\""+self.clone_directory+"\"",
                         self.source_directory]

        try:
            # We run CMake in the specified build directory
            subprocess.run(cmake_command, cwd=self.build_directory, check=True)
            print(f"CMake has been successfully launched!")
        except subprocess.CalledProcessError as e:
            raise NameError(f"Error when launching CMake: {e}")

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
            raise NameError(f"Error when building the project: {e}")

    def modifi_settings(self):
        section = "Aether"
        try:
            parameter = "wifiSsid"
            new_value = self.wifi_ssid
            modify_ini_file(self.ini_file_path, section, parameter, new_value)
        except ValueError as e:
            raise NameError(f"Error in the settings modification:", e)

        try:
            parameter = "wifiPass"
            new_value = self.wifi_pass
            modify_ini_file(self.ini_file_path, section, parameter, new_value)
        except ValueError as e:
            raise NameError(f"Error in the settings modification:", e)

    def register_clients(self):
        # The command to run CMake
        register_command = [self.registrator_path,
                            self.ini_file_path,
                            "config/file_system_init.h"]

        print(register_command)
        try:
            # We run CMake in the specified build directory
            subprocess.run(register_command, cwd=self.release_directory, check=True)
            print(f"Aether registrator has been successfully launched!")
        except subprocess.CalledProcessError as e:
            raise NameError(f"Error when launching Aether registrator: {e}")

    def copy_header_file(self):
        try:
            shutil.copy(self.release_directory+"/config/file_system_init.h",
                        self.clone_directory+"/config/file_system_init.h")
        except PermissionError:
            raise NameError(f"Permission denied!")
        except OSError as e:
            raise NameError(f"Error occurred: {e}")

    def open_ide(self):
        if self.ide == "VSCode":
            # Checking if the specified folder exists
            if not os.path.isdir(self.project_folder):
                print(f"Folder '{self.project_folder}' does not exist.")
                return

            # The command to run VS Code and open the folder
            # The 'code' command should be available in the PATH
            vscode_path = "Code.exe"
            command = [vscode_path, self.project_folder]

            try:
                # Launching VS Code
                subprocess.run(command, check=True)
                print(f"VS Code is running and opened the folder: {self.project_folder}")
            except FileNotFoundError:
                raise NameError("VS Code was not found. Make sure that the 'Code.exe' is available in the PATH.")
            except subprocess.CalledProcessError as e:
                raise NameError(f"Error when starting VS Code: {e}")
