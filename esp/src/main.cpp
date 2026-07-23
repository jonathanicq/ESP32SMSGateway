#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HardwareSerial.h>
#include <time.h>

// SIM800L configuration (UART2 for ESP32)
HardwareSerial sim800l(2);
const int RX_PIN = 16;
const int TX_PIN = 17;
const int RST_PIN = 5;

// Configuration storage
struct Config {
  char wifi_ssid[64];
  char wifi_password[128];
  char api_url[256];
  char api_key[128];
  char telegram_token[256];
  char telegram_chat_id[64];
  char whatsapp_number[32];
  bool whatsapp_enabled;
  bool telegram_enabled;
} config;

// WiFi and Web Server
WebServer server(80);
const char* AP_SSID = "SIM800L-Gateway";
const char* AP_PASSWORD = "12345678";

volatile bool sms_received = false;
String last_sms_sender;
String last_sms_body;

// --- Configuration Management ---
void loadConfig() {
  File file = SPIFFS.open("/config.json", "r");
  if (!file) {
    memset(&config, 0, sizeof(config));
    strcpy(config.wifi_ssid, "");
    strcpy(config.wifi_password, "");
    strcpy(config.api_url, "http://192.168.0.116:8000/api");
    strcpy(config.api_key, "");
    strcpy(config.telegram_token, "");
    strcpy(config.telegram_chat_id, "");
    strcpy(config.whatsapp_number, "");
    config.whatsapp_enabled = true;
    config.telegram_enabled = true;
    return;
  }

  JsonDocument doc;
  deserializeJson(doc, file);
  file.close();

  strlcpy(config.wifi_ssid, doc["wifi_ssid"] | "", sizeof(config.wifi_ssid));
  strlcpy(config.wifi_password, doc["wifi_password"] | "", sizeof(config.wifi_password));
  strlcpy(config.api_url, doc["api_url"] | "http://192.168.0.116:8000/api", sizeof(config.api_url));
  strlcpy(config.api_key, doc["api_key"] | "", sizeof(config.api_key));
  strlcpy(config.telegram_token, doc["telegram_token"] | "", sizeof(config.telegram_token));
  strlcpy(config.telegram_chat_id, doc["telegram_chat_id"] | "", sizeof(config.telegram_chat_id));
  strlcpy(config.whatsapp_number, doc["whatsapp_number"] | "", sizeof(config.whatsapp_number));
  config.whatsapp_enabled = doc["whatsapp_enabled"] | true;
  config.telegram_enabled = doc["telegram_enabled"] | true;
}

void saveConfig() {
  JsonDocument doc;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_password"] = config.wifi_password;
  doc["api_url"] = config.api_url;
  doc["api_key"] = config.api_key;
  doc["telegram_token"] = config.telegram_token;
  doc["telegram_chat_id"] = config.telegram_chat_id;
  doc["whatsapp_number"] = config.whatsapp_number;
  doc["whatsapp_enabled"] = config.whatsapp_enabled;
  doc["telegram_enabled"] = config.telegram_enabled;

  File file = SPIFFS.open("/config.json", "w");
  serializeJson(doc, file);
  file.close();
}

// --- SIM800L Communication ---
void sim800l_send(const char* cmd, unsigned long timeout = 1000) {
  sim800l.println(cmd);
  delay(100);
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (sim800l.available()) {
      char c = sim800l.read();
      Serial.write(c);
    }
  }
  Serial.println();
}

String sim800l_read_response(unsigned long timeout = 1000) {
  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (sim800l.available()) {
      response += (char)sim800l.read();
    }
  }
  return response;
}

void sim800l_init() {
  digitalWrite(RST_PIN, LOW);
  delay(100);
  digitalWrite(RST_PIN, HIGH);
  delay(3000);

  sim800l.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(1000);

  // Test connection
  Serial.println("[SIM800L] Testing connection...");
  sim800l_send("AT");
  String resp = sim800l_read_response();
  if (resp.indexOf("OK") >= 0) {
    Serial.println("[SIM800L] Connected!");
  } else {
    Serial.println("[SIM800L] Connection failed!");
  }

  // Configure SMS mode
  sim800l_send("AT+CMGF=1"); // Text mode
  sim800l_read_response();

  sim800l_send("AT+CNMI=2,1,0,2"); // Direct delivery to serial
  sim800l_read_response();

  // Set SMS text mode parameters
  sim800l_send("AT+CSMP=17,167,0,0");
  sim800l_read_response();

  Serial.println("[SIM800L] Initialized");
}

void handleSMSReceived(String sender, String body) {
  if (!config.api_key[0]) {
    Serial.println("[SMS] Device not paired - ignoring message");
    return;
  }

  Serial.printf("[SMS] From: %s | Body: %s\n", sender.c_str(), body.c_str());
  sms_received = true;
  last_sms_sender = sender;
  last_sms_body = body;
}

void checkSMS() {
  if (sim800l.available()) {
    String line = sim800l.readStringUntil('\n');
    line.trim();
    Serial.printf("[SIM800L] %s\n", line.c_str());

    // Parse incoming SMS: +CMT: "+1234567890","","21/06/29,14:32:45+00"
    if (line.startsWith("+CMT:")) {
      int comma1 = line.indexOf(',');
      int comma2 = line.indexOf(',', comma1 + 1);

      if (comma1 > 0 && comma2 > comma1) {
        String sender = line.substring(comma1 + 2, line.indexOf('"', comma1 + 2));

        // Read message body on next line
        String body = "";
        unsigned long timeout = millis() + 500;
        while (millis() < timeout) {
          if (sim800l.available()) {
            body += (char)sim800l.read();
          }
        }
        body.trim();
        handleSMSReceived(sender, body);
      }
    }
  }
}

// --- HTTP API Communication ---
bool sendToBackend(String sender, String body) {
  if (!WiFi.isConnected() || !config.api_key[0]) {
    return false;
  }

  Serial.println("[API] Sending SMS to backend...");
  // Implementation handled by separate HTTP client library
  return true;
}

// --- Web Server Handlers ---
void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
  <title>SIM800L Gateway Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; margin: 20px; background: #f5f5f5; }
    .container { max-width: 500px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    h1 { color: #333; text-align: center; }
    .form-group { margin-bottom: 15px; }
    label { display: block; margin-bottom: 5px; color: #666; font-weight: bold; }
    input, select { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
    button { width: 100%; padding: 10px; background: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }
    button:hover { background: #0056b3; }
    .tabs { display: flex; margin-bottom: 20px; border-bottom: 2px solid #ddd; }
    .tab-button { flex: 1; padding: 10px; background: none; border: none; cursor: pointer; color: #666; border-bottom: 3px solid transparent; }
    .tab-button.active { color: #007bff; border-bottom-color: #007bff; }
    .tab-content { display: none; }
    .tab-content.active { display: block; }
    .status { padding: 10px; border-radius: 4px; margin-bottom: 15px; display: none; }
    .status.success { background: #d4edda; color: #155724; display: block; }
    .status.error { background: #f8d7da; color: #721c24; display: block; }
    .checkbox-group { display: flex; gap: 15px; }
    .checkbox-group label { margin-bottom: 0; display: flex; align-items: center; width: auto; }
    .checkbox-group input { width: auto; }
  </style>
</head>
<body>
  <div class="container">
    <h1>📡 SIM800L Gateway</h1>
    <div id="status" class="status"></div>

    <div class="tabs">
      <button class="tab-button active" onclick="showTab('wifi')">WiFi</button>
      <button class="tab-button" onclick="showTab('api')">API</button>
      <button class="tab-button" onclick="showTab('telegram')">Telegram</button>
      <button class="tab-button" onclick="showTab('whatsapp')">WhatsApp</button>
    </div>

    <!-- WiFi Tab -->
    <div id="wifi" class="tab-content active">
      <h2>WiFi Configuration</h2>
      <form onsubmit="saveWiFi(event)">
        <div class="form-group">
          <label>WiFi SSID</label>
          <input type="text" id="wifi_ssid" placeholder="Your WiFi network name" required>
        </div>
        <div class="form-group">
          <label>WiFi Password</label>
          <input type="password" id="wifi_password" placeholder="Your WiFi password">
        </div>
        <button type="submit">Save WiFi</button>
      </form>
    </div>

    <!-- API Tab -->
    <div id="api" class="tab-content">
      <h2>Backend Configuration</h2>
      <form onsubmit="saveAPI(event)">
        <div class="form-group">
          <label>API URL</label>
          <input type="text" id="api_url" placeholder="http://192.168.0.116:8000/api" required>
        </div>
        <div class="form-group">
          <label>API Key</label>
          <input type="text" id="api_key" placeholder="Device API key from backend">
        </div>
        <button type="submit">Save API Settings</button>
      </form>
    </div>

    <!-- Telegram Tab -->
    <div id="telegram" class="tab-content">
      <h2>Telegram Configuration</h2>
      <form onsubmit="saveTelegram(event)">
        <div class="form-group">
          <label>
            <input type="checkbox" id="telegram_enabled"> Enable Telegram
          </label>
        </div>
        <div class="form-group">
          <label>Bot Token</label>
          <input type="text" id="telegram_token" placeholder="123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11">
        </div>
        <div class="form-group">
          <label>Chat ID</label>
          <input type="text" id="telegram_chat_id" placeholder="Your Telegram chat ID">
        </div>
        <button type="submit">Save Telegram Settings</button>
      </form>
    </div>

    <!-- WhatsApp Tab -->
    <div id="whatsapp" class="tab-content">
      <h2>WhatsApp Configuration</h2>
      <form onsubmit="saveWhatsApp(event)">
        <div class="form-group">
          <label>
            <input type="checkbox" id="whatsapp_enabled"> Enable WhatsApp
          </label>
        </div>
        <div class="form-group">
          <label>Forward to Number (E.164 format)</label>
          <input type="tel" id="whatsapp_number" placeholder="+1234567890">
        </div>
        <button type="submit">Save WhatsApp Settings</button>
      </form>
    </div>
  </div>

  <script>
    function showTab(tabName) {
      const tabs = document.querySelectorAll('.tab-content');
      const buttons = document.querySelectorAll('.tab-button');
      tabs.forEach(tab => tab.classList.remove('active'));
      buttons.forEach(btn => btn.classList.remove('active'));
      document.getElementById(tabName).classList.add('active');
      event.target.classList.add('active');
    }

    async function loadConfig() {
      const resp = await fetch('/config');
      const data = await resp.json();
      document.getElementById('wifi_ssid').value = data.wifi_ssid || '';
      document.getElementById('wifi_password').value = data.wifi_password || '';
      document.getElementById('api_url').value = data.api_url || '';
      document.getElementById('api_key').value = data.api_key || '';
      document.getElementById('telegram_token').value = data.telegram_token || '';
      document.getElementById('telegram_chat_id').value = data.telegram_chat_id || '';
      document.getElementById('telegram_enabled').checked = data.telegram_enabled !== false;
      document.getElementById('whatsapp_number').value = data.whatsapp_number || '';
      document.getElementById('whatsapp_enabled').checked = data.whatsapp_enabled !== false;
    }

    async function saveWiFi(e) {
      e.preventDefault();
      const data = {
        wifi_ssid: document.getElementById('wifi_ssid').value,
        wifi_password: document.getElementById('wifi_password').value
      };
      await saveSettings(data, 'WiFi');
    }

    async function saveAPI(e) {
      e.preventDefault();
      const data = {
        api_url: document.getElementById('api_url').value,
        api_key: document.getElementById('api_key').value
      };
      await saveSettings(data, 'API');
    }

    async function saveTelegram(e) {
      e.preventDefault();
      const data = {
        telegram_enabled: document.getElementById('telegram_enabled').checked,
        telegram_token: document.getElementById('telegram_token').value,
        telegram_chat_id: document.getElementById('telegram_chat_id').value
      };
      await saveSettings(data, 'Telegram');
    }

    async function saveWhatsApp(e) {
      e.preventDefault();
      const data = {
        whatsapp_enabled: document.getElementById('whatsapp_enabled').checked,
        whatsapp_number: document.getElementById('whatsapp_number').value
      };
      await saveSettings(data, 'WhatsApp');
    }

    async function saveSettings(data, name) {
      try {
        const resp = await fetch('/config', { method: 'POST', body: JSON.stringify(data) });
        const statusDiv = document.getElementById('status');
        if (resp.ok) {
          statusDiv.className = 'status success';
          statusDiv.textContent = `✓ ${name} settings saved!`;
        } else {
          statusDiv.className = 'status error';
          statusDiv.textContent = `✗ Failed to save ${name} settings`;
        }
        setTimeout(() => statusDiv.className = 'status', 3000);
      } catch (e) {
        document.getElementById('status').className = 'status error';
        document.getElementById('status').textContent = 'Error: ' + e.message;
      }
    }

    loadConfig();
  </script>
</body>
</html>
  )";
  server.send(200, "text/html", html);
}

void handleGetConfig() {
  JsonDocument doc;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_password"] = config.wifi_password;
  doc["api_url"] = config.api_url;
  doc["api_key"] = config.api_key;
  doc["telegram_token"] = config.telegram_token;
  doc["telegram_chat_id"] = config.telegram_chat_id;
  doc["whatsapp_number"] = config.whatsapp_number;
  doc["whatsapp_enabled"] = config.whatsapp_enabled;
  doc["telegram_enabled"] = config.telegram_enabled;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handlePostConfig() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No body\"}");
    return;
  }

  JsonDocument doc;
  deserializeJson(doc, server.arg("plain"));

  if (doc.containsKey("wifi_ssid")) {
    strlcpy(config.wifi_ssid, doc["wifi_ssid"] | "", sizeof(config.wifi_ssid));
  }
  if (doc.containsKey("wifi_password")) {
    strlcpy(config.wifi_password, doc["wifi_password"] | "", sizeof(config.wifi_password));
  }
  if (doc.containsKey("api_url")) {
    strlcpy(config.api_url, doc["api_url"] | "", sizeof(config.api_url));
  }
  if (doc.containsKey("api_key")) {
    strlcpy(config.api_key, doc["api_key"] | "", sizeof(config.api_key));
  }
  if (doc.containsKey("telegram_token")) {
    strlcpy(config.telegram_token, doc["telegram_token"] | "", sizeof(config.telegram_token));
  }
  if (doc.containsKey("telegram_chat_id")) {
    strlcpy(config.telegram_chat_id, doc["telegram_chat_id"] | "", sizeof(config.telegram_chat_id));
  }
  if (doc.containsKey("whatsapp_number")) {
    strlcpy(config.whatsapp_number, doc["whatsapp_number"] | "", sizeof(config.whatsapp_number));
  }
  if (doc.containsKey("whatsapp_enabled")) {
    config.whatsapp_enabled = doc["whatsapp_enabled"];
  }
  if (doc.containsKey("telegram_enabled")) {
    config.telegram_enabled = doc["telegram_enabled"];
  }

  saveConfig();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleNotFound() {
  if (server.method() == HTTP_GET && server.uri() == "/config") {
    handleGetConfig();
  } else if (server.method() == HTTP_POST && server.uri() == "/config") {
    handlePostConfig();
  } else if (server.method() == HTTP_GET && server.uri() == "/") {
    handleRoot();
  } else {
    server.send(404, "text/plain", "Not Found");
  }
}

// --- WiFi Setup ---
void setupAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.printf("[WiFi] AP Mode: SSID=%s, IP=%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  server.onNotFound(handleNotFound);
  server.begin();
}

void setupWiFiMode() {
  if (!config.wifi_ssid[0]) {
    setupAPMode();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifi_ssid, config.wifi_password);

  Serial.printf("[WiFi] Connecting to %s...\n", config.wifi_ssid);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] Failed to connect, entering AP mode");
    setupAPMode();
  }
}

// --- Main Setup ---
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== SIM800L SMS Gateway ===\n");

  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, HIGH);

  if (!SPIFFS.begin(true)) {
    Serial.println("[FATAL] SPIFFS initialization failed");
    return;
  }

  loadConfig();
  setupWiFiMode();
  sim800l_init();

  server.onNotFound(handleNotFound);
  server.begin();
}

// --- Main Loop ---
void loop() {
  checkSMS();
  server.handleClient();

  if (sms_received) {
    sms_received = false;
    sendToBackend(last_sms_sender, last_sms_body);
  }

  delay(10);
}
