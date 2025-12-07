// Webserver implementation for ESP-NOW radio control configuration
#include "webserver.h"
#include "webserver_page.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "webserver";
static httpd_handle_t http_server = NULL;
static device_settings_t *g_settings = NULL;

static esp_err_t handler_index(httpd_req_t *req) {
    return httpd_resp_send(req, html_page, strlen(html_page));
}

static esp_err_t handler_get_settings(httpd_req_t *req) {
    char response[2048];
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             g_settings->peer_mac[0], g_settings->peer_mac[1], g_settings->peer_mac[2],
             g_settings->peer_mac[3], g_settings->peer_mac[4], g_settings->peer_mac[5]);

    snprintf(response, sizeof(response),
             "{"
             "\"device_role\":%d,"
             "\"peer_mac\":\"%s\","
             "\"channel\":%d,"
             "\"ch1_min\":%d,\"ch1_max\":%d,"
             "\"ch2_min\":%d,\"ch2_max\":%d,"
             "\"ch3_min\":%d,\"ch3_max\":%d,"
             "\"ch4_min\":%d,\"ch4_max\":%d,"
             "\"ch5_min\":%d,\"ch5_max\":%d,"
             "\"ch6_min\":%d,\"ch6_max\":%d,"
             "\"ch1_smin\":%u,\"ch1_sctr\":%u,\"ch1_smax\":%u,\"ch1_expo\":%.1f,"
             "\"ch2_smin\":%u,\"ch2_sctr\":%u,\"ch2_smax\":%u,\"ch2_expo\":%.1f,"
             "\"ch3_smin\":%u,\"ch3_sctr\":%u,\"ch3_smax\":%u,\"ch3_expo\":%.1f,"
             "\"ch4_smin\":%u,\"ch4_sctr\":%u,\"ch4_smax\":%u,\"ch4_expo\":%.1f,"
             "\"ch5_smin\":%u,\"ch5_sctr\":%u,\"ch5_smax\":%u,\"ch5_expo\":%.1f,"
             "\"ch6_smin\":%u,\"ch6_sctr\":%u,\"ch6_smax\":%u,\"ch6_expo\":%.1f"
             "}",
             g_settings->device_role, mac_str, g_settings->channel,
             g_settings->ch_min[0], g_settings->ch_max[0],
             g_settings->ch_min[1], g_settings->ch_max[1],
             g_settings->ch_min[2], g_settings->ch_max[2],
             g_settings->ch_min[3], g_settings->ch_max[3],
             g_settings->ch_min[4], g_settings->ch_max[4],
             g_settings->ch_min[5], g_settings->ch_max[5],
             g_settings->servo_min[0], g_settings->servo_center[0], g_settings->servo_max[0], 
             g_settings->expo[0],
             g_settings->servo_min[1], g_settings->servo_center[1], g_settings->servo_max[1], 
             g_settings->expo[1],
             g_settings->servo_min[2], g_settings->servo_center[2], g_settings->servo_max[2], 
             g_settings->expo[2],
             g_settings->servo_min[3], g_settings->servo_center[3], g_settings->servo_max[3], 
             g_settings->expo[3],
             g_settings->servo_min[4], g_settings->servo_center[4], g_settings->servo_max[4], 
             g_settings->expo[4],
             g_settings->servo_min[5], g_settings->servo_center[5], g_settings->servo_max[5], 
             g_settings->expo[5]);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}

static esp_err_t handler_get_status(httpd_req_t *req) {
    char response[768];
    connection_status_t status = get_connection_status();
    control_packet_t pkt = get_last_control_packet();
    uint16_t servo_us[6];
    get_servo_positions(servo_us);
    
    // Get device MAC address
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    snprintf(response, sizeof(response),
             "{"
             "\"device_mac\":\"%s\","
             "\"connected\":%d,"
             "\"rssi\":%d,"
             "\"last_packet\":%lu,"
             "\"ch\":[%u,%u,%u,%u,%u,%u],"
             "\"servo_us\":[%u,%u,%u,%u,%u,%u],"
             "\"lights\":%u"
             "}",
             mac_str,
             status.connected, status.rssi, status.last_packet,
             pkt.ch[0], pkt.ch[1], pkt.ch[2], pkt.ch[3], pkt.ch[4], pkt.ch[5],
             servo_us[0], servo_us[1], servo_us[2], servo_us[3], servo_us[4], servo_us[5],
             pkt.lights);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}

static esp_err_t handler_post_settings(httpd_req_t *req) {
    char buffer[1024] = {0};
    int ret = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
    if (ret <= 0) {
        return httpd_resp_send_500(req);
    }

    // Parse JSON (simple parsing for now)
    if (sscanf(buffer, "{\"device_role\":%hhu,\"peer_mac\":\"%17[^\"]\"", 
               &g_settings->device_role, (char*)g_settings->peer_mac) > 0) {
        // Parse MAC address from string format
        uint8_t mac[6];
        if (sscanf((char*)g_settings->peer_mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                   &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6) {
            memcpy(g_settings->peer_mac, mac, 6);
        }
    }
    
    sscanf(buffer, "\"channel\":%hhu", &g_settings->channel);
    
    // Parse calibration for all channels
    for (int i = 0; i < NUM_CHANNELS; i++) {
        char key_min[16], key_max[16];
        snprintf(key_min, sizeof(key_min), "\"ch%d_min\"", i + 1);
        snprintf(key_max, sizeof(key_max), "\"ch%d_max\"", i + 1);
        
        char pattern_min[32], pattern_max[32];
        snprintf(pattern_min, sizeof(pattern_min), "%s:%%hu", key_min);
        snprintf(pattern_max, sizeof(pattern_max), "%s:%%hu", key_max);
        
        sscanf(buffer, pattern_min, &g_settings->ch_min[i]);
        sscanf(buffer, pattern_max, &g_settings->ch_max[i]);
    }
    
    // Parse per-channel servo positions and expo
    for (int i = 0; i < NUM_CHANNELS; i++) {
        char key_smin[16], key_sctr[16], key_smax[16];
        char key_expo[16];
        snprintf(key_smin, sizeof(key_smin), "\"ch%d_smin\"", i + 1);
        snprintf(key_sctr, sizeof(key_sctr), "\"ch%d_sctr\"", i + 1);
        snprintf(key_smax, sizeof(key_smax), "\"ch%d_smax\"", i + 1);
        snprintf(key_expo, sizeof(key_expo), "\"ch%d_expo\"", i + 1);
        
        char pattern_smin[32], pattern_sctr[32], pattern_smax[32];
        char pattern_expo[32];
        snprintf(pattern_smin, sizeof(pattern_smin), "%s:%%hu", key_smin);
        snprintf(pattern_sctr, sizeof(pattern_sctr), "%s:%%hu", key_sctr);
        snprintf(pattern_smax, sizeof(pattern_smax), "%s:%%hu", key_smax);
        snprintf(pattern_expo, sizeof(pattern_expo), "%s:%%f", key_expo);
        
        sscanf(buffer, pattern_smin, &g_settings->servo_min[i]);
        sscanf(buffer, pattern_sctr, &g_settings->servo_center[i]);
        sscanf(buffer, pattern_smax, &g_settings->servo_max[i]);
        sscanf(buffer, pattern_expo, &g_settings->expo[i]);
    }
    
    
    // Update receiver with new settings
    receiver_set_settings(g_settings);

    const char *response = "{\"message\":\"Settings saved\"}";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}

void webserver_start(device_settings_t *settings) {
    if (http_server != NULL) {
        ESP_LOGW(TAG, "Webserver already running");
        return;
    }

    g_settings = settings;

    // Switch WiFi to AP mode for configuration
    ESP_LOGI(TAG, "Switching WiFi to AP mode...");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "esp-radio-control",
            .password = "",
            .ssid_len = strlen("esp-radio-control"),
            .channel = 1,
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = 4,
        }
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi started, configuring AP network...");
    
    // Now get the AP netif handle (after WiFi is started)
    esp_netif_t *netif_ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif_ap == NULL) {
        ESP_LOGE(TAG, "Failed to get AP netif handle");
        return;
    }
    
    // Stop DHCP and configure static IP for AP
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(netif_ap));
    
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    
    ESP_ERROR_CHECK(esp_netif_set_ip_info(netif_ap, &ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(netif_ap));
    
    ESP_LOGI(TAG, "WiFi AP ready: SSID='esp-radio-control', IP=192.168.4.1/24, DHCP enabled");
    ESP_LOGI(TAG, "Webserver accessible at: http://192.168.4.1");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 4;
    config.max_uri_handlers = 7;

    if (httpd_start(&http_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start webserver");
        return;
    }

    httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handler_index,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server, &uri_root);

    httpd_uri_t uri_get = {
        .uri = "/api/settings",
        .method = HTTP_GET,
        .handler = handler_get_settings,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server, &uri_get);

    httpd_uri_t uri_post = {
        .uri = "/api/settings",
        .method = HTTP_POST,
        .handler = handler_post_settings,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server, &uri_post);

    httpd_uri_t uri_status = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = handler_get_status,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server, &uri_status);

    ESP_LOGI(TAG, "Webserver started on http://192.168.4.1");
    
    // mDNS is configured in sdkconfig (CONFIG_MDNS_ENABLED=y)
    // The service is accessible at http://esp-radio-control.local
}

void webserver_stop(void) {
    if (http_server != NULL) {
        httpd_stop(http_server);
        http_server = NULL;
        ESP_LOGI(TAG, "Webserver stopped");
        
        // Switch WiFi back to STA mode for ESP-NOW
        ESP_LOGI(TAG, "Switching WiFi back to STA mode...");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_LOGI(TAG, "WiFi switched to STA mode for ESP-NOW");
    }
}

bool webserver_is_running(void) {
    return http_server != NULL;
}
