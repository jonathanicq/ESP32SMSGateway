# WiFi SMS Gateway - Complete Guide

Send and receive SMS via WiFi without internet connection!

## 🚀 Quick Start

### 1. Upload Firmware
Replace the old `SIM800L_Gateway.ino` with the new `WiFi_SMS_API.ino` that includes:
- ✅ Web Dashboard (send/view SMS)
- ✅ RESTful API for SMS
- ✅ SMS History Storage
- ✅ No internet required
- ✅ Works completely offline

### 2. Connect to WiFi
1. Power on ESP32
2. Find WiFi: **SIM800L-Gateway**
3. Password: **12345678**
4. Open browser: `http://192.168.4.1/`

### 3. Send SMS
1. Go to "✉️ Send SMS" tab
2. Enter phone number (E.164 format: `+1234567890`)
3. Type message
4. Click "Send SMS"

### 4. View Messages
1. Go to "📥 Inbox" tab
2. See all sent/received SMS with timestamps
3. Click "🔄 Refresh" to update

---

## 💻 Web Dashboard Features

### Send SMS Tab
- **Phone Number:** Enter in E.164 format (e.g., `+1234567890`)
- **Message:** Up to 160 characters (shown in real-time)
- **Status:** Immediate feedback if SMS was sent

### Inbox Tab
- **All Messages:** Sent and received SMS
- **Color Coding:**
  - 🟢 Green border = Received SMS
  - 🔵 Blue border = Sent SMS
- **Auto-refresh:** Updates every 5 seconds
- **Timestamps:** Exact date/time for each message

### Status Tab
- SIM800L connection status
- WiFi AP information
- Total SMS count
- Device uptime

---

## 🔌 REST API Reference

### Get All SMS Messages

```bash
GET http://192.168.4.1/api/sms/list
```

**Response:**
```json
{
  "messages": [
    {
      "id": "1",
      "phone": "+1234567890",
      "text": "Hello world",
      "direction": "outbound",
      "timestamp": "23/07/2026 19:11:05"
    },
    {
      "id": "2",
      "phone": "+1234567890",
      "text": "Hi there!",
      "direction": "inbound",
      "timestamp": "23/07/2026 19:11:10"
    }
  ],
  "total": 2
}
```

### Send SMS

```bash
POST http://192.168.4.1/api/sms/send
Content-Type: application/json

{
  "phone": "+1234567890",
  "text": "Your message here"
}
```

**Response (Success):**
```json
{
  "success": true,
  "message": "SMS sent"
}
```

**Response (Error):**
```json
{
  "success": false,
  "error": "Failed to send SMS"
}
```

### Get Device Status

```bash
GET http://192.168.4.1/api/status
```

**Response:**
```json
{
  "sim800l_status": "Connected",
  "wifi_ap": "SIM800L-Gateway",
  "total_sms": 42,
  "uptime": 3600,
  "ip_address": "192.168.4.1"
}
```

---

## 📱 Example: Send SMS via cURL

```bash
# Send SMS to +1234567890
curl -X POST http://192.168.4.1/api/sms/send \
  -H "Content-Type: application/json" \
  -d '{"phone":"+1234567890","text":"Hello from WiFi SMS!"}'

# Get all SMS
curl http://192.168.4.1/api/sms/list

# Get status
curl http://192.168.4.1/api/status
```

---

## 🔑 Phone Number Format (E.164)

Always use international format with `+` prefix:

| Country | Format | Example |
|---------|--------|---------|
| USA | +1 | +12025551234 |
| UK | +44 | +442071838750 |
| Brazil | +55 | +5511987654321 |
| Portugal | +351 | +351912345678 |
| Spain | +34 | +34912345678 |

---

## 💾 Storage & Limits

- **SMS Storage:** Up to 100 messages in memory
- **Persistent:** All SMS saved to SPIFFS
- **Auto-save:** Messages saved immediately after send/receive
- **Survives power loss:** SMS history restored on reboot

---

## ⚙️ Technical Details

### Architecture
```
┌─────────────────────┐
│   Web Dashboard     │
│  (HTML/JavaScript)  │
└──────────┬──────────┘
           │ HTTP
           ▼
┌─────────────────────┐
│   WiFi API Server   │
│   (ESP32)           │
└──────────┬──────────┘
           │ Serial
           ▼
┌─────────────────────┐
│   SIM800L Module    │
│  (GSM/GPRS)         │
└─────────────────────┘
```

### Data Flow

**Sending SMS:**
1. Web form → HTTP POST to `/api/sms/send`
2. API validates phone & message
3. AT command sent to SIM800L: `AT+CMGS="phone"`
4. Message transmitted via GSM network
5. SMS stored in SPIFFS
6. Browser gets success/error response

**Receiving SMS:**
1. Incoming SMS from GSM network
2. SIM800L sends unsolicited result: `+CMT:`
3. ESP32 parses sender & body
4. Message stored in SPIFFS
5. Auto-refresh shows new message (5s interval)

---

## 🔧 Troubleshooting

### "Cannot connect to WiFi"
- WiFi SSID: `SIM800L-Gateway`
- Password: `12345678`
- Try: Unplug ESP32, wait 5s, plug back in

### "Send SMS Failed"
- Check SIM card is inserted
- Verify phone number format (starts with `+`)
- Check SIM has GSM signal (if available)
- Check SIM800L is initialized (wait 5s after boot)

### "Dashboard won't load"
- Try: `http://192.168.4.1/` (IP address)
- Alternative: `http://smsgateway.local` (if mDNS works)
- Clear browser cache (Ctrl+Shift+Delete)

### "Messages not saving"
- Check SPIFFS not full (limited to 100 messages)
- Power cycle device
- Check Serial Monitor for errors

---

## 🎯 Use Cases

### 1. **Offline SMS Gateway**
- No internet needed
- Send/receive SMS in areas without connectivity
- Perfect for remote locations

### 2. **Mobile App Integration**
Use the REST API from your mobile app:
```javascript
// JavaScript example
const response = await fetch('http://192.168.4.1/api/sms/send', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    phone: '+1234567890',
    text: 'Hello from my app!'
  })
});
```

### 3. **Business SMS System**
- Bulk SMS campaigns
- Customer notifications
- No backend server needed

### 4. **Emergency Communications**
- Works without internet
- Direct WiFi connection
- Immediate SMS delivery

---

## 📊 Browser Compatibility

- ✅ Chrome/Edge
- ✅ Firefox
- ✅ Safari
- ✅ Opera
- ✅ Mobile browsers (iOS Safari, Chrome)

---

## 🔒 Security Notes

- **WiFi Password:** Change default password in firmware (line 31)
- **No Encryption:** Local WiFi only, not internet-facing
- **No Authentication:** Anyone on WiFi can send SMS (by design for simplicity)
- **Production:** Add API key authentication if needed

---

## 📈 Monitoring

### Check Device Status
```bash
curl http://192.168.4.1/api/status | jq
```

### Monitor SMS Activity
```bash
# Watch for new messages
watch -n 5 'curl -s http://192.168.4.1/api/sms/list | jq .messages[-1]'
```

### View Logs (Serial Monitor)
```bash
# Connect via USB at 115200 baud
platformio device monitor --baud 115200
```

---

## 🎓 Advanced: Custom Integration

### Send SMS from Python
```python
import requests

url = "http://192.168.4.1/api/sms/send"
data = {
    "phone": "+1234567890",
    "text": "Hello from Python!"
}

response = requests.post(url, json=data)
print(response.json())
```

### Automate with Node.js
```javascript
const axios = require('axios');

async function sendSMS(phone, text) {
  try {
    const response = await axios.post('http://192.168.4.1/api/sms/send', {
      phone,
      text
    });
    console.log('SMS sent:', response.data);
  } catch (error) {
    console.error('Error:', error.message);
  }
}

sendSMS('+1234567890', 'Hello from Node.js!');
```

### Bash Automation
```bash
#!/bin/bash

ESP_IP="192.168.4.1"
PHONE="+1234567890"
MESSAGE="Automated SMS from bash"

curl -X POST http://$ESP_IP/api/sms/send \
  -H "Content-Type: application/json" \
  -d "{\"phone\":\"$PHONE\",\"text\":\"$MESSAGE\"}"
```

---

## 📝 Message Limits

| Parameter | Limit | Notes |
|-----------|-------|-------|
| Phone length | 15 chars | E.164 format |
| Message length | 160 chars | Standard SMS |
| Total SMS stored | 100 | In-memory + persistent |
| Concurrent sends | 1 | One at a time |
| Send timeout | 10s | Per message |

---

## 🔄 Auto-Refresh Intervals

- Dashboard: 5 seconds
- Status: Manual refresh
- Web server: Always running

---

## 📞 Example: Send Daily SMS

Save as `daily_sms.sh`:
```bash
#!/bin/bash

# Send SMS every day at 9 AM
0 9 * * * /home/user/daily_sms.sh

# Script content:
curl -X POST http://192.168.4.1/api/sms/send \
  -H "Content-Type: application/json" \
  -d '{"phone":"+1234567890","text":"Good morning!"}'
```

---

## ✅ Checklist Before Use

- [ ] Firmware uploaded to ESP32
- [ ] SIM800L module initialized (wait 5s)
- [ ] SIM card inserted (GSM coverage)
- [ ] WiFi connected to SIM800L-Gateway
- [ ] Browser loads dashboard at http://192.168.4.1/
- [ ] Phone numbers in E.164 format
- [ ] API endpoints reachable from your device

---

## 🎉 You're Ready!

Now you can send and receive SMS via WiFi without needing internet or a backend server. Everything runs locally on the ESP32!

**Happy SMS sending!** 📱✅
