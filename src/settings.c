// NVS-based settings implementation
#include "settings.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

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
    memset(settings->peer_mac, 0xFF, 6);
    settings->channel = 1;
    settings->throttle_min = 0;
    settings->throttle_max = 4095;
    settings->steering_min = 0;
    settings->steering_max = 4095;
    settings->rate_mode = 0; // low rate
    settings->device_role = 0; // receiver by default
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
    
    nvs_get_u16(handle, "thr_min", &settings->throttle_min);
    if (settings->throttle_min == 0 && nvs_get_u16(handle, "thr_min", &settings->throttle_min) != ESP_OK) {
        settings->throttle_min = 0;
    }
    
    nvs_get_u16(handle, "thr_max", &settings->throttle_max);
    if (settings->throttle_max == 0) settings->throttle_max = 4095;
    
    nvs_get_u16(handle, "str_min", &settings->steering_min);
    if (settings->steering_min == 0) settings->steering_min = 0;
    
    nvs_get_u16(handle, "str_max", &settings->steering_max);
    if (settings->steering_max == 0) settings->steering_max = 4095;
    
    nvs_get_u8(handle, "rate_mode", &settings->rate_mode);
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

    ESP_ERROR_CHECK(nvs_set_blob(handle, "peer_mac", settings->peer_mac, 6));
    ESP_ERROR_CHECK(nvs_set_u8(handle, "channel", settings->channel));
    ESP_ERROR_CHECK(nvs_set_u16(handle, "thr_min", settings->throttle_min));
    ESP_ERROR_CHECK(nvs_set_u16(handle, "thr_max", settings->throttle_max));
    ESP_ERROR_CHECK(nvs_set_u16(handle, "str_min", settings->steering_min));
    ESP_ERROR_CHECK(nvs_set_u16(handle, "str_max", settings->steering_max));
    ESP_ERROR_CHECK(nvs_set_u8(handle, "rate_mode", settings->rate_mode));
    ESP_ERROR_CHECK(nvs_set_u8(handle, "dev_role", settings->device_role));
    ESP_ERROR_CHECK(nvs_set_u8(handle, "configured", settings->is_configured ? 1 : 0));

    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
    ESP_LOGI(TAG, "Settings saved to NVS");
}
