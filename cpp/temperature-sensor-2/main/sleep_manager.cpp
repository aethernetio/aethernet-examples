/*
 * Copyright 2026 Aethernet Inc.
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

#include "sleep_manager.h"

#if ESP_SLEEP_MANAGER_ENABLED

#  include <freertos/FreeRTOS.h>

namespace ae {
static const char* TAG = "SleepManager";

SleepManager::SleepManager()
    : chip_type(ChipType::UNKNOWN),
      wakeup_gpio_mask(0),
      rtc_memory_preserved(true),
      peripherals_configured(false) {}

SleepManager::~SleepManager() { DisableAllWakeupSources(); }

esp_err_t SleepManager::Init() {
  DetectChipType();
  esp_err_t ret = ConfigureChipSpecificSettings();
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "SleepManager initialized for %s",
             ChipTypeToString(chip_type));
    PrintChipCapabilities();
  }
  return ret;
}

void SleepManager::DetectChipType() {
#  if CONFIG_IDF_TARGET_ESP32
  chip_type = ChipType::ESP32;
  // Check for D2WD variant (has 2MB embedded flash)
  // This requires checking efuse or specific features
#  elif CONFIG_IDF_TARGET_ESP32S2
  chip_type = ChipType::ESP32_S2;
#  elif CONFIG_IDF_TARGET_ESP32S3
  chip_type = ChipType::ESP32_S3;
#  elif CONFIG_IDF_TARGET_ESP32C3
  chip_type = ChipType::ESP32_C3;
#  elif CONFIG_IDF_TARGET_ESP32C6
  chip_type = ChipType::ESP32_C6;
#  elif CONFIG_IDF_TARGET_ESP32H2
  chip_type = ChipType::ESP32_H2;
#  elif CONFIG_IDF_TARGET_ESP32C2
  chip_type = ChipType::ESP32_C2;
#  elif CONFIG_IDF_TARGET_ESP32P4
  chip_type = ChipType::ESP32_P4;
#  else
  chip_type = ChipType::UNKNOWN;
#  endif
}

esp_err_t SleepManager::ConfigureChipSpecificSettings() {
  esp_err_t ret = ESP_OK;

  switch (chip_type) {
    case ChipType::ESP32:
    case ChipType::ESP32_D2WD:
      // Common ESP32 settings
      break;

    case ChipType::ESP32_S2:
    case ChipType::ESP32_S3:
      ret = SetupExtWakeupForS2S3();
      break;

    case ChipType::ESP32_C3:
    case ChipType::ESP32_C6:
      ret = SetupExtWakeupForC3C6();
      break;

    case ChipType::ESP32_P4:
      ret = SetupP4Specifics();
      break;

    default:
      ESP_LOGW(TAG, "Chip type not fully supported, using generic settings");
      break;
  }

  return ret;
}

esp_err_t SleepManager::EnableTimerWakeup(uint64_t time_us) {
  if (!IsFeatureSupported(WakeupSource::WAKEUP_TIMER)) {
    return ESP_ERR_NOT_SUPPORTED;
  }

  esp_sleep_enable_timer_wakeup(time_us);
  ESP_LOGI(TAG, "Timer wakeup enabled: %llu us", time_us);
  return ESP_OK;
}

esp_err_t SleepManager::EnableExt0Wakeup(gpio_num_t gpio_num, int level) {
  if (!IsFeatureSupported(WakeupSource::WAKEUP_EXT0)) {
    return ESP_ERR_NOT_SUPPORTED;
  }
#  if SOC_PM_SUPPORT_EXT0_WAKEUP
  // Validate GPIO for current chip
  bool valid = false;
  switch (chip_type) {
    case ChipType::ESP32:
    case ChipType::ESP32_D2WD:
      // ESP32: GPIOs 0, 2, 4, 12-15, 25-27, 32-39
      if (gpio_num <= 39) {
        valid = true;
      }
      break;

    case ChipType::ESP32_S2:
    case ChipType::ESP32_S3:
      // S2/S3: Most GPIOs support RTC
      valid = true;
      break;

    case ChipType::ESP32_C3:
    case ChipType::ESP32_C6:
      // C3/C6: GPIOs 0-14 are RTC
      if (gpio_num <= 14) {
        valid = true;
      }
      break;

    default:
      valid = false;
      break;
  }

  if (!valid) {
    ESP_LOGE(TAG, "GPIO %d not valid for EXT0 wakeup on this chip", gpio_num);
    return ESP_ERR_INVALID_ARG;
  }

  esp_sleep_enable_ext0_wakeup(gpio_num, level);
  ESP_LOGI(TAG, "EXT0 wakeup enabled on GPIO%d, level: %d", gpio_num, level);
#  endif
  return ESP_OK;
}

esp_err_t SleepManager::EnableExt1Wakeup(uint64_t mask,
                                         esp_sleep_ext1_wakeup_mode_t mode) {
  if (!IsFeatureSupported(WakeupSource::WAKEUP_EXT1)) {
    return ESP_ERR_NOT_SUPPORTED;
  }
#  if SOC_PM_SUPPORT_EXT0_WAKEUP
  esp_sleep_enable_ext1_wakeup(mask, mode);
  wakeup_gpio_mask = mask;
  ESP_LOGI(TAG, "EXT1 wakeup enabled, mask: 0x%llx", mask);
#  endif
  return ESP_OK;
}

esp_err_t SleepManager::EnableGpioWakeup(gpio_num_t gpio_num, int level) {
  if (!IsFeatureSupported(WakeupSource::WAKEUP_GPIO)) {
    return ESP_ERR_NOT_SUPPORTED;
  }

  gpio_int_type_t intr_type =
      level ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL;

  esp_err_t ret = gpio_wakeup_enable(gpio_num, intr_type);
  if (ret == ESP_OK) {
    esp_sleep_enable_gpio_wakeup();
    ESP_LOGI(TAG, "GPIO wakeup enabled on GPIO%d, level: %d", gpio_num, level);
  }
  return ret;
}

esp_err_t SleepManager::EnterSleep(SleepMode mode, bool preserve_memory) {
  esp_err_t ret = ESP_OK;

  rtc_memory_preserved = preserve_memory;

  switch (mode) {
    case SleepMode::LIGHT_SLEEP:
      ret = EnterLightSleepInternal();
      break;

    case SleepMode::DEEP_SLEEP:
      ret = EnterDeepSleepInternal();
      break;

    case SleepMode::MODEM_SLEEP:
      ret = EnterModemSleepInternal();
      break;

    default:
      ret = ESP_ERR_INVALID_ARG;
      break;
  }

  return ret;
}

SleepManager::WakeupSource SleepManager::GetWakeupCause() {
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  return static_cast<SleepManager::WakeupSource>(cause);
}

esp_err_t SleepManager::DisableAllWakeupSources() {
  // A function for completely disabling all sources of awakening
  esp_err_t ret = ESP_OK;

  // 1. Disable all major wake-up sources via the ESP-IDF API
  ret = esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (ret != ESP_OK) {
    // If the ESP_SLEEP_WAKEUP_ALL macro is not supported,
    // disable each source individually
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TOUCHPAD);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ULP);

#  if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || \
      CONFIG_IDF_TARGET_ESP32S3
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_UART);
#  endif

#  if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32C3 || \
      CONFIG_IDF_TARGET_ESP32S3
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_WIFI);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_BT);
#  endif
  }

  // 2. Disable GPIO wakeup (for light sleep) First,
  // disable all GPIO from wakeup
  for (int i = 0; i < GPIO_NUM_MAX; i++) {
    gpio_wakeup_disable((gpio_num_t)i);
  }

// 3. Disable RTC GPIO pullups/pulldowns to reduce consumption
#  if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 ||   \
      CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3 || \
      CONFIG_IDF_TARGET_ESP32C6
  for (int i = 0; i < GPIO_NUM_MAX; i++) {
    // Checking if GPIO is RTC GPIO
    if (rtc_gpio_is_valid_gpio((gpio_num_t)i)) {
      rtc_gpio_pullup_dis((gpio_num_t)i);
      rtc_gpio_pulldown_dis((gpio_num_t)i);
      rtc_gpio_hold_dis((gpio_num_t)i);
    }
  }
#  endif

  return ESP_OK;
}

bool SleepManager::IsFeatureSupported(WakeupSource feature) {
  switch (feature) {
    case WakeupSource::WAKEUP_TIMER:
      return true;  // All chips support timer

    case WakeupSource::WAKEUP_TOUCHPAD:
      return (
          chip_type == ChipType::ESP32 || chip_type == ChipType::ESP32_D2WD ||
          chip_type == ChipType::ESP32_S2 || chip_type == ChipType::ESP32_S3);

    case WakeupSource::WAKEUP_EXT0:
    case WakeupSource::WAKEUP_EXT1:
      // Most chips support EXT0/EXT1 except some very specific cases
      return (chip_type != ChipType::UNKNOWN);

    case WakeupSource::WAKEUP_GPIO:
      return true;  // All chips support GPIO wakeup for light sleep

    case WakeupSource::WAKEUP_UART:
      return (chip_type == ChipType::ESP32_C3 ||
              chip_type == ChipType::ESP32_C6 ||
              chip_type == ChipType::ESP32_S3);

    case WakeupSource::WAKEUP_USB:
      return (chip_type == ChipType::ESP32_S2 ||
              chip_type == ChipType::ESP32_S3);

    case WakeupSource::WAKEUP_BT:
      return (
          chip_type == ChipType::ESP32 || chip_type == ChipType::ESP32_D2WD ||
          chip_type == ChipType::ESP32_C3 || chip_type == ChipType::ESP32_S3);

    case WakeupSource::WAKEUP_WIFI:
      return (
          chip_type == ChipType::ESP32 || chip_type == ChipType::ESP32_D2WD ||
          chip_type == ChipType::ESP32_C3 || chip_type == ChipType::ESP32_S3);

    default:
      return false;
  }
}

void SleepManager::PrintChipCapabilities() {
  ESP_LOGI(TAG, "=== Chip Capabilities ===");
  ESP_LOGI(TAG, "Chip Type: %d", static_cast<int>(chip_type));
  ESP_LOGI(TAG, "Timer Wakeup: %s",
           IsFeatureSupported(WakeupSource::WAKEUP_TIMER) ? "YES" : "NO");
  ESP_LOGI(TAG, "Touch Wakeup: %s",
           IsFeatureSupported(WakeupSource::WAKEUP_TOUCHPAD) ? "YES" : "NO");
  ESP_LOGI(TAG, "EXT0/EXT1 Wakeup: %s",
           IsFeatureSupported(WakeupSource::WAKEUP_EXT0) ? "YES" : "NO");
  ESP_LOGI(TAG, "UART Wakeup: %s",
           IsFeatureSupported(WakeupSource::WAKEUP_UART) ? "YES" : "NO");
  ESP_LOGI(TAG, "USB Wakeup: %s",
           IsFeatureSupported(WakeupSource::WAKEUP_USB) ? "YES" : "NO");
  ESP_LOGI(TAG, "Bluetooth Wakeup: %s",
           IsFeatureSupported(WakeupSource::WAKEUP_BT) ? "YES" : "NO");
  ESP_LOGI(TAG, "Wi-Fi Wakeup: %s",
           IsFeatureSupported(WakeupSource::WAKEUP_WIFI) ? "YES" : "NO");
}

// Chip-specific implementations
esp_err_t SleepManager::SetupTouchWakeupESP32(uint64_t mask) {
#  if CONFIG_IDF_TARGET_ESP32
  if (mask == 0) {
    mask = 0xFFF;  // All 14 touch channels
  }
  esp_sleep_enable_touchpad_wakeup();
  touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
  touch_pad_clear_status();
  return ESP_OK;
#  else
  return ESP_ERR_NOT_SUPPORTED;
#  endif
}

esp_err_t SleepManager::SetupTouchWakeupS2S3(uint64_t mask) {
#  if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
  esp_sleep_enable_touchpad_wakeup();
  // S2/S3 have different touch API
  return ESP_OK;
#  else
  return ESP_ERR_NOT_SUPPORTED;
#  endif
}

esp_err_t SleepManager::SetupExtWakeupForC3C6() {
#  if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6
  // C3/C6 specific external wakeup configuration
  return ESP_OK;
#  else
  return ESP_ERR_NOT_SUPPORTED;
#  endif
}

esp_err_t SleepManager::SetupExtWakeupForS2S3() {
#  if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
  // S2/S3 specific external wakeup configuration
  return ESP_OK;
#  else
  return ESP_ERR_NOT_SUPPORTED;
#  endif
}

esp_err_t SleepManager::EnterLightSleepInternal() {
  ESP_LOGI(TAG, "Entering light sleep...");

  // Save current configuration
  if (rtc_memory_preserved) {
#  if SOC_PM_SUPPORT_RTC_SLOW_MEM_PD
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
#  endif
#  if SOC_PM_SUPPORT_RTC_FAST_MEM_PD
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);
#  endif
  }

  esp_err_t ret = esp_light_sleep_start();

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Light sleep failed: %s", esp_err_to_name(ret));
  }

  return ret;
}

esp_err_t SleepManager::EnterDeepSleepInternal() {
  ESP_LOGI(TAG, "Entering deep sleep...");

  // Final configuration
  if (rtc_memory_preserved) {
#  if SOC_PM_SUPPORT_RTC_SLOW_MEM_PD
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
#  endif
#  if SOC_PM_SUPPORT_RTC_FAST_MEM_PD
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);
#  endif
  }

  // Enter deep sleep
  esp_err_t ret = esp_deep_sleep_try_to_start();

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Deep sleep failed: %s", esp_err_to_name(ret));
  }

  return ret;  // Never reached
}

esp_err_t SleepManager::EnterModemSleepInternal() {
  // Modem sleep implementation (Wi-Fi/BLE power save)
  // This is chip-specific and may require additional configuration

  ESP_LOGI(TAG, "Entering modem sleep...");

  switch (chip_type) {
    case ChipType::ESP32:
    case ChipType::ESP32_D2WD:
      // ESP32 modem sleep configuration
      break;

    case ChipType::ESP32_C3:
    case ChipType::ESP32_S3:
      // C3/S3 modem sleep configuration
      break;

    default:
      ESP_LOGW(TAG, "Modem sleep not fully implemented for this chip");
      break;
  }

  return ESP_OK;
}

// Helper function to convert chip type to string
const char* SleepManager::ChipTypeToString(SleepManager::ChipType type) {
  switch (type) {
    case SleepManager::ChipType::ESP32:
      return "ESP32";
    case SleepManager::ChipType::ESP32_D2WD:
      return "ESP32-D2WD";
    case SleepManager::ChipType::ESP32_S2:
      return "ESP32-S2";
    case SleepManager::ChipType::ESP32_S3:
      return "ESP32-S3";
    case SleepManager::ChipType::ESP32_C3:
      return "ESP32-C3";
    case SleepManager::ChipType::ESP32_C6:
      return "ESP32-C6";
    case SleepManager::ChipType::ESP32_H2:
      return "ESP32-H2";
    case SleepManager::ChipType::ESP32_C2:
      return "ESP32-C2";
    case SleepManager::ChipType::ESP32_P4:
      return "ESP32-P4";
    default:
      return "UNKNOWN";
  }
}
}  // namespace ae
#endif  // ESP_SLEEP_MANAGER_ENABLED
