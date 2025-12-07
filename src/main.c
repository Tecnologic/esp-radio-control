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
extern void sender_set_light_states(uint8_t states);
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
        .pin_bit_mask = (1ULL << PIN_USER_BUTTON) | (1ULL << PIN_LIGHT_BTN1) | 
                        (1ULL << PIN_LIGHT_BTN2) | (1ULL << PIN_LIGHT_BTN3) | 
                        (1ULL << PIN_LIGHT_BTN4),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
}

// LED pattern controller for 4 states
static void led_update(void) {
    TickType_t now = xTaskGetTickCount();
    connection_status_t conn_status = get_connection_status();
    bool webserver_active = webserver_is_running();
    
    // 4 states:
    // A: Disconnected (no ESP-NOW packets), Webserver off    -> Double blink (100ms on, 100ms off, 100ms on, 300ms off)
    // B: Connected (receiving packets), Webserver off        -> Slow blink (500ms on, 500ms off)
    // C: Disconnected (no ESP-NOW packets), Webserver on     -> Fast blink (250ms on, 250ms off)
    // D: Connected (receiving packets), Webserver on         -> Triple blink (80ms on, 80ms off x3, 200ms off)
    
    bool led_on = false;
    
    if (!conn_status.connected && !webserver_active) {
        // State A: Double blink (600ms cycle)
        uint32_t cycle = now % 600;
        led_on = (cycle < 100) || (cycle >= 200 && cycle < 300);
    } else if (conn_status.connected && !webserver_active) {
        // State B: Slow blink (1000ms cycle)
        uint32_t cycle = now % 1000;
        led_on = (cycle < 500);
    } else if (!conn_status.connected && webserver_active) {
        // State C: Fast blink (500ms cycle)
        uint32_t cycle = now % 500;
        led_on = (cycle < 250);
    } else {
        // State D: Triple blink (440ms cycle)
        uint32_t cycle = now % 440;
        led_on = (cycle < 80) || (cycle >= 160 && cycle < 240);
    }
    
    led_set(led_on);
}

static void control_task(void *arg) {
    uint32_t button_press_count = 0;
    bool long_press_triggered = false;
    uint8_t light_states = 0; // Bit mask for 4 lights
    uint8_t light_btn_state[4] = {1, 1, 1, 1}; // Track previous state of each light button
    const uint8_t light_pins[4] = {PIN_LIGHT_BTN1, PIN_LIGHT_BTN2, PIN_LIGHT_BTN3, PIN_LIGHT_BTN4};

    while (1) {
        // Check user button for config mode
        if (gpio_get_level(PIN_USER_BUTTON) == 0) {
            // Button pressed
            button_press_count++;

            // Long press: toggle webserver (trigger only once while held)
            if (button_press_count >= 300 && !long_press_triggered) { // ~3 seconds at 10ms poll
                long_press_triggered = true;
                if (webserver_is_running()) {
                    ESP_LOGI(TAG, "Stopping webserver...");
                    webserver_stop();
                } else {
                    ESP_LOGI(TAG, "Starting webserver...");
                    webserver_start(&current_settings);
                }
            }
        } else {
            // Button released
            if (button_press_count > 0 && button_press_count < 300) {
                // Short press: do nothing (removed role toggle)
                ESP_LOGI(TAG, "Short press detected (%lu iterations)", button_press_count);
            }
            button_press_count = 0;
            long_press_triggered = false;
        }

        // Check light buttons for toggle
        for (int i = 0; i < 4; i++) {
            uint8_t btn_level = gpio_get_level(light_pins[i]);
            // Detect falling edge (button press, active-low)
            if (light_btn_state[i] == 1 && btn_level == 0) {
                // Toggle corresponding light bit
                light_states ^= (1 << i);
                ESP_LOGI(TAG, "Light %d toggled, states: 0x%02x", i + 1, light_states);
                // If running as sender, update light states
                if (is_running && current_settings.device_role == ROLE_SENDER) {
                    sender_set_light_states(light_states);
                }
            }
            light_btn_state[i] = btn_level;
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
        } else {
            // Webserver active: stop ESP-NOW operation if it was running
            if (is_running) {
                ESP_LOGI(TAG, "Stopping ESP-NOW (webserver active)");
                if (current_settings.device_role == ROLE_SENDER) {
                    sender_stop();
                } else {
                    receiver_stop();
                }
                is_running = false;
            }
        }

        // Update LED pattern based on connection state and webserver status
        led_update();

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