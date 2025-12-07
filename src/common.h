// Common definitions for ESP-NOW radio control project
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"

// ESP-NOW configuration
#ifndef ESP_NOW_CHANNEL
#define ESP_NOW_CHANNEL 1
#endif

#ifndef ESPNOW_PMK
#define ESPNOW_PMK "pmk1234567890"
#endif

#ifndef ESPNOW_LMK
#define ESPNOW_LMK "lmk1234567890"
#endif

// LOLIN S2 Mini Pin Definitions
#define PIN_USER_BUTTON 0       // GPIO0 - User button (active-low, boot button)
#define PIN_LED 15              // GPIO15 - Status LED

// Light control buttons (GPIO pins for 4 light channels)
#define PIN_LIGHT_BTN1 6        // GPIO6 - Light 1 button
#define PIN_LIGHT_BTN2 7        // GPIO7 - Light 2 button
#define PIN_LIGHT_BTN3 8        // GPIO8 - Light 3 button
#define PIN_LIGHT_BTN4 9        // GPIO9 - Light 4 button

// Light output pins
#define PIN_LIGHT_OUT1 10       // GPIO10 - Light 1 output
#define PIN_LIGHT_OUT2 11       // GPIO11 - Light 2 output
#define PIN_LIGHT_OUT3 12       // GPIO12 - Light 3 output
#define PIN_LIGHT_OUT4 13       // GPIO13 - Light 4 output

// ADC channels (ESP32-S2 has ADC1 and ADC2)
// Common ADC pins on S2 Mini: GPIO1-GPIO10 (ADC1), GPIO11-GPIO20 (ADC2)
#define ADC_THROTTLE_CHANNEL ADC_CHANNEL_0 // GPIO1 (ADC1_CH0)
#define ADC_STEERING_CHANNEL ADC_CHANNEL_1 // GPIO2 (ADC1_CH1)

// Servo output pins (PWM via LEDC)
#define PIN_SERVO_THROTTLE 4    // GPIO4 - Servo 1 PWM
#define PIN_SERVO_STEERING 5    // GPIO5 - Servo 2 PWM

// Servo parameters
#define SERVO_FREQ_HZ 50           // 20 ms period
#define SERVO_US_MIN 1000          // 1.0 ms
#define SERVO_US_MAX 2000          // 2.0 ms

// Rate scaling
#define RATE_LOW_SCALE 0.5f
#define RATE_HIGH_SCALE 1.0f

// Data packet sent via ESP-NOW
typedef struct __attribute__((packed)) {
    uint16_t throttle; // 0..4095
    uint16_t steering; // 0..4095
    uint8_t lights; // bit 0-3: light states
} control_packet_t;

// Role enum for runtime selection
typedef enum {
    ROLE_RECEIVER = 0,
    ROLE_SENDER = 1
} device_role_t;

// Connection status
typedef struct {
    bool connected;          // Is peer connected
    int8_t rssi;            // Signal strength (-120 to 0 dBm)
    uint32_t last_packet;   // Timestamp of last packet
} connection_status_t;

// Shared initialization functions
void common_wifi_init(void);
void common_espnow_init(void);

// Status functions
connection_status_t get_connection_status(void);
void update_connection_status(bool connected, int8_t rssi);

// Utility functions
uint32_t servo_us_to_duty(uint32_t us);
uint32_t map_adc_to_us(uint16_t adc_raw, float scale);

#endif // COMMON_H
