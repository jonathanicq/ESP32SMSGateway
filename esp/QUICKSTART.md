# Quick Start Guide - SIM800L ESP Gateway

Get up and running in 15 minutes.

## Prerequisites

1. **Hardware:**
   - ESP32 DevKit board
   - SIM800L module
   - 5V power supply (2A+)
   - Micro USB cable for ESP32 programming
   - Active GSM SIM card

2. **Software:**
   - Python 3.8+
   - VS Code or PlatformIO CLI

## Step 1: Install PlatformIO (2 min)

```bash
# Via VS Code (recommended)
# Install "PlatformIO IDE" extension

# OR via CLI
pip install platformio
```

## Step 2: Hardware Assembly (5 min)

Follow the [wiring guide](wiring.md):
1. Connect SIM800L to ESP32 UART2 (GPIO 16/17)
2. Connect RST to GPIO 5
3. Power SIM800L with regulated 3.3V (minimum 2A capacity)
4. **Double-check all GND connections!**

## Step 3: Upload Firmware (3 min)

```bash
cd esp

# For ESP32 (recommended)
platformio run --target upload -e esp32

# For ESP8266
# platformio run --target upload -e esp8266
```

Watch for upload progress. Device will reboot automatically.

## Step 4: Initial Setup (5 min)

1. **Open serial monitor:**
   ```bash
   platformio device monitor --baud 115200
   ```
   
   Should see:
   ```
   === SIM800L SMS Gateway ===
   [SIM800L] Testing connection...
   [SIM800L] Connected!
   [WiFi] AP Mode: SSID=SIM800L-Gateway, IP=192.168.4.1
   ```

2. **From your phone/computer:**
   - Find WiFi network `SIM800L-Gateway`
   - Password: `12345678`
   - Open browser: `http://192.168.4.1/`

3. **Configure:**
   - **WiFi tab:** Enter your home WiFi credentials
   - **API tab:** 
     - URL: Backend server (e.g., `http://192.168.0.116:8000/api`)
     - Key: Get from backend device pairing
   - **Telegram tab** (optional):
     - Token: From `@BotFather` on Telegram
     - Chat ID: Your Telegram chat ID
   - **WhatsApp tab** (optional):
     - Target number: Phone to receive forwarded SMS

4. **Save and reboot** — device connects to your WiFi

## Step 5: Verify SMS Reception

1. Send an SMS to the SIM card in SIM800L
2. Watch the serial monitor — should see:
   ```
   [SMS] From: +447700900123 | Body: Hello test
   ```

3. Check backend `/api/messages` — SMS should appear there

## Telegram Integration

### Get Bot Token
1. Chat with `@BotFather` on Telegram
2. Send `/newbot` 
3. Follow prompts (give it a name like "SMSGateway")
4. Save the bot token

### Get Chat ID
1. Chat with your new bot (send any message)
2. Go to `@userinfobot` and forward any message
3. Copy the Chat ID number

### Send SMS via Telegram
Send this to your bot:
```
/sms +1234567890 Your message here
```

The SMS will be sent from the gateway device!

## Backend Pairing

Get the API key from the backend:

1. **Backend must be running:**
   ```bash
   cd backend
   uvicorn app.main:app --reload
   ```

2. **Pair the device:**
   - Open backoffice: `http://localhost:5173/`
   - Go to Devices → Pair a phone
   - Instead of pairing a phone app, create a device manually:
     - Name: "SIM800L Gateway"
     - Leave a note with the API key

3. **Copy API key** into ESP config

## Troubleshooting

### "SIM800L not connecting"
```
[SIM800L] Testing connection...
[SIM800L] Connection failed!
```

**Fix:**
- Check UART wiring (GPIO 16/17)
- Verify 3.3V power supply
- Try different baud rate (serial monitor shows 9600)
- Look for shorts in wiring

### "WiFi won't connect"
Device returns to AP mode if WiFi fails. Reconnect to `SIM800L-Gateway` and:
- Verify SSID spelling
- Check password length (<64 chars)
- Try 2.4GHz network (not 5GHz)

### "No SMS received"
- Check SIM card has signal (look for signal bars on module)
- Verify SIM is not locked with PIN
- Try sending SMS to the number directly
- Check firewall isn't blocking serial

### "Telegram not receiving"
- Go to `http://192.168.4.1/config` and verify bot token/chat ID are saved
- Message the bot first to establish chat
- Check bot has permission to send messages

## Next Steps

- **Monitor dashboard:** Check `http://192.168.4.1/` anytime to view/change config
- **API integration:** Your backend can now send SMS via this gateway
- **Add more gateways:** Repeat setup with multiple SIM800L devices
- **OTA updates:** (Coming soon) Update firmware over WiFi

## File Locations

- **ESP firmware:** `esp/src/main.cpp`
- **Configuration:** Device SPIFFS flash memory
- **Logs:** Serial monitor output only (no disk storage)

## Need Help?

1. **Check serial output** — enables verbose debugging
2. **Review README.md** — more detailed documentation
3. **Check wiring.md** — verify all connections
4. **Inspect main.cpp** — code is well-commented

## Production Deployment

Before going live:
1. Change WiFi AP password (line 23 in main.cpp)
2. Use HTTPS backend URLs
3. Test SMS load (send 10+ messages)
4. Monitor power consumption
5. Enable serial logging for diagnostics
6. Set up monitoring webhook to track device health

---

**Estimated time:** 20 minutes total  
**Success indicator:** Green LED on SIM800L module lighting up when SMS arrives
