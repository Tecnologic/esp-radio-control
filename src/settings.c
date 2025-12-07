// NVS-based settings implementation
#include "settings.h"
#include "common.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "settings";
static const char *NVS_NAMESPACE = "radio";

void settings_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG, "NVS initialized");
}

void settings_get_defaults(device_settings_t *settings) {
    // Default broadcast MAC (all FF for broadcast)
    memset(settings->peer_mac, PEER_MAC_BROADCAST, PEER_MAC_LEN);
    settings->channel = 1;
    // Default calibration: full ADC range for all channels
    for (int i = 0; i < NUM_CHANNELS; i++) {
        settings->ch_min[i] = 0;
        settings->ch_max[i] = ADC_MAX_VALUE;
        // Default servo positions for each channel (standard RC servo range)
        settings->servo_min[i] = SERVO_US_MIN;     // Full left/backward
        settings->servo_center[i] = SERVO_US_CENTER;  // Neutral
        settings->servo_max[i] = SERVO_US_MAX;     // Full right/forward
        // Default expo for each channel (0.0 = linear, 1.0 = strong S-curve)
        settings->expo[i] = 0.0f;
    }
    settings->device_role = ROLE_RECEIVER;      // receiver by default
    settings->is_configured = false;
}

void settings_reset_defaults(device_settings_t *settings) {
    settings_get_defaults(settings);
    settings_save(settings);
}

void settings_load(device_settings_t *settings) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found, using defaults");
        settings_get_defaults(settings);
        return;
    }

    // Load peer MAC (6 bytes)
    size_t len = 6;
    err = nvs_get_blob(handle, "peer_mac", settings->peer_mac, &len);
    if (err != ESP_OK) {
        memset(settings->peer_mac, 0xFF, 6);
    }

    // Load other settings
    nvs_get_u8(handle, "channel", &settings->channel);
    if (settings->channel == 0) settings->channel = 1;
    
    // Load calibration for all channels
    for (int i = 0; i < NUM_CHANNELS; i++) {
        char key_min[16], key_max[16];
        snprintf(key_min, sizeof(key_min), "ch%d_min", i + 1);
        snprintf(key_max, sizeof(key_max), "ch%d_max", i + 1);
        
        if (nvs_get_u16(handle, key_min, &settings->ch_min[i]) != ESP_OK) {
            settings->ch_min[i] = 0;
        }
        if (nvs_get_u16(handle, key_max, &settings->ch_max[i]) != ESP_OK) {
            settings->ch_max[i] = ADC_MAX_VALUE;
        }
    }
    
    // Load per-channel servo positions and expo
    for (int i = 0; i < NUM_CHANNELS; i++) {
        char key_smin[16], key_scenter[16], key_smax[16];
        char key_expo[16];
        snprintf(key_smin, sizeof(key_smin), "ch%d_smin", i + 1);
        snprintf(key_scenter, sizeof(key_scenter), "ch%d_sctr", i + 1);
        snprintf(key_smax, sizeof(key_smax), "ch%d_smax", i + 1);
        snprintf(key_expo, sizeof(key_expo), "ch%d_expo", i + 1);
        
        if (nvs_get_u16(handle, key_smin, &settings->servo_min[i]) != ESP_OK) {
            settings->servo_min[i] = SERVO_US_MIN;
        }
        if (nvs_get_u16(handle, key_scenter, &settings->servo_center[i]) != ESP_OK) {
            settings->servo_center[i] = SERVO_US_CENTER;
        }
        if (nvs_get_u16(handle, key_smax, &settings->servo_max[i]) != ESP_OK) {
            settings->servo_max[i] = SERVO_US_MAX;
        }
        if (nvs_get_u32(handle, key_expo, (uint32_t*)&settings->expo[i]) != ESP_OK) {
            settings->expo[i] = 0.0f;
        }
    }
    
    nvs_get_u8(handle, "dev_role", &settings->device_role);
    
    uint8_t configured = 0;
    nvs_get_u8(handle, "configured", &configured);
    settings->is_configured = (configured != 0);

    nvs_close(handle);
    ESP_LOGI(TAG, "Settings loaded from NVS");
}

void settings_save(const device_settings_t *settings) {
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));

    ESP_ERROR_CHECK(nvs_set_blob(handle, "peer_mac", settings->peer_mac, PEER_MAC_LEN));
    ESP_ERROR_CHECK(nvs_set_u8(handle, "channel", settings->channel));
    
    // Save calibration for all channels
    for (int i = 0; i < NUM_CHANNELS; i++) {
        char key_min[16], key_max[16];
        snprintf(key_min, sizeof(key_min), "ch%d_min", i + 1);
        snprintf(key_max, sizeof(key_max), "ch%d_max", i + 1);
        ESP_ERROR_CHECK(nvs_set_u16(handle, key_min, settings->ch_min[i]));
        ESP_ERROR_CHECK(nvs_set_u16(handle, key_max, settings->ch_max[i]));
    }
    
    // Save per-channel servo positions and expo
    for (int i = 0; i < NUM_CHANNELS; i++) {
        char key_smin[16], key_scenter[16], key_smax[16];
        char key_expo[16];
        snprintf(key_smin, sizeof(key_smin), "ch%d_smin", i + 1);
        snprintf(key_scenter, sizeof(key_scenter), "ch%d_sctr", i + 1);
        snprintf(key_smax, sizeof(key_smax), "ch%d_smax", i + 1);
        snprintf(key_expo, sizeof(key_expo), "ch%d_expo", i + 1);
        
        ESP_ERROR_CHECK(nvs_set_u16(handle, key_smin, settings->servo_min[i]));
        ESP_ERROR_CHECK(nvs_set_u16(handle, key_scenter, settings->servo_center[i]));
        ESP_ERROR_CHECK(nvs_set_u16(handle, key_smax, settings->servo_max[i]));
        ESP_ERROR_CHECK(nvs_set_u32(handle, key_expo, *(uint32_t*)&settings->expo[i]));
    }
    
    ESP_ERROR_CHECK(nvs_set_u8(handle, "dev_role", settings->device_role));
    ESP_ERROR_CHECK(nvs_set_u8(handle, "configured", settings->is_configured ? 1 : 0));

    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
    ESP_LOGI(TAG, "Settings saved to NVS");
}
