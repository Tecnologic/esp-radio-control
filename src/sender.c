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
static adc_oneshot_unit_handle_t adc_unit = NULL;
static TaskHandle_t sender_task_handle = NULL;
static uint8_t shared_light_states = 0; // Will be updated by main task
static bool adc_initialized = false;

void sender_set_light_states(uint8_t states) {
    shared_light_states = states;
}

static void send_cb(const wifi_tx_info_t *info, esp_now_send_status_t status) {
    // Update connection status based on send success
    // RSSI is not available for sender, use a placeholder value
    if (status == ESP_NOW_SEND_SUCCESS) {
        update_connection_status(true, -50);  // Placeholder RSSI for successful send
    } else {
        update_connection_status(false, -120);
    }
}

static void adc_init(void) {
    if (adc_initialized) {
        ESP_LOGI(TAG, "ADC already initialized");
        return;
    }
    
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_unit));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    // Configure all 6 ADC channels
    for (int i = 0; i < 6; i++) {
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_unit, (adc_channel_t)i, &chan_cfg));
    }
    adc_initialized = true;
    ESP_LOGI(TAG, "ADC initialized for 6 channels");
}

static void sender_task(void *arg) {
    const uint8_t *peer_mac = (const uint8_t *)arg;
    
    // Register send callback to track connection status
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_cb));
    
    // Add peer (or update if it already exists)
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, peer_mac, 6);
    peer.channel = ESP_NOW_CHANNEL;
    peer.ifidx = ESP_IF_WIFI_STA;
    peer.encrypt = false;
    
    // Try to add peer, if it already exists, modify it instead
    esp_err_t ret = esp_now_add_peer(&peer);
    if (ret == ESP_ERR_ESPNOW_EXIST) {
        // Peer already exists, update it
        ret = esp_now_mod_peer(&peer);
    }
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add/modify peer: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Sender task started");

    adc_init();

    while (1) {
        int ch_raw[6] = {0};
        for (int i = 0; i < 6; i++) {
            ESP_ERROR_CHECK(adc_oneshot_read(adc_unit, (adc_channel_t)i, &ch_raw[i]));
        }

        control_packet_t pkt = {0};
        for (int i = 0; i < 6; i++) {
            pkt.ch[i] = (uint16_t)ch_raw[i];
        }
        pkt.lights = shared_light_states;

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

