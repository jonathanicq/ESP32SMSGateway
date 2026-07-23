# Upload via Arduino IDE (Windows)

Since PlatformIO CLI requires Python setup, use Arduino IDE instead.

## Step 1: Install Arduino IDE
1. Download from: https://www.arduino.cc/en/software
2. Install (default settings OK)
3. Launch Arduino IDE

## Step 2: Install ESP32 Board Support
1. **File** → **Preferences**
2. Find **Additional Boards Manager URLs**
3. Paste: `https://dl.espressif.com/dl/package_esp32_index.json`
4. Click **OK**
5. **Tools** → **Board** → **Boards Manager**
6. Search: `esp32`
7. Click **Install** (latest version)
8. Wait for installation (~5 minutes)

## Step 3: Install Required Libraries
In Arduino IDE:
1. **Sketch** → **Include Library** → **Manage Libraries**
2. Search and install each:
   - `ArduinoJson` by Benoit Blanchon (v6.21.0+)
   - `TinyGPS++` by Mikal Hart (v1.0.3+)
   - `TinyGSM` by Volodymyr Shymanskyy (v0.11.7+)

Wait for each to install fully.

## Step 4: Upload Firmware
1. **Tools** → **Board** → Select **ESP32 Dev Module**
2. **Tools** → **Port** → Select **COM3** (or your port)
3. **Tools** → **Upload Speed** → Select **921600**
4. Open: `esp/src/main.cpp`
5. Click **Upload** (right arrow button)

**Wait for: "Hard resetting via RTS pin..."**

## Step 5: Verify Upload
1. **Tools** → **Serial Monitor**
2. Set baud rate to **115200** (bottom right)
3. Should see output:
```
=== SIM800L SMS Gateway ===
[SIM800L] Testing connection...
[SIM800L] Connected!
[WiFi] AP Mode: SSID=SIM800L-Gateway, IP=192.168.4.1
```

## Troubleshooting

**"Port not available"**
- Unplug ESP, wait 5 seconds, plug back in
- Check Device Manager: should show "USB-SERIAL CH340 (COM3)"
- If not, install CH340 driver from: https://www.wemos.cc/en/latest/ch340_driver.html

**"Upload failed"**
- Hold down **BOOT** button while uploading
- Or try slower baud rate (460800 or 115200)

**"Board not recognized"**
- Check **Tools** → **Board** is "ESP32 Dev Module"
- Restart Arduino IDE
- Reinstall ESP32 board support

**Garbage in Serial Monitor**
- Check baud rate is 115200
- Make sure no other program has COM3 open

## Next Steps

After successful upload:
1. Check serial output as above
2. Connect to WiFi AP: `SIM800L-Gateway` (password: `12345678`)
3. Open browser: `http://192.168.4.1/`
4. Configure WiFi, backend URL, Telegram (see `esp/README.md`)
