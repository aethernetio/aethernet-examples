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


## Documentation for a function.
#
#  More details.
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