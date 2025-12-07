// Shared WiFi/ESP-NOW initialization and utility functions
#include "common.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_event.h"
#include "esp_netif.h"
#include <string.h>

static const char *TAG = "shared";

const uint8_t PEER_BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Connection status tracking
static connection_status_t conn_status = {
    .connected = false,
    .rssi = -120,
    .last_packet = 0
};

connection_status_t get_connection_status(void) {
    return conn_status;
}

void update_connection_status(bool connected, int8_t rssi) {
    conn_status.connected = connected;
    conn_status.rssi = rssi;
    conn_status.last_packet = xTaskGetTickCount();
}

void common_wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    // Set fixed channel for ESP-NOW
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_LOGI(TAG, "WiFi initialized on channel %d", ESP_NOW_CHANNEL);
}

void common_espnow_init(void) {
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_set_pmk((const uint8_t *)ESPNOW_PMK));
    ESP_LOGI(TAG, "ESP-NOW initialized");
}

uint32_t servo_us_to_duty(uint32_t us) {
    // 16-bit duty over 20ms period; duty = (us / period_us) * 2^16
    const uint32_t period_us = 1000000UL / SERVO_FREQ_HZ; // 20000 us
    uint32_t duty = (us * (1UL << 16)) / period_us;
    if (duty > ((1UL << 16) - 1)) duty = (1UL << 16) - 1;
    return duty;
}

uint32_t map_adc_to_us(uint16_t adc_raw, float scale) {
    float norm = (float)adc_raw / 4095.0f; // 0..1
    float span = (float)(SERVO_US_MAX - SERVO_US_MIN) * scale;
    float center = (SERVO_US_MIN + SERVO_US_MAX) * 0.5f;
    // Map: -1..1 around center using steering/throttle proportional
    float val = center + (norm - 0.5f) * 2.0f * (span * 0.5f);
    if (val < SERVO_US_MIN) val = SERVO_US_MIN;
    if (val > SERVO_US_MAX) val = SERVO_US_MAX;
    return (uint32_t)val;
}
