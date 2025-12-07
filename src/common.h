// Common definitions for ESP-NOW radio control project
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

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
    uint8_t button_light; // 0/1
    uint8_t button_rate;  // 0/1
} control_packet_t;

// Role enum for runtime selection
typedef enum {
    ROLE_RECEIVER = 0,
    ROLE_SENDER = 1
} device_role_t;

// Shared initialization functions
void common_wifi_init(void);
void common_espnow_init(void);

// Utility functions
uint32_t servo_us_to_duty(uint32_t us);
uint32_t map_adc_to_us(uint16_t adc_raw, float scale);

#endif // COMMON_H
