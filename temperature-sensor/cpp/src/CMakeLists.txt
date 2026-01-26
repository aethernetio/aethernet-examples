# Copyright 2024 Aethernet Inc.
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

cmake_minimum_required(VERSION 3.16.0)

list(APPEND src_list
  "controller.cpp"
  "wifi_provisioning.cpp"
  "sleep_manager.cpp"
  "main.cpp"
)

list(APPEND esp_src_list
  "BME68x_SensorAPI/bme68x.c"
)

if(NOT CM_PLATFORM)
  project("temperature-sensor-app" VERSION "1.0.0" LANGUAGES C CXX)

  add_executable(${PROJECT_NAME} ${src_list})
  target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(${PROJECT_NAME} PRIVATE aether)

  include(GNUInstallDirs)
  install(TARGETS ${PROJECT_NAME}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

else()
  if(CM_PLATFORM STREQUAL "ESP32")
    set(WIFI_SSID "" CACHE STRING "WiFi SSID")
    set(WIFI_PASSWORD "" CACHE STRING "WiFi Password")

    #ESP32 CMake
    idf_component_register(SRCS ${src_list} ${esp_src_list}
      INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
      REQUIRES led_strip
      PRIV_REQUIRES
        aether
        esp_driver_tsens
        esp_driver_gpio
        esp_http_server
        esp_event
        esp_netif
        esp_hw_support
        driver)

    set(TARGET_NAME "${COMPONENT_LIB}")
    if (NOT "${WIFI_SSID}" STREQUAL "")
      target_compile_definitions(${TARGET_NAME} PRIVATE "WIFI_SSID=\"${WIFI_SSID}\"")
    endif()
    if (NOT "${WIFI_PASSWORD}" STREQUAL "")
      target_compile_definitions(${TARGET_NAME} PRIVATE "WIFI_PASSWORD=\"${WIFI_PASSWORD}\"")
    endif()

  else()
    #Other platforms
    message(FATAL_ERROR "Platform ${CM_PLATFORM} is not supported")
  endif()
endif()
