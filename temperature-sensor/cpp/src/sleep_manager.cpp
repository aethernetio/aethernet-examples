#include <freertos/FreeRTOS.h>

#include "sleep_manager.h"

static const char* TAG = "SleepManager";

SleepManager::SleepManager() 
    : rtc_memory_preserved(false), wakeup_gpio_mask(0) {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
}

SleepManager::~SleepManager() {
    disableAllWakeupSources();
}

void SleepManager::enableTimerWakeup(uint64_t time_us) {
    esp_sleep_enable_timer_wakeup(time_us);
    ESP_LOGI(TAG, "Timer wakeup enabled: %llu us", time_us);
}

#if SOC_TOUCH_SENSOR_SUPPORTED
void SleepManager::enableTouchWakeup(uint64_t touch_mask) {
    if (touch_mask == 0) {
        // Enable all touch channels
        touch_mask = 0xFFF; // ESP32 has 14 touch channels
    }
    esp_sleep_enable_touchpad_wakeup();
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_clear_status();
    ESP_LOGI(TAG, "Touch wakeup enabled, mask: 0x%llx", touch_mask);
}
#endif

#if SOC_PM_SUPPORT_EXT0_WAKEUP
void SleepManager::enableExt0Wakeup(gpio_num_t gpio_num, int level) {
    esp_sleep_enable_ext0_wakeup(gpio_num, level);
    ESP_LOGI(TAG, "EXT0 wakeup enabled on GPIO%d, level: %d", 
             gpio_num, level);
}
#endif
void SleepManager::enableExt1Wakeup(uint64_t mask, 
                                   esp_sleep_ext1_wakeup_mode_t mode) {
    esp_sleep_enable_ext1_wakeup(mask, mode);
    wakeup_gpio_mask = mask;
    ESP_LOGI(TAG, "EXT1 wakeup enabled, mask: 0x%llx, mode: %d", 
             mask, mode);
}

void SleepManager::enableGpioWakeup(gpio_num_t gpio_num, int level) {
    esp_sleep_enable_gpio_wakeup();
    gpio_wakeup_enable(gpio_num, 
                      level ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
    ESP_LOGI(TAG, "GPIO wakeup enabled on GPIO%d, level: %d", 
             gpio_num, level);
}

esp_err_t SleepManager::enterLightSleep() {
    ESP_LOGI(TAG, "Entering light sleep...");
    
    // Configure GPIO for power saving
    ConfigureGpioForSleep();
    
    // Enter light sleep
    esp_err_t ret = esp_light_sleep_start();
    
    // Execute after wakeup
    if (ret == ESP_OK) {
        LogWakeupCause();
    } else {
        ESP_LOGE(TAG, "Light sleep failed: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t SleepManager::enterDeepSleep() {
    ESP_LOGI(TAG, "Entering deep sleep...");
    
    // Configure GPIO for power saving
    ConfigureGpioForSleep();
    
    // Preserve RTC memory settings
    if (rtc_memory_preserved) {
#if SOC_PM_SUPPORT_RTC_SLOW_MEM_PD
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
#endif
#if SOC_PM_SUPPORT_RTC_FAST_MEM_PD
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);
#endif
    }
    
    // Print wakeup information
    printWakeupInfo();
    
    // Enter deep sleep
    esp_deep_sleep_start();
    
    // This line won't execute as the system will reboot
    return ESP_OK;
}

SleepManager::WakeupSource SleepManager::getWakeupCause() {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    
    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER:
            return WakeupSource::TIMER;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            return WakeupSource::TOUCH;
        case ESP_SLEEP_WAKEUP_EXT0:
            return WakeupSource::EXT0;
        case ESP_SLEEP_WAKEUP_EXT1:
            return WakeupSource::EXT1;
        case ESP_SLEEP_WAKEUP_GPIO:
            return WakeupSource::GPIO;
        case ESP_SLEEP_WAKEUP_ULP:
            return WakeupSource::ULP;
        default:
            return WakeupSource::ALL;
    }
}

void SleepManager::disableAllWakeupSources() {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    ESP_LOGI(TAG, "All wakeup sources disabled");
}

void SleepManager::preserveRtcMemory(bool preserve) {
    rtc_memory_preserved = preserve;
    ESP_LOGI(TAG, "RTC memory preserve: %s", 
             preserve ? "enabled" : "disabled");
}

void SleepManager::setPowerDomainRetention(esp_sleep_pd_domain_t pd, 
                                          esp_sleep_pd_option_t option) {
    esp_sleep_pd_config(pd, option);
    ESP_LOGI(TAG, "Power domain %d set to option %d", pd, option);
}

void SleepManager::printWakeupInfo() {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    
    ESP_LOGI(TAG, "=== Sleep Configuration ===");
    ESP_LOGI(TAG, "Wakeup cause: %d", cause);
    
    if (cause == ESP_SLEEP_WAKEUP_EXT1) {
        uint64_t gpio_mask = esp_sleep_get_ext1_wakeup_status();
        ESP_LOGI(TAG, "Wakeup GPIO mask: 0x%llx", gpio_mask);
    } else if (cause == ESP_SLEEP_WAKEUP_TOUCHPAD) {
#if SOC_TOUCH_SENSOR_SUPPORTED
        touch_pad_t touch_pin = esp_sleep_get_touchpad_wakeup_status();
        ESP_LOGI(TAG, "Wakeup touchpad: %d", touch_pin);
#endif
    }
}

void SleepManager::delayedSleep(uint32_t delay_ms) {
    ESP_LOGI(TAG, "Delaying sleep for %lu ms", delay_ms);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

void SleepManager::ConfigureGpioForSleep() {
    // Configure all GPIOs in analog mode for power saving
    for (int i = 0; i < GPIO_NUM_MAX; i++) {
        // Skip wakeup GPIOs
        if (wakeup_gpio_mask & (1ULL << i)) {
            continue;
        }
        
        // For non-wakeup GPIOs, set as input pullup/pulldown or analog mode
        gpio_set_direction((gpio_num_t)i, GPIO_MODE_INPUT);
        gpio_pullup_en((gpio_num_t)i);
        gpio_pulldown_dis((gpio_num_t)i);
    }
}

void SleepManager::LogWakeupCause() {
    WakeupSource cause = getWakeupCause();
    
    switch (cause) {
        case WakeupSource::TIMER:
            ESP_LOGI(TAG, "Woke up by TIMER");
            break;
        case WakeupSource::TOUCH:
            ESP_LOGI(TAG, "Woke up by TOUCH");
            break;
        case WakeupSource::EXT0:
            ESP_LOGI(TAG, "Woke up by EXT0");
            break;
        case WakeupSource::EXT1:
            ESP_LOGI(TAG, "Woke up by EXT1");
            break;
        case WakeupSource::GPIO:
            ESP_LOGI(TAG, "Woke up by GPIO");
            break;
        case WakeupSource::ULP:
            ESP_LOGI(TAG, "Woke up by ULP");
            break;
        default:
            ESP_LOGI(TAG, "Woke up by unknown cause");
            break;
    }
}
