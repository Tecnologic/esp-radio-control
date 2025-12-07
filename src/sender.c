// Sender implementation for ESP-NOW radio control
#include "common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_now.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "sender";
static adc_oneshot_unit_handle_t adc_unit;
static TaskHandle_t sender_task_handle = NULL;

static void adc_init(void) {
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_unit));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_unit, ADC_THROTTLE_CHANNEL, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_unit, ADC_STEERING_CHANNEL, &chan_cfg));
    ESP_LOGI(TAG, "ADC initialized");
}

static void sender_task(void *arg) {
    const uint8_t *peer_mac = (const uint8_t *)arg;
    
    // Add peer
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, peer_mac, 6);
    peer.channel = ESP_NOW_CHANNEL;
    peer.ifidx = ESP_IF_WIFI_STA;
    peer.encrypt = false;
    
    if (esp_now_add_peer(&peer) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add peer");
        return;
    }
    ESP_LOGI(TAG, "Sender task started");

    adc_init();

    while (1) {
        int throttle_raw = 0, steering_raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_unit, ADC_THROTTLE_CHANNEL, &throttle_raw));
        ESP_ERROR_CHECK(adc_oneshot_read(adc_unit, ADC_STEERING_CHANNEL, &steering_raw));

        control_packet_t pkt = {
            .throttle = (uint16_t)throttle_raw,
            .steering = (uint16_t)steering_raw,
            .button_light = 0,
            .button_rate = 0,
        };

        esp_err_t err = esp_now_send(peer_mac, (uint8_t *)&pkt, sizeof(pkt));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "ESP-NOW send failed: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // ~50 Hz
    }
}

void sender_start(const uint8_t *peer_mac) {
    if (sender_task_handle != NULL) {
        ESP_LOGW(TAG, "Sender already running");
        return;
    }
    
    xTaskCreate(sender_task, "sender", 4096, (void *)peer_mac, 5, &sender_task_handle);
}

void sender_stop(void) {
    if (sender_task_handle != NULL) {
        vTaskDelete(sender_task_handle);
        sender_task_handle = NULL;
        ESP_LOGI(TAG, "Sender stopped");
    }
}

