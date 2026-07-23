# SIM800L SMS Gateway (ESP8266/ESP32)

A complete SMS gateway solution using ESP8266/ESP32 + SIM800L module with WiFi captive portal for easy configuration. Forwards SMS messages to Telegram and WhatsApp.

## Features

- ✅ SIM800L GSM/GPRS module support
- ✅ WiFi connectivity (ESP8266 / ESP32)
- ✅ Captive portal for easy setup (no hardcoded WiFi)
- ✅ SMS receive & forward to backend API
- ✅ Telegram bot integration
- ✅ WhatsApp bridge support
- ✅ Web-based configuration dashboard
- ✅ JSON configuration storage

## Hardware Requirements

### Microcontroller
- **ESP32** (recommended) - more memory, faster
- **ESP8266** - lower cost alternative

### SIM800L Module
- SIM800L GSM/GPRS module
- Voltage regulator (3.3V, >2A)
- Reset circuit (capacitor + resistor)
- SIM card (GSM capable)

### Wiring Diagram

```
ESP32 Pin 16 (RX2) → SIM800L TX
ESP32 Pin 17 (TX2) → SIM800L RX
ESP32 Pin 5        → SIM800L RST (via pullup)
ESP32 GND          → SIM800L GND (common ground)
5V Power Supply    → SIM800L VCC (via 3.3V regulator)
```

See `wiring.md` for complete schematics.

## Installation

### 1. Install PlatformIO
```bash
pip install platformio
```

### 2. Clone and Setup
```bash
cd esp
platformio run --target upload -e esp32
# or for ESP8266:
# platformio run --target upload -e esp8266
```

### 3. Monitor Serial Output
```bash
platformio device monitor --baud 115200
```

## Configuration

### Initial Setup (Captive Portal)

1. **Power on the device** — it will create a WiFi AP named `SIM800L-Gateway`
2. **Connect** to this AP (password: `12345678`)
3. **Open browser** → `http://192.168.4.1/`
4. **Configure tabs:**
   - **WiFi:** Enter your home WiFi SSID + password
   - **API:** Backend URL + API key (from Android SMS Gateway)
   - **Telegram:** Bot token + chat ID
   - **WhatsApp:** Target phone number for forwarding

### After First Boot

Once WiFi is configured, the device:
1. Connects to your home WiFi
2. Initializes SIM800L
3. Starts listening for incoming SMS
4. Becomes accessible at `http://<ip>:80/` for reconfiguration

To re-enter setup mode, hold the reset button or power-cycle without valid WiFi config.

## Configuration Fields

### WiFi Tab
- **SSID:** Your WiFi network name
- **Password:** WiFi password

### API Tab
- **API URL:** Backend endpoint (e.g., `http://192.168.0.116:8000/api`)
- **API Key:** Device API key from backend pairing

### Telegram Tab
- **Enable:** Toggle Telegram forwarding
- **Bot Token:** From Telegram @BotFather
- **Chat ID:** Numeric ID of your Telegram chat/group

### WhatsApp Tab
- **Enable:** Toggle WhatsApp forwarding
- **Forward to Number:** E.164 format (e.g., `+1234567890`)

## Getting Telegram Credentials

1. **Create a bot:**
   - Chat with `@BotFather` on Telegram
   - Send `/newbot` and follow prompts
   - Save the **Bot Token** (e.g., `123456:ABC-DEF1234...`)

2. **Get your Chat ID:**
   - Chat with your new bot
   - Forward any message to `@userinfobot`
   - Copy the Chat ID value

## Backend Integration

The ESP device sends SMS to the backend via HTTP POST:
```bash
POST /api/device/messages/inbound
X-API-Key: <api_key>
Content-Type: application/json

{
  "phone_number": "+447700900123",
  "text": "Hello from SIM800L"
}
```

## Telegram Commands

Send SMS from Telegram using:
```
/sms +1234567890 Your message here
```

The message will be sent as an SMS from the gateway device.

## Troubleshooting

### SIM800L not responding
- Check power supply (needs 3-4A peak, 2A continuous)
- Verify UART pins are correct
- Check SIM card is inserted properly
- Try resetting with RST pin

### WiFi won't connect
- Device falls back to AP mode if connection fails
- Connect to `SIM800L-Gateway` and reconfigure WiFi
- Check SSID/password are correct

### SMS not forwarding
- Check device is paired with backend (API key set)
- Verify backend API URL is reachable from device's WiFi
- Check Telegram/WhatsApp settings are enabled and filled
- Watch serial output for error messages

### Captive Portal not working
- Ensure you're connecting to `SIM800L-Gateway` AP
- Try manually opening `http://192.168.4.1/` in browser
- Clear browser cache / try incognito mode

## API Endpoints

**GET /config** — Retrieve current configuration
```bash
curl http://192.168.4.1/config
```

**POST /config** — Update configuration
```bash
curl -X POST http://192.168.4.1/config \
  -H "Content-Type: application/json" \
  -d '{"wifi_ssid":"MyWiFi","wifi_password":"123456"}'
```

**GET /** — Captive portal dashboard
```bash
curl http://192.168.4.1/
```

## Memory & Storage

- **Flash:** ~500KB (program + libraries)
- **SPIFFS:** Configuration stored in `/config.json`
- **RAM:** ~100KB available for buffers
- **No SD card:** Logs are streamed to serial only

## Power Consumption

- **Idle:** ~80mA
- **Active WiFi:** ~150mA
- **SMS sending:** ~1.5A (peak)
- **Recommendation:** Use 2A+ power supply minimum

## Security Notes

- ⚠️ WiFi AP password is hardcoded (change in `main.cpp` line 27)
- ⚠️ Configuration stored unencrypted in SPIFFS
- ⚠️ Use HTTPS for backend in production
- ⚠️ Never commit API keys to version control

## File Structure

```
esp/
├── platformio.ini          # PlatformIO config (ESP32/ESP8266)
├── src/
│   └── main.cpp           # Main firmware
├── include/               # Header files (if any)
├── README.md              # This file
└── wiring.md              # Hardware schematic
```

## Development

### Modify main.cpp
- **WiFi AP settings:** Line 23-24
- **SIM800L pins:** Line 8-10
- **Captive portal HTML:** `handleRoot()` function
- **Configuration schema:** `struct Config`

### Build Targets
```bash
# ESP32 build
platformio run -e esp32

# ESP8266 build
platformio run -e esp8266

# Upload to device
platformio run --target upload -e esp32

# Clean build
platformio run --target clean -e esp32
```

### Serial Debugging
```bash
platformio device monitor -p /dev/ttyUSB0 --baud 115200
```

## Future Enhancements

- [ ] MQTT broker integration
- [ ] Multiple SIM cards (dual SIM support)
- [ ] OTA (over-the-air) firmware updates
- [ ] Web dashboard with real-time message log
- [ ] GPS tracking (if using SIM800L with GPS)
- [ ] Battery backup with deep sleep
- [ ] WebSocket real-time notifications

## License

Same as parent SMS Gateway project

## Support

For issues:
1. Check serial output (`platformio device monitor`)
2. Verify hardware connections
3. Try factory reset (delete `/config.json` from SPIFFS)
4. Check GitHub issues in parent repository
