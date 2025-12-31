// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "driver/gpio.h"
// #include "esp_log.h"
// #include "led_strip.h"

// #define BLINK_GPIO (gpio_num_t)35   // Internal RGB LED for AtomS3 Lite
// #define BUTTON_GPIO (gpio_num_t)41  // Main Button for AtomS3 Lite

// static const char *TAG = "ATOM_S3_APP";
// static uint32_t blink_delay = 500; // Starting speed (ms)

// // Task to handle the blinking LED
// void led_task(void *pvParameters) {
//     led_strip_handle_t led_strip;
//     led_strip_config_t strip_config = {
//         .strip_gpio_num = BLINK_GPIO,
//         .max_leds = 1, 
//     };
//     led_strip_rmt_config_t rmt_config = {
//         .resolution_hz = 10 * 1000 * 1000, // 10MHz
//     };
    
//     ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

//     bool led_on = false;
//     while (1) {
//         if (led_on) {
//             led_strip_set_pixel(led_strip, 0, 0, 255, 0); // Green
//             led_strip_refresh(led_strip);
//         } else {
//             led_strip_clear(led_strip);
//         }
//         led_on = !led_on;
//         vTaskDelay(pdMS_TO_TICKS(blink_delay));
//     }
// }

// // Task to monitor the button
// void button_task(void *pvParameters) {
//     gpio_config_t io_conf = {
//         .pin_bit_mask = (1ULL << BUTTON_GPIO),
//         .mode = GPIO_MODE_INPUT,
//         .pull_up_en = GPIO_PULLUP_ENABLE,
//     };
//     gpio_config(&io_conf);

//     int last_state = 1;
//     while (1) {
//         int current_state = gpio_get_level(BUTTON_GPIO);
//         if (last_state == 1 && current_state == 0) { // Button Pressed
//             if (blink_delay == 500) blink_delay = 200;
//             else if (blink_delay == 200) blink_delay = 50;
//             else blink_delay = 500;
            
//             ESP_LOGI(TAG, "Frequency changed! Delay now: %ld ms", blink_delay);
//         }
//         last_state = current_state;
//         vTaskDelay(pdMS_TO_TICKS(20)); // Debounce delay
//     }
// }

// extern "C" void app_main(void) {
//     ESP_LOGI(TAG, "Starting LED and Button Demo...");
//     xTaskCreate(led_task, "led_task", 4096, NULL, 5, NULL);
//     xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
// }

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

#if defined ESP_PLATFORM
#  include <freertos/FreeRTOS.h>
#  include <esp_log.h>
#  include <esp_task_wdt.h>
#  include "wifi_provisioning.h"
#endif

extern void setup();
extern void loop();

#if defined ESP_PLATFORM
extern "C" void app_main(void) {
  esp_task_wdt_config_t config_wdt = {
      /*.timeout_ms = */ 60000,
      /*.idle_core_mask = */ 0,  // i.e. do not watch any idle task
      /*.trigger_panic = */ true};

  esp_err_t err = esp_task_wdt_reconfigure(&config_wdt);
  if (err != 0) {
    ESP_LOGE("TEMP_SENSOR_APP", "Reconfigure WDT is failed!");
  }

  if (!WifiProvisioning()) {
    ESP_LOGE("TEMP_SENSOR_APP", "Wifi Provisioning failed!");
    return;
  }

  setup();
  while (1) loop();
}
#else
int main() {
  setup();
  while (1) loop();
}
#endif
