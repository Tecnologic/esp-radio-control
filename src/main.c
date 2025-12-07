// ESP-NOW Radio Control - Unified sender/receiver with webserver config
#include "common.h"
#include "settings.h"
#include "webserver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "main";

// Forward declarations
extern void sender_start(const uint8_t *peer_mac);
extern void sender_stop(void);
extern void receiver_start(void);
extern void receiver_stop(void);

static device_settings_t current_settings;
static bool is_running = false;
static TaskHandle_t control_task_handle = NULL;

static void led_init(void) {
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
}

static void led_set(bool on) {
    gpio_set_level(PIN_LED, on ? 1 : 0);
}

static void button_init(void) {
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_USER_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
}

static void control_task(void *arg) {
    uint32_t button_press_count = 0;
    uint32_t button_press_start = 0;

    while (1) {
        // Check user button
        if (gpio_get_level(PIN_USER_BUTTON) == 0) {
            // Button pressed
            if (button_press_count == 0) {
                button_press_start = xTaskGetTickCount();
            }
            button_press_count++;

            // Long press: start webserver
            if (button_press_count > 300) { // ~3 seconds at 10ms poll
                if (!webserver_is_running()) {
                    ESP_LOGI(TAG, "Starting webserver...");
                    // Pause active role
                    if (is_running) {
                        if (current_settings.device_role == ROLE_SENDER) {
                            sender_stop();
                        } else {
                            receiver_stop();
                        }
                        is_running = false;
                    }
                    webserver_start(&current_settings);
                    led_set(true); // LED on during config
                }
                button_press_count = 0;
            }
        } else {
            // Button released
            if (button_press_count > 0 && button_press_count < 300) {
                // Short press: toggle role
                ESP_LOGI(TAG, "Short press: toggling role");
                if (webserver_is_running()) {
                    webserver_stop();
                    led_set(false);
                }
                current_settings.device_role = (current_settings.device_role == ROLE_SENDER) ? ROLE_RECEIVER : ROLE_SENDER;
                settings_save(&current_settings);
            }
            button_press_count = 0;
        }

        // Start/stop appropriate role if not in config mode
        if (!webserver_is_running()) {
            if (!is_running) {
                ESP_LOGI(TAG, "Starting %s", 
                         current_settings.device_role == ROLE_SENDER ? "SENDER" : "RECEIVER");
                if (current_settings.device_role == ROLE_SENDER) {
                    sender_start(current_settings.peer_mac);
                } else {
                    receiver_start();
                }
                is_running = true;
            }
            // LED blinks to indicate active
            led_set(xTaskGetTickCount() % 500 < 250);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "ESP-NOW Radio Control starting...");

    // Initialize NVS and settings
    settings_init();
    settings_load(&current_settings);

    // Initialize WiFi and ESP-NOW
    common_wifi_init();
    common_espnow_init();

    // Initialize hardware
    led_init();
    button_init();

    // Start control task
    xTaskCreate(control_task, "control", 4096, NULL, 5, &control_task_handle);

    ESP_LOGI(TAG, "Initialization complete. Device role: %s", 
             current_settings.device_role == ROLE_SENDER ? "SENDER" : "RECEIVER");
}