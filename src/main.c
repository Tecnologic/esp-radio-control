// ESP-NOW Radio Control - Main entry point
#include "common.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "main";

// Forward declarations for role-specific start functions
#ifdef APP_ROLE_SENDER
extern void sender_start(void);
#else
extern void receiver_start(void);
#endif

void app_main(void) {
    ESP_LOGI(TAG, "ESP-NOW Radio Control starting...");
    
    // Initialize NVS (required for WiFi)
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Initialize WiFi and ESP-NOW
    common_wifi_init();
    common_espnow_init();
    
    // Start role-specific task
#ifdef APP_ROLE_SENDER
    ESP_LOGI(TAG, "Starting SENDER role");
    sender_start();
#else
    ESP_LOGI(TAG, "Starting RECEIVER role");
    receiver_start();
#endif
}