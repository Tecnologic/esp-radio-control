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

// Pin definitions - Sender
#define PIN_BUTTON_LIGHT 4      // GPIO4 - light button (sender)
#define PIN_BUTTON_RATE 5       // GPIO5 - rate switch (sender)

// Pin definitions - Receiver
#define PIN_SERVO_THROTTLE 6    // GPIO6 - servo 1 (receiver)
#define PIN_SERVO_STEERING 7    // GPIO7 - servo 2 (receiver)
#define PIN_LIGHT_OUTPUT 3      // GPIO3 - light output (receiver)

// ADC channels (ESP32-C3 oneshot on ADC1)
#define ADC_THROTTLE_CHANNEL ADC_CHANNEL_0 // GPIO0 default, adapt
#define ADC_STEERING_CHANNEL ADC_CHANNEL_1 // GPIO1 default, adapt

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

// Broadcast MAC address for ESP-NOW
extern const uint8_t PEER_BROADCAST[6];

// Shared initialization functions
void common_wifi_init(void);
void common_espnow_init(void);

// Utility functions
uint32_t servo_us_to_duty(uint32_t us);
uint32_t map_adc_to_us(uint16_t adc_raw, float scale);

#endif // COMMON_H
