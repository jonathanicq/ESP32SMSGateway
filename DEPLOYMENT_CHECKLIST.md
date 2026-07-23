# Deployment Checklist - SMS Gateway with ESP8266/ESP32 + SIM800L

Complete checklist for deploying the full SMS Gateway system with Telegram and WhatsApp integration.

## Pre-Deployment (Planning)

### Hardware Preparation (if using ESP)
- [ ] Order components:
  - [ ] ESP32 DevKit board (~$10)
  - [ ] SIM800L module (~$15)
  - [ ] Buck converter 5V→3.3V (~$3)
  - [ ] Capacitors, resistors (bulk electronics kit)
  - [ ] Active GSM SIM card
  - [ ] 5V power supply (2A+)

- [ ] Assemble hardware:
  - [ ] Review `esp/wiring.md` for pin connections
  - [ ] Solder connections (or breadboard for testing)
  - [ ] Test power supply voltage: 3.3V ±0.1V
  - [ ] Verify all GND connections

### Telegram Bot Setup
- [ ] Create bot via @BotFather:
  - [ ] Chat with @BotFather
  - [ ] Send `/newbot`
  - [ ] Choose name (e.g., "SMSGateway")
  - [ ] Copy Bot Token (e.g., `123456:ABC-DEF...`)
  - [ ] **Save bot token securely**

- [ ] Get Chat ID:
  - [ ] Message your new bot
  - [ ] Forward message to @userinfobot
  - [ ] Copy Chat ID (numeric)
  - [ ] **Save chat ID securely**

### Backend Infrastructure
- [ ] Provision server:
  - [ ] Linux VPS (Ubuntu 20.04+ recommended)
  - [ ] 2GB RAM minimum, 10GB disk
  - [ ] Public IP + domain name
  - [ ] SSL certificate (Let's Encrypt free)

- [ ] Install dependencies:
  - [ ] Python 3.8+
  - [ ] Node.js 16+
  - [ ] Docker & Docker Compose (recommended)
  - [ ] MySQL 8.0+ (or use Docker image)
  - [ ] Nginx (reverse proxy)

## Backend Deployment

### 1. Environment Configuration
```bash
# Clone repository
git clone <repo-url>
cd androidSmsGateway-master

# Create .env from template
cp backend/.env.example backend/.env

# Edit backend/.env
# Set these MANDATORY fields:
# - DATABASE_URL (MySQL connection string)
# - JWT_SECRET (generate with: openssl rand -hex 32)
# - SUPER_ADMIN_PASSWORD (change from default)

# Set these OPTIONAL fields:
# - TELEGRAM_BOT_TOKEN (from @BotFather)
# - TELEGRAM_WEBHOOK_CHAT_ID (from @userinfobot)
# - WAHA_BASE_URL & WAHA_API_KEY (if using WhatsApp)
```

Checklist:
- [ ] Database URL is correct format
- [ ] JWT_SECRET is 32+ characters (random)
- [ ] Super admin email is valid
- [ ] Super admin password changed from default
- [ ] Telegram bot token is valid (from @BotFather)
- [ ] Telegram chat ID is numeric
- [ ] No hardcoded secrets in repo (use .env only)

### 2. Database Setup

**Option A: Docker (Recommended)**
```bash
# Edit docker-compose.yml to adjust MySQL image if needed
# Database auto-creates on first run
docker-compose up -d db
sleep 10
docker-compose logs db  # Verify startup
```

Checklist:
- [ ] MySQL container running (`docker ps`)
- [ ] Port 3306 listening
- [ ] No connection errors in logs

**Option B: Manual MySQL**
```bash
mysql -u root -p
CREATE DATABASE sms_gateway CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'smsgw'@'localhost' IDENTIFIED BY 'your-password';
GRANT ALL PRIVILEGES ON sms_gateway.* TO 'smsgw'@'localhost';
FLUSH PRIVILEGES;
```

Checklist:
- [ ] Database created with UTF-8 collation
- [ ] User created with correct password
- [ ] Permissions granted
- [ ] CONNECTION_STRING matches database details

### 3. Backend Service Deployment

**Option A: Docker (Recommended)**
```bash
# Build and start services
docker-compose up -d backend

# Verify logs
docker-compose logs -f backend

# Expected output:
# app [INFO] v0.1.0 env=development
# app [INFO] Application startup complete
```

Checklist:
- [ ] Backend container running
- [ ] Database tables created automatically
- [ ] No startup errors in logs
- [ ] Health check passes: `curl http://localhost:8000/api/health`

**Option B: Manual Python**
```bash
cd backend
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
uvicorn app.main:app --host 0.0.0.0 --port 8000
```

Checklist:
- [ ] Virtual environment created
- [ ] Dependencies installed (no errors)
- [ ] Application started
- [ ] Listening on port 8000
- [ ] Database tables created

### 4. Frontend Deployment

```bash
# Build frontend
cd webfront
npm install
npm run build

# Deploy dist/ to web server:
# - Copy dist/* to /var/www/sms-gateway/
# - Or use: npm run preview (for testing)
```

Checklist:
- [ ] Dependencies installed
- [ ] Build succeeds (no errors)
- [ ] dist/ folder created
- [ ] Static files deployed to web server
- [ ] Nginx configured to serve /
- [ ] API URL points to backend

### 5. Reverse Proxy (Nginx)

```nginx
# /etc/nginx/sites-available/sms-gateway.yourdomain.com

upstream backend {
  server 127.0.0.1:8000;
}

server {
  listen 443 ssl http2;
  server_name sms-gateway.yourdomain.com;
  
  ssl_certificate /etc/letsencrypt/live/sms-gateway.yourdomain.com/fullchain.pem;
  ssl_certificate_key /etc/letsencrypt/live/sms-gateway.yourdomain.com/privkey.pem;
  
  # API Proxy
  location /api {
    proxy_pass http://backend;
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
  }
  
  # Frontend
  location / {
    root /var/www/sms-gateway;
    try_files $uri /index.html;
  }
}

# HTTP → HTTPS redirect
server {
  listen 80;
  server_name sms-gateway.yourdomain.com;
  return 301 https://$server_name$request_uri;
}
```

Checklist:
- [ ] Nginx config is valid (`nginx -t`)
- [ ] SSL certificate installed
- [ ] DNS records point to server
- [ ] CORS headers configured
- [ ] Proxy passes X-Forwarded-* headers
- [ ] Backoffice accessible at https://domain.com/

## Android App Deployment

### Installation

**Pre-built APK (Easiest):**
```bash
# Download latest from releases
wget https://github.com/thecoordinates/android-sms-gateway/releases/download/v0.1.3/sms-gateway-v0.1.3.apk

# Install
adb install sms-gateway-v0.1.3.apk
```

**From Source:**
```bash
cd android
./gradlew assembleRelease
adb install app/build/outputs/apk/release/app-release.apk
```

Checklist:
- [ ] APK downloaded or built
- [ ] Device connected via USB
- [ ] USB debugging enabled on device
- [ ] App installs without errors
- [ ] App launches successfully

### Initial Setup

- [ ] Open app on Android phone
- [ ] Grant SMS permissions
- [ ] Enter server URL: `https://sms-gateway.yourdomain.com`
- [ ] Tap "Pair with backoffice"
- [ ] Note 6-character code

## ESP8266/ESP32 Gateway Deployment

### Firmware Flashing

```bash
cd esp

# For ESP32
platformio run --target upload -e esp32

# For ESP8266
platformio run --target upload -e esp8266

# Monitor output
platformio device monitor --baud 115200
```

Expected output:
```
=== SIM800L SMS Gateway ===
[SIM800L] Testing connection...
[SIM800L] Connected!
[WiFi] AP Mode: SSID=SIM800L-Gateway, IP=192.168.4.1
```

Checklist:
- [ ] Firmware builds without errors
- [ ] Device detected by platformio
- [ ] Upload completes successfully
- [ ] Serial monitor shows startup messages
- [ ] WiFi AP is broadcasting

### Device Configuration

```
1. Connect to WiFi: SIM800L-Gateway (pwd: 12345678)
2. Open browser: http://192.168.4.1/
3. Configure tabs:

WiFi Tab:
  - SSID: Your home WiFi network
  - Password: Your WiFi password
  [Save]

API Tab:
  - URL: https://sms-gateway.yourdomain.com/api
  - Key: <Get from backoffice device pairing>
  [Save]

Telegram Tab (optional):
  - Enable: [✓]
  - Token: 123456:ABC-DEF... (from @BotFather)
  - Chat ID: 987654321 (from @userinfobot)
  [Save]

WhatsApp Tab (optional):
  - Enable: [✓]
  - Number: +1234567890 (forward SMS to this number)
  [Save]

4. Device reboots and connects to home WiFi
```

Checklist:
- [ ] WiFi credentials entered correctly
- [ ] Backend API URL is HTTPS
- [ ] API key from backoffice obtained
- [ ] Telegram bot token is valid (starts with numbers)
- [ ] Telegram chat ID is numeric
- [ ] WhatsApp number is E.164 format
- [ ] All configurations saved
- [ ] Device reboots successfully
- [ ] Serial monitor shows: "[WiFi] Connected! IP: 192.168.x.x"

## Backoffice Setup

### Initial Login
```
URL: https://sms-gateway.yourdomain.com/
Email: admin@thecoordinates.xyz
Password: <from SUPER_ADMIN_PASSWORD in .env>
```

Checklist:
- [ ] Login page loads
- [ ] Credentials accepted
- [ ] Dashboard displays

### First Configuration

#### 1. Change Admin Password
- [ ] Click Profile (top-right menu)
- [ ] Change password from default
- [ ] Confirm change

#### 2. Pair Android Device
- [ ] Open Devices → Pair a phone
- [ ] Enter 6-character code from app
- [ ] Confirm pairing
- [ ] Device appears in device list

#### 3. Pair ESP Gateway
- [ ] Open Devices → Create new device
- [ ] Name: "SIM800L Gateway 1"
- [ ] Save
- [ ] Copy API key
- [ ] Paste into ESP web portal (`http://192.168.4.1/api-tab`)
- [ ] Device becomes active

#### 4. Configure Telegram Bridge (if using)
- [ ] Open Settings → Telegram
- [ ] Enable: [✓]
- [ ] Bot Token: Paste from @BotFather
- [ ] Chat ID: Paste from @userinfobot
- [ ] Save
- [ ] Test: Send SMS to device, should receive in Telegram

#### 5. Configure WhatsApp Bridge (if using)
- [ ] Open Settings → WhatsApp
- [ ] Enable: [✓]
- [ ] Configure WAHA endpoint and API key
- [ ] Test: Send SMS to device, should forward to WhatsApp

#### 6. Set Up Webhooks (optional)
- [ ] Open Webhooks
- [ ] Add webhook: `https://your-service.com/webhook`
- [ ] Select event: `sms:received`
- [ ] Save
- [ ] Test by sending SMS to device

Checklist:
- [ ] Admin password changed
- [ ] All devices paired and visible
- [ ] Telegram settings configured and tested
- [ ] WhatsApp settings configured (optional)
- [ ] Webhooks configured (optional)
- [ ] Dashboard shows "Devices: X" (devices count)

## Testing & Validation

### Test Matrix

```
Test Case                      Android    ESP32      Expected
──────────────────────────────────────────────────────────
Receive SMS                    ✓          ✓          Appears in backoffice
Send SMS from backoffice       ✓          ✓          SMS sent successfully
Forward to Telegram            ✓          ✓          Message in Telegram chat
Send SMS via Telegram          ✓          ✓          SMS sent by device
Webhook delivery               ✓          ✓          External API receives event
Multiple devices               ✓          ✓          All relay independently
Device offline resilience      ✓          ✓          Queued messages sent on reconnect
API key revocation             ✓          ✓          Device stops working
Browser backoffice             ✓          ✓          Full functionality
Message history                ✓          ✓          All SMS logged
Device status (last_seen)      ✓          ✓          Updates correctly
```

### Automated Tests

```bash
# Backend health check
curl https://sms-gateway.yourdomain.com/api/health

# Login and get token
curl -X POST https://sms-gateway.yourdomain.com/api/auth/login \
  -d "email=admin@thecoordinates.xyz&password=YourPassword"

# List devices
curl -H "Authorization: Bearer <token>" \
  https://sms-gateway.yourdomain.com/api/devices

# Send test SMS
curl -X POST https://sms-gateway.yourdomain.com/api/messages \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": 1,
    "phone_number": "+447700900123",
    "text": "Test message"
  }'
```

Checklist:
- [ ] Health check returns 200 OK
- [ ] Login returns valid JWT token
- [ ] Device list shows all paired devices
- [ ] Send SMS creates message in database
- [ ] Message status updates to SENT
- [ ] All devices receive and report status

## Monitoring & Maintenance

### Daily Checks
```bash
# Backend logs
docker-compose logs -f backend | grep -i error

# Database connectivity
curl https://sms-gateway.yourdomain.com/api/health

# Device activity
curl -H "Authorization: Bearer <token>" \
  https://sms-gateway.yourdomain.com/api/devices | \
  jq '.[] | {name, last_seen}'

# Message queue
curl -H "Authorization: Bearer <token>" \
  https://sms-gateway.yourdomain.com/api/messages?status=pending | \
  jq '.[].status'
```

Checklist (Daily):
- [ ] No error logs
- [ ] All devices connected (`last_seen` within 5 minutes)
- [ ] No pending messages stuck
- [ ] API responsive

### Weekly Checks
```bash
# Database size
mysql -u smsgw -p sms_gateway -e "SELECT table_name, 
  ROUND(((data_length + index_length) / 1024 / 1024), 2) AS size_mb 
  FROM information_schema.tables 
  WHERE table_schema = 'sms_gateway'"

# Message throughput
curl -H "Authorization: Bearer <token>" \
  https://sms-gateway.yourdomain.com/api/messages?limit=1000 | \
  jq 'group_by(.created_at | split("T")[0]) | 
  map({date: .[0].created_at | split("T")[0], count: length})'

# Device health
curl -H "Authorization: Bearer <token>" \
  https://sms-gateway.yourdomain.com/api/devices | \
  jq '.[] | {name, last_seen, age_minutes: 
  ((now - (.last_seen | fromdateiso8601)) / 60 | floor)}'
```

Checklist (Weekly):
- [ ] Database size under control
- [ ] Message throughput within expectations
- [ ] No devices inactive for >24 hours
- [ ] Telegram bot still responding
- [ ] SSL certificate valid

### Monthly Maintenance
```bash
# Backup database
mysqldump -u smsgw -p sms_gateway > backup-$(date +%Y%m%d).sql

# Rotate logs
docker-compose logs backend > logs/backend-$(date +%Y%m%d).log
docker-compose logs webfront > logs/webfront-$(date +%Y%m%d).log

# Check for updates
git fetch origin
git log --oneline -5 origin/main

# SSL certificate renewal (auto with Let's Encrypt)
certbot renew --dry-run
```

Checklist (Monthly):
- [ ] Database backup created
- [ ] Logs archived
- [ ] Git upstream checked for updates
- [ ] SSL certificate renewal tested

## Rollback Plan

### If Backend Breaks
```bash
# Stop services
docker-compose down

# Restore from backup
mysql -u smsgw -p sms_gateway < backup-YYYYMMDD.sql

# Check git history
git log --oneline | head -5

# Revert if needed
git revert <commit-hash>

# Restart
docker-compose up -d
```

### If Devices Stop Working
```bash
# Check device logs
platformio device monitor --baud 115200

# Common issues:
# 1. WiFi disconnected → Check AP, reconfigure
# 2. API key invalid → Re-pair in backoffice
# 3. Power issue → Check 3.3V supply

# Factory reset (last resort)
# Delete config.json from SPIFFS via serial
# Device will recreate WiFi AP on reboot
```

## Security Hardening

Post-Deployment:
- [ ] Change default admin password ✓ (already listed above)
- [ ] Enable HTTPS only (redirect HTTP → HTTPS) ✓
- [ ] Set JWT_SECRET to 32+ random characters ✓
- [ ] Change WiFi AP password (esp/main.cpp line 23)
- [ ] Restrict database access to localhost only
- [ ] Enable MySQL binary logging (replication backup)
- [ ] Set up firewall rules:
  - [ ] Allow port 443 (HTTPS)
  - [ ] Allow port 80 (HTTP redirect)
  - [ ] Restrict port 3306 (MySQL) to localhost
  - [ ] Restrict port 5173 (frontend) if applicable
- [ ] Enable rate limiting on /api endpoints
- [ ] Set up monitoring/alerting
- [ ] Regular security updates: `apt update && apt upgrade`

## Final Validation

```bash
# 1. Backoffice loads and is responsive
curl -L https://sms-gateway.yourdomain.com/ | head -20

# 2. Can login
curl -X POST https://sms-gateway.yourdomain.com/api/auth/login \
  -d "email=admin@domain&password=pwd"

# 3. Devices are paired
curl -H "Authorization: Bearer <token>" \
  https://sms-gateway.yourdomain.com/api/devices | jq '.length'

# 4. Send test SMS succeeds
curl -X POST https://sms-gateway.yourdomain.com/api/messages \
  -H "Authorization: Bearer <token>" \
  -H "Content-Type: application/json" \
  -d '{"device_id":1,"phone_number":"+1234567890","text":"test"}'

# 5. Message appears in backoffice
# (Open browser and check Messages tab)

# 6. Telegram receives SMS
# (Send SMS to device, check Telegram)

# 7. Can send SMS via Telegram
# (Send "/sms +1234567890 test" in Telegram)
```

Final Checklist:
- [ ] All tests pass
- [ ] No error messages
- [ ] Devices are responding
- [ ] Backoffice functional
- [ ] Telegram integration working
- [ ] Ready for production use

---

**Deployment Status:** Ready when all checkboxes are ✓

**Support:** See `/SETUP_GUIDE.md` for troubleshooting, `/INTEGRATION.md` for architecture
