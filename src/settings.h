// NVS-based settings management for ESP-NOW radio control
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations (defined in common.h)
#ifndef NUM_CHANNELS
#define NUM_CHANNELS 6
#endif
#ifndef PEER_MAC_LEN
#define PEER_MAC_LEN 6
#endif

typedef struct {
    uint8_t peer_mac[PEER_MAC_LEN];          // Target peer MAC address
    uint8_t channel;              // ESP-NOW channel (1-13)
    uint16_t ch_min[NUM_CHANNELS];           // Min ADC value for each proportional channel
    uint16_t ch_max[NUM_CHANNELS];           // Max ADC value for each proportional channel
    // Per-channel servo configuration
    uint16_t servo_min[NUM_CHANNELS];        // Minimum servo position (µs) for each channel
    uint16_t servo_center[NUM_CHANNELS];     // Center servo position (µs) for each channel
    uint16_t servo_max[NUM_CHANNELS];        // Maximum servo position (µs) for each channel
    // Per-channel expo (input-side S-curve, like Betaflight)
    float expo[NUM_CHANNELS];                // Expo value (0.0-1.0) for each channel, 0=linear, 1=strong S-curve
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
