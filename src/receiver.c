// Receiver implementation for ESP-NOW radio control
#include "common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_now.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include <string.h>

static const char *TAG = "receiver";
static volatile control_packet_t last_pkt = {0};
static volatile uint8_t have_pkt = 0;
static TaskHandle_t receiver_task_handle = NULL;

static void light_outputs_init(void) {
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_LIGHT_OUT1) | (1ULL << PIN_LIGHT_OUT2) | 
                        (1ULL << PIN_LIGHT_OUT3) | (1ULL << PIN_LIGHT_OUT4),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    ESP_LOGI(TAG, "Light outputs initialized");
}

static void ledc_servo_init(void) {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t ch1 = {
        .gpio_num = PIN_SERVO_THROTTLE,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config_t ch2 = ch1;
    ch2.gpio_num = PIN_SERVO_STEERING;
    ch2.channel = LEDC_CHANNEL_1;
    ESP_ERROR_CHECK(ledc_channel_config(&ch1));
    ESP_ERROR_CHECK(ledc_channel_config(&ch2));
    ESP_LOGI(TAG, "LEDC servos initialized");
}

static void recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == sizeof(control_packet_t)) {
        memcpy((void *)&last_pkt, data, sizeof(last_pkt));
        have_pkt = 1;
        // Update connection status with RSSI from the received packet
        if (info && info->rx_ctrl) {
            update_connection_status(true, info->rx_ctrl->rssi);
        } else {
            update_connection_status(true, -120);
        }
    }
}

static void receiver_task(void *arg) {
    ESP_ERROR_CHECK(esp_now_register_recv_cb(recv_cb));
    ledc_servo_init();
    light_outputs_init();
    ESP_LOGI(TAG, "Receiver task started");

    float rate_scale = RATE_LOW_SCALE;
    const uint8_t light_pins[4] = {PIN_LIGHT_OUT1, PIN_LIGHT_OUT2, PIN_LIGHT_OUT3, PIN_LIGHT_OUT4};
    
    while (1) {
        // Check for connection timeout (no packet received for 1 second)
        TickType_t now = xTaskGetTickCount();
        if (get_connection_status().connected && (now - get_connection_status().last_packet > pdMS_TO_TICKS(1000))) {
            update_connection_status(false, -120);
        }
        
        if (have_pkt) {
            rate_scale = last_pkt.lights & 0x08 ? RATE_HIGH_SCALE : RATE_LOW_SCALE;
            uint32_t us_throttle = map_adc_to_us(last_pkt.throttle, rate_scale);
            uint32_t us_steering = map_adc_to_us(last_pkt.steering, rate_scale);
            uint32_t duty_thr = servo_us_to_duty(us_throttle);
            uint32_t duty_ste = servo_us_to_duty(us_steering);

            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_thr);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty_ste);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

            // Set light outputs based on packet bits 0-3
            for (int i = 0; i < 4; i++) {
                gpio_set_level(light_pins[i], (last_pkt.lights & (1 << i)) ? 1 : 0);
            }

            have_pkt = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void receiver_start(void) {
    if (receiver_task_handle != NULL) {
        ESP_LOGW(TAG, "Receiver already running");
        return;
    }
    
    xTaskCreate(receiver_task, "receiver", 4096, NULL, 5, &receiver_task_handle);
}

void receiver_stop(void) {
    if (receiver_task_handle != NULL) {
        vTaskDelete(receiver_task_handle);
        receiver_task_handle = NULL;
        ESP_LOGI(TAG, "Receiver stopped");
    }
}
