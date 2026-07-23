# Quick Reference - SMS Gateway

One-page guide to key information for development and deployment.

## Architecture at a Glance

```
User (Web/Telegram/Android) → Backend API → [Android Phone | ESP + SIM800L]
                                  ↓
                            Database (MySQL)
                                  ↓
                    [Webhooks | WhatsApp | Telegram]
```

## Key Ports & URLs

| Service | Port | URL | Purpose |
|---------|------|-----|---------|
| Backend | 8000 | http://localhost:8000/api | REST API |
| Frontend | 5173 | http://localhost:5173 | Web dashboard |
| Backoffice | 3050 | http://localhost:3050 | Alternative dashboard |
| MySQL | 3306 | - | Database |
| ESP WiFi | 192.168.4.1 | http://192.168.4.1/ | Captive portal (AP mode) |
| ESP WiFi | 80 | http://<esp-ip>/ | Config portal (connected) |

## Environment Variables (Critical)

```bash
# Backend
APP_ENV=production
DATABASE_URL=mysql+pymysql://user:pass@host/db
JWT_SECRET=<32+ random chars: openssl rand -hex 32>
SUPER_ADMIN_PASSWORD=<change from default>

# Telegram (Optional)
TELEGRAM_BOT_TOKEN=<from @BotFather>
TELEGRAM_WEBHOOK_CHAT_ID=<numeric ID>

# WhatsApp (Optional)
WAHA_BASE_URL=http://192.168.0.135:3099
WAHA_API_KEY=<your key>
```

## API Endpoints (Authenticated)

```bash
# Messages
POST   /api/messages                  # Send SMS
GET    /api/messages                  # List SMS
GET    /api/messages/:id              # Get SMS detail

# Devices
GET    /api/devices                   # List devices
POST   /api/devices                   # Create device
DELETE /api/devices/:id               # Delete device

# Telegrams
GET    /api/telegram/settings         # Get Telegram config
POST   /api/telegram/settings         # Update config
POST   /api/telegram/webhook          # Telegram webhook (public)

# Webhooks
POST   /api/webhooks                  # Create webhook
GET    /api/webhooks                  # List webhooks
DELETE /api/webhooks/:id              # Delete webhook
```

## Device API Endpoints (X-API-Key header)

```bash
GET    /api/device/messages/pending   # Poll for SMS to send
POST   /api/device/messages/:id/status # Report delivery status
POST   /api/device/messages/inbound   # Report incoming SMS
```

## SMS Message Status Lifecycle

```
OUTBOUND:  PENDING → SENT → DELIVERED (or FAILED)
INBOUND:   RECEIVED → [forwarded to bridges]
```

## Telegram Commands

```
/sms +1234567890 Message text   # Send SMS to +1234567890
/sms +447700900123 Hello        # Another example
```

**Grammar:** `/sms <E.164> <message>`

## ESP Configuration Portal Tabs

| Tab | Fields | Notes |
|-----|--------|-------|
| WiFi | SSID, Password | Home network credentials |
| API | URL, API Key | Backend endpoint + device key |
| Telegram | Token, Chat ID | Bot token + your chat ID |
| WhatsApp | Enable, Number | Target phone (E.164) |

## Hardware Pin Configuration (ESP32)

| ESP32 | ← SIM800L |
|-------|-----------|
| Pin 16 (RX2) | TX |
| Pin 17 (TX2) | RX |
| Pin 5 (GPIO5) | RST |
| GND | GND |
| 3.3V (via regulator) | VCC |

## Getting Telegram Credentials

### Bot Token
1. Chat: @BotFather
2. Command: `/newbot`
3. Save: `123456:ABC-DEF...`

### Chat ID
1. Message your bot
2. Chat: @userinfobot
3. Save: `987654321` (numeric)

## Getting WhatsApp Credentials (WAHA)

1. Install WAHA: `docker run -it devlikeapro/whatsapp-http-api:latest`
2. Get auth link: `http://localhost:3099/api/auth/qr`
3. Scan QR → WhatsApp -> Linked devices
4. Save API key from WAHA settings

## Docker Commands

```bash
# Start all services
docker-compose up -d

# View logs
docker-compose logs -f backend      # Backend logs
docker-compose logs -f webfront     # Frontend logs

# Stop services
docker-compose down

# Rebuild images
docker-compose build --no-cache

# Database shell
docker-compose exec db mysql -u root -p
```

## Common Tasks

### Add a New Device
```bash
# Via backoffice:
Devices → "Add Device" → Enter name → Get API key

# Via API:
curl -X POST http://localhost:8000/api/devices \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"name": "Device Name"}'
```

### Send SMS
```bash
curl -X POST http://localhost:8000/api/messages \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": 1,
    "phone_number": "+1234567890",
    "text": "Hello"
  }'
```

### Get Device List
```bash
curl -H "Authorization: Bearer <token>" \
  http://localhost:8000/api/devices | jq
```

### Check Device Health
```bash
curl -H "Authorization: Bearer <token>" \
  http://localhost:8000/api/devices | \
  jq '.[] | {name, last_seen}'
```

## File Structure

```
project/
├── esp/                    # ESP firmware
│   ├── platformio.ini     # Build config
│   ├── src/main.cpp       # Firmware code
│   ├── README.md          # Documentation
│   ├── QUICKSTART.md      # 15-min setup
│   └── wiring.md          # Hardware guide
├── backend/               # FastAPI backend
│   ├── app/
│   │   ├── main.py       # App entry
│   │   ├── config.py     # Settings
│   │   ├── models.py     # Database models
│   │   ├── telegram.py   # Telegram client
│   │   └── routers/      # API routes
│   ├── requirements.txt   # Python deps
│   └── .env             # Configuration
├── webfront/              # React frontend
│   ├── src/
│   ├── package.json      # JS deps
│   └── vite.config.ts    # Build config
├── android/               # Kotlin app
│   ├── app/
│   ├── build.gradle.kts
│   └── gradlew
├── docker-compose.yml     # Container orchestration
├── README.md              # Main docs
├── SETUP_GUIDE.md         # Setup walkthrough
├── INTEGRATION.md         # Architecture
├── ARCHITECTURE.md        # System diagrams
├── DEPLOYMENT_CHECKLIST.md # Go-live checklist
└── QUICK_REFERENCE.md     # This file
```

## Database Schema (Key Tables)

```sql
-- Users
users (id, email, hashed_password, role, tier, created_at)

-- Devices
devices (id, name, api_key, owner_id, last_seen, created_at)

-- Messages
messages (id, device_id, direction, phone_number, text, status, error, created_at)

-- Bridge Settings
bridge_settings (id, owner_id, 
  telegram_enabled, telegram_bot_token, telegram_chat_id,
  waha_session, forward_to_chat, send_device_id, created_at)

-- Webhooks
webhooks (id, owner_id, url, event, is_active, created_at)
```

## Troubleshooting One-Liners

```bash
# Backend won't start?
docker-compose logs backend | tail -50

# Device not connecting?
platformio device monitor --baud 115200

# Telegram not receiving?
curl "https://api.telegram.org/bot<TOKEN>/getMe"

# Database connectivity issue?
docker-compose exec db mysql -u root -p -e "SELECT 1"

# Frontend shows blank?
curl http://localhost:5173 | head -20

# API not responding?
curl http://localhost:8000/api/health | jq

# Check device status
curl -H "Authorization: Bearer <token>" \
  http://localhost:8000/api/devices | jq '.[] | {name, last_seen}'
```

## Security Checklist

- [ ] JWT_SECRET is 32+ random characters
- [ ] SUPER_ADMIN_PASSWORD changed from default
- [ ] HTTPS enabled (SSL certificate installed)
- [ ] Database password is strong
- [ ] Telegram bot token in .env (not hardcoded)
- [ ] API keys not logged in debug output
- [ ] CORS origins restricted to trusted domains
- [ ] Rate limiting enabled on public endpoints
- [ ] Firewall restricts DB access to localhost

## Performance Tuning

| Setting | Default | Recommendation |
|---------|---------|-----------------|
| Device poll interval | 5s | 10-30s (trade-off latency/load) |
| Message retention | Unlimited | Archive after 90 days |
| Database connections | 10 | Match available pool |
| Telegram timeout | 10s | OK for most cases |
| SMS delivery timeout | 30s | Adjust per carrier |

## Scaling Strategies

1. **Horizontal:** Add more backend instances behind load balancer
2. **Caching:** Redis cache for device lookups
3. **Queue:** RabbitMQ/Redis for message queue
4. **Database:** MySQL cluster or managed service
5. **CDN:** CloudFlare for frontend assets

## Monitoring URLs

```
Backend Health:    http://localhost:8000/api/health
Frontend Metrics:  http://localhost:5173/metrics (if enabled)
Database Status:   mysql -u smsgw -p -e "SHOW PROCESSLIST"
Docker Stats:      docker stats
```

## Emergency Contacts

- **Telegram Bot Support:** @BotFather
- **WAHA Issues:** https://waha.devlike.pro/
- **PlatformIO Issues:** https://community.platformio.org/

## Documentation Quick Links

| Need | File |
|------|------|
| Setup guide | SETUP_GUIDE.md |
| Architecture | ARCHITECTURE.md |
| Integration flows | INTEGRATION.md |
| ESP firmware | esp/README.md |
| ESP quickstart | esp/QUICKSTART.md |
| ESP wiring | esp/wiring.md |
| Deployment checklist | DEPLOYMENT_CHECKLIST.md |
| Code changes | ESP_ADDITIONS.md |

## Key Dates to Remember

- **JWT token expiry:** 60 minutes (configurable)
- **Refresh token:** 30 days
- **Message retention:** Unlimited (set policy as needed)
- **SSL cert renewal:** Auto (Let's Encrypt every 90 days)

## Command Cheat Sheet

```bash
# Start everything
docker-compose up -d

# Stop everything
docker-compose down

# Backend only
docker-compose up -d backend
docker-compose logs -f backend

# Database shell
docker-compose exec db mysql -u root -p sms_gateway

# ESP firmware build & upload
cd esp && platformio run --target upload -e esp32

# Generate new JWT secret
openssl rand -hex 32

# Test API
curl http://localhost:8000/api/health

# Get JWT token
curl -X POST http://localhost:8000/api/auth/login \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "username=admin@domain&password=password"
```

---

**Last Updated:** 2026-06-29  
**Version:** 0.1.0 (with ESP gateway)

For detailed help, see the full documentation in `/` directory.
