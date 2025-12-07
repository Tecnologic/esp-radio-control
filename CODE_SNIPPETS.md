# Key Code Snippets: Concurrent ESP-NOW + Status Tracking

## 1. Connection Status Struct and API (common.h)

```c
// Connection status tracking
typedef struct {
    bool connected;          // Is peer connected
    int8_t rssi;            // Signal strength (-120 to 0 dBm)
    uint32_t last_packet;   // Timestamp of last packet
} connection_status_t;

// Status functions
connection_status_t get_connection_status(void);
void update_connection_status(bool connected, int8_t rssi);
```

## 2. Connection Status Implementation (shared.c)

```c
// Connection status tracking
static connection_status_t conn_status = {
    .connected = false,
    .rssi = -120,
    .last_packet = 0
};

connection_status_t get_connection_status(void) {
    return conn_status;
}

void update_connection_status(bool connected, int8_t rssi) {
    conn_status.connected = connected;
    conn_status.rssi = rssi;
    conn_status.last_packet = xTaskGetTickCount();
}
```

## 3. ESP-NOW Receive Callback with RSSI (receiver.c)

```c
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
```

## 4. Connection Timeout Detection (receiver.c)

```c
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
        
        // ... rest of receiver task ...
    }
}
```

## 5. Webserver Status API Handler (webserver.c)

```c
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
```

## 6. Webserver Handler Registration (webserver.c)

```c
void webserver_start(device_settings_t *settings) {
    // ... existing code ...

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 2;
    config.max_uri_handlers = 5;  // Increased from 4 to 5

    // ... existing handlers ...

    httpd_uri_t uri_status = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = handler_get_status,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server, &uri_status);

    ESP_LOGI(TAG, "Webserver started on http://192.168.4.1");
}
```

## 7. 4-State LED Pattern Machine (main.c)

```c
// LED pattern controller for 4 states
static void led_update(void) {
    TickType_t now = xTaskGetTickCount();
    connection_status_t conn_status = get_connection_status();
    bool webserver_active = webserver_is_running();
    
    // 4 states:
    // A: Disconnected (no ESP-NOW packets), Webserver off    -> Double blink (100ms on, 100ms off, 100ms on, 300ms off)
    // B: Connected (receiving packets), Webserver off        -> Slow blink (500ms on, 500ms off)
    // C: Disconnected (no ESP-NOW packets), Webserver on     -> Fast blink (250ms on, 250ms off)
    // D: Connected (receiving packets), Webserver on         -> Triple blink (80ms on, 80ms off x3, 200ms off)
    
    bool led_on = false;
    
    if (!conn_status.connected && !webserver_active) {
        // State A: Double blink (600ms cycle)
        uint32_t cycle = now % 600;
        led_on = (cycle < 100) || (cycle >= 200 && cycle < 300);
    } else if (conn_status.connected && !webserver_active) {
        // State B: Slow blink (1000ms cycle)
        uint32_t cycle = now % 1000;
        led_on = (cycle < 500);
    } else if (!conn_status.connected && webserver_active) {
        // State C: Fast blink (500ms cycle)
        uint32_t cycle = now % 500;
        led_on = (cycle < 250);
    } else {
        // State D: Triple blink (440ms cycle)
        uint32_t cycle = now % 440;
        led_on = (cycle < 80) || (cycle >= 160 && cycle < 240);
    }
    
    led_set(led_on);
}
```

## 8. Concurrent Operation Control Loop (main.c)

```c
static void control_task(void *arg) {
    uint32_t button_press_count = 0;
    uint8_t light_states = 0; // Bit mask for 4 lights
    uint8_t light_btn_state[4] = {1, 1, 1, 1};
    const uint8_t light_pins[4] = {PIN_LIGHT_BTN1, PIN_LIGHT_BTN2, PIN_LIGHT_BTN3, PIN_LIGHT_BTN4};

    while (1) {
        // Check user button for config mode
        if (gpio_get_level(PIN_USER_BUTTON) == 0) {
            button_press_count++;
            // Long press: toggle webserver (now independent of ESP-NOW)
            if (button_press_count > 300) { // ~3 seconds at 10ms poll
                if (webserver_is_running()) {
                    ESP_LOGI(TAG, "Stopping webserver...");
                    webserver_stop();
                } else {
                    ESP_LOGI(TAG, "Starting webserver...");
                    webserver_start(&current_settings);
                }
                button_press_count = 0;
            }
        } else {
            button_press_count = 0;
        }

        // Check light buttons for toggle
        for (int i = 0; i < 4; i++) {
            uint8_t btn_level = gpio_get_level(light_pins[i]);
            if (light_btn_state[i] == 1 && btn_level == 0) {
                light_states ^= (1 << i);
                ESP_LOGI(TAG, "Light %d toggled, states: 0x%02x", i + 1, light_states);
                if (is_running && current_settings.device_role == ROLE_SENDER) {
                    sender_set_light_states(light_states);
                }
            }
            light_btn_state[i] = btn_level;
        }

        // Start/stop appropriate role if not in config mode
        if (!webserver_is_running()) {
            if (!is_running) {
                ESP_LOGI(TAG, "Starting %s", 
                         current_settings.device_role == ROLE_SENDER ? "SENDER" : "RECEIVER");
                if (current_settings.device_role == ROLE_SENDER) {
                    sender_start(current_settings.peer_mac);
                } else {
                    receiver_start();
                }
                is_running = true;
            }
        } else {
            // Webserver active: stop ESP-NOW operation if it was running
            if (is_running) {
                ESP_LOGI(TAG, "Stopping ESP-NOW (webserver active)");
                if (current_settings.device_role == ROLE_SENDER) {
                    sender_stop();
                } else {
                    receiver_stop();
                }
                is_running = false;
            }
        }

        // Update LED pattern based on connection state and webserver status
        led_update();

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## 9. Webserver Status UI JavaScript (webserver.c HTML)

```javascript
// Update status display every 500ms
setInterval(function() {
  fetch('/api/status')
  .then(r => r.json())
  .then(d => {
    const connStatus = document.getElementById('connStatus');
    const rssiValue = document.getElementById('rssiValue');
    
    if (d.connected) {
      connStatus.textContent = 'Connected';
      connStatus.className = 'status-value status-connected';
      rssiValue.textContent = d.rssi + ' dBm';
    } else {
      connStatus.textContent = 'Disconnected';
      connStatus.className = 'status-value status-disconnected';
      rssiValue.textContent = 'N/A';
    }
  })
  .catch(e => console.log('Status fetch error'));
}, 500);
```

## 10. LED Pattern Timing Reference

```
State A (Disconnected + No Webserver):
    100ms ON, 100ms OFF, 100ms ON, 300ms OFF = 600ms cycle
    Visual: ■ _ ■ ___  ■ _ ■ ___  ...

State B (Connected + No Webserver):
    500ms ON, 500ms OFF = 1000ms cycle
    Visual: ■ ___  ■ ___  ...

State C (Disconnected + Webserver On):
    250ms ON, 250ms OFF = 500ms cycle
    Visual: ■ _ ■ _ ■ _ ...

State D (Connected + Webserver On):
    80ms ON, 80ms OFF, 80ms ON, 80ms OFF, 80ms ON, 200ms OFF = 440ms cycle
    Visual: ■ _ ■ _ ■ _____  ■ _ ■ _ ■ _____  ...
```

## Testing Commands

```bash
# Build the project
platformio run

# Flash to device
platformio run --target upload

# Monitor serial output
platformio device monitor

# Check connection status (in another terminal)
curl http://192.168.4.1/api/status

# Get current settings
curl http://192.168.4.1/api/settings

# Update settings
curl -X POST http://192.168.4.1/api/settings \
  -H "Content-Type: application/json" \
  -d '{
    "device_role": 0,
    "peer_mac": "84:f7:03:b2:f1:c5",
    "channel": 1,
    "throttle_min": 0,
    "throttle_max": 4095,
    "steering_min": 0,
    "steering_max": 4095,
    "rate_mode": 0
  }'
```
