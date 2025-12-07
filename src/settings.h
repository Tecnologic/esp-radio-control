// NVS-based settings management for ESP-NOW radio control
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t peer_mac[6];          // Target peer MAC address
    uint8_t channel;              // ESP-NOW channel (1-13)
    uint16_t throttle_min;        // Min ADC value for throttle
    uint16_t throttle_max;        // Max ADC value for throttle
    uint16_t steering_min;        // Min ADC value for steering
    uint16_t steering_max;        // Max ADC value for steering
    uint8_t rate_mode;            // 0=low, 1=high
    uint8_t device_role;          // 0=receiver, 1=sender
    bool is_configured;           // Has been configured at least once
} device_settings_t;

// Initialize NVS and load settings
void settings_init(void);

// Load settings from NVS
void settings_load(device_settings_t *settings);

// Save settings to NVS
void settings_save(const device_settings_t *settings);

// Reset settings to defaults
void settings_reset_defaults(device_settings_t *settings);

// Get default settings
void settings_get_defaults(device_settings_t *settings);

#endif // SETTINGS_H
