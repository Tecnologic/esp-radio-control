# ESP-NOW Radio Control

A unified ESP-NOW peer-to-peer wireless radio control system for robotics and remote-controlled devices, built on the **ESP32-S2 LOLIN Mini** board. Features runtime-selectable sender/receiver roles, persistent NVS configuration, web-based settings management, 4-channel light control, and comprehensive LED status indication.

## Table of Contents

- [Features](#features)
- [Hardware Setup](#hardware-setup)
- [Building and Flashing](#building-and-flashing)
- [Operation Modes](#operation-modes)
- [Configuration](#configuration)
- [LED Status Indicators](#led-status-indicators)
- [Button Functions](#button-functions)
- [Light Control](#light-control)
- [API Reference](#api-reference)
- [Troubleshooting](#troubleshooting)

## Features

- **Unified Firmware**: Single codebase supports both sender (RC transmitter) and receiver (motor/servo controller)
- **Runtime Role Selection**: Switch between sender and receiver via webserver without reflashing
- **Persistent Configuration**: All settings saved to NVS flash (peer MAC, channel, ADC ranges, rate mode)
- **Web-Based Management**: HTTP webserver on port 80 with JSON API for remote configuration
- **4-Channel Light Control**: Toggle 4 independent loads via buttons and webserver
- **ESP-NOW Communication**: Low-latency peer-to-peer wireless at ~100ms packet rate
- **Servo Control**: Two PWM servo outputs (throttle and steering) at 50 Hz
- **Real-Time Status**: LED patterns and webserver display show connection state and RSSI
- **USB Serial Logging**: Full ESP_LOG output over USB CDC for debugging

## Hardware Setup

### Board
- **Microcontroller**: ESP32-S2 LOLIN Mini
- **Voltage**: 5V USB power (micro-USB connector)
- **Flash**: 4 MB
- **RAM**: 320 KB

### Pin Assignments

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| **GPIO0** | User Button | Input | Long press (3s): toggle webserver; active-low with internal pull-up |
| **GPIO15** | Status LED | Output | Indicator of operation state and connection status |
| **GPIO4** | Servo PWM (Throttle) | Output | 50 Hz PWM for RC receiver channel 1 |
| **GPIO5** | Servo PWM (Steering) | Output | 50 Hz PWM for RC receiver channel 2 |
| **GPIO6** | Light Button 1 | Input | Toggle light 1; active-low with internal pull-up |
| **GPIO7** | Light Button 2 | Input | Toggle light 2; active-low with internal pull-up |
| **GPIO8** | Light Button 3 | Input | Toggle light 3; active-low with internal pull-up |
| **GPIO9** | Light Button 4 | Input | Toggle light 4; active-low with internal pull-up |
| **GPIO10** | Light Output 1 | Output | GPIO digital output for light 1 |
| **GPIO11** | Light Output 2 | Output | GPIO digital output for light 2 |
| **GPIO12** | Light Output 3 | Output | GPIO digital output for light 3 |
| **GPIO13** | Light Output 4 | Output | GPIO digital output for light 4 |
| **GPIO1-2** | ADC Channels | Analog Input | ADC1 channel 0-1 for throttle/steering (sender only) |
| **GPIO20-21** | USB D+/D- | Communication | USB CDC virtual COM port for logging |

### Sender Hardware Example

```
┌─────────────────────────────┐
│   ESP32-S2 LOLIN Mini       │
├─────────────────────────────┤
│ GPIO0  ─[Button]────GND     │
│ GPIO6  ─[Light BTN 1]──GND  │
│ GPIO7  ─[Light BTN 2]──GND  │
│ GPIO8  ─[Light BTN 3]──GND  │
│ GPIO9  ─[Light BTN 4]──GND  │
│ GPIO1  ─[Potentiometer]─    │
│ GPIO2  ─[Potentiometer]─    │
│ GPIO15 ─[LED]─────[150Ω]──GND
│ GND    ─[GND of buttons]    │
│ 5V     ─[5V from USB]       │
└─────────────────────────────┘
```

### Receiver Hardware Example

```
┌──────────────────────────────┐
│   ESP32-S2 LOLIN Mini        │
├──────────────────────────────┤
│ GPIO0  ─[User Button]───GND  │
│ GPIO6  ─[Light BTN 1]───GND  │
│ GPIO7  ─[Light BTN 2]───GND  │
│ GPIO8  ─[Light BTN 3]───GND  │
│ GPIO9  ─[Light BTN 4]───GND  │
│ GPIO4  ─[Servo1 PWM]         │
│ GPIO5  ─[Servo2 PWM]         │
│ GPIO10 ─[Relay 1]    ─GND    │
│ GPIO11 ─[Relay 2]    ─GND    │
│ GPIO12 ─[Relay 3]    ─GND    │
│ GPIO13 ─[Relay 4]    ─GND    │
│ GPIO15 ─[LED]────[150Ω]──GND │
│ GND    ─[GND of buttons]     │
│ 5V     ─[5V from USB]        │
└──────────────────────────────┘
```

## Building and Flashing

### Prerequisites

- **ESP-IDF 5.4.1** (or compatible) installed via PlatformIO
- **PlatformIO Core** and VS Code PlatformIO extension
- **USB cable** (micro-USB) for flashing and serial monitor

### Build

```bash
cd ~/projects/esp-radio-control
platformio run
```

Build artifacts stored in `.pio/build/esp32s2_lolin/`

### Flash to Device

Plug in the board via USB. The board will appear as `/dev/ttyUSB0` or `/dev/ttyACM0` (Linux).

```bash
platformio run --target upload
```

### Serial Monitor

Monitor logs in real-time (115200 baud):

```bash
platformio device monitor
```

Example output:
```
[    54][main]: ESP-NOW Radio Control starting...
[    54][shared]: WiFi initialized in STA mode
[   154][shared]: ESP-NOW initialized
[   154][main]: Initialization complete. Device role: SENDER
[  1154][control]: Starting SENDER
[  2154][sender]: Sender task started
[  5154][sender]: Sent throttle=2048, steering=2048, lights=0x00
```

## Operation Modes

### Sender Mode

**Role**: Transmits RC commands (ADC values for throttle/steering) and light states to receiver.

**Typical Use**: RC transmitter, joystick controller, motion controller

**Operation**:
1. Reads ADC channels (GPIO1-2) for analog input (e.g., joysticks)
2. Reads light button states (GPIO6-9) for toggle control
3. Maps ADC values to servo microsecond range (1000-2000 µs) with rate scaling
4. Transmits `control_packet_t` via ESP-NOW at ~50 Hz
5. LED shows connection status to receiver

### Receiver Mode

**Role**: Receives control packets and applies PWM servo commands and light GPIO outputs.

**Typical Use**: Vehicle motor controller, robot manipulator, quad-rotor receiver

**Operation**:
1. Registers ESP-NOW receive callback
2. Processes incoming control packets
3. Applies throttle/steering as 50 Hz PWM to GPIO4-5
4. Sets light outputs (GPIO10-13) as digital GPIOs
5. Monitors connection; clears "connected" flag if no packet for 1 second

### Concurrent Operation

Both sender and receiver tasks can run simultaneously. However:
- **When webserver is OFF**: The configured role (sender/receiver) is active, ESP-NOW operational
- **When webserver is ON**: ESP-NOW operation is halted; device enters configuration mode
- **Long Press User Button (3s)**: Toggle webserver on/off

## Configuration

### Web Interface

Access the web interface at `http://192.168.4.1` (Wi-Fi AP mode) when webserver is running.

**URL**: http://192.168.4.1  
**Port**: 80  
**Authentication**: None (device is in AP mode; no external internet access)

### Configuration Parameters

#### Device Role

- **Receiver** (0): Applies received servo PWM and light outputs
- **Sender** (1): Reads ADC and transmits control packets

#### Peer MAC Address

MAC address of the ESP-NOW peer device. Format: `XX:XX:XX:XX:XX:XX`

Example:
- Sender peer MAC: `84:F7:03:B2:F1:C4` (receiver's MAC)
- Receiver peer MAC: `84:F7:03:B2:F1:C5` (sender's MAC)

Find your device MAC in serial log: `[shared]: WiFi initialized...`

#### ESP-NOW Channel

1-13 (default: 1). Both devices must use same channel for communication.

#### ADC Calibration (Sender Only)

- **Throttle Min**: ADC value at minimum position (e.g., 0)
- **Throttle Max**: ADC value at maximum position (e.g., 4095)
- **Steering Min**: ADC value at left position (e.g., 0)
- **Steering Max**: ADC value at right position (e.g., 4095)

Servos will map to 1000-2000 µs range.

#### Rate Mode

- **Low** (0): ×0.5 scale on servo range (for precise control)
- **High** (1): ×1.0 scale on servo range (full range)

Controlled by light button 4 (GPIO9 toggle sends bit 3 of lights byte).

### API Endpoints

#### GET /api/settings

Returns current configuration as JSON.

```bash
curl http://192.168.4.1/api/settings
```

Response:
```json
{
  "device_role": 0,
  "peer_mac": "84:f7:03:b2:f1:c5",
  "channel": 1,
  "throttle_min": 0,
  "throttle_max": 4095,
  "steering_min": 0,
  "steering_max": 4095,
  "rate_mode": 0
}
```

#### POST /api/settings

Update configuration.

```bash
curl -X POST http://192.168.4.1/api/settings \
  -H "Content-Type: application/json" \
  -d '{
    "device_role": 1,
    "peer_mac": "84:f7:03:b2:f1:c4",
    "channel": 1,
    "throttle_min": 100,
    "throttle_max": 3900,
    "steering_min": 100,
    "steering_max": 3900,
    "rate_mode": 0
  }'
```

Response:
```json
{"message": "Settings saved"}
```

#### GET /api/status

Returns real-time connection status.

```bash
curl http://192.168.4.1/api/status
```

Response:
```json
{
  "connected": 1,
  "rssi": -65,
  "last_packet": 1234567890
}
```

| Field | Type | Notes |
|-------|------|-------|
| `connected` | int (0/1) | 1 = receiving packets; 0 = no packets for 1+ second |
| `rssi` | int (dBm) | Signal strength, -120 to 0; -120 indicates disconnected |
| `last_packet` | uint32_t | FreeRTOS tick count when last packet received |

## LED Status Indicators

The status LED (GPIO15) provides visual feedback about device operation state. Observe the LED pattern to understand current status.

### LED Pattern Reference

| State | Condition | Pattern | Duration | Interpretation |
|-------|-----------|---------|----------|-----------------|
| **A** | Disconnected + No Webserver | ■ 100ms OFF 100ms ON 100ms OFF 300ms | 600ms cycle | Device ready, awaiting peer connection |
| **B** | Connected + No Webserver | ■ 500ms OFF 500ms | 1000ms cycle | Communication active with peer |
| **C** | Disconnected + Webserver On | ■ 250ms OFF 250ms | 500ms cycle | In configuration mode, no peer |
| **D** | Connected + Webserver On | ■ 80ms (3× blink) OFF 200ms | 440ms cycle | Configuration mode with active peer |

**Legend**: ■ = LED on, blank = LED off

### Visual Interpretation

```
State A (Double-blink): ■ _ ■ ___  ■ _ ■ ___  ... (slow, 2 pulses)
State B (Slow-blink):   ■ ___     ■ ___     ... (1 Hz, steady)
State C (Fast-blink):   ■ _ ■ _   ■ _ ■ _   ... (2 Hz, rapid)
State D (Triple-blink): ■ _ ■ _ ■ _____     ... (fast pulses, pause)
```

## Button Functions

| Button | GPIO | Function | Action |
|--------|------|----------|--------|
| **User** | GPIO0 | Config Toggle | Long press (3s): Toggle webserver on/off |
| **Light 1** | GPIO6 | Toggle Light 1 | Quick press: Toggle bit 0 of light states |
| **Light 2** | GPIO7 | Toggle Light 2 | Quick press: Toggle bit 1 of light states |
| **Light 3** | GPIO8 | Toggle Light 3 | Quick press: Toggle bit 2 of light states |
| **Light 4** | GPIO9 | Toggle Light 4 / Rate | Quick press: Toggle bit 3 / Sender uses for rate mode |

### Button Behavior

- **Active-Low**: Buttons pull GPIO to GND when pressed
- **Debouncing**: 10 ms polling cycle in control task
- **Edge Detection**: Falling edge (1→0) triggers action
- **Light State Persistence**: Light toggles maintained in memory (sender mode); sent in every packet

## Light Control

### Sender Light Control

On the sender, press light buttons (GPIO6-9) to toggle local light states. These states are packed into bit 0-3 of the lights field in the control packet:

```c
control_packet_t {
    uint16_t throttle;    // ADC value for throttle servo
    uint16_t steering;    // ADC value for steering servo
    uint8_t lights;       // Bits 0-3: light 1-4, Bit 3: rate mode indicator
};
```

Transmitted at ~50 Hz.

### Receiver Light Output

Upon receiving a packet, the receiver sets GPIO10-13 as digital outputs based on the lights byte:

- **GPIO10**: Light 1 (bit 0)
- **GPIO11**: Light 2 (bit 1)
- **GPIO12**: Light 3 (bit 2)
- **GPIO13**: Light 4 (bit 3 / rate mode flag)

Level is HIGH (3.3V) when bit is set, LOW (GND) when clear. Use external transistors/relays for higher current loads.

### Webserver Light Control

When webserver is running, lights can also be toggled via the web interface buttons (future enhancement).

## API Reference

### Control Packet Structure

```c
typedef struct {
    uint16_t throttle;
    uint16_t steering;
    uint8_t lights;
} control_packet_t;
```

| Field | Type | Range | Notes |
|-------|------|-------|-------|
| `throttle` | uint16_t | 0-4095 | ADC raw 12-bit value; mapped to 1000-2000 µs servo range |
| `steering` | uint16_t | 0-4095 | ADC raw 12-bit value; mapped to 1000-2000 µs servo range |
| `lights` | uint8_t | 0-15 | Bits 0-3: light toggle states |

### Connection Status Structure

```c
typedef struct {
    bool connected;
    int8_t rssi;
    uint32_t last_packet;
} connection_status_t;
```

| Field | Type | Meaning |
|-------|------|---------|
| `connected` | bool | true = packets received within 1s; false = timeout or idle |
| `rssi` | int8_t | Signal strength in dBm (-120 to 0); -120 when disconnected |
| `last_packet` | uint32_t | FreeRTOS tick count (ms) of last received packet |

## Troubleshooting

### Device Not Responding to Commands

1. **Check Connection**:
   - Verify peer MAC address matches in both devices' settings
   - Confirm both devices on same ESP-NOW channel (1-13)
   - Use `/api/status` endpoint to check RSSI and connection flag

2. **Check LED Pattern**:
   - Pattern A or C (disconnected): No peer connection
   - Pattern B or D (connected): Packets are flowing

3. **Check Serial Log**:
   ```bash
   platformio device monitor
   ```
   - Look for "Sender task started" or "Receiver task started"
   - Check for any error messages (ERR log level)

### Webserver Not Accessible

1. **Device not in AP mode**: Long-press user button (GPIO0) for 3+ seconds to toggle webserver
2. **Incorrect IP**: Device uses default IP `192.168.4.1`; ensure you're on the right network
3. **USB Power**: Device must remain powered (5V via USB) for webserver to run

### ADC Readings Incorrect (Sender)

1. **Calibrate ADC Range**:
   - Move analog input (joystick) to minimum position
   - Record ADC value from serial log: `[sender]: ADC throttle=XXXX`
   - Move to maximum and record value
   - Update throttle_min and throttle_max via webserver

2. **Check Potentiometer Connections**:
   - Verify GPIO1-2 are connected to analog voltage divider
   - Voltage should be 0-3.3V

### Servo Not Moving (Receiver)

1. **Check PWM Output**:
   - Oscilloscope on GPIO4 or GPIO5
   - Should see 50 Hz square wave (20ms period)
   - Pulse width: 1000-2000 µs

2. **Check Receiver Compatibility**:
   - Standard RC servos expect:
     - 1000 µs: Full reverse
     - 1500 µs: Center
     - 2000 µs: Full forward
   - Verify servo pulse interpretation

3. **Calibration**:
   - Use webserver to set throttle_min/max to match joystick range
   - Ensure rate_mode is set correctly for desired response

### Build Errors

**CMake IDF_PATH Error**:
```
CMake Error at CMakeLists.txt:2 (include): include could not find requested file:
  /tools/cmake/project.cmake
```

**Solution**: Ensure ESP-IDF 5.4.1 is installed and `IDF_PATH` environment variable is set:
```bash
export IDF_PATH=/path/to/esp-idf
```

**Undefined Reference Errors**:

Ensure all component files are in `src/CMakeLists.txt`:
```cmake
set(COMMON_SOURCES "main.c" "shared.c" "sender.c" "receiver.c" "settings.c" "webserver.c")
```

## Project Structure

```
esp-radio-control/
├── CMakeLists.txt              # Main ESP-IDF build config
├── platformio.ini              # PlatformIO environment settings
├── sdkconfig.esp32s2_lolin     # Hardware-specific Kconfig
├── README.md                   # This file
├── include/
│   └── README                  # Placeholder
├── lib/
│   └── README                  # Placeholder
├── src/
│   ├── CMakeLists.txt          # Source build config
│   ├── main.c                  # Entry point, control task, LED state machine
│   ├── common.h                # Shared definitions, pin mappings, data structures
│   ├── shared.c                # WiFi init, ESP-NOW init, utility functions
│   ├── sender.c                # ADC reading, packet transmission
│   ├── receiver.c              # Packet reception, servo PWM, light GPIO output
│   ├── settings.h/c            # NVS persistent configuration storage
│   ├── webserver.h/c           # HTTP server with JSON API
└── test/
    └── README                  # Test placeholder
```

## Contributing

Improvements and bug reports welcome! Key areas for enhancement:

- [ ] Webserver light toggle controls
- [ ] Rate-adaptive ADC filtering for smoother servo response
- [ ] OTA firmware update support
- [ ] Multi-peer support (broadcast to multiple receivers)
- [ ] IMU telemetry feedback from receiver to sender
- [ ] BLE fallback communication for extended range

## License

MIT License - See LICENSE file for details (if present)

## References

- [ESP32-S2 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s2_datasheet_en.pdf)
- [LOLIN S2 Mini Board Documentation](https://www.wemos.cc/en/latest/s2/s2_mini.html)
- [ESP-NOW Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s2/api-reference/network/esp_now.html)
- [ESP-IDF HTTP Server Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s2/api-reference/protocols/esp_http_server.html)
- [ESP-IDF LEDC (PWM) Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s2/api-reference/peripherals/ledc.html)

---

**Last Updated**: 2025
**Firmware Version**: 1.0.0
