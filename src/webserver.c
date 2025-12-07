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
"<title>ESP Radio Control</title>"
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
"<h1>ESP Radio Control Config</h1>"
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
"<h2>Channel & Light Feedback</h2>"
"<p style='color: #666; font-size: 0.9em;'>Live input values from sender device:</p>"
"<div id='channelFeedback'>"
"<div style='margin: 12px 0;'>"
"<div style='display: flex; justify-content: space-between; margin-bottom: 4px;'>"
"<span style='font-weight: bold; color: #555;'>Channel 1:</span>"
"<span id='ch1_val' style='font-family: monospace; color: #2196F3;'>0/4095</span>"
"</div>"
"<div style='background: #e0e0e0; height: 20px; border-radius: 3px; overflow: hidden;'>"
"<div id='ch1_bar' style='background: #2196F3; height: 100%; width: 0%; transition: width 0.1s;'></div>"
"</div>"
"</div>"
"<div style='margin: 12px 0;'>"
"<div style='display: flex; justify-content: space-between; margin-bottom: 4px;'>"
"<span style='font-weight: bold; color: #555;'>Channel 2:</span>"
"<span id='ch2_val' style='font-family: monospace; color: #2196F3;'>0/4095</span>"
"</div>"
"<div style='background: #e0e0e0; height: 20px; border-radius: 3px; overflow: hidden;'>"
"<div id='ch2_bar' style='background: #2196F3; height: 100%; width: 0%; transition: width 0.1s;'></div>"
"</div>"
"</div>"
"<div style='margin: 12px 0;'>"
"<div style='display: flex; justify-content: space-between; margin-bottom: 4px;'>"
"<span style='font-weight: bold; color: #555;'>Channel 3:</span>"
"<span id='ch3_val' style='font-family: monospace; color: #2196F3;'>0/4095</span>"
"</div>"
"<div style='background: #e0e0e0; height: 20px; border-radius: 3px; overflow: hidden;'>"
"<div id='ch3_bar' style='background: #2196F3; height: 100%; width: 0%; transition: width 0.1s;'></div>"
"</div>"
"</div>"
"<div style='margin: 12px 0;'>"
"<div style='display: flex; justify-content: space-between; margin-bottom: 4px;'>"
"<span style='font-weight: bold; color: #555;'>Channel 4:</span>"
"<span id='ch4_val' style='font-family: monospace; color: #2196F3;'>0/4095</span>"
"</div>"
"<div style='background: #e0e0e0; height: 20px; border-radius: 3px; overflow: hidden;'>"
"<div id='ch4_bar' style='background: #2196F3; height: 100%; width: 0%; transition: width 0.1s;'></div>"
"</div>"
"</div>"
"<div style='margin: 12px 0;'>"
"<div style='display: flex; justify-content: space-between; margin-bottom: 4px;'>"
"<span style='font-weight: bold; color: #555;'>Channel 5:</span>"
"<span id='ch5_val' style='font-family: monospace; color: #2196F3;'>0/4095</span>"
"</div>"
"<div style='background: #e0e0e0; height: 20px; border-radius: 3px; overflow: hidden;'>"
"<div id='ch5_bar' style='background: #2196F3; height: 100%; width: 0%; transition: width 0.1s;'></div>"
"</div>"
"</div>"
"<div style='margin: 12px 0;'>"
"<div style='display: flex; justify-content: space-between; margin-bottom: 4px;'>"
"<span style='font-weight: bold; color: #555;'>Channel 6:</span>"
"<span id='ch6_val' style='font-family: monospace; color: #2196F3;'>0/4095</span>"
"</div>"
"<div style='background: #e0e0e0; height: 20px; border-radius: 3px; overflow: hidden;'>"
"<div id='ch6_bar' style='background: #2196F3; height: 100%; width: 0%; transition: width 0.1s;'></div>"
"</div>"
"</div>"
"</div>"
"<h3 style='margin-top: 20px; margin-bottom: 10px;'>Lights</h3>"
"<div style='display: grid; grid-template-columns: repeat(4, 1fr); gap: 10px;'>"
"<div style='text-align: center;'>"
"<div id='light1' style='width: 60px; height: 60px; margin: 0 auto 8px; border-radius: 50%; background: #ccc; border: 2px solid #999; transition: all 0.1s;'></div>"
"<span style='font-weight: bold; color: #555;'>Light 1</span>"
"</div>"
"<div style='text-align: center;'>"
"<div id='light2' style='width: 60px; height: 60px; margin: 0 auto 8px; border-radius: 50%; background: #ccc; border: 2px solid #999; transition: all 0.1s;'></div>"
"<span style='font-weight: bold; color: #555;'>Light 2</span>"
"</div>"
"<div style='text-align: center;'>"
"<div id='light3' style='width: 60px; height: 60px; margin: 0 auto 8px; border-radius: 50%; background: #ccc; border: 2px solid #999; transition: all 0.1s;'></div>"
"<span style='font-weight: bold; color: #555;'>Light 3</span>"
"</div>"
"<div style='text-align: center;'>"
"<div id='light4' style='width: 60px; height: 60px; margin: 0 auto 8px; border-radius: 50%; background: #ccc; border: 2px solid #999; transition: all 0.1s;'></div>"
"<span style='font-weight: bold; color: #555;'>Light 4</span>"
"</div>"
"</div>"
"</div>"
""
"<div class='status-section'>"
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
"<h3>Proportional Channel Calibration</h3>"
"<div class='form-group'>"
"<label>Channel 1 Min:</label>"
"<input type='number' name='ch1_min' min='0' max='4095'>"
"</div>"
"<div class='form-group'>"
"<label>Channel 1 Max:</label>"
"<input type='number' name='ch1_max' min='0' max='4095'>"
"</div>"
"<div class='form-group'>"
"<label>Channel 2 Min:</label>"
"<input type='number' name='ch2_min' min='0' max='4095'>"
"</div>"
"<div class='form-group'>"
"<label>Channel 2 Max:</label>"
"<input type='number' name='ch2_max' min='0' max='4095'>"
"</div>"
"<div class='form-group'>"
"<label>Channel 3 Min:</label>"
"<input type='number' name='ch3_min' min='0' max='4095'>"
"</div>"
"<div class='form-group'>"
"<label>Channel 3 Max:</label>"
"<input type='number' name='ch3_max' min='0' max='4095'>"
"</div>"
"<div class='form-group'>"
"<label>Channel 4 Min:</label>"
"<input type='number' name='ch4_min' min='0' max='4095'>"
"</div>"
"<div class='form-group'>"
"<label>Channel 4 Max:</label>"
"<input type='number' name='ch4_max' min='0' max='4095'>"
"</div>"
"<div class='form-group'>"
"<label>Channel 5 Min:</label>"
"<input type='number' name='ch5_min' min='0' max='4095'>"
"</div>"
"<div class='form-group'>"
"<label>Channel 5 Max:</label>"
"<input type='number' name='ch5_max' min='0' max='4095'>"
"</div>"
"<div class='form-group'>"
"<label>Channel 6 Min:</label>"
"<input type='number' name='ch6_min' min='0' max='4095'>"
"</div>"
"<div class='form-group'>"
"<label>Channel 6 Max:</label>"
"<input type='number' name='ch6_max' min='0' max='4095'>"
"</div>"
"<h3>Per-Channel Servo & Rate Configuration</h3>"
"<div style='background: #f0f0f0; padding: 15px; border-radius: 5px;'>"
"<div style='display: grid; grid-template-columns: repeat(3, 1fr); gap: 15px;'>"
"<div><strong>Channel 1</strong>"
"<div><label>Min (µs):</label><input type='number' name='ch1_smin' min='500' max='2500'></div>"
"<div><label>Center (µs):</label><input type='number' name='ch1_sctr' min='500' max='2500'></div>"
"<div><label>Max (µs):</label><input type='number' name='ch1_smax' min='500' max='2500'></div>"
"<div><label>Expo (0.0-1.0):</label><input type='number' name='ch1_expo' min='0' max='1' step='0.05'></div>"
"</div>"
"<div><strong>Channel 2</strong>"
"<div><label>Min (µs):</label><input type='number' name='ch2_smin' min='500' max='2500'></div>"
"<div><label>Center (µs):</label><input type='number' name='ch2_sctr' min='500' max='2500'></div>"
"<div><label>Max (µs):</label><input type='number' name='ch2_smax' min='500' max='2500'></div>"
"<div><label>Expo (0.0-1.0):</label><input type='number' name='ch2_expo' min='0' max='1' step='0.05'></div>"
"</div>"
"<div><strong>Channel 3</strong>"
"<div><label>Min (µs):</label><input type='number' name='ch3_smin' min='500' max='2500'></div>"
"<div><label>Center (µs):</label><input type='number' name='ch3_sctr' min='500' max='2500'></div>"
"<div><label>Max (µs):</label><input type='number' name='ch3_smax' min='500' max='2500'></div>"
"<div><label>Expo (0.0-1.0):</label><input type='number' name='ch3_expo' min='0' max='1' step='0.05'></div>"
"</div>"
"<div><strong>Channel 4</strong>"
"<div><label>Min (µs):</label><input type='number' name='ch4_smin' min='500' max='2500'></div>"
"<div><label>Center (µs):</label><input type='number' name='ch4_sctr' min='500' max='2500'></div>"
"<div><label>Max (µs):</label><input type='number' name='ch4_smax' min='500' max='2500'></div>"
"<div><label>Expo (0.0-1.0):</label><input type='number' name='ch4_expo' min='0' max='1' step='0.05'></div>"
"</div>"
"<div><strong>Channel 5</strong>"
"<div><label>Min (µs):</label><input type='number' name='ch5_smin' min='500' max='2500'></div>"
"<div><label>Center (µs):</label><input type='number' name='ch5_sctr' min='500' max='2500'></div>"
"<div><label>Max (µs):</label><input type='number' name='ch5_smax' min='500' max='2500'></div>"
"<div><label>Expo (0.0-1.0):</label><input type='number' name='ch5_expo' min='0' max='1' step='0.05'></div>"
"</div>"
"<div><strong>Channel 6</strong>"
"<div><label>Min (µs):</label><input type='number' name='ch6_smin' min='500' max='2500'></div>"
"<div><label>Center (µs):</label><input type='number' name='ch6_sctr' min='500' max='2500'></div>"
"<div><label>Max (µs):</label><input type='number' name='ch6_smax' min='500' max='2500'></div>"
"<div><label>Rate (0.0-1.0):</label><input type='number' name='ch6_rate' min='0' max='1' step='0.05'></div>"
"</div>"
"</div>"
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
"    "
"    // Update channel bars and values (servo microseconds 1000-2000us)"
"    for (let i = 0; i < 6; i++) {"
"      const chNum = i + 1;"
"      const us = d.servo_us[i];"
"      const pct = ((us - 1000) / 1000 * 100).toFixed(0);"
"      document.getElementById('ch' + chNum + '_val').textContent = us + 'µs';"
"      document.getElementById('ch' + chNum + '_bar').style.width = pct + '%';"
"    }"
"    "
"    // Update light indicators"
"    for (let i = 0; i < 4; i++) {"
"      const lightEl = document.getElementById('light' + (i + 1));"
"      const isOn = (d.lights & (1 << i)) ? true : false;"
"      if (isOn) {"
"        lightEl.style.background = '#FFD700';"
"        lightEl.style.borderColor = '#FFA500';"
"        lightEl.style.boxShadow = '0 0 10px rgba(255, 215, 0, 0.8)';"
"      } else {"
"        lightEl.style.background = '#ccc';"
"        lightEl.style.borderColor = '#999';"
"        lightEl.style.boxShadow = 'none';"
"      }"
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
"  for (let i = 1; i <= 6; i++) {"
"    document.querySelector('[name=ch' + i + '_min]').value = d['ch' + i + '_min'];"
"    document.querySelector('[name=ch' + i + '_max]').value = d['ch' + i + '_max'];"
"    document.querySelector('[name=ch' + i + '_smin]').value = d['ch' + i + '_smin'];"
"    document.querySelector('[name=ch' + i + '_sctr]').value = d['ch' + i + '_sctr'];"
"    document.querySelector('[name=ch' + i + '_smax]').value = d['ch' + i + '_smax'];"
"    document.querySelector('[name=ch' + i + '_expo]').value = d['ch' + i + '_expo'];"
"  }"
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
    
    snprintf(response, sizeof(response),
             "{"
             "\"connected\":%d,"
             "\"rssi\":%d,"
             "\"last_packet\":%lu,"
             "\"ch\":[%u,%u,%u,%u,%u,%u],"
             "\"servo_us\":[%u,%u,%u,%u,%u,%u],"
             "\"lights\":%u"
             "}",
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
    
    // Parse calibration for all 6 channels
    for (int i = 0; i < 6; i++) {
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
    for (int i = 0; i < 6; i++) {
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
