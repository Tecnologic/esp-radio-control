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
    
    // Create default WiFi STA and AP netif (needed for mode switching)
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    
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

// Apply S-curve expo to normalized input value (0..1)
// expo: 0.0 = linear, 1.0 = strong S-curve
static float apply_expo(float normalized, float expo) {
    if (expo <= 0.0f) return normalized;
    if (expo >= 1.0f) expo = 1.0f;
    
    // S-curve: cubic interpolation between linear and cubic
    // output = normalized + (normalized^3 - normalized) * expo
    float cubic = normalized * normalized * normalized;
    return normalized + (cubic - normalized) * expo;
}

uint32_t servo_us_to_duty(uint32_t us) {
    // 16-bit duty over 20ms period; duty = (us / period_us) * 2^16
    const uint32_t period_us = 1000000UL / SERVO_FREQ_HZ; // 20000 us
    uint32_t duty = (us * (1UL << 16)) / period_us;
    if (duty > ((1UL << 16) - 1)) duty = (1UL << 16) - 1;
    return duty;
}

uint32_t map_adc_to_us(uint16_t adc_raw, float scale) {
    float norm = (float)adc_raw / ADC_NORMALIZE; // 0..1
    float span = (float)(SERVO_US_MAX - SERVO_US_MIN) * scale;
    float center = (SERVO_US_MIN + SERVO_US_MAX) * 0.5f;
    // Map: -1..1 around center using steering/throttle proportional
    float val = center + (norm - 0.5f) * 2.0f * (span * 0.5f);
    if (val < SERVO_US_MIN) val = SERVO_US_MIN;
    if (val > SERVO_US_MAX) val = SERVO_US_MAX;
    return (uint32_t)val;
}

// Map ADC value to servo microseconds using custom min/center/max positions with S-curve expo
// expo: 0.0 = linear, 1.0 = strong S-curve
uint32_t map_adc_to_us_custom(uint16_t adc_raw, float expo, uint16_t srv_min, uint16_t srv_center, uint16_t srv_max) {
    float norm = (float)adc_raw / ADC_NORMALIZE; // 0..1
    
    // Apply S-curve expo to the normalized input
    norm = apply_expo(norm, expo);
    
    float range_low = (float)(srv_center - srv_min);
    float range_high = (float)(srv_max - srv_center);
    float val;
    
    if (norm < 0.5f) {
        // Map 0..0.5 to srv_min..srv_center
        float n = norm * 2.0f; // 0..1
        val = srv_min + n * range_low;
    } else {
        // Map 0.5..1 to srv_center..srv_max
        float n = (norm - 0.5f) * 2.0f; // 0..1
        val = srv_center + n * range_high;
    }
    
    if (val < srv_min) val = srv_min;
    if (val > srv_max) val = srv_max;
    return (uint32_t)val;
}
