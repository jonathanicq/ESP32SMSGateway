# Android SMS Gateway (+ SIM800L ESP Module)

Native Android app that turns a phone into an SMS gateway (send/receive SMS over HTTP API), **plus** a dedicated ESP8266/ESP32 + SIM800L gateway for hardwired SMS relaying. Paired with a private server and a web backoffice to view and manage messages. Optional **bridges** forward inbound SMS to Telegram and WhatsApp and transcode inbound messages into outbound SMS via keyword grammar.

Modeled on [`capcom6/android-sms-gateway`](https://github.com/capcom6/android-sms-gateway) (Apache-2.0); differentiators are:
- Web backoffice with real-time message dashboard
- **New:** Dedicated ESP8266/ESP32 + SIM800L SMS gateway with WiFi setup
- Telegram bot integration (send/receive SMS via Telegram)
- WhatsApp bridge (forward SMS ⇄ WhatsApp transcoding)

## Product Tiers
- **Free:** SMS ⇄ HTTP API, webhooks, message status, backoffice viewing.
- **Premium:** Bridges — forward inbound SMS → Telegram/WhatsApp; transcode inbound Telegram/WhatsApp → SMS via `/sms <E164> <message>` or `@sms <E164> <message>`.

## Architecture
| Component | Stack | Path | Purpose |
|-----------|-------|------|---------|
| Backend + backoffice API | FastAPI (Python), MySQL | `backend/` | Message relay, device pairing, Telegram/WhatsApp bridges |
| Web backoffice | React + TypeScript + Tailwind | `webfront/` | Dashboard for sending SMS, viewing messages, managing devices |
| Android gateway app | Kotlin, Ktor, Room | `android/` | Phone-based SMS gateway (native SMS send/receive) |
| **ESP SIM800L Gateway** | **Arduino (C++), PlatformIO** | **`esp/`** | **Dedicated GSM module with WiFi, captive portal setup** |

## Status
MVP built (phases 0–4). **NEW:** ESP8266/ESP32 + SIM800L gateway module added with captive portal setup, Telegram integration, and WhatsApp forwarding.

For detailed architecture and integration flows, see [`INTEGRATION.md`](INTEGRATION.md).  
For project plan, see [`MASTER_PLAN.md`](MASTER_PLAN.md) and [`PROJECT_CONFIG.md`](PROJECT_CONFIG.md) (locked).

## Gateways: Android Phone vs. ESP Module

### Android Phone Gateway
- ✅ No hardware cost (use existing phone)
- ✅ Easy to set up (download APK, pair with code)
- ✅ Supports all Android phone features
- ❌ Drains battery, requires data plan
- ❌ Can't run 24/7 reliably

### ESP8266/ESP32 + SIM800L Gateway
- ✅ Dedicated hardware (24/7 uptime)
- ✅ Low power consumption (~100mA idle)
- ✅ Captive portal setup (no manual WiFi config)
- ✅ Perfect for unattended deployments
- ❌ Requires hardware assembly
- ❌ Initial cost (~$30-50)

**Use both:** Multi-gateway setup sends SMS via both Android phone and ESP device for redundancy.

## Download / Install

### Android APK

- Reliable (own edge, valid cert): https://test.sms-gateway.thecoordinates.app/downloads/sms-gateway-latest.apk
- Public CDN (DreamHost): https://automation.thecoordinates.xyz/apps/sms-gateway/android/sms-gateway-latest.apk
- Versioned: `sms-gateway-v0.1.1.apk` at the same paths.

> **Release-signed** (APK Signature Scheme v2, not debuggable). Enable "Install unknown apps" to sideload. Use the test-edge link — the DreamHost host can intermittently serve a mismatched TLS cert.
>
> ⚠️ If you previously installed a **debug** build, **uninstall it first** — the release key differs, so Android refuses the update with "App not installed" until the old one is removed.

### Xiaomi / Redmi / POCO (MIUI / HyperOS) — "App not installed"
MIUI blocks sideloaded apps that request **SMS permissions** (which this app needs). To allow it:
1. **Settings → Privacy protection → Special permissions → Install unknown apps → Chrome → Allow.**
2. Open the **Security** app → **Settings (gear)** → turn **OFF** "Scan apps before installing" / "Send apps to cloud for scan".
3. **Settings → Additional settings → Developer options → turn OFF "MIUI optimization"** (enable Developer options by tapping MIUI Version 7×), then reboot and retry.
4. HyperOS "Enhanced sideloading protection": keep the phone **online** during install (it phones home to verify), or temporarily toggle it off in Developer options.
5. Most reliable fallback — install over **ADB** from a PC: `adb install sms-gateway-latest.apk` (bypasses the MIUI installer scan).

## Install / update over USB (ADB)
One helper script handles locating `adb`, checking the device state, installing, and launching:
```bash
scripts/install-apk.sh                 # installs releases/android/sms-gateway-latest.apk
scripts/install-apk.sh path/to.apk     # installs a specific build
```

**One-time device setup** (Settings → Additional settings → Developer options):
`USB debugging` ON, plus — on Xiaomi/MIUI/HyperOS — `Install via USB` ON and `USB debugging (Security settings)` ON (these two may require a SIM inserted and a Mi account signed in). Approve the "Allow USB debugging?" prompt and tick "Always allow".

**One-time host setup** (so `adb` never reports "no permissions" again):
```bash
sudo cp scripts/udev/51-android.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger   # then replug the phone
```

After this one-time setup, **updating the app is just `scripts/install-apk.sh`** — the device toggles and the "always allow" authorization persist across reboots.

## Setup

### Android Phone Gateway
1. Install + open the app, set the server URL (default `https://test.sms-gateway.thecoordinates.app`), tap **Pair with backoffice** → it shows a 6-character code.
2. In the backoffice → **Devices → Pair a phone**, enter the code.
3. The phone polls, receives its API key automatically, then **Start** begins relaying.

### ESP8266/ESP32 + SIM800L Gateway
1. Assemble hardware: Connect SIM800L to ESP32 UART (see [`esp/wiring.md`](esp/wiring.md))
2. Flash firmware: `cd esp && platformio run --target upload -e esp32`
3. Device creates WiFi AP: `SIM800L-Gateway` (password: `12345678`)
4. Connect to AP → Open `http://192.168.4.1/`
5. Configure: WiFi, backend API URL + key, Telegram (optional), WhatsApp (optional)
6. Reboot → Device connects to home WiFi and starts relaying SMS

**Full quickstart:** See [`esp/QUICKSTART.md`](esp/QUICKSTART.md) (15 minutes to operational)

## Hosting
- Test: `test.sms-gateway.thecoordinates.app` (192.168.0.116)
- Production: `sms-gateway.thecoordinates.app` (VPS, GitHub Actions deploy)

## Development

### Backend
```bash
cd backend && uv sync && uvicorn app.main:app --reload
```

### Frontend
```bash
cd webfront && npm install && npm run dev
```

### ESP Firmware
```bash
cd esp && platformio run --target upload -e esp32      # Build & upload to ESP32
platformio device monitor --baud 115200                # Watch serial output
```

### Health Check
```bash
GET /api/health       # Backend health
GET /api/devices      # List paired gateways
GET /api/messages     # List SMS messages
```

**For complete development setup, see** [`INTEGRATION.md`](INTEGRATION.md) (with data flow diagrams).
