#!/usr/bin/env python3
#
# Copyright 2025 Aethernet Inc.
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
import re
import stat
import shutil
import argparse
from platform import system
import subprocess

repo_urls = {"Aether":"https://github.com/aethernetio/aether-client-cpp.git",
             "Arduino":"https://github.com/aethernetio/aether-client-arduino-library.git"}

known_projects = ['preregistered', 'selfregistered']

class BaseRunner:
  def clone(self, dir:str, repo_url:str):
    if not os.path.exists(dir):
      subprocess.run(["git", "clone", repo_url, dir], check=True)
      print("The repository {} has been successfully cloned in {}".format(repo_url, dir))

  def install_arduino_lib(self, lib_name:str, repo_url:str):
    lib_dirs = self.get_arduino_lib_dir()
    repo_lib_dir = os.path.join(lib_dirs, lib_name)

    if os.path.exists(repo_lib_dir):
      def remove_readonly(func, path, info):
          os.chmod(path, stat.S_IWRITE)
          func(path)

      shutil.rmtree(repo_lib_dir, onerror=remove_readonly)

    self.clone(repo_lib_dir, repo_url)

  def cmake_config(self, project_dir:str, build_dir:str, *cmake_opts):
    if not os.path.exists(build_dir):
      os.makedirs(build_dir)
    cmake_cmd = self.get_cmake_cmd('-S ' + project_dir, '-B ' + build_dir, *cmake_opts)
    print("cmake_cmd ", cmake_cmd)
    subprocess.run(cmake_cmd, check=True)

  def cmake_build(self, build_dir:str, target:str):
    cmake_cmd = self.get_cmake_cmd('--build', build_dir, '--target', target, '--config', 'Release', '--parallel', str(os.cpu_count()))
    print("cmake_cmd ", cmake_cmd)
    subprocess.run(cmake_cmd, check=True)
    return os.path.join(self.get_cmake_binary_dir(build_dir), target)

  def run_arduino_ide(self, sketch_path) -> subprocess.Popen:
    return None

  def run_vscode(self, path) -> subprocess.Popen:
    return None

  def get_arduino_lib_dir(self) -> str:
    return ""

  def get_cmake_cmd(self, *args) -> list:
    cmd = ['cmake', *args]
    return cmd

  def get_cmake_binary_dir(self, build_dir:str) -> str:
    return build_dir

class LinuxRunner(BaseRunner):
  def run_arduino_ide(self, sketch_path) -> subprocess.Popen:
      return subprocess.Popen(['arduino-ide', sketch_path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

  def run_vscode(self, path) -> subprocess.Popen:
    return subprocess.Popen(['code', path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

  def get_arduino_lib_dir(self):
    return os.path.expanduser("~/Arduino/libraries")

class WindowsRunner(BaseRunner):
  def run_arduino_ide(self, sketch_path) -> subprocess.Popen:
    arduino_path = os.path.expanduser("~/AppData/Local/Programs/arduino-ide/Arduino IDE.exe")
    if not os.path.exists(arduino_path):
      arduino_path='Arduino IDE.exe'
    print("cmd", [arduino_path, sketch_path])
    return subprocess.Popen([arduino_path, sketch_path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

  def run_vscode(self, path) -> subprocess.Popen:
    return subprocess.Popen(['code.cmd', path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

  def get_arduino_lib_dir(self):
    return os.path.expanduser("~/Documents/Arduino/libraries")

  def get_cmake_binary_dir(self, build_dir:str) -> str:
    return os.path.join(build_dir, 'Release')

class MacosRunner(BaseRunner):
  def run_arduino_ide(self, sketch_path) -> subprocess.Popen:
      return subprocess.Popen(['open', '-a', 'Arduino IDE.app', sketch_path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

  def run_vscode(self, path) -> subprocess.Popen:
    return subprocess.Popen(['open', '-a', 'Visual Studio Code.app', path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

  def get_arduino_lib_dir(self):
    return os.path.expanduser("~/Documents/Arduino/libraries")

class IdeRunner:
  def run(self, project_dir:str, platform:str):
    pass

class ArduinoRunner(IdeRunner):
  def __init__(self, runner: BaseRunner):
    self._runner = runner

  def run(self, project_dir:str, platform:str):
    project_name = os.path.split(project_dir)[1]
    sketch_path = os.path.join(project_dir, 'arduino', project_name, project_dir + '.ino')
    print(sketch_path)
    self._process = self._runner.run_arduino_ide(sketch_path)

class VSCodeWorkspaceRunner(IdeRunner):
  def __init__(self, runner: BaseRunner):
    self._runner = runner

  def run(self, project_dir:str, platform:str):
    workspace_path = os.path.join(project_dir, platform, platform + '.code-workspace')
    self._process = self._runner.run_vscode(workspace_path)

class VSCodeRunner(IdeRunner):
  def __init__(self, runner: BaseRunner):
    self._runner = runner

  def run(self, project_dir:str, platform:str):
    self._process = self._runner.run_vscode(project_dir)

class BuilderRunner:
  def build(self, project_dir:str, project_name:str, utm_id:int) -> str:
    return ""
  def run(self, target_path:str):
    pass

class CmakeBuilderRunner(BuilderRunner):
  def __init__(self, runner: BaseRunner):
    self._runner = runner

  def build(self, project_dir:str, project_name:str, utm_id:int) -> str:
    cmake_dir = os.path.join(project_dir, 'cmake')
    build_dir = "build-" + project_name
    self._runner.cmake_config(cmake_dir, build_dir, '-DUTM_ID=' + str(utm_id))
    return self._runner.cmake_build(build_dir, project_name)

  def run(self, target_path:str):
    subprocess.run([target_path], check=True)

class ProjectOpener:
  def __init__(self, runner, project, platform, ide, wifi_ssid, wifi_pass, utm_id):
    self._runner = runner
    if not project:
      self._project = "preregistered"
    else:
      self._project = project.lower()

    if not platform:
      self._platform = "cmake"
    else:
      self._platform = platform.lower()

    if self._platform == 'arduino':
      self._ide = 'arduino'
    elif self._platform == 'platformio':
      self._ide = 'VSCodeWorkspace'
    elif self._platform == 'esp-idf':
      self._ide = 'VSCodeWorkspace'
    else:
      if not ide:
        self._ide = 'vscode'
      else:
        self._ide = ide.lower()

    self._ide_runner = None
    if self._ide == 'arduino':
      self._ide_runner = ArduinoRunner(self._runner)
    elif self._ide == 'VSCodeWorkspace':
      self._ide_runner = VSCodeWorkspaceRunner(self._runner)
    elif self._ide == 'vscode':
      self._ide_runner = VSCodeRunner(self._runner)

    self._builder_runner = None
    if self._platform.lower() == 'cmake':
      self._builder_runner = CmakeBuilderRunner(self._runner)

    self._wifi_ssid = wifi_ssid
    self._wifi_pass = wifi_pass
    if not utm_id:
      self._utm_id = 0
    else:
      self._utm_id = utm_id

  def run(self):
    assert self._project in known_projects

    print("Install Aether lib")

    def clone_aether_client_cpp():
      self._runner.clone('aether-client-cpp', repo_urls['Aether'])

    if self._platform == 'arduino':
      self._runner.install_arduino_lib('Aether', repo_urls['Arduino'])
    else:
      clone_aether_client_cpp()

    if self._project == 'preregistered':
      print("Run register")
      if not os.path.exists('aether-client-cpp'):
        clone_aether_client_cpp()
      reg_tool = self._get_registrator()
      print("Prepare config file ...")
      #make clients registration
      self._prepare_config_file()
      print("Run registrator tool ...")
      self._register(reg_tool)
    elif self._project == 'selfregistered':
      print("Prepare project config ...")
      self._prepare_project_config()

    print("Open project in IDE ...")
    self._start_ide()
    self._build_and_run()

  def _prepare_config_file(self):
    conf_dict = {}
    if self._platform != 'cmake':
      assert self._wifi_ssid, f'Self wifi ssid required for {self._platform} platform'
      assert self._wifi_pass, f'Self wifi pass required for {self._platform} platform'
      conf_dict['wifi_ssid'] = self._wifi_ssid
      conf_dict['wifi_pass'] = self._wifi_pass
    self._modify_input_file(os.path.join('preregistered', 'project_config.ini.in'), os.path.join('build-registrator', 'project_config.ini'), conf_dict)

  def _prepare_project_config(self):
    conf_dict = {}
    if self._platform != 'cmake':
      assert self._wifi_ssid, f'Self wifi ssid required for {self._platform} platform'
      assert self._wifi_pass, f'Self wifi pass required for {self._platform} platform'
      conf_dict['wifi_ssid'] = self._wifi_ssid
      conf_dict['wifi_pass'] = self._wifi_pass
    self._modify_input_file(os.path.join('selfregistered', 'project_config.h.in'), os.path.join('selfregistered', 'project_config.h'), conf_dict)

  def _get_registrator(self):
    print("Build registrator ...")
    # configure registrator
    self._runner.cmake_config(os.path.join('aether-client-cpp', 'tools', 'registrator'), 'build-registrator', '-DAE_DISTILLATION=On', '-DUSER_CONFIG=./preregistered/user_config.h')
    #build registrator
    target_binary = self._runner.cmake_build('build-registrator', 'aether-registrator')
    return target_binary

  def _register(self, reg_tool:str):
    subprocess.run([reg_tool, os.path.join('build-registrator', 'project_config.ini'), os.path.join('preregistered', 'registered_state.h')], check=True)

  def _modify_input_file(self, file_src:str, file_dst:str, value_dict:dict):
      replacement_re = re.compile(r'\@([\w_-]+)\@')
      with open(file_src) as src_file:
        with open(file_dst, 'w') as dst_file:
          for l in src_file:
            m = replacement_re.search(l)
            if m:
              key = m.group(1)
              if key in value_dict:
                l = l.replace('@{}@'.format(key), value_dict[key])
              else:
                l = ''
            dst_file.write(l)

  def _start_ide(self):
    if not self._ide_runner:
      return
    print("Start ide {} for project {}".format(self._ide, self._project))
    if self._project == 'preregistered':
      self._ide_runner.run('preregistered', self._platform)
    elif self._project == 'selfregistered':
      self._ide_runner.run('selfregistered', self._platform)

  def _build_and_run(self):
    if not self._builder_runner:
      return
    print("Build and run project {} for platform {}".format(self._project, self._platform))
    target_path = ''
    if self._project == 'preregistered':
      target_path = self._builder_runner.build('preregistered', 'preregistered', self._utm_id)
    elif self._project == 'selfregistered':
      target_path =  self._builder_runner.build('selfregistered', 'selfregistered', self._utm_id)
    if not target_path:
      print("Failed to build project")
      return
    self._builder_runner.run(target_path)


def open_project(project, platform, ide, wifi_ssid, wifi_pass, utm_id):
  print("open project with parameters: [{}], [{}], [{}], [{}], [{}] [{}]!".format(project, platform, ide, wifi_ssid, wifi_pass, utm_id))

  # Get info about OS
  os_info = system()

  if os_info == 'Windows':
      print("Script runs on Windows")
      runner = WindowsRunner()
  elif os_info == 'Linux':
      print("Script runs on Linux")
      runner = LinuxRunner()
  elif os_info == 'Darwin':
      print("Script runs on macOS")
      runner = MacosRunner()
  else:
      print(f"Unknown OS: {os_info}")
      return

  opener = ProjectOpener(runner, project, platform, ide, wifi_ssid, wifi_pass, utm_id)
  opener.run()

def clean():
  shutil.rmtree('aether-client-cpp', ignore_errors = True)
  shutil.rmtree('build-registrator',ignore_errors = True)
  for p in known_projects:
    shutil.rmtree('build-' + p, ignore_errors = True)
  shutil.rmtree('preregistered/registered_state.h',ignore_errors = True)
  shutil.rmtree('preregistered/CMakeLists.txt',ignore_errors = True)
  shutil.rmtree('selfregistered/CMakeLists.txt',ignore_errors = True)
  shutil.rmtree('selfregistered/project_config.h',ignore_errors = True)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Register Aetger clients and open IDE with project.'
    )
    parser.add_argument('-project',  type=str, help="Project to open {} \'{}\' - is default".format(known_projects, known_projects[0]))
    parser.add_argument('-platform', type=str, help='Platform type (Arduino, cmake, platformio, esp-idf)')
    parser.add_argument('-ide', type=str,  help='IDE type (Arduino, VSCode)')
    parser.add_argument('-ssid', help='Your WiFi SSID')
    parser.add_argument('-password', help='Your WiFi PASS')
    parser.add_argument('-utm_id', help='User Tracking ID, must be an int32 value')
    parser.add_argument('-clean', action='store_true', help='Clear temporary files')

    args = parser.parse_args()
    if args.clean:
      clean()
    else:
      open_project(args.project, args.platform, args.ide, args.ssid, args.password, args.utm_id)
