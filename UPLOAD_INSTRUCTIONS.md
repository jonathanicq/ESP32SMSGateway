# ESP32 Upload Instructions - Windows (COM3)

Your ESP is connected on **COM3**. Follow these steps to upload the firmware.

## Quick Start (5 minutes)

### 1. Install Arduino IDE
- Download: https://www.arduino.cc/en/software
- Install (default settings OK)
- Launch

### 2. Add ESP32 Board Support
1. **File** → **Preferences**
2. Find "Additional Boards Manager URLs" field
3. Paste: `https://dl.espressif.com/dl/package_esp32_index.json`
4. Click **OK**
5. **Tools** → **Board** → **Boards Manager**
6. Search: `esp32`
7. Click **Install** (v2.0.0+) - wait 5 minutes
8. Close Boards Manager

### 3. Install Libraries
**Sketch** → **Include Library** → **Manage Libraries**

Search and click **Install** for each:
- `ArduinoJson` by Benoit Blanchon
- `TinyGPS++` by Mikal Hart
- `TinyGSM` by Volodymyr Shymanskyy

(Install one at a time, wait for each to finish)

### 4. Configure Upload Settings

**Tools Menu - Set these:**
- **Board:** `ESP32 Dev Module`
- **Port:** `COM3`
- **Upload Speed:** `921600`
- **Core Debug Level:** `None`

### 5. Open Firmware

In Arduino IDE, open:
```
esp/SIM800L_Gateway/SIM800L_Gateway.ino
```

### 6. Upload

Click the **Upload** button (right arrow →)

**Wait for message:** 
```
Hard resetting via RTS pin...
```

✅ **Upload complete!**

### 7. Verify Success

**Tools** → **Serial Monitor**

Bottom right: Set baud to **115200**

Should see:
```
=== SIM800L SMS Gateway ===

[SIM800L] Testing connection...
[SIM800L] Connected!
[WiFi] AP Mode: SSID=SIM800L-Gateway, IP=192.168.4.1
```

## Troubleshooting

### "COM3 not found" or "Port not available"
```
1. Unplug ESP from USB
2. Wait 5 seconds
3. Plug back in
4. Go to Tools → Port → Select COM3
5. Try upload again
```

### "Upload failed" / "Connection lost"
```
1. Try slower upload speed:
   Tools → Upload Speed → 460800
2. Or hold BOOT button while uploading
3. Try again
```

### "Not in sync; got 0xff, expected 0xc0"
```
Install CH340 driver (USB chip):
1. Download: https://www.wemos.cc/en/latest/ch340_driver.html
2. Install driver
3. Restart computer
4. Unplug/plug ESP
5. Try upload again
```

### Serial monitor shows garbage
```
Check baud rate is 115200 (bottom right)
```

### Compilation errors
```
1. Check all libraries installed (ArduinoJson, TinyGPS++, TinyGSM)
2. Restart Arduino IDE
3. Try Sketch → Verify (compile only)
4. If still error, check red text for details
```

## Next Steps (After Upload)

### Initial Setup
1. **Serial monitor shows:**
   ```
   [WiFi] AP Mode: SSID=SIM800L-Gateway, IP=192.168.4.1
   ```

2. **From your phone/computer:**
   - Find WiFi network: `SIM800L-Gateway`
   - Password: `12345678`
   - Open browser: `http://192.168.4.1/`

3. **Configure:**
   - **WiFi Tab:** Your home WiFi SSID + password
   - **API Tab:** Backend URL + API key (see backend setup)
   - **Telegram Tab:** Bot token + chat ID (optional)
   - Click **Save** on each tab

4. **Device reboots and connects to your WiFi**

### Verify WiFi Connection
- Watch Serial Monitor
- Should see: `[WiFi] Connected! IP: 192.168.x.x`

### Test SMS Reception
1. Send SMS to the SIM card in SIM800L
2. Serial Monitor shows: `[SMS] From: +447700900123 | Body: Hello`
3. Message appears in backoffice

## Features Enabled After Upload

✅ SMS Reception - Receives SMS from carrier
✅ Telegram Integration - Forwards SMS to Telegram
✅ WiFi Configuration - Captive portal setup
✅ Backend Connectivity - Polls for outbound messages
✅ WhatsApp Support - Ready for bridge integration
✅ SPIFFS Storage - Saves configuration

## Firmware Details

- **Language:** Arduino C++
- **Size:** ~800KB
- **Flash:** Fits in ESP32 (4MB)
- **Libraries:** ArduinoJson, WiFi, WebServer, SPIFFS

## Serial Port (COM3)

```
Device:     USB-SERIAL CH340
Port:       COM3
Baud:       115200 (Serial Monitor)
            9600 (SIM800L UART)
```

## If Upload Keeps Failing

**Last Resort - Factory Reset:**
1. In Arduino IDE, open this sketch
2. Replace `setup()` with:
   ```cpp
   void setup() {
     Serial.begin(115200);
     SPIFFS.format();
     Serial.println("SPIFFS formatted");
   }
   ```
3. Upload this version
4. Wait for "SPIFFS formatted" in Serial
5. Restore original code
6. Upload again

This will clear any corrupted config and start fresh.

## Files

- **Main code:** `esp/SIM800L_Gateway/SIM800L_Gateway.ino`
- **Config location:** SPIFFS `/config.json` (on device)
- **Documentation:** `esp/README.md` and `esp/wiring.md`

## Need Help?

1. **Check Serial Monitor** - Most issues show there
2. **Read error messages carefully** - They tell you exactly what's wrong
3. **Try slower baud rate** for upload (460800 instead of 921600)
4. **Reinstall CH340 driver** if port connection is unstable
5. **Restart computer** if port disappears

---

**Device Status:** Ready to upload ✓
**Port:** COM3 ✓
**Hardware:** USB-SERIAL CH340 ✓

**Next:** Follow the Quick Start above to upload firmware!
