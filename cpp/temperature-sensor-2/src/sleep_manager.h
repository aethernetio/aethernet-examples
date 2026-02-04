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

#pragma once
#include <esp_sleep.h>
#include <esp_timer.h>
#include <driver/rtc_io.h>
#include <driver/gpio.h>
#include <esp_log.h>

namespace ae {  
class SleepManager {
public:
    enum class WakeupSource {
        TIMER,
        TOUCH,
        EXT0,   // RTC_GPIO
        EXT1,   // Multiple RTC_GPIO pins
        ULP,    // ULP coprocessor
        GPIO,   // Light sleep only
        ALL     // All supported wakeup sources
    };

    SleepManager();
    ~SleepManager();

    /**
     * @brief Configure timer wakeup
     * @param time_us Wakeup time in microseconds
     */
    void enableTimerWakeup(uint64_t time_us);

    /**
     * @brief Configure touch wakeup
     * @param touch_mask Touch channel mask
     */
#  if SOC_TOUCH_SENSOR_SUPPORTED
    void enableTouchWakeup(uint64_t touch_mask = 0);
#  endif

    /**
     * @brief Configure external wakeup (EXT0 - single GPIO)
     * @param gpio_num GPIO number
     * @param level Wakeup level (0=low, 1=high)
     */
#  if SOC_PM_SUPPORT_EXT0_WAKEUP
    void enableExt0Wakeup(gpio_num_t gpio_num, int level);
#  endif

    /**
     * @brief Configure external wakeup (EXT1 - multiple GPIOs)
     * @param mask GPIO mask
     * @param mode Wakeup mode
     */
    void enableExt1Wakeup(uint64_t mask, esp_sleep_ext1_wakeup_mode_t mode);

    /**
     * @brief Configure GPIO wakeup (Light sleep only)
     * @param gpio_num GPIO number
     * @param level Wakeup level
     */
    void enableGpioWakeup(gpio_num_t gpio_num, int level);

    /**
     * @brief Enter Light sleep mode
     * @note Program continues execution after wakeup
     */
    esp_err_t enterLightSleep();

    /**
     * @brief Enter Deep sleep mode
     * @note System reboots after wakeup
     */
    esp_err_t enterDeepSleep();

    /**
     * @brief Get wakeup cause
     * @return WakeupSource Wakeup source
     */
    WakeupSource getWakeupCause();

    /**
     * @brief Disable all wakeup sources
     */
    void disableAllWakeupSources();

    /**
     * @brief Preserve RTC memory power
     * @param preserve Whether to preserve
     */
    void preserveRtcMemory(bool preserve);

    /**
     * @brief Configure power domain retention
     * @param pd Power domain
     * @param retain Whether to retain
     */
    void setPowerDomainRetention(esp_sleep_pd_domain_t pd, esp_sleep_pd_option_t option);

    /**
     * @brief Print wakeup information
     */
    void printWakeupInfo();

    /**
     * @brief Delayed sleep entry (for testing or debugging)
     * @param delay_ms Delay time in milliseconds
     */
    void delayedSleep(uint32_t delay_ms);

private:
    bool rtc_memory_preserved;
    uint64_t wakeup_gpio_mask;
    
    void ConfigureGpioForSleep();
    void LogWakeupCause();
};
}  // namespace ae

#endif  // SLEEP_MANAGER_H_
