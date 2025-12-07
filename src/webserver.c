// Webserver implementation for ESP-NOW radio control configuration
#include "webserver.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>

static const char *TAG = "webserver";
static httpd_handle_t http_server = NULL;
static device_settings_t *g_settings = NULL;

static const char *html_page = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>ESP-NOW Radio Control</title>"
"<style>"
"body { font-family: Arial, sans-serif; max-width: 600px; margin: 50px auto; padding: 20px; background: #f5f5f5; }"
"h1 { color: #333; }"
"h2 { color: #555; font-size: 1.1em; margin-top: 25px; border-bottom: 2px solid #4CAF50; padding-bottom: 5px; }"
".form-group { margin: 15px 0; background: white; padding: 15px; border-radius: 5px; }"
".status-section { background: white; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 4px solid #2196F3; }"
".status-item { margin: 10px 0; display: flex; justify-content: space-between; align-items: center; }"
".status-label { font-weight: bold; color: #555; }"
".status-value { color: #2196F3; font-family: monospace; }"
".status-connected { color: #4CAF50; }"
".status-disconnected { color: #f44336; }"
".led-pattern { margin: 15px 0; padding: 10px; background: #f9f9f9; border-left: 3px solid #FF9800; border-radius: 3px; }"
".led-state { font-weight: bold; color: #FF9800; }"
".pattern-timing { font-size: 0.9em; color: #666; font-family: monospace; }"
"label { display: block; margin-bottom: 5px; font-weight: bold; color: #555; }"
"input[type='text'], input[type='number'], select { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }"
"button { background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; width: 100%; margin-top: 10px; }"
"button:hover { background: #45a049; }"
".reset { background: #f44336; margin-top: 20px; }"
".reset:hover { background: #da190b; }"
"</style>"
"</head>"
"<body>"
"<h1>ESP-NOW Radio Control Config</h1>"
""
"<div class='status-section'>"
"<h2>Connection Status</h2>"
"<div class='status-item'>"
"<span class='status-label'>Peer Connection:</span>"
"<span id='connStatus' class='status-value status-disconnected'>Disconnected</span>"
"</div>"
"<div class='status-item'>"
"<span class='status-label'>Signal Strength:</span>"
"<span id='rssiValue' class='status-value'>N/A</span>"
"</div>"
"<div class='status-item'>"
"<span class='status-label'>LED Pattern:</span>"
"<span id='ledPattern' class='status-value'>Initializing...</span>"
"</div>"
"</div>"
""
"<div class='status-section'>"
"<h2>LED Indicators</h2>"
"<p style='color: #666; font-size: 0.9em;'>The status LED provides feedback about device operation:</p>"
"<div class='led-pattern'>"
"<div class='led-state'>State A: Disconnected + No Webserver</div>"
"<div class='pattern-timing'>Pattern: ■ 100ms | ■ 100ms | ■ 100ms | ■ 300ms (600ms cycle)</div>"
"<div style='color: #666; font-size: 0.9em;'>ESP-NOW not connected, webserver is off</div>"
"</div>"
"<div class='led-pattern'>"
"<div class='led-state'>State B: Connected + No Webserver</div>"
"<div class='pattern-timing'>Pattern: ■ 500ms | ■ 500ms (1000ms cycle)</div>"
"<div style='color: #666; font-size: 0.9em;'>Receiving packets from peer, webserver is off</div>"
"</div>"
"<div class='led-pattern'>"
"<div class='led-state'>State C: Disconnected + Webserver On</div>"
"<div class='pattern-timing'>Pattern: ■ 250ms | ■ 250ms (500ms cycle)</div>"
"<div style='color: #666; font-size: 0.9em;'>No packets received, in configuration mode</div>"
"</div>"
"<div class='led-pattern'>"
"<div class='led-state'>State D: Connected + Webserver On</div>"
"<div class='pattern-timing'>Pattern: ■ 80ms | ■ 80ms | ■ 80ms | ■ 80ms | ■ 80ms | ■ 200ms (440ms cycle)</div>"
"<div style='color: #666; font-size: 0.9em;'>Receiving packets and in configuration mode</div>"
"</div>"
"</div>"
""
"<div class='status-section'>"
"<h2>Configuration</h2>"
"<form id='settingsForm'>"
"<div class='form-group'>"
"<label>Device Role:</label>"
"<select name='device_role'>"
"<option value='0'>Receiver</option>"
"<option value='1'>Sender</option>"
"</select>"
"</div>"
"<div class='form-group'>"
"<label>Peer MAC Address (XX:XX:XX:XX:XX:XX):</label>"
"<input type='text' name='peer_mac' placeholder='FF:FF:FF:FF:FF:FF'>"
"</div>"
"<div class='form-group'>"
"<label>ESP-NOW Channel (1-13):</label>"
"<input type='number' name='channel' min='1' max='13' value='1'>"
"</div>"
"<div class='form-group'>"
"<label>Throttle Min:</label>"
"<input type='number' name='thr_min' min='0' max='4095'>"
"</div>"
"<div class='form-group'>"
"<label>Throttle Max:</label>"
"<input type='number' name='thr_max' min='0' max='4095'>"
"</div>"
"<div class='form-group'>"
"<label>Steering Min:</label>"
"<input type='number' name='str_min' min='0' max='4095'>"
"</div>"
"<div class='form-group'>"
"<label>Steering Max:</label>"
"<input type='number' name='str_max' min='0' max='4095'>"
"</div>"
"<div class='form-group'>"
"<label>Rate Mode:</label>"
"<select name='rate_mode'>"
"<option value='0'>Low</option>"
"<option value='1'>High</option>"
"</select>"
"</div>"
"<button type='submit'>Save Settings</button>"
"<button type='reset' class='reset'>Reset to Defaults</button>"
"</form>"
"</div>"
""
"<script>"
"// Update status display every 500ms"
"setInterval(function() {"
"  fetch('/api/status')"
"  .then(r => r.json())"
"  .then(d => {"
"    const connStatus = document.getElementById('connStatus');"
"    const rssiValue = document.getElementById('rssiValue');"
"    const ledPattern = document.getElementById('ledPattern');"
"    "
"    if (d.connected) {"
"      connStatus.textContent = 'Connected';"
"      connStatus.className = 'status-value status-connected';"
"      rssiValue.textContent = d.rssi + ' dBm';"
"    } else {"
"      connStatus.textContent = 'Disconnected';"
"      connStatus.className = 'status-value status-disconnected';"
"      rssiValue.textContent = 'N/A';"
"    }"
"  })"
"  .catch(e => console.log('Status fetch error'));"
"}, 500);"
""
"document.getElementById('settingsForm').addEventListener('submit', function(e) {"
"  e.preventDefault();"
"  const data = new FormData(this);"
"  const obj = {};"
"  data.forEach((value, key) => obj[key] = value);"
"  fetch('/api/settings', {"
"    method: 'POST',"
"    headers: { 'Content-Type': 'application/json' },"
"    body: JSON.stringify(obj)"
"  })"
"  .then(r => r.json())"
"  .then(r => alert(r.message))"
"  .catch(e => alert('Error: ' + e));"
"});"
"fetch('/api/settings')"
".then(r => r.json())"
".then(d => {"
"  document.querySelector('[name=device_role]').value = d.device_role;"
"  document.querySelector('[name=peer_mac]').value = d.peer_mac;"
"  document.querySelector('[name=channel]').value = d.channel;"
"  document.querySelector('[name=thr_min]').value = d.throttle_min;"
"  document.querySelector('[name=thr_max]').value = d.throttle_max;"
"  document.querySelector('[name=str_min]').value = d.steering_min;"
"  document.querySelector('[name=str_max]').value = d.steering_max;"
"  document.querySelector('[name=rate_mode]').value = d.rate_mode;"
"});"
"</script>"
"</body>"
"</html>";

static esp_err_t handler_index(httpd_req_t *req) {
    return httpd_resp_send(req, html_page, strlen(html_page));
}

// Captive portal handler - redirect all unknown requests to main page
static esp_err_t handler_captive_portal(httpd_req_t *req) {
    // Return 302 redirect to root
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t handler_get_settings(httpd_req_t *req) {
    char response[512];
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             g_settings->peer_mac[0], g_settings->peer_mac[1], g_settings->peer_mac[2],
             g_settings->peer_mac[3], g_settings->peer_mac[4], g_settings->peer_mac[5]);

    snprintf(response, sizeof(response),
             "{"
             "\"device_role\":%d,"
             "\"peer_mac\":\"%s\","
             "\"channel\":%d,"
             "\"throttle_min\":%d,"
             "\"throttle_max\":%d,"
             "\"steering_min\":%d,"
             "\"steering_max\":%d,"
             "\"rate_mode\":%d"
             "}",
             g_settings->device_role, mac_str, g_settings->channel,
             g_settings->throttle_min, g_settings->throttle_max,
             g_settings->steering_min, g_settings->steering_max,
             g_settings->rate_mode);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}

static esp_err_t handler_get_status(httpd_req_t *req) {
    char response[256];
    connection_status_t status = get_connection_status();
    
    snprintf(response, sizeof(response),
             "{"
             "\"connected\":%d,"
             "\"rssi\":%d,"
             "\"last_packet\":%lu"
             "}",
             status.connected, status.rssi, status.last_packet);

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
    sscanf(buffer, "\"thr_min\":%hu", &g_settings->throttle_min);
    sscanf(buffer, "\"thr_max\":%hu", &g_settings->throttle_max);
    sscanf(buffer, "\"str_min\":%hu", &g_settings->steering_min);
    sscanf(buffer, "\"str_max\":%hu", &g_settings->steering_max);
    sscanf(buffer, "\"rate_mode\":%hhu", &g_settings->rate_mode);

    settings_save(g_settings);

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
    
    // Configure DHCP server for AP interface
    esp_netif_t *netif_ap = esp_netif_get_handle_from_ifkey("WIFI_AP");
    if (netif_ap != NULL) {
        esp_netif_dhcps_stop(netif_ap);
        
        esp_netif_ip_info_t ip_info;
        memset(&ip_info, 0, sizeof(ip_info));
        
        // Set IP address to 192.168.4.1
        inet_pton(AF_INET, "192.168.4.1", &ip_info.ip);
        inet_pton(AF_INET, "255.255.255.0", &ip_info.netmask);
        inet_pton(AF_INET, "192.168.4.1", &ip_info.gw);
        
        esp_netif_set_ip_info(netif_ap, &ip_info);
        esp_netif_dhcps_start(netif_ap);
        ESP_LOGI(TAG, "DHCP server started for AP interface");
    }
    
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi AP started: SSID='esp-radio-control', IP=192.168.4.1, DHCP enabled");

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

    // Register wildcard handler for captive portal (catch all unknown requests)
    httpd_uri_t uri_captive = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = handler_captive_portal,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server, &uri_captive);

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
