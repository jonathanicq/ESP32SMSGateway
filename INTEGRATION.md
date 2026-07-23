# SMS Gateway - Complete Integration Guide

This document explains how the Android SMS Gateway, Backend, Web Frontend, and new ESP8266/ESP32 SIM800L gateway work together.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         End Users                                │
├──────────────────────────────────────────────────────────────────┤
│                                                                    │
│  ┌──────────────────┐          ┌──────────────────┐              │
│  │ Android Phone    │          │   Telegram Bot   │              │
│  │ (SMS App)        │          │   @YourBot       │              │
│  │                  │          │                  │              │
│  │ Can send/receive │          │ Send SMS via:    │              │
│  │ SMS directly     │          │ /sms +123...     │              │
│  └────────┬─────────┘          └────────┬─────────┘              │
│           │                             │                        │
│  ┌────────▼─────────────────────────────▼─────────┐              │
│  │            FastAPI Backend Server               │              │
│  │   (device pairing, message relay, webhooks)     │              │
│  │   - Device authentication (API keys)            │              │
│  │   - Message storage (MySQL/SQLite)              │              │
│  │   - WhatsApp bridge (WAHA integration)          │              │
│  │   - Telegram integration (Bot API)              │              │
│  │   - Webhook delivery to external APIs           │              │
│  └────────┬──────────────────────────────┬─────────┘              │
│           │                              │                        │
│  ┌────────▼──────────────────┐  ┌───────▼─────────────────────┐  │
│  │ Web Backoffice            │  │ SMS Gateways (Devices)       │  │
│  │ (React + TypeScript)      │  │                              │  │
│  │                           │  │ Option 1: Android Phone      │  │
│  │ Dashboard:                │  │  - Paired via 6-char code    │  │
│  │ - Messages view           │  │  - Sends SMS via cellular    │  │
│  │ - Device management       │  │  - Polls for pending msgs    │  │
│  │ - Webhook config          │  │                              │  │
│  │ - Telegram/WhatsApp       │  │ Option 2: ESP32 + SIM800L   │  │
│  │   settings                │  │  - Captive portal for setup  │  │
│  └───────────────────────────┘  │  - Sends SMS via SIM module  │  │
│                                  │  - WiFi based                │  │
│                                  │  - Config via web dashboard  │  │
│                                  └──────────────────────────────┘  │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │ Database (MySQL / SQLite)                                     │ │
│  │ - Users & authentication                                      │ │
│  │ - Devices (API keys, last_seen)                               │ │
│  │ - Messages (inbound/outbound, status)                         │ │
│  │ - Bridge settings (WhatsApp + Telegram config)                │ │
│  │ - Webhooks                                                    │ │
│  └──────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

## Component Details

### 1. Android SMS Gateway App
**Path:** `android/`  
**Technology:** Kotlin + Ktor

**Responsibilities:**
- Runs on Android phone
- Receives SMS via Android SMS BroadcastReceiver
- Polls `/api/device/messages/pending` for outbound messages
- Sends SMS natively via Android SmsManager
- Reports delivery/failure status back to backend

**Setup:**
1. Install APK on phone
2. Launch app → Set server URL → "Pair with backoffice"
3. App shows 6-char code → Enter in backoffice
4. App automatically receives API key → Ready to relay SMS

### 2. Backend API Server
**Path:** `backend/`  
**Technology:** FastAPI (Python)

**Key Endpoints:**
```
POST   /api/auth/login              - User authentication
POST   /api/devices                 - Create device
POST   /api/device/messages/pending - Poll for messages (device)
POST   /api/device/messages/inbound - Report incoming SMS
POST   /api/messages                - Send SMS (from backoffice)
POST   /api/telegram/settings       - Configure Telegram
POST   /api/telegram/webhook        - Receive Telegram messages
POST   /api/whatsapp/webhook        - Receive WhatsApp messages
```

**Data Flow - Inbound SMS:**
```
1. Device (Android/ESP) sends to: POST /api/device/messages/inbound
   {
     "phone_number": "+447700900123",
     "text": "Hello from SMS"
   }

2. Backend stores message in database
   Message(direction=INBOUND, status=RECEIVED)

3. Backend triggers:
   - Fire webhooks for "sms:received"
   - Forward to WhatsApp (if configured)
   - Forward to Telegram (if configured)

4. Backoffice receives message in real-time
```

**Data Flow - Outbound SMS:**
```
1. Backoffice user sends SMS via web UI
   POST /api/messages
   {
     "device_id": 1,
     "phone_number": "+1234567890",
     "text": "Message to send"
   }

2. Message stored as: Message(direction=OUTBOUND, status=PENDING)

3. Device polls: GET /api/device/messages/pending
   - Android phone or ESP device asks "any messages for me?"

4. Device receives message and sends SMS natively

5. Device reports back: POST /api/device/messages/{id}/status
   {
     "status": "sent" or "failed",
     "error": "optional error description"
   }

6. Backoffice sees status update
```

### 3. Web Backoffice
**Path:** `webfront/`  
**Technology:** React + TypeScript + Tailwind CSS

**Features:**
- **Messages:** View all inbound/outbound SMS with timestamps
- **Devices:** Manage paired phones/gateways
- **Send SMS:** Compose and send messages from web UI
- **Webhooks:** Configure external API notifications
- **Settings:** WhatsApp + Telegram bridge configuration
- **Device Pairing:** Use 6-char code to add new devices

**Access:**
- Development: `http://localhost:5173/`
- Production: `https://sms-gateway.thecoordinates.app/`

### 4. ESP8266/ESP32 + SIM800L Gateway
**Path:** `esp/`  
**Technology:** Arduino C++

**Key Features:**
- **Captive Portal:** Connect to `SIM800L-Gateway` WiFi to configure
- **Dashboard:** `http://192.168.4.1/` for settings UI
- **SMS Reception:** Listens on SIM800L UART for incoming SMS
- **Message Forwarding:** 
  - Sends to backend API (`/api/device/messages/inbound`)
  - Forwards to Telegram bot
  - Forwards to WhatsApp (via backend)

**Setup Flow:**
```
1. Power on ESP → Creates "SIM800L-Gateway" AP
2. User connects to AP (password: 12345678)
3. Opens http://192.168.4.1/ in browser
4. Enters WiFi SSID + password
5. Enters backend API URL + API key
6. Optionally enters Telegram bot token + chat ID
7. Saves configuration → device reboots
8. Device connects to home WiFi → ready to relay SMS
```

**Configuration Storage:**
- Stored in SPIFFS (embedded file system)
- File: `/config.json`
- Survives power cycles
- Can be reset by deleting file

## Data Models

### Device
```python
Device:
  - id: Unique identifier
  - name: User-friendly name
  - api_key: Secret for authentication
  - owner_id: Foreign key to User
  - last_seen: For activity monitoring
  - created_at: Registration timestamp
  
Types:
  1. Android phone (type=ANDROID)
  2. ESP8266/ESP32 (type=ESP)
```

### Message
```python
Message:
  - id: Unique ID
  - device_id: Which device sent/received
  - direction: INBOUND or OUTBOUND
  - phone_number: E.164 format (+1234567890)
  - text: Message body
  - status: PENDING, SENT, DELIVERED, FAILED, RECEIVED
  - error: Error description (if failed)
  - created_at, updated_at: Timestamps
```

### BridgeSettings
```python
BridgeSettings:
  - owner_id: User who owns the bridge
  - enabled: Master toggle
  
  # WhatsApp (via WAHA)
  - waha_session: Which WAHA session to use
  - forward_to_chat: Target WhatsApp contact/group
  
  # Telegram (via Telegram Bot API)
  - telegram_enabled: Toggle Telegram forwarding
  - telegram_bot_token: Bot token from @BotFather
  - telegram_chat_id: Chat ID to receive messages
  
  - send_device_id: Device to use for Telegram→SMS
```

## Integration Scenarios

### Scenario 1: Send SMS via Android App
```
User → Android App → Backend → Android Phone (SMS) → Carrier
         ↓
    Backend stores message
         ↓
    Backoffice shows "SENT" status
```

### Scenario 2: Receive SMS (Android)
```
Carrier → Android Phone (SMS) → Android App polls /device/messages/pending
         (No wait, SMS arrives automatically)
         
Actually:
Carrier → Android Phone (receives SMS) → SmsReceiver.onReceive()
         ↓
    API POST /device/messages/inbound
         ↓
    Backend stores, triggers webhooks
         ↓
    If Telegram enabled: Forward to Telegram
         ↓
    Backoffice receives real-time update
```

### Scenario 3: Receive SMS (ESP8266)
```
Carrier → SIM800L → UART → ESP32
         ↓
    Parse incoming SMS
         ↓
    API POST /device/messages/inbound
         ↓
    Backend processes (same as Android)
         ↓
    Forward to Telegram via Telegram.send_telegram()
         ↓
    Show in backoffice + deliver webhooks
```

### Scenario 4: Send SMS via Telegram
```
User → Telegram Chat with Bot
         ↓
    Type: "/sms +1234567890 Hello there"
         ↓
    POST /api/telegram/webhook (from Telegram)
         ↓
    Backend parses command
         ↓
    Creates outbound Message with ESP device
         ↓
    ESP polls and receives pending message
         ↓
    ESP sends SMS via SIM800L
         ↓
    Reports status back to backend
         ↓
    User receives confirmation in Telegram
```

### Scenario 5: Forward SMS to Telegram
```
Incoming SMS to gateway
         ↓
    Device reports to backend (/device/messages/inbound)
         ↓
    Backend triggers: _forward_inbound_to_telegram()
         ↓
    Telegram.send_telegram() → Telegram Bot API
         ↓
    User receives formatted message in Telegram chat
```

## Authentication & Security

### API Key Authentication
- Device uses X-API-Key header
- Generated on device pairing
- Each device gets unique key
- Stored in database, never transmitted

### User Authentication
- JWT tokens for web backoffice
- Login → Get access token + refresh token
- Tokens expire (configurable TTL)
- Refresh token used to get new access tokens

### Webhook Authentication
- Optional webhook token (WHATSAPP_WEBHOOK_TOKEN)
- WAHA verifies this token in webhook requests
- Telegram webhook uses bot token verification

## Configuration Management

### Backend (.env file)
```
# Database connection
DATABASE_URL=sqlite:///./sms_gateway.db

# JWT secrets
JWT_SECRET=your-secret-key

# WhatsApp (WAHA)
WAHA_BASE_URL=http://192.168.0.135:3099
WAHA_API_KEY=your-api-key

# Telegram (optional)
TELEGRAM_BOT_TOKEN=123456:ABC...
TELEGRAM_WEBHOOK_CHAT_ID=987654321
```

### ESP Device (Captive Portal)
```
WiFi SSID: MyHomeWiFi
WiFi Password: xxxxxx
API URL: http://backend.local:8000/api
API Key: <key from backend>
Telegram Token: <from @BotFather>
Telegram Chat ID: <your chat id>
```

## Deployment

### Local Development
```bash
# Terminal 1: Backend
cd backend && uvicorn app.main:app --reload

# Terminal 2: Frontend
cd webfront && npm run dev

# Terminal 3: Android emulator
# Or install APK on physical phone

# Terminal 4: ESP (serial monitor)
platformio device monitor
```

### Docker (Production)
```bash
docker-compose up -d

# Backend: http://localhost:8000/api
# Frontend: http://localhost:3000
# Backoffice: http://localhost:3050
```

### Raspberry Pi / VPS
```bash
# See deployment docs for full instructions
# Typically:
# 1. Clone repo
# 2. Install dependencies (pip, node)
# 3. Configure .env
# 4. Run uvicorn + nginx/reverse-proxy
# 5. Set up systemd service for auto-start
```

## Monitoring & Debugging

### Check Device Status
```bash
# Backend endpoint
GET /api/devices

# Response shows: last_seen, created_at, message_count
```

### View Message Queue
```bash
# Pending outbound messages
GET /api/device/messages/pending?device_id=1

# All messages
GET /api/messages?limit=100&direction=inbound
```

### Serial Logs (ESP)
```bash
platformio device monitor --baud 115200

# Should show:
# [SIM800L] Connected!
# [WiFi] Connected! IP: 192.168.1.100
# [SMS] From: +447700900123 | Body: Hello
# [API] Sending SMS to backend...
```

### API Logs
```bash
# Backend logs (if running with --reload)
# Shows all HTTP requests/responses
```

## Troubleshooting Checklist

### SMS not sending from Android
- [ ] Device is paired (check backoffice)
- [ ] Device has active internet connection
- [ ] Device is polling `/device/messages/pending`
- [ ] Check Android app logs
- [ ] Verify device has SMS permissions

### SMS not receiving on Android
- [ ] Device has received SMS permission
- [ ] Device has active internet connection
- [ ] Check if SMS is being intercepted by another app
- [ ] Verify SIM card and mobile service

### SMS not sending from ESP
- [ ] SIM800L has power (3.3V)
- [ ] SIM800L initialized (check serial log)
- [ ] SIM card is active (has signal bars)
- [ ] Device is connected to WiFi
- [ ] Backend API key is configured
- [ ] Backend is reachable from device's network

### Telegram not receiving SMS
- [ ] Bot token is correct (from @BotFather)
- [ ] Chat ID is numeric and correct
- [ ] Bot has been messaged first (establishes chat)
- [ ] Check backend logs for Telegram API errors
- [ ] Verify TELEGRAM_BOT_TOKEN in .env or device config

### Backoffice not showing messages
- [ ] Backend is running and reachable
- [ ] User is logged in (JWT token valid)
- [ ] Device is paired with user's account
- [ ] Device is polling backend (check last_seen)
- [ ] Database connectivity (MySQL or SQLite)

## Performance & Scaling

### Current Limits
- Single backend instance: ~100 concurrent devices
- Message throughput: ~10 SMS/second
- Database: MySQL supports 1000+ messages/day

### Scaling Strategies
1. **Horizontal scaling:** Add backend instances behind load balancer
2. **Message queue:** Use Redis/RabbitMQ for decoupling
3. **Database:** Migrate to MySQL cluster or managed service
4. **CDN:** Cache static backoffice assets
5. **Caching:** Redis cache for device/user lookups

## Future Roadmap

- [ ] Scheduled message delivery
- [ ] SMS templates
- [ ] Two-way Telegram conversations (not just SMS forwarding)
- [ ] WhatsApp media support (images, documents)
- [ ] Device groups (manage multiple gateways as one)
- [ ] OTA firmware updates for ESP devices
- [ ] Backup/restore configuration
- [ ] SMS cost tracking and billing
- [ ] Advanced routing (multiple devices per user)
- [ ] SMPP gateway support
- [ ] Multi-language support

## Support & Contribution

See main [README.md](README.md) for links and contribution guidelines.
