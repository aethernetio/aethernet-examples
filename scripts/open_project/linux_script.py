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

class LinuxScript:
    def __init__(self, current_directory, repo_url, ide, wifi_ssid, wifi_pass):
        self.current_directory = current_directory
        self.repo_url = repo_url
        self.ide = ide
        self.wifi_ssid = wifi_ssid
        self.wifi_pass = wifi_pass

        self.clone_directory = os.path.join(current_directory,"Aether")
        self.source_directory = os.path.join(current_directory,"Aether","projects","cmake")
        self.project_folder = os.path.join(current_directory,"Aether","projects","espressif_riscv","vscode","aether-client-cpp")
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
        #self.modifi_settings()
        #self.register_clients()
        #self.copy_header_file()
        #self.open_ide()

    def clone_repository(self):
        print(f"Directory for clone is {self.clone_directory}")
        # Execute git clone
        try:
            subprocess.run(["git", "clone", self.repo_url, self.clone_directory], check=True)
            print(f"The repository has been successfully cloned in {self.clone_directory}")
        except subprocess.CalledProcessError as e:
            raise NameError(f"Error when cloning the repository: {e}")

    def apply_patches(self):
        script_path = os.path.join(self.clone_directory, "git_init.sh")

        # The command to run git_init.ps1
        git_init_command = ["sh",
                            script_path]
        try:
            subprocess.run(git_init_command, cwd=self.clone_directory, check=True)
            print(f"Script git_init.sh has been successfully launched!")
        except subprocess.CalledProcessError as e:
            raise NameError(f"Error when launching Script git_init.sh: {e}")

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

        # Go to the build directory
        os.chdir(self.current_directory+"/build")

        # The command to build a project using Linux make
        linux_command = [
            "make"
        ]

        try:
            # We specify the configuration (Release or Debug)
            subprocess.run(linux_command, cwd=self.current_directory+"/build", check=True)

            print(f"The build has been completed successfully!")
        except subprocess.CalledProcessError as e:
            raise NameError(f"Error when building the project: {e}")