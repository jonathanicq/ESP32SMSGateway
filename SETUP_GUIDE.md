# Complete Setup Guide - SMS Gateway

This guide covers setting up the entire SMS Gateway system including the backend, frontend, Android app, and the new ESP8266/ESP32 + SIM800L module.

## Prerequisites

### For Backend/Frontend
- Docker & Docker Compose (recommended) OR
- Python 3.8+, Node.js 16+, MySQL 8.0+
- 2GB disk space

### For Android App
- Android phone (5.0+) with active SMS capability
- USB cable for ADB (optional)

### For ESP Gateway
- ESP32 DevKit or ESP8266 board (~$10)
- SIM800L module (~$15)
- 5V power supply (2A+)
- Assembled hardware (see [`esp/wiring.md`](esp/wiring.md))

## Quick Start (Docker)

### 1. Clone & Navigate
```bash
git clone <repo-url> && cd androidSmsGateway-master
```

### 2. Configure Environment
```bash
cp backend/.env.example backend/.env

# Edit backend/.env with your settings:
# - Set JWT_SECRET to a random string
# - Configure TELEGRAM_BOT_TOKEN and TELEGRAM_WEBHOOK_CHAT_ID (optional)
# - Configure WAHA settings if using WhatsApp (optional)
```

### 3. Start Services
```bash
docker-compose up -d

# Wait ~30 seconds for startup
# Backend: http://localhost:8000/api
# Frontend: http://localhost:5173
# Backoffice: http://localhost:3050
```

### 4. Verify Installation
```bash
# Test backend
curl http://localhost:8000/api/health

# Check logs
docker-compose logs -f backend
docker-compose logs -f webfront
```

### 5. First Login
- Open `http://localhost:3050/` in browser
- Use credentials from `.env`:
  - Email: `admin@thecoordinates.xyz`
  - Password: `ChangeMe123!` (or your override)
- **IMPORTANT:** Change password immediately in Devices → Profile

## Traditional Setup (No Docker)

### Backend

#### 1. Create Virtual Environment
```bash
cd backend
python3 -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
```

#### 2. Install Dependencies
```bash
pip install -r requirements.txt
# Or using uv (faster):
uv sync
```

#### 3. Database Setup
```bash
# For development (SQLite):
# No setup needed, will auto-create on startup

# For production (MySQL):
mysql -u root -p
CREATE DATABASE sms_gateway;
CREATE USER 'smsgw'@'localhost' IDENTIFIED BY 'password';
GRANT ALL PRIVILEGES ON sms_gateway.* TO 'smsgw'@'localhost';
FLUSH PRIVILEGES;
```

#### 4. Configure Environment
```bash
cp .env.example .env

# Edit .env:
# - For SQLite (dev): DATABASE_URL=sqlite:///./sms_gateway.db
# - For MySQL: DATABASE_URL=mysql+pymysql://smsgw:password@localhost/sms_gateway
# - Set JWT_SECRET
# - Add Telegram config (optional)
```

#### 5. Run Backend
```bash
uvicorn app.main:app --reload --host 0.0.0.0 --port 8000
```

Backend now at `http://localhost:8000/api`

### Frontend

#### 1. Install Dependencies
```bash
cd webfront
npm install
```

#### 2. Create .env (if needed)
```bash
# Frontend uses VITE_API_URL for backend endpoint
# Default: http://localhost:8000/api
# Usually auto-detected, but create .env if needed:
echo "VITE_API_URL=http://localhost:8000/api" > .env
```

#### 3. Run Development Server
```bash
npm run dev
```

Frontend now at `http://localhost:5173`

### Android App

#### Option A: Install Pre-Built APK
```bash
# Download latest APK
wget https://test.sms-gateway.thecoordinates.app/downloads/sms-gateway-latest.apk

# Install via ADB
adb install sms-gateway-latest.apk

# Or via file manager on phone
# Enable "Install unknown apps" in Settings
```

#### Option B: Build from Source
```bash
cd android

# Build with Gradle
./gradlew assembleDebug

# Install built APK
adb install app/build/outputs/apk/debug/app-debug.apk

# Or open android/ in Android Studio and click "Run"
```

### ESP Gateway

#### 1. Install PlatformIO
```bash
pip install platformio
# Or via VS Code extension
```

#### 2. Build & Flash
```bash
cd esp
platformio run --target upload -e esp32
# Wait for "✓ Program size" message
```

#### 3. Configure Device
```bash
# Device creates WiFi AP: SIM800L-Gateway
# Connect with password: 12345678
# Open: http://192.168.4.1/

# Configure:
# - WiFi: Your home WiFi SSID + password
# - API URL: http://<backend-ip>:8000/api
# - API Key: Get from backend device pairing
# - Telegram (optional): Bot token + chat ID

# Save → Device reboots and connects
```

## Backend Configuration

### Environment Variables (.env)

```env
# === Basic ===
APP_ENV=production                    # development | production
APP_VERSION=0.1.0
GIT_COMMIT=unknown

# === Database ===
DATABASE_URL=mysql+pymysql://user:pass@host/db

# === JWT Auth ===
JWT_SECRET=use-a-long-random-string-here
JWT_ALGORITHM=HS256
ACCESS_TOKEN_TTL_MINUTES=60
REFRESH_TOKEN_TTL_DAYS=30

# === Master Admin ===
SUPER_ADMIN_EMAIL=admin@thecoordinates.xyz
SUPER_ADMIN_PASSWORD=ChangeMe123!

# === WhatsApp (Optional) ===
WAHA_BASE_URL=http://192.168.0.135:3099      # If using WAHA
WAHA_API_KEY=your-api-key-here
WAHA_SESSION=default
WHATSAPP_TRIGGER_KEYWORD=@sms

# === Telegram (Optional) ===
TELEGRAM_BOT_TOKEN=123456:ABC-DEF...        # From @BotFather
TELEGRAM_WEBHOOK_CHAT_ID=987654321          # Your chat ID
TELEGRAM_SMS_TRIGGER_KEYWORD=/sms
```

### Getting Telegram Credentials

**Create Bot:**
1. Open Telegram, chat with `@BotFather`
2. Send `/newbot`
3. Follow prompts (name, username)
4. Save the **Bot Token**

**Get Chat ID:**
1. Message your new bot
2. Open `@userinfobot`
3. Forward a message from your bot
4. Copy **Your ID** from the response

## Device Pairing

### From Backoffice (Recommended)

1. Open `http://localhost:5173/`
2. Login with admin credentials
3. Go to **Devices** → **Pair a phone**
4. Follow one of two methods:

#### Method A: Via Code (Android App)
1. Open Android app → Set server URL
2. Tap **Pair with backoffice**
3. Note 6-character code shown
4. Backoffice: Enter code → Get API key
5. App: Polls and receives key automatically

#### Method B: Manual (ESP Device)
1. Backoffice: Create device manually
2. Give it a name (e.g., "SIM800L #1")
3. Copy generated API key
4. Paste into ESP setup portal

### From Command Line

```bash
# Get device details
curl -H "Authorization: Bearer <token>" \
  http://localhost:8000/api/devices

# Manually create device
curl -X POST http://localhost:8000/api/devices \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"name": "My Gateway"}'
```

## Testing the System

### 1. Verify Backend Connectivity
```bash
# Health check
curl http://localhost:8000/api/health

# List devices
curl -H "Authorization: Bearer <token>" \
  http://localhost:8000/api/devices
```

### 2. Test Android Gateway
```bash
# In backoffice:
# 1. Go to Messages
# 2. Select device
# 3. Send test SMS to a number you can receive on
# 4. Phone should send the SMS
# 5. Watch message status change to "SENT"
```

### 3. Test ESP Gateway
```bash
# Serial monitor
platformio device monitor --baud 115200

# Send SMS from backoffice to number you can test
# Should see in serial:
# [SMS] Sending SMS via SIM800L...
# [API] Sending SMS to backend...
```

### 4. Test Telegram Integration
```bash
# Configure bot token + chat ID in backend
# Send SMS to gateway
# Should receive formatted message in Telegram

# Send from Telegram:
/sms +1234567890 Test message

# Should create outbound SMS in backend
# Device picks up and sends
```

### 5. Test Webhooks
```bash
# Backoffice: Set up webhook to your service
# URL: http://your-server.com/webhook
# Event: sms:received

# Receive SMS on gateway
# Backend should POST to your webhook:
# {
#   "event": "sms:received",
#   "message": {
#     "id": 1,
#     "phone_number": "+447700900123",
#     "text": "Hello",
#     ...
#   }
# }
```

## Troubleshooting

### Backend won't start
```bash
# Check logs
docker-compose logs backend

# Common issues:
# - Database connection failed → Check DATABASE_URL
# - Port 8000 in use → Change port in docker-compose.yml
# - Out of disk space → Check df -h
```

### Frontend shows "Cannot connect to backend"
```bash
# Check VITE_API_URL
# Should be http://localhost:8000 (dev) or https://domain.com (prod)

# Test connectivity
curl http://localhost:8000/api/health
```

### Android app can't pair
```bash
# Check:
# - Server URL is correct
# - Backend is running
# - Device has internet connection
# - Firewall not blocking port 8000
```

### ESP device won't connect
```bash
# Check serial output
platformio device monitor

# Issues:
# [SIM800L] Connected! ✓ - Hardware is OK
# [WiFi] AP Mode - Needs WiFi config
# No output - Check USB connection or baud rate

# Reconnect to SIM800L-Gateway AP and reconfigure
```

### SMS not forwarding to Telegram
```bash
# Check:
# - Bot token is correct (from @BotFather)
# - Chat ID is numeric and correct
# - You've messaged the bot first (establishes chat)
# - Backend logs show: "Sending to Telegram..."

# Verify bot is working
curl -X GET "https://api.telegram.org/bot<TOKEN>/getMe"
# Should return: {"ok":true,"result":{"id":123...}}
```

## Production Deployment

### Hostname & SSL
```bash
# Use a proper domain with SSL certificate
# Edit backend/.env:
CORS_ORIGINS=https://yourdomain.com,https://www.yourdomain.com
```

### Database
```bash
# Use managed MySQL (AWS RDS, Google Cloud SQL, etc.)
DATABASE_URL=mysql+pymysql://user:pass@cloudsql-host/db
```

### Reverse Proxy
```nginx
# nginx configuration
server {
  listen 443 ssl http2;
  server_name sms-gateway.yourdomain.com;
  
  ssl_certificate /etc/letsencrypt/live/sms-gateway.yourdomain.com/fullchain.pem;
  ssl_certificate_key /etc/letsencrypt/live/sms-gateway.yourdomain.com/privkey.pem;
  
  location /api {
    proxy_pass http://localhost:8000;
    proxy_set_header X-Real-IP $remote_addr;
  }
  
  location / {
    proxy_pass http://localhost:5173;
  }
}
```

### Monitoring
```bash
# Set up health check endpoint monitoring
curl -s http://localhost:8000/api/health | grep '"detail":"ok"'

# Monitor device activity
curl http://localhost:8000/api/devices | jq '.[] | {name, last_seen}'

# Check message queue
curl http://localhost:8000/api/messages?direction=outbound&status=pending
```

## Next Steps

1. **Pair additional devices:** Add more Android phones or ESP modules
2. **Configure webhooks:** Send SMS events to external services
3. **Set up monitoring:** Alert on device offline or message failures
4. **Enable backups:** Regular database backups for production
5. **Scale horizontally:** Add more backend instances behind load balancer

## Support

- **Issues:** Check `docker-compose logs` or serial output
- **Questions:** See [`INTEGRATION.md`](INTEGRATION.md) for architecture overview
- **Contributing:** Pull requests welcome for bugs and features

---

**Ready?** Start with: `docker-compose up -d` and open `http://localhost:5173/`
