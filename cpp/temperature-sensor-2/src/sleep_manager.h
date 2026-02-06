/*
 * Copyright 2025 Aethernet Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SLEEP_MANAGER_H_
#define SLEEP_MANAGER_H_

#ifdef ESP_PLATFORM

#  define ESP_SLEEP_MANAGER_ENABLED 1

#  include <esp_sleep.h>
#  include <esp_timer.h>
#  include <esp_log.h>
#  include <hal/uart_types.h>
#  include <soc/gpio_num.h>

// Conditional includes based on chip
#  if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || \
      CONFIG_IDF_TARGET_ESP32S3
#    include <driver/touch_pad.h>
#    include <driver/rtc_io.h>
#    include <driver/gpio.h>
#    include <driver/rtc_cntl.h>
#  endif

// Conditional includes based on chip
#  if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || \
      CONFIG_IDF_TARGET_ESP32S3
#    include <driver/touch_pad.h>
#    include <driver/rtc_io.h>
#    include <driver/gpio.h>
#    include <driver/rtc_cntl.h>
#  endif

#  if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || \
      CONFIG_IDF_TARGET_ESP32H2 || CONFIG_IDF_TARGET_ESP32C2
#    include <driver/rtc_io.h>
#    include <driver/gpio.h>
#  endif

#  if CONFIG_IDF_TARGET_ESP32P4
// P4 specific headers if needed
#  endif

namespace ae {
class SleepManager {
 public:
  enum class SleepMode {
    LIGHT_SLEEP,
    DEEP_SLEEP,
    MODEM_SLEEP  // For Wi-Fi/BLE low power modes
  };

  enum class WakeupSource {
    WAKEUP_UNDEFINED,  //!< In case of deep sleep, reset was not caused by exit
                       //!< from deep sleep
    WAKEUP_ALL,    //!< Not a wakeup cause, used to disable all wakeup sources
                   //!< with esp_sleep_disable_wakeup_source
    WAKEUP_EXT0,   //!< Wakeup caused by external signal using RTC_IO
    WAKEUP_EXT1,   //!< Wakeup caused by external signal using RTC_CNTL
    WAKEUP_TIMER,  //!< Wakeup caused by timer
    WAKEUP_TOUCHPAD,  //!< Wakeup caused by touchpad
    WAKEUP_ULP,       //!< Wakeup caused by ULP program
    WAKEUP_GPIO,   //!< Wakeup caused by GPIO (light sleep only on ESP32, S2 and
                   //!< S3)
    WAKEUP_UART,   //!< Wakeup caused by UART (light sleep only)
    WAKEUP_WIFI,   //!< Wakeup caused by WIFI (light sleep only)
    WAKEUP_COCPU,  //!< Wakeup caused by COCPU int
    WAKEUP_COCPU_TRAP_TRIG,  //!< Wakeup caused by COCPU crash
    WAKEUP_BT,               //!< Wakeup caused by BT (light sleep only)
    WAKEUP_VAD,              //!< Wakeup caused by VAD
    WAKEUP_VBAT_UNDER_VOLT,  //!< Wakeup caused by VDD_BAT under
                             //!< voltage.
    WAKEUP_USB
  };

  enum class ChipType {
    ESP32,
    ESP32_D2WD,
    ESP32_S2,
    ESP32_S3,
    ESP32_C3,
    ESP32_C6,
    ESP32_H2,
    ESP32_C2,
    ESP32_P4,
    UNKNOWN
  };

  SleepManager();
  ~SleepManager();

  /**
   * @brief Initialize sleep manager
   */
  esp_err_t Init();

  /**
   * @brief Get chip type
   */
  ChipType GetChipType();

  /**
   * @brief Configure timer wakeup
   * @param time_ms Wakeup time in milliseconds
   */
  esp_err_t EnableTimerWakeup(uint64_t time_us);

  /**
   * @brief Configure touch wakeup (if supported)
   * @param touch_mask Touch channel mask
   */
  esp_err_t EnableTouchWakeup(uint64_t touch_mask = 0);

  /**
   * @brief Configure external wakeup (EXT0 - single GPIO)
   * @param gpio_num GPIO number
   * @param level Wakeup level (0=low, 1=high)
   */
  esp_err_t EnableExt0Wakeup(gpio_num_t gpio_num, int level);

  /**
   * @brief Configure external wakeup (EXT1 - multiple GPIOs)
   * @param mask GPIO mask
   * @param mode Wakeup mode
   */
  esp_err_t EnableExt1Wakeup(uint64_t mask, esp_sleep_ext1_wakeup_mode_t mode);

  /**
   * @brief Configure GPIO wakeup for light sleep
   * @param gpio_num GPIO number
   * @param level Wakeup level
   */
  esp_err_t EnableGpioWakeup(gpio_num_t gpio_num, int level);

  /**
   * @brief Configure UART wakeup (for chips that support it)
   * @param uart_num UART number
   */
  esp_err_t EnableUartWakeup(uart_port_t uart_num);

  /**
   * @brief Enter specified sleep mode
   * @param mode Sleep mode
   * @param preserve_memory Whether to preserve RTC memory
   */
  esp_err_t EnterSleep(SleepMode mode, bool preserve_memory = true);

  /**
   * @brief Get wakeup cause
   */
  WakeupSource GetWakeupCause();

  /**
   * @brief Disable all wakeup sources
   */
  esp_err_t DisableAllWakeupSources();

  /**
   * @brief Set sleep retention for peripherals
   * @param peripheral Peripheral to retain
   * @param retain Whether to retain
   */
  esp_err_t SetPeripheralRetention(uint32_t peripheral, bool retain);

  /**
   * @brief Check if feature is supported by current chip
   * @param feature Feature to check
   */
  bool IsFeatureSupported(WakeupSource feature);

  /**
   * @brief Print chip capabilities
   */
  void PrintChipCapabilities();

  /**
   * @brief Get recommended sleep mode for current configuration
   */
  SleepMode GetRecommendedSleepMode();

  /**
   * @brief Configure wakeup stub (for deep sleep)
   * @param stub Wakeup stub function
   */
  esp_err_t SetWakeStub(esp_deep_sleep_wake_stub_fn_t stub);

  /**
   * @brief Enable wakeup by Bluetooth
   */
  esp_err_t EnableBluetoothWakeup();

  /**
   * @brief Enable wakeup by Wi-Fi
   */
  esp_err_t EnableWifiWakeup();

  /**
   * @brief Configure wakeup from USB (ESP32-S2/S3)
   */
  esp_err_t EnableUsbWakeup();

 private:
  ChipType chip_type;
  uint64_t wakeup_gpio_mask;
  bool rtc_memory_preserved;
  bool peripherals_configured;

  void DetectChipType();
  // Helper function to convert chip type to string
  const char* ChipTypeToString(SleepManager::ChipType type);
  esp_err_t ConfigureChipSpecificSettings();
  esp_err_t ValidateWakeupSource(WakeupSource source);
  esp_err_t EnterLightSleepInternal();
  esp_err_t EnterDeepSleepInternal();
  esp_err_t EnterModemSleepInternal();

  // Chip-specific implementations
  esp_err_t SetupTouchWakeupESP32(uint64_t mask);
  esp_err_t SetupTouchWakeupS2S3(uint64_t mask);
  esp_err_t SetupExtWakeupForC3C6();
  esp_err_t SetupExtWakeupForS2S3();
  esp_err_t SetupP4Specifics();
};
}  // namespace ae
#endif  // ESP_PLATFORM

#endif  // SLEEP_MANAGER_H_
