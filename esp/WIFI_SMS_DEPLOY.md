# WiFi SMS Gateway - Deployment Guide

Fast deployment of the WiFi SMS Gateway feature.

## 📦 What's New

The new `WiFi_SMS_API.ino` replaces the old firmware with:
- ✅ **Web Dashboard** to send/view SMS
- ✅ **REST API** for SMS operations
- ✅ **Local Storage** of SMS history
- ✅ **No Internet** required
- ✅ **Offline** operation
- ✅ **Real-time** dashboard updates

---

## 🚀 Upload to ESP32 (5 minutes)

### Step 1: Open Arduino IDE
```
Arduino IDE → File → Open
Navigate to: esp/SIM800L_Gateway/WiFi_SMS_API.ino
```

### Step 2: Configure Upload Settings
```
Tools → Board: ESP32 Dev Module
Tools → Port: COM4 (or your port)
Tools → Upload Speed: 921600
```

### Step 3: Upload
```
Click Upload button (▶️)
Wait for: "Hard resetting via RTS pin..."
```

### Step 4: Verify
```
Tools → Serial Monitor (115200 baud)

Expected output:
[SIM800L] WiFi SMS Gateway ===
[WiFi] AP Mode: SIM800L-Gateway
[WiFi] IP: 192.168.4.1
[HTTP] Server started on port 80
```

---

## 💻 Access Dashboard

### In Browser (any device)
```
http://192.168.4.1/
```

Or via mDNS:
```
http://smsgateway.local/
```

### Features Available
- ✉️ **Send SMS** tab - compose and send
- 📥 **Inbox** tab - view all messages
- ℹ️ **Status** tab - device info

---

## 🔌 Quick API Test

### Send SMS (via Terminal/Command Prompt)
```bash
curl -X POST http://192.168.4.1/api/sms/send \
  -H "Content-Type: application/json" \
  -d '{"phone":"+1234567890","text":"Hello WiFi SMS!"}'
```

### Get SMS List
```bash
curl http://192.168.4.1/api/sms/list
```

### Get Status
```bash
curl http://192.168.4.1/api/status
```

---

## 📋 Device Setup Checklist

- [ ] Upload `WiFi_SMS_API.ino` to ESP32
- [ ] Serial Monitor shows "Server started"
- [ ] WiFi AP broadcasting (SIM800L-Gateway)
- [ ] Can connect to WiFi
- [ ] Dashboard loads at http://192.168.4.1/
- [ ] SIM800L initialized (wait 5 seconds)
- [ ] Send test SMS via dashboard
- [ ] Check inbox for received SMS

---

## 📱 Phone Number Format

Always use E.164 international format:
- ✅ Correct: `+1234567890`
- ✅ Correct: `+5511987654321`
- ❌ Wrong: `1234567890` (missing +)
- ❌ Wrong: `+1-234-567-890` (has dashes)

---

## 🎯 Basic Usage

### Send SMS via Dashboard
1. Open: http://192.168.4.1/
2. Go to: "✉️ Send SMS" tab
3. Enter phone: `+1234567890`
4. Type message
5. Click: "Send SMS"
6. Status shows: ✓ SMS sent successfully!

### View Messages
1. Go to: "📥 Inbox" tab
2. See all sent/received SMS
3. Messages update every 5 seconds

### Check Device Info
1. Go to: "ℹ️ Status" tab
2. View SIM800L status
3. View total SMS count
4. View device uptime

---

## 🔧 Troubleshooting

### WiFi not found
- Check ESP32 is powered
- Check antenna is connected
- Look for "SIM800L-Gateway" network
- Try: Unplug 5 seconds, plug back in

### Dashboard won't load
- Try: `http://192.168.4.1/` (IP)
- Or: `http://smsgateway.local` (mDNS)
- Check you're connected to SIM800L-Gateway WiFi
- Clear browser cache (Ctrl+Shift+Del)

### SMS won't send
- Check SIM card is inserted
- Check phone number format (starts with +)
- Check SIM800L initialized (wait 5s)
- Check message not empty
- Try again, SIM800L may need time

### No serial output
- Check port is correct (Tools → Port)
- Check baud rate is 115200
- Check USB cable connection
- Unplug/replug ESP32

---

## 📊 What's Different From Old Firmware

| Feature | Old Firmware | WiFi SMS Gateway |
|---------|-------------|-----------------|
| Send SMS | Backend API | Web Dashboard + API |
| View SMS | Backoffice | Local Dashboard |
| Internet | Required | Not required |
| Storage | Backend DB | Local SPIFFS |
| Offline | ❌ No | ✅ Yes |
| Web UI | Complex setup | Simple local |

---

## 🚀 Next Steps

1. **Upload firmware** (5 min)
2. **Test dashboard** (2 min)
3. **Send test SMS** (1 min)
4. **Explore API** (optional)
5. **Integrate with apps** (optional)

---

## 📖 Documentation

- Full guide: `WIFI_SMS_GATEWAY.md`
- Original docs: `esp/README.md`
- Integration: `INTEGRATION.md`

---

## ⏱️ Expected Timeline

| Task | Time |
|------|------|
| Install Arduino IDE | 10 min |
| Install ESP32 board | 10 min |
| Upload firmware | 2 min |
| First SMS | 5 min |
| **Total** | **~25 min** |

---

## ✅ Success Indicators

✓ Serial Monitor shows: `Server started on port 80`
✓ Can connect to WiFi: `SIM800L-Gateway`
✓ Dashboard loads at: `http://192.168.4.1/`
✓ Can send SMS via dashboard
✓ SMS appears in Inbox
✓ API returns JSON responses

---

## 🎓 Examples

### Python Script
```python
import requests

# Send SMS
requests.post('http://192.168.4.1/api/sms/send', json={
    'phone': '+1234567890',
    'text': 'Hello from Python!'
})

# Get messages
response = requests.get('http://192.168.4.1/api/sms/list')
for msg in response.json()['messages']:
    print(f"{msg['phone']}: {msg['text']}")
```

### JavaScript Fetch
```javascript
// Send SMS
fetch('http://192.168.4.1/api/sms/send', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    phone: '+1234567890',
    text: 'Hello from JavaScript!'
  })
}).then(r => r.json()).then(console.log);
```

### Bash cURL
```bash
# Send SMS
curl -X POST http://192.168.4.1/api/sms/send \
  -H "Content-Type: application/json" \
  -d '{"phone":"+1234567890","text":"Hello from Bash!"}'
```

---

## 📞 Support

If issues occur:
1. Check Serial Monitor for errors
2. Verify hardware connections
3. Read `WIFI_SMS_GATEWAY.md` for detailed info
4. Check troubleshooting section above

---

**Ready to deploy? Let's go!** 🚀📱
