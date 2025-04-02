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

from parent_script import ParentScript

## Documentation for LinuxScript class.
#
#  Class for automating project setup and configuration on Linux systems.
#  Handles repository cloning, building, configuration and IDE setup.
#
class LinuxScript(ParentScript):
    ## Documentation for __init__ function.
    #
    #  Initializes WindowsScript with project configuration parameters.
    #
    #  Args:
    #      current_directory (str): Base directory for project setup.
    #      repo_urls (dict): URLs for repositories to clone.
    #      ide (str): Target IDE ("VSCode", "Platformio" or "Arduino").
    #      architecture (str): Target architecture ("Risc-V", "Lx6" or default).
    #      wifi_ssid (str): Wi-Fi network name for configuration.
    #      wifi_pass (str): Wi-Fi password for configuration.
    #
    def __init__(self, current_directory, repo_urls, ide, architecture, wifi_ssid, wifi_pass):
        super().__init__(current_directory, repo_urls, ide, architecture, wifi_ssid, wifi_pass)

        # Directories
        self.libraries_directory_arduino = os.path.expanduser("~/Arduino/libraries")
        # Build
        self.build_directory = os.path.join(current_directory,"build")
        self.release_directory = os.path.join(current_directory,"build")
        self.registrator_executable = os.path.join(current_directory,"build","aether-registrator")

    ## Documentation for apply_patches function.
    #
    #  Applies initial patches using git_init.ps1 script.
    #
    #  Raises:
    #      NameError: If script execution fails.
    #
    def apply_patches(self):
        script_path = os.path.join(self.clone_directory_aether, "git_init.sh")

        # The command to run git_init.ps1
        git_init_command = ["sh",
                            script_path]
        try:
            subprocess.run(git_init_command, cwd=self.clone_directory_aether, check=True)
            print("Script git_init.sh has been successfully launched!")
        except subprocess.CalledProcessError as e:
            raise NameError("Error when launching Script git_init.sh: {}".format(e))

    ## Documentation for cmake_registrator function.
    #
    #  Configures CMake build system for the Registrator project.
    #
    #  Raises:
    #      NameError: If CMake configuration fails.
    #
    def cmake_registrator(self):
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
        # The command to build a project using Linux make
        linux_command = [
            "ninja"
        ]

        try:
            # We specify the configuration (Release or Debug)
            subprocess.run(linux_command, cwd=self.build_directory, check=True)

            print("The build has been completed successfully!")
        except subprocess.CalledProcessError as e:
            raise NameError("Error when building the project: {}".format(e))

    ## Documentation for register_clients function.
    #
    #  Runs client registration process.
    #
    #  Raises:
    #      NameError: If registration fails.
    #
    def register_clients(self):
        # The command to run CMake
        register_command = [self.registrator_executable,
                            self.ini_file,
                            self.ini_file_out]

        print("The client registration command is {}".format(register_command))
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
        destination_ini_file = os.path.join(self.clone_directory_aether, self.ini_file_out)
        if self.ide == "Arduino":
            destination_ini_file = os.path.join(self.clone_directory_arduino, "src", self.ini_file_out)

        print("Source ini file is {}".format(source_ini_file))
        print("Destination ini file is {}".format(destination_ini_file))

        try:
            shutil.copy(source_ini_file, destination_ini_file)
        except PermissionError:
            raise NameError("Permission denied!")
        except OSError as e:
            raise NameError("Error occurred: {}".format(e))

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
                print("Error occurred: {e}")

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
                print("Folder {} does not exist.".format(self.project_directory_aether))
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
                subprocess.run(command, check=False)
                print("VS Code is running and opened the folder: {}".format(self.project_directory_aether))
            except FileNotFoundError:
                raise NameError("VS Code was not found. Make sure that the 'Code.exe' is available in the PATH.")
            except subprocess.CalledProcessError as e:
                raise NameError("Error when starting VS Code: {}".format(e))
        elif self.ide == "Arduino":
            # Checking if the specified folder exists
            if not os.path.isdir(self.project_directory_arduino):
                print("Folder '{}' does not exist.".format(self.project_directory_arduino))
                return

            # The command to run Arduino and open the folder
            # The 'code' command should be available in the PATH
            arduino_path = "Arduino"
            command = [arduino_path, self.project_directory_arduino]
            print(command)
            try:
                # Launching Arduino
                subprocess.run(command, check=False)
                print("Arduino is running and opened the folder: {}".format(self.project_directory_arduino))
            except FileNotFoundError:
                raise NameError("Arduino was not found. Make sure that the 'Arduino IDE.exe' is available in the PATH.")
            except subprocess.CalledProcessError as e:
                raise NameError("Error when starting Arduino: {}".format(e))