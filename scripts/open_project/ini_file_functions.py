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

## @package ini_file_functions
#  Documentation for this module.
#
#  More details.


import configparser

from typing import TYPE_CHECKING
if TYPE_CHECKING:
    from _typeshed import SupportsRead, SupportsWrite


## Documentation for modify_ini_file function.
#
#  Modifies a specified parameter in an INI configuration file.
#
#  Args:
#      file_path (str): Path to the INI configuration file.
#      section (str): Name of the section containing the parameter.
#      parameter (str): Name of the parameter to modify.
#      new_value (str): New value to assign to the parameter.
#
#  Raises:
#      NameError: If specified section or parameter doesn't exist.
#
#  Notes:
#      - Preserves case sensitivity of parameters.
#      - Directly overwrites the original file.
#      - Accepts empty strings as valid values.
#
#  Example:
#      >>> modify_ini_file('settings.ini', 'Database', 'port', '5432')
#      Parameter 'port' in the section '[Database]' changed to '5432'.
#
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
        #print(f"Parameter '{parameter}' in the section '[{section}]' changed to '{new_value}'.")
        print("Parameter {} in the section [{}] changed to {}.".format(parameter, section, new_value))
    else:
        raise NameError("Section '[{}]' or parameter '{}' not found in the file.".format(section, parameter))