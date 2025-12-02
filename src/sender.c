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

static void buttons_init(void) {
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_BUTTON_LIGHT) | (1ULL << PIN_BUTTON_RATE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    ESP_LOGI(TAG, "Buttons initialized");
}

static void sender_task(void *arg) {
    // Add broadcast peer
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, PEER_BROADCAST, 6);
    peer.channel = ESP_NOW_CHANNEL;
    peer.ifidx = ESP_IF_WIFI_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    ESP_LOGI(TAG, "Broadcast peer added");

    adc_init();
    buttons_init();
    ESP_LOGI(TAG, "Sender task started");

    while (1) {
        int throttle_raw = 0, steering_raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_unit, ADC_THROTTLE_CHANNEL, &throttle_raw));
        ESP_ERROR_CHECK(adc_oneshot_read(adc_unit, ADC_STEERING_CHANNEL, &steering_raw));
        uint8_t b_light = gpio_get_level(PIN_BUTTON_LIGHT) ? 0 : 1; // active-low if pull-up
        uint8_t b_rate  = gpio_get_level(PIN_BUTTON_RATE) ? 0 : 1;

        control_packet_t pkt = {
            .throttle = (uint16_t)throttle_raw,
            .steering = (uint16_t)steering_raw,
            .button_light = b_light,
            .button_rate = b_rate,
        };

        esp_err_t err = esp_now_send(PEER_BROADCAST, (uint8_t *)&pkt, sizeof(pkt));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "ESP-NOW send failed: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // ~50 Hz
    }
}

void sender_start(void) {
    xTaskCreate(sender_task, "sender", 4096, NULL, 5, NULL);
}
