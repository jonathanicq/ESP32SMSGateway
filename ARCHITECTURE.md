# SMS Gateway Architecture Overview

## System Diagram

```
┌──────────────────────────────────────────────────────────────────────────┐
│                          External Services                                │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  Telegram Bot API              WAHA (WhatsApp)         Mobile Carriers   │
│       ↑ ↓                           ↑ ↓                      ↑ ↓         │
│   Receive/Send                 Session Manager           Send/Receive     │
│   Messages                     Message Bridge            SMS              │
│                                                                            │
└──────────────────────────────────────────────────────────────────────────┘
         ↑                           ↑                          ↑
         │                           │                          │
         └───────────────────────────┼──────────────────────────┘
                                     │
                        ┌────────────▼────────────┐
                        │  FastAPI Backend        │
                        │  (api.thecoordinates...) │
                        │                         │
                        │  /api/messages          │
                        │  /api/devices           │
                        │  /api/telegram          │
                        │  /api/whatsapp          │
                        │  /api/webhooks          │
                        └────────────┬────────────┘
                                     │
                        ┌────────────▼────────────┐
                        │   Database (MySQL)      │
                        │                         │
                        │  - Users                │
                        │  - Devices              │
                        │  - Messages             │
                        │  - BridgeSettings       │
                        └────────────┬────────────┘
                                     │
                    ┌────────────────┼────────────────┐
                    │                │                │
        ┌───────────▼──────────┐ ┌──▼──────────┐ ┌──▼──────────┐
        │  Web Backoffice      │ │  Android    │ │ ESP32 +     │
        │  (React / TS)        │ │  App        │ │ SIM800L     │
        │                      │ │  (Kotlin)   │ │ (Arduino)   │
        │  ✓ Dashboard         │ │             │ │             │
        │  ✓ Manage Devices    │ │ ✓ Receive   │ │ ✓ Receive   │
        │  ✓ Send SMS          │ │   SMS       │ │   SMS       │
        │  ✓ Bridge Config     │ │ ✓ Send SMS  │ │ ✓ Send SMS  │
        │  ✓ Message History   │ │ ✓ Auto-    │ │ ✓ WiFi      │
        │  ✓ User Settings     │ │   forward   │ │   Setup     │
        │                      │ │             │ │ ✓ Telegram  │
        │ http://localhost:    │ │ Uses:       │ │   Config    │
        │   5173               │ │ - Cellular  │ │             │
        │                      │ │ - SMS API   │ │ Uses:       │
        │                      │ │             │ │ - SIM Card  │
        │                      │ │             │ │ - WiFi      │
        └──────────────────────┘ └─────────────┘ └─────────────┘
```

## Message Flow: Inbound SMS

```
┌─ ANDROID GATEWAY ──────────────────────────────────────────┐
│                                                              │
│  [SMS Arrives] → [Android Receiver] → [API Call]            │
│                                                              │
└──┬──────────────────────────────────────────────────────────┘
   │
   │ POST /api/device/messages/inbound
   │ {
   │   "phone_number": "+447700900123",
   │   "text": "Hello from SMS"
   │ }
   │
   ▼
┌─ BACKEND ─────────────────────────────────────────────────┐
│                                                             │
│  1. Store Message(INBOUND, RECEIVED)                       │
│  2. Dispatch Webhooks("sms:received", message)             │
│  3. Forward to WhatsApp (if enabled)                       │
│  4. Forward to Telegram (if enabled)                       │
│                                                             │
└──┬──────────────────────────────────────────────────────────┘
   │
   ├─────────────────┬──────────────────┬──────────────────┐
   │                 │                  │                  │
   ▼                 ▼                  ▼                  ▼
┌──────────┐  ┌─────────────┐  ┌──────────────┐  ┌──────────┐
│ Database │  │  Backoffice │  │   WAHA       │  │ Telegram │
│ Stores   │  │   Receives  │  │   (WhatsApp) │  │   Bot    │
│ Message  │  │   Real-time │  │   Forwards   │  │   Sends  │
│          │  │   Update    │  │   Message    │  │ to Chat  │
└──────────┘  └─────────────┘  └──────────────┘  └──────────┘
```

## Message Flow: Send SMS via Web

```
┌─ BACKOFFICE (Web UI) ──────────────────────┐
│                                             │
│  [User] → [Compose SMS] → [Send]           │
│                                             │
└──┬──────────────────────────────────────────┘
   │
   │ POST /api/messages
   │ {
   │   "device_id": 1,
   │   "phone_number": "+1234567890",
   │   "text": "Message text"
   │ }
   │
   ▼
┌─ BACKEND ─────────────────────────────────┐
│                                             │
│  Store Message(OUTBOUND, PENDING)          │
│  Message ID: 123                           │
│                                             │
└──┬──────────────────────────────────────────┘
   │
   ├─────────────────────────────────────┐
   │                                     │
   ▼                                     ▼
┌──────────────────┐          ┌──────────────────┐
│ ANDROID GATEWAY  │          │ ESP32 + SIM800L  │
│                  │          │                  │
│ Polls:           │          │ Polls:           │
│ GET /pending     │          │ GET /pending     │
│                  │          │                  │
│ Receives:        │          │ Receives:        │
│ [{id:123,        │          │ [{id:123,        │
│   to:...,        │          │   to:...,        │
│   text:...}]     │          │   text:...}]     │
│                  │          │                  │
│ Sends SMS        │          │ Sends SMS        │
│ Natively via     │          │ Via SIM800L      │
│ Android SMS API  │          │ (AT Commands)    │
│                  │          │                  │
│ Reports:         │          │ Reports:         │
│ POST /status     │          │ POST /status     │
│ {status: SENT}   │          │ {status: SENT}   │
│                  │          │                  │
└──────────────────┘          └──────────────────┘
   │                                     │
   └─────────────┬───────────────────────┘
                 │
                 ▼
         ┌──────────────┐
         │  Backoffice  │
         │ Shows "SENT" │
         └──────────────┘
```

## Message Flow: Send via Telegram

```
┌─ TELEGRAM USER ────────────────┐
│                                │
│  /sms +1234567890 Hello there  │
│                                │
└──┬────────────────────────────┘
   │
   ▼
┌─ TELEGRAM BOT ─────────────────┐
│                                │
│  Receives Message              │
│  Sends Webhook to Backend      │
│                                │
└──┬────────────────────────────┘
   │
   │ POST /api/telegram/webhook
   │ {
   │   "message": {
   │     "chat": {"id": 987654321},
   │     "text": "/sms +1234567890 Hello there",
   │     "from": {"first_name": "John"}
   │   }
   │ }
   │
   ▼
┌─ BACKEND ──────────────────────────────────┐
│                                             │
│ 1. Parse Command                           │
│    trigger: /sms                           │
│    recipient: +1234567890                  │
│    body: Hello there                       │
│                                             │
│ 2. Create Outbound Message                 │
│    {                                       │
│      device_id: 1,                         │
│      direction: OUTBOUND,                  │
│      phone_number: +1234567890,            │
│      text: Hello there,                    │
│      status: PENDING                       │
│    }                                       │
│                                             │
└──┬────────────────────────────────────────┘
   │
   ▼
┌─ DEVICE POLLS ─────────────────┐
│                                │
│ GET /device/messages/pending   │
│                                │
│ Receives Message               │
│                                │
│ Sends SMS via SIM module       │
│                                │
│ Reports Status: SENT           │
│                                │
└──┬────────────────────────────┘
   │
   ▼
┌─ BACKEND ──────────────────────────────────┐
│                                             │
│ Sends Confirmation to Telegram:            │
│ "✓ SMS sent to +1234567890"               │
│                                             │
└────────────────────────────────────────────┘
```

## ESP32 Setup Flow

```
┌─ POWER ON ──────────────────────┐
│                                 │
│ ESP32 Boots                     │
│                                 │
└──┬──────────────────────────────┘
   │
   ├─ Check for WiFi config
   │   (in SPIFFS)
   │
   ▼ No config found
┌──────────────────────────────────┐
│ Create WiFi AP Mode              │
│                                  │
│ SSID: SIM800L-Gateway            │
│ Password: 12345678               │
│ IP: 192.168.4.1                 │
│                                  │
│ Start Web Server                 │
│ (Captive Portal)                 │
│                                  │
└──┬───────────────────────────────┘
   │
   ▼
┌──────────────────────────────────┐
│ USER CONNECTS                    │
│                                  │
│ 1. Find AP "SIM800L-Gateway"     │
│ 2. Connect (password: 12345678)  │
│ 3. Open http://192.168.4.1/      │
│                                  │
└──┬───────────────────────────────┘
   │
   ▼
┌──────────────────────────────────┐
│ WEB CONFIGURATION PORTAL         │
│                                  │
│ Tabs:                            │
│ [WiFi] [API] [Telegram] [WA]    │
│                                  │
│ WiFi Tab:                        │
│   SSID: MyWiFi                   │
│   Password: ****                 │
│                                  │
│ API Tab:                         │
│   URL: http://192.168.0.116/api │
│   Key: abc123...                 │
│                                  │
│ Telegram Tab:                    │
│   Enable: [✓]                    │
│   Token: 123456:ABC...           │
│   Chat ID: 987654321             │
│                                  │
│ [Save]                           │
│                                  │
└──┬───────────────────────────────┘
   │
   ▼ Configuration saved to SPIFFS
┌──────────────────────────────────┐
│ DEVICE REBOOTS                   │
│                                  │
│ Reads WiFi config                │
│ Connects to MyWiFi               │
│ Initializes SIM800L              │
│ Ready to relay SMS               │
│                                  │
└──────────────────────────────────┘
```

## Component Interactions

```
User
 │
 ├─ Backoffice (Web)  ───────────┐
 │  - Send SMS                    │
 │  - View messages               │
 │  - Configure bridges           │
 │                                │
 ├─ Telegram Bot      ───────────┤
 │  - /sms <number>               │
 │                                │
 └─ Android App       ───────────┤
    (if installed)                │
                                  │
                         ┌────────▼─────────┐
                         │  FastAPI Backend │
                         │                  │
                         │ Routing Logic:   │
                         │ - Store message  │
                         │ - Forward to     │
                         │   devices        │
                         │ - Dispatch hooks │
                         │ - Send bridges   │
                         └────────┬─────────┘
                                  │
                ┌─────────────────┼─────────────────┐
                │                 │                 │
         ┌──────▼──────┐   ┌──────▼──────┐  ┌──────▼──────┐
         │Android Phone│   │ ESP32 Device│  │   Database  │
         │             │   │             │  │             │
         │ - Receives  │   │ - Receives  │  │ - Persists  │
         │ - Sends SMS │   │ - Sends SMS │  │   messages  │
         │ - Polls API │   │ - Polls API │  │ - Tracks    │
         │             │   │ - WiFi      │  │   status    │
         └─────────────┘   └─────────────┘  └─────────────┘
```

## Authentication Flow

```
New User Installation
│
├─ Android: Install APK
│  │
│  └─→ Launch App
│      │
│      └─→ Enter server URL
│          │
│          └─→ [Pair with backoffice]
│              │
│              └─→ App generates 6-char code
│                  │
│                  └─→ Display code to user
│                      │
│                      └─→ User enters code in backoffice
│                          │
│                          └─→ Backend generates API key
│                              │
│                              └─→ App polls and receives key
│                                  │
│                                  └─→ [Ready] → Device fully paired
│
│
├─ ESP Device: Flash firmware
│  │
│  └─→ Power on
│      │
│      └─→ Creates WiFi AP (SIM800L-Gateway)
│          │
│          └─→ User connects to AP
│              │
│              └─→ Opens http://192.168.4.1/
│                  │
│                  └─→ Enters WiFi credentials
│                      │
│                      └─→ Gets API key from backoffice
│                          │
│                          └─→ Enters key in web portal
│                              │
│                              └─→ Saves config
│                                  │
│                                  └─→ [Ready] → Device reboots & connects
│
│
└─ Backoffice: Authentication
   │
   └─→ Login page
       │
       └─→ Enter email + password
           │
           └─→ Backend verifies credentials
               │
               └─→ Generates JWT token
                   │
                   └─→ Token stored in browser
                       │
                       └─→ [Authenticated] → Access dashboard
```

## Message Status Lifecycle

```
OUTBOUND MESSAGE (User sends SMS)
│
├─→ PENDING
│   (Created, awaiting device pickup)
│
├─→ SENT
│   (Device confirmed SMS sent)
│
└─→ DELIVERED or FAILED
    (Status update from device)


INBOUND MESSAGE (SMS arrives on device)
│
├─→ RECEIVED
│   (Device received from carrier, reported to backend)
│
├─→ [Webhooks fired] (sms:received)
│   (External services notified)
│
├─→ [Forwarded to Telegram]
│   (If user has Telegram enabled)
│
└─→ [Forwarded to WhatsApp]
    (If user has WhatsApp bridge enabled)
```

## Network Topology

```
          ┌─────────────────────────────────────┐
          │      Internet (Public)              │
          │                                     │
          │  - Telegram Bot API                │
          │  - WAHA Server (WhatsApp)          │
          │  - Mobile Carriers                 │
          │                                     │
          └──────────────────┬──────────────────┘
                             │
                    ┌────────▼─────────┐
                    │   Your Network    │
                    │ (LAN / 192.168.x) │
                    │                   │
                    │  Backend Server   │
                    │  (Linux / Docker) │
                    │  :8000            │
                    │                   │
                    ├─── MySQL Database │
                    │                   │
                    └────┬──────────────┘
                         │
        ┌────────────────┬┴────────────┬────────────────┐
        │                │             │                │
        ▼                ▼             ▼                ▼
    ┌────────┐      ┌─────────┐   ┌──────────┐    ┌──────────┐
    │Browser │      │Android  │   │ESP32     │    │WAHA      │
    │Backoff│      │Phone    │   │SIM800L   │    │Server    │
    │(5173)  │      │         │   │          │    │(3099)    │
    │        │      │Private  │   │WiFi      │    │          │
    │        │      │Cellular │   │Cellular  │    │          │
    │        │      │         │   │(SIM Card)│    │          │
    └────────┘      └─────────┘   └──────────┘    └──────────┘
```

---

**Key Takeaways:**
1. **Unified Backend** handles routing from all gateway types
2. **Multiple Gateways** work simultaneously (Android + ESP)
3. **Bidirectional** Telegram/WhatsApp for SMS relay and sending
4. **Persistent Storage** in database for all messages
5. **Real-time** backoffice updates via webhooks and polling
6. **Flexible** configuration (WiFi, Telegram, WhatsApp per device)

For detailed flows, see [`INTEGRATION.md`](INTEGRATION.md)
