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
        with open(file_path, 'w') as configfile:
            config.write(configfile)
        print(f"Parameter '{parameter}' in the section '[{section}]' changed to '{new_value}'.")
    else:
        print(f"Section '[{section}]' or parameter '{parameter}' not found in the file.")

class windowsScript:
    def __init__(self, current_directory, repo_url, ide, wifi_ssid, wifi_pass):
        self.current_directory = current_directory
        self.clone_directory = current_directory + "\\Aether"
        self.source_directory = current_directory + "\\Aether\\projects\\Cmake"
        self.build_directory = "./build"
        self.release_directory = "./build/Release"
        self.registrator_path = "./build/Release/aether-registrator.exe"
        self.ini_file_path = current_directory + "\\Aether\\examples\\registered\\config\\registered_config.ini"
        self.repo_url = repo_url
        self.ide = ide
        self.wifi_ssid = wifi_ssid
        self.wifi_pass = wifi_pass

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
        print(self.clone_directory)
        # Execute git clone
        try:
            subprocess.run(["git", "clone", self.repo_url, self.clone_directory], check=True)
            print(f"The repository has been successfully cloned in {self.clone_directory}")
        except subprocess.CalledProcessError as e:
            print(f"Error when cloning the repository: {e}")

    def apply_patches(self):
        script_path = os.path.join(self.clone_directory, "git_init.ps1")
        subprocess.run(["powershell.exe", "-NoProfile", "-File", script_path], cwd=self.clone_directory, check=True)

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
            print(f"Error when launching CMake: {e}")

    def compile_registrator(self):
        print("Building project...")
        # The command to build a project using MSBuild
        msbuild_command = [
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\amd64\\msbuild.exe",
            "ALL_BUILD.vcxproj",  # Building the entire project
            "-p:Configuration=Release"  # We specify the configuration (Release или Debug)
        ]

        try:
            # We specify the configuration (Release or Debug)
            subprocess.run(msbuild_command, cwd=self.build_directory, check=True)

            print(f"The build has been completed successfully!")
        except subprocess.CalledProcessError as e:
            print(f"Error when building the project: {e}")

    def modifi_settings(self):
        section = "Aether"
        parameter = "wifiSsid"
        new_value = self.wifi_ssid
        modify_ini_file(self.ini_file_path, section, parameter, new_value)
        parameter = "wifiPass"
        new_value = self.wifi_pass
        modify_ini_file(self.ini_file_path, section, parameter, new_value)

    def register_clients(self):
        # The command to run CMake
        register_command = [self.registrator_path,
                            self.ini_file_path,
                            "config/file_system_init.h"
                           ]
        print(register_command)
        try:
            # We run CMake in the specified build directory
            subprocess.run(register_command, cwd=self.release_directory, check=True)
            print(f"Aether registrator has been successfully launched!")
        except subprocess.CalledProcessError as e:
            print(f"Error when launching Aether registrator: {e}")

    def copy_header_file(self):
        shutil.copy(self.release_directory+"/config/file_system_init.h", self.clone_directory+"\\config\\file_system_init.h")

    def open_ide(self):
        if self.ide == "VSCode":
            project_folder = self.clone_directory + "\\projects\\espressif_riscv\\vscode\\aether-client-cpp"
            # Checking if the specified folder exists
            if not os.path.isdir(project_folder):
                print(f"Folder '{project_folder}' does not exist.")
                return

            # The command to run VS Code and open the folder
            # The 'code' command should be available in the PATH
            vscode_path = "C:\\Users\\Vise\\AppData\\Local\\Programs\\Microsoft VS Code\\Code.exe"
            command = [vscode_path, project_folder]

            try:
                # Launching VS Code
                subprocess.run(command, check=True)
                print(f"VS Code is running and opened the folder: {project_folder}")
            except FileNotFoundError:
                print("VS Code was not found. Make sure that the 'Code.exe' is available in the PATH.")
            except subprocess.CalledProcessError as e:
                print(f"Error when starting VS Code: {e}")