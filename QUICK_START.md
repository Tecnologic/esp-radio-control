# Quick Start Guide: ESP-NOW Radio Control Setup

## 5-Minute Setup

### 1. Flash Firmware
```bash
cd ~/projects/esp-radio-control
platformio run --target upload
```

### 2. Check Serial Output
```bash
platformio device monitor
# Look for: "Initialization complete. Device role: SENDER" (or RECEIVER)
```

### 3. Access Webserver
Press and hold **User Button (GPIO0)** for 3+ seconds to start webserver.
- LED will change pattern (faster blink = webserver active)
- Open browser: `http://192.168.4.1`

### 4. Configure Peers

**On Sender Device** (via webserver):
1. Note receiver's MAC address from receiver's serial log
2. Set "Device Role" = **Sender**
3. Set "Peer MAC Address" = receiver's MAC
4. Click "Save Settings"

**On Receiver Device** (via webserver):
1. Note sender's MAC address from sender's serial log
2. Set "Device Role" = **Receiver**
3. Set "Peer MAC Address" = sender's MAC
4. Click "Save Settings"

### 5. Exit Webserver & Start Communication
Press User Button for 3+ seconds again to exit webserver and start ESP-NOW.
- LED should show **State B pattern** (slow blink = connected)

## Device Status at a Glance

| LED Pattern | Meaning |
|-------------|---------|
| **Double-blink** | Disconnected from peer |
| **Slow blink** | Connected and communicating |
| **Fast blink** | In webserver config mode (disconnected) |
| **Triple-blink** | In webserver config mode (connected) |

## Getting Your Device MAC

On device serial monitor, look for:
```
[shared]: WiFi initialized in STA mode
MAC: 84:F7:03:B2:F1:C5
```

## Testing Communication

### Method 1: Check Status via API
```bash
curl http://192.168.4.1/api/status
# Response: {"connected":1,"rssi":-65,"last_packet":1234567}
```

### Method 2: Check Light Control (Sender)
- Press Light Button 1 (GPIO6)
- On receiver device, GPIO10 should toggle (measure with multimeter or LED)

### Method 3: Monitor Servo Output (Receiver)
- On sender: Move analog input (ADC on GPIO1-2)
- On receiver: Measure GPIO4-5 with oscilloscope (PWM should vary)
- Pulse width range: 1000-2000 µs at 50 Hz

## Configuration Profiles

### RC Vehicle Setup (Sender + Receiver)

**Sender Settings**:
- Device Role: Sender
- Peer MAC: (receiver's MAC)
- Channel: 1
- Throttle Min: 100 (rest position)
- Throttle Max: 3900 (full forward)
- Steering Min: 100 (left)
- Steering Max: 3900 (right)
- Rate Mode: Low

**Receiver Settings**:
- Device Role: Receiver
- Peer MAC: (sender's MAC)
- Channel: 1
- Throttle Min: 100
- Throttle Max: 3900
- Steering Min: 100
- Steering Max: 3900
- Rate Mode: Low

### Light Control Setup

**Physical Setup**:
1. Connect relay/transistor to GPIO10-13 outputs
2. Wire relay coils to 12V supply and ground
3. Wire normally-open contacts in series with 12V loads

**Control**:
- Press Light Button 1-4 to toggle respective output
- Monitor GPIO10-13 with multimeter (HIGH/LOW = 3.3V/GND)

## Troubleshooting Quick Ref

| Problem | Solution |
|---------|----------|
| Can't access webserver | Long-press user button 3+ seconds; wait for fast blink |
| Device keeps disconnecting | Check both devices on same ESP-NOW channel |
| No servo movement | Check PWM output on GPIO4-5 with oscilloscope |
| ADC readings wrong | Recalibrate throttle/steering min/max via webserver |
| Settings not saved | Check NVS flash isn't full (use `nvs_flash_erase` if needed) |

## Power Consumption

- **Idle (webserver on)**: ~50 mA at 5V
- **Communicating (ESP-NOW)**: ~80 mA at 5V
- **Both**: ~100 mA at 5V

Powered by 5V USB; suitable for bench testing. For field deployment, use 5V battery pack (e.g., USB power bank).

## Next Steps

1. **Add More Sensors**: Expand ADC channels or add I2C sensors
2. **Increase Range**: Use external antenna (requires PCB mod)
3. **Add Telemetry**: Implement bidirectional packets with feedback from receiver
4. **Multi-Peer**: Extend code to support multiple receivers (broadcast)
5. **OTA Updates**: Implement firmware updates over-the-air via webserver

## Hardware Pinout Reference

```
ESP32-S2 LOLIN Mini Pinout:
┌────────────────────────────┐
│ 3V3  GND  GPIO36  GPIO37   │
│ GPIO9(BTN4) GPIO8(BTN3)    │
│ GPIO7(BTN2) GPIO6(BTN1)    │
│ GPIO5(SRV2) GPIO4(SRV1)    │
│ GPIO3       GPIO2(ADC2)    │
│ GPIO1(ADC1) GPIO0(BTN)     │
│ GPIO13(OUT4) GPIO12(OUT3)  │
│ GPIO11(OUT2) GPIO10(OUT1)  │
│ GPIO15(LED) GPIO14         │
│ 5V   GND   TX   RX        │
└────────────────────────────┘
```

## Support Resources

- **Serial Monitor**: Run `platformio device monitor` for detailed logs
- **API Docs**: See README.md section "API Reference"
- **Code Examples**: See CODE_SNIPPETS.md for implementation details
- **Project Structure**: See README.md section "Project Structure"

## Safety Notes

- Device runs at 5V USB; safe for bench work
- GPIO outputs are 3.3V logic; use level shifters for 5V loads
- Light outputs can drive ~10mA per GPIO directly; use external transistors for higher loads
- Ensure proper power supply decoupling (0.1µF cap across 5V/GND near board)

---

**Ready to go!** Start with a single sender-receiver pair, then expand functionality as needed.
