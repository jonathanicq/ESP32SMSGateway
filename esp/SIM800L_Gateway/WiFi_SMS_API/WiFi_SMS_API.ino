#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HardwareSerial.h>
#include <ArduinoOTA.h>
#include <time.h>

// SIM800L configuration (UART2 for ESP32)
HardwareSerial sim800l(2);
const int RX_PIN = 16;
const int TX_PIN = 17;
const int RST_PIN = 5;

// Log structure
struct LogEntry {
  char timestamp[32];
  char type[16];
  char message[256];
};

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
  bool roaming_enabled;
  char gsm_network[64];
  char apn[128];
  char apn_user[64];
  char apn_password[64];
  char sim_pin[8];
} config;

// Log system
#define MAX_LOGS 100
LogEntry logs[MAX_LOGS];
int log_count = 0;

// WiFi and Web Server
WebServer server(80);
const char* AP_SSID = "SIM800L-Gateway";
const char* AP_PASSWORD = "12345678";

volatile bool sms_received = false;
String last_sms_sender;
String last_sms_body;

// Signal strength
int signal_strength = 0;
unsigned long last_signal_check = 0;

// WiFi Scan
struct WiFiNetwork {
  String ssid;
  int rssi;
  String encryption;
};
WiFiNetwork available_networks[20];
int available_networks_count = 0;
unsigned long last_wifi_scan = 0;

// --- Configuration Management ---
void addLog(const char* type, const char* message) {
  if (log_count >= MAX_LOGS) {
    for (int i = 0; i < MAX_LOGS - 1; i++) {
      logs[i] = logs[i + 1];
    }
    log_count--;
  }
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  strftime(logs[log_count].timestamp, sizeof(logs[log_count].timestamp), "%d/%m %H:%M:%S", timeinfo);
  strlcpy(logs[log_count].type, type, sizeof(logs[log_count].type));
  strlcpy(logs[log_count].message, message, sizeof(logs[log_count].message));
  log_count++;
  Serial.printf("[LOG] %s | %s | %s\n", logs[log_count-1].timestamp, type, message);
}

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
    strcpy(config.gsm_network, "AUTO");
    strcpy(config.apn, "");
    strcpy(config.apn_user, "");
    strcpy(config.apn_password, "");
    config.whatsapp_enabled = true;
    config.telegram_enabled = true;
    config.roaming_enabled = false;
    addLog("BOOT", "Sistema iniciado");
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
  strlcpy(config.gsm_network, doc["gsm_network"] | "AUTO", sizeof(config.gsm_network));
  strlcpy(config.apn, doc["apn"] | "", sizeof(config.apn));
  strlcpy(config.apn_user, doc["apn_user"] | "", sizeof(config.apn_user));
  strlcpy(config.apn_password, doc["apn_password"] | "", sizeof(config.apn_password));
  strlcpy(config.sim_pin, doc["sim_pin"] | "", sizeof(config.sim_pin));
  config.whatsapp_enabled = doc["whatsapp_enabled"] | true;
  config.telegram_enabled = doc["telegram_enabled"] | true;
  config.roaming_enabled = doc["roaming_enabled"] | false;
  addLog("CONFIG", "Configuracoes carregadas");
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
  doc["roaming_enabled"] = config.roaming_enabled;
  doc["gsm_network"] = config.gsm_network;
  doc["apn"] = config.apn;
  doc["apn_user"] = config.apn_user;
  doc["apn_password"] = config.apn_password;
  doc["sim_pin"] = config.sim_pin;

  File file = SPIFFS.open("/config.json", "w");
  serializeJson(doc, file);
  file.close();
  addLog("CONFIG", "Configuracoes salvas");
}

// --- SIM800L Communication ---
void sim800l_send(const char* cmd, unsigned long timeout = 2000) {
  while (sim800l.available()) sim800l.read();  // Clear buffer
  Serial.printf("[AT] Sending: %s\n", cmd);
  sim800l.println(cmd);
  delay(200);
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (sim800l.available()) {
      char c = sim800l.read();
      Serial.write(c);
    }
  }
  Serial.println();
}

String sim800l_read_response(unsigned long timeout = 2000) {
  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (sim800l.available()) {
      response += (char)sim800l.read();
    }
    delay(10);
  }
  Serial.printf("[Response] %s\n", response.c_str());
  return response;
}

void sim800l_init() {
  Serial.println("\n\n[SIM800L] Starting initialization...");
  Serial.printf("[SIM800L] RST_PIN=%d, RX_PIN=%d, TX_PIN=%d\n", RST_PIN, RX_PIN, TX_PIN);

  // Configure RST pin - Keep HIGH (don't reset)
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, HIGH);
  Serial.println("[SIM800L] RST pin set to HIGH");
  delay(3000);  // Wait for any pending initialization

  Serial.println("[SIM800L] Starting serial at 9600 baud...");
  sim800l.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(2000);

  Serial.println("[SIM800L] Clearing buffer...");
  delay(1000);
  while (sim800l.available()) {
    char c = sim800l.read();
    Serial.write(c);
  }
  Serial.println("[SIM800L] Buffer cleared");

  String resp;

  // Test connection - retry 3 times
  addLog("SIM800L", "Testando conexao...");
  bool connected = false;
  for (int i = 0; i < 3; i++) {
    Serial.printf("[SIM800L] Connection attempt %d/3\n", i + 1);
    sim800l_send("AT", 3000);
    resp = sim800l_read_response(3000);
    if (resp.indexOf("OK") >= 0) {
      addLog("SIM800L", "Conectado!");
      connected = true;
      break;
    }
    delay(1000);
  }
  if (!connected) {
    addLog("SIM800L", "Conexao falhou!");
    return;
  }

  // Check and unlock SIM if PIN configured
  if (config.sim_pin[0]) {
    addLog("SIM800L", "Verificando PIN da SIM...");
    sim800l_send("AT+CPIN?");
    resp = sim800l_read_response();
    if (resp.indexOf("SIM PIN") >= 0) {
      Serial.printf("[SIM800L] SIM locked, entering PIN: %s\n", config.sim_pin);
      char cmd[32];
      snprintf(cmd, sizeof(cmd), "AT+CPIN=\"%s\"", config.sim_pin);
      sim800l_send(cmd);
      resp = sim800l_read_response();
      if (resp.indexOf("OK") >= 0) {
        addLog("SIM800L", "PIN desbloqueado com sucesso!");
        delay(2000);  // Wait for SIM to be ready
      } else {
        addLog("SIM800L", "Erro ao desbloquear PIN!");
      }
    } else if (resp.indexOf("READY") >= 0) {
      addLog("SIM800L", "SIM ja desbloqueada");
    }
  }

  // Configure SMS mode
  addLog("SIM800L", "Configurando SMS...");
  sim800l_send("AT+CMGF=1");
  sim800l_read_response();

  sim800l_send("AT+CNMI=2,1,0,2");
  sim800l_read_response();

  sim800l_send("AT+CSMP=17,167,0,0");
  sim800l_read_response();

  // Configure roaming if enabled
  if (config.roaming_enabled) {
    addLog("SIM800L", "Ativando roaming internacional...");
    sim800l_send("AT+CROAMING=1");
    sim800l_read_response();
  }

  // Set network if configured
  if (config.gsm_network[0] && strcmp(config.gsm_network, "AUTO") != 0) {
    addLog("SIM800L", "Configurando rede GSM...");
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+COPS=1,2,%s", config.gsm_network);
    sim800l_send(cmd);
    sim800l_read_response();
  }

  // Set APN if configured
  if (config.apn[0]) {
    addLog("SIM800L", "Configurando APN...");
    char cmd[256];
    if (config.apn_user[0] && config.apn_password[0]) {
      snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,1,Contype,GPRS");
      sim800l_send(cmd);
      sim800l_read_response();

      snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,1,APN,%s", config.apn);
      sim800l_send(cmd);
      sim800l_read_response();

      snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,1,USER,%s", config.apn_user);
      sim800l_send(cmd);
      sim800l_read_response();

      snprintf(cmd, sizeof(cmd), "AT+SAPBR=3,1,PWD,%s", config.apn_password);
      sim800l_send(cmd);
      sim800l_read_response();
    }
  }

  addLog("SIM800L", "Inicializacao concluida");
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

void scanWiFiNetworks() {
  if (millis() - last_wifi_scan < 30000) return;

  addLog("WiFi", "Procurando redes...");
  int num_networks = WiFi.scanNetworks();
  available_networks_count = 0;

  for (int i = 0; i < num_networks && available_networks_count < 20; i++) {
    available_networks[available_networks_count].ssid = WiFi.SSID(i);
    available_networks[available_networks_count].rssi = WiFi.RSSI(i);

    int encryption = WiFi.encryptionType(i);
    if (encryption == WIFI_AUTH_OPEN) {
      available_networks[available_networks_count].encryption = "Open";
    } else if (encryption == WIFI_AUTH_WEP) {
      available_networks[available_networks_count].encryption = "WEP";
    } else if (encryption == WIFI_AUTH_WPA_PSK) {
      available_networks[available_networks_count].encryption = "WPA";
    } else if (encryption == WIFI_AUTH_WPA2_PSK) {
      available_networks[available_networks_count].encryption = "WPA2";
    } else if (encryption == WIFI_AUTH_WPA_WPA2_PSK) {
      available_networks[available_networks_count].encryption = "WPA/WPA2";
    } else {
      available_networks[available_networks_count].encryption = "Other";
    }
    available_networks_count++;
  }

  last_wifi_scan = millis();
  Serial.printf("[WiFi] Found %d networks\n", available_networks_count);
  addLog("WiFi", (String("Encontradas ") + available_networks_count + " redes").c_str());
  WiFi.scanDelete();
}

int getSignalStrength() {
  sim800l_send("AT+CSQ");
  String resp = sim800l_read_response();

  int rssi = 0;
  int index = resp.indexOf("+CSQ: ");
  if (index >= 0) {
    int comma = resp.indexOf(',', index);
    String rssi_str = resp.substring(index + 6, comma);
    rssi = rssi_str.toInt();
    if (rssi == 0) rssi = 0;
    else if (rssi == 1) rssi = 5;
    else if (rssi <= 9) rssi = (rssi - 2) * 10;
    else if (rssi <= 14) rssi = (rssi - 9) * 10 + 50;
    else if (rssi <= 19) rssi = (rssi - 14) * 10 + 80;
    else if (rssi <= 30) rssi = 90 + (rssi - 20);
    else rssi = 99;
  }
  return rssi;
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
  String html = "<!DOCTYPE html><html><head><title>SIM800L Gateway</title><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><style>body{font-family:Arial;margin:20px;background:#f5f5f5}.container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}h1{color:#333;text-align:center;margin:0 0 10px 0}.signal-bar{background:#007bff;height:30px;border-radius:4px;margin:15px 0;display:flex;align-items:center;justify-content:center;color:white;font-weight:bold;font-size:14px}.signal-label{color:#666;font-weight:bold;margin-bottom:5px}h2{color:#555;font-size:18px;margin:15px 0 10px 0}.form-group{margin-bottom:15px}label{display:block;margin-bottom:5px;color:#666;font-weight:bold}input,select,textarea{width:100%;padding:8px;border:1px solid #ddd;border-radius:4px;box-sizing:border-box;font-family:Arial}button{width:100%;padding:10px;background:#007bff;color:white;border:none;border-radius:4px;cursor:pointer;font-size:16px}button:hover{background:#0056b3}.tabs{display:flex;flex-wrap:wrap;margin-bottom:20px;border-bottom:2px solid #ddd;overflow-x:auto}.tab-button{flex:0 0 auto;padding:10px 15px;background:0;border:none;cursor:pointer;color:#666;border-bottom:3px solid transparent;white-space:nowrap;font-size:14px}.tab-button.active{color:#007bff;border-bottom-color:#007bff}.tab-content{display:none}.tab-content.active{display:block}.status{padding:10px;border-radius:4px;margin-bottom:15px;display:none}.status.success{background:#d4edda;color:#155724;display:block}.status.error{background:#f8d7da;color:#721c24;display:block}.checkbox-group{display:flex;gap:15px}.checkbox-group label{margin-bottom:0;display:flex;align-items:center;width:auto}.checkbox-group input{width:auto}.log-list{background:#f9f9f9;border:1px solid #ddd;border-radius:4px;max-height:300px;overflow-y:auto;font-size:12px;font-family:monospace}.log-item{padding:8px;border-bottom:1px solid #eee;display:flex;gap:10px}.log-time{color:#999;min-width:70px}.log-type{color:#007bff;font-weight:bold;min-width:70px}.log-msg{color:#333;flex:1}</style></head><body><div class=\"container\"><h1>SIM800L Gateway</h1><div class=\"signal-label\">Signal Strength</div><div class=\"signal-bar\" id=\"signal-bar\" style=\"width:0%\">-- %</div><div id=\"status\" class=\"status\"></div><div class=\"tabs\"><button class=\"tab-button active\" onclick=\"showTab('wifi')\">WiFi</button><button class=\"tab-button\" onclick=\"showTab('api')\">API</button><button class=\"tab-button\" onclick=\"showTab('gsm')\">GSM</button><button class=\"tab-button\" onclick=\"showTab('telegram')\">Telegram</button><button class=\"tab-button\" onclick=\"showTab('whatsapp')\">WhatsApp</button><button class=\"tab-button\" onclick=\"showTab('logs')\">Log</button></div><div id=\"wifi\" class=\"tab-content active\"><h2>WiFi</h2><form onsubmit=\"saveWiFi(event)\"><div class=\"form-group\"><label>Available Networks</label><select id=\"wifi_list\" onchange=\"selectNetwork()\"><option value=\"\">-- Procurando redes... --</option></select></div><div class=\"form-group\"><label>SSID</label><input type=\"text\" id=\"wifi_ssid\" placeholder=\"WiFi network name\" required></div><div class=\"form-group\"><label>Password</label><input type=\"password\" id=\"wifi_password\" placeholder=\"WiFi password\"></div><button type=\"submit\">Save</button></form></div><div id=\"api\" class=\"tab-content\"><h2>Backend API</h2><form onsubmit=\"saveAPI(event)\"><div class=\"form-group\"><label>API URL</label><input type=\"text\" id=\"api_url\" placeholder=\"http://192.168.0.116:8000/api\" required></div><div class=\"form-group\"><label>API Key</label><input type=\"text\" id=\"api_key\" placeholder=\"Device API key\"></div><button type=\"submit\">Save</button></form></div><div id=\"gsm\" class=\"tab-content\"><h2>GSM Config</h2><form onsubmit=\"saveGSM(event)\"><div class=\"form-group\"><label><input type=\"checkbox\" id=\"roaming_enabled\"> Enable Roaming Internacional</label></div><div class=\"form-group\"><label>Network Selection</label><select id=\"gsm_network\"><option value=\"AUTO\">AUTO (Manual)</option><optgroup label=\"Portugal\"><option value=\"22801\">Vodafone PT (22801)</option><option value=\"22806\">Optimus PT (22806)</option><option value=\"22804\">MEO/Altice PT (22804)</option><option value=\"22805\">MEO/Altice PT (22805)</option><option value=\"22807\">CTT Móvel PT (22807)</option><option value=\"22808\">Telemóvel PT (22808)</option><option value=\"22888\">FREE Mobile PT (22888)</option></optgroup><optgroup label=\"France\"><option value=\"20801\">Orange FR (20801)</option><option value=\"20810\">SFR/Vodafone FR (20810)</option><option value=\"20811\">SFR FR (20811)</option><option value=\"20815\">FREE Mobile FR (20815)</option><option value=\"20820\">Bouygues FR (20820)</option><option value=\"20821\">Bouygues FR (20821)</option></optgroup><optgroup label=\"Other\"><option value=\"31026\">Vodafone ES (31026)</option><option value=\"21407\">Vodafone BR (21407)</option><option value=\"21601\">Vivo BR (21601)</option><option value=\"21901\">Claro BR (21901)</option><option value=\"24001\">Vodafone DE (24001)</option><option value=\"44000\">USA Carriers (44000)</option></optgroup></select></div><div class=\"form-group\"><label>APN</label><input type=\"text\" id=\"apn\" placeholder=\"APN (ex: internet.vodafone.pt)\"></div><div class=\"form-group\"><label>APN User</label><input type=\"text\" id=\"apn_user\" placeholder=\"APN Username\"></div><div class=\"form-group\"><label>APN Password</label><input type=\"password\" id=\"apn_password\" placeholder=\"APN Password\"></div><div class=\"form-group\"><label>SIM PIN (deixe em branco se sem PIN)</label><input type=\"password\" id=\"sim_pin\" placeholder=\"1234\" maxlength=\"4\"></div><button type=\"submit\">Save</button></form></div><div id=\"telegram\" class=\"tab-content\"><h2>Telegram</h2><form onsubmit=\"saveTelegram(event)\"><div class=\"form-group\"><label><input type=\"checkbox\" id=\"telegram_enabled\"> Enable Telegram</label></div><div class=\"form-group\"><label>Bot Token</label><input type=\"text\" id=\"telegram_token\" placeholder=\"Bot token\"></div><div class=\"form-group\"><label>Chat ID</label><input type=\"text\" id=\"telegram_chat_id\" placeholder=\"Chat ID\"></div><button type=\"submit\">Save</button></form></div><div id=\"whatsapp\" class=\"tab-content\"><h2>WhatsApp</h2><form onsubmit=\"saveWhatsApp(event)\"><div class=\"form-group\"><label><input type=\"checkbox\" id=\"whatsapp_enabled\"> Enable WhatsApp</label></div><div class=\"form-group\"><label>Forward Number (E.164)</label><input type=\"tel\" id=\"whatsapp_number\" placeholder=\"+1234567890\"></div><button type=\"submit\">Save</button></form></div><div id=\"logs\" class=\"tab-content\"><h2>System Log</h2><div class=\"log-list\" id=\"log-list\">Loading...</div><button onclick=\"refreshLogs()\" style=\"margin-top:10px;width:auto;padding:8px 15px\">Refresh</button></div></div><script>setInterval(updateSignal,2000);setInterval(scanWiFi,15000);async function updateSignal(){const e=await fetch('/signal'),t=await e.json(),o=document.getElementById('signal-bar');o.style.width=t.signal+'%',o.textContent=(t.signal+'').padStart(2,'0')+'% - '+t.quality}async function scanWiFi(){try{const e=await fetch('/api/wifi/scan'),t=await e.json(),o=document.getElementById('wifi_list');if(t.networks&&t.networks.length>0){o.innerHTML=t.networks.map(e=>'<option value=\"'+e.ssid+'\">'+e.ssid+' ('+e.signal+'% - '+e.encryption+')</option>').join('')}else{o.innerHTML='<option value=\"\">-- Nenhuma rede encontrada --</option>'}}catch(e){console.error('Erro ao scanear WiFi:',e)}}function selectNetwork(){const e=document.getElementById('wifi_list').value;e&&(document.getElementById('wifi_ssid').value=e)}scanWiFi();function showTab(t){document.querySelectorAll('.tab-content').forEach(e=>e.classList.remove('active'));document.querySelectorAll('.tab-button').forEach(e=>e.classList.remove('active'));document.getElementById(t).classList.add('active');event.target.classList.add('active');t==='logs'&&refreshLogs()}async function loadConfig(){const e=await fetch('/config'),t=await e.json();document.getElementById('wifi_ssid').value=t.wifi_ssid||'';document.getElementById('wifi_password').value=t.wifi_password||'';document.getElementById('api_url').value=t.api_url||'';document.getElementById('api_key').value=t.api_key||'';document.getElementById('telegram_token').value=t.telegram_token||'';document.getElementById('telegram_chat_id').value=t.telegram_chat_id||'';document.getElementById('telegram_enabled').checked=t.telegram_enabled!==false;document.getElementById('whatsapp_number').value=t.whatsapp_number||'';document.getElementById('whatsapp_enabled').checked=t.whatsapp_enabled!==false;document.getElementById('roaming_enabled').checked=t.roaming_enabled||false;document.getElementById('gsm_network').value=t.gsm_network||'AUTO';document.getElementById('apn').value=t.apn||'';document.getElementById('apn_user').value=t.apn_user||'';document.getElementById('apn_password').value=t.apn_password||'';document.getElementById('sim_pin').value=t.sim_pin||''}async function refreshLogs(){const e=await fetch('/logs'),t=await e.json(),o=document.getElementById('log-list');o.innerHTML=t.logs.map(e=>'<div class=\"log-item\"><div class=\"log-time\">'+e.timestamp+'</div><div class=\"log-type\">'+e.type+'</div><div class=\"log-msg\">'+e.message+'</div></div>').join('')}async function saveWiFi(e){e.preventDefault();await saveSettings({wifi_ssid:document.getElementById('wifi_ssid').value,wifi_password:document.getElementById('wifi_password').value},'WiFi')}async function saveAPI(e){e.preventDefault();await saveSettings({api_url:document.getElementById('api_url').value,api_key:document.getElementById('api_key').value},'API')}async function saveGSM(e){e.preventDefault();await saveSettings({roaming_enabled:document.getElementById('roaming_enabled').checked,gsm_network:document.getElementById('gsm_network').value,apn:document.getElementById('apn').value,apn_user:document.getElementById('apn_user').value,apn_password:document.getElementById('apn_password').value,sim_pin:document.getElementById('sim_pin').value},'GSM')}async function saveTelegram(e){e.preventDefault();await saveSettings({telegram_enabled:document.getElementById('telegram_enabled').checked,telegram_token:document.getElementById('telegram_token').value,telegram_chat_id:document.getElementById('telegram_chat_id').value},'Telegram')}async function saveWhatsApp(e){e.preventDefault();await saveSettings({whatsapp_enabled:document.getElementById('whatsapp_enabled').checked,whatsapp_number:document.getElementById('whatsapp_number').value},'WhatsApp')}async function saveSettings(e,t){try{const o=await fetch('/config',{method:'POST',body:JSON.stringify(e)}),a=document.getElementById('status');o.ok?(a.className='status success',a.textContent='[OK] '+t+' saved!'):(a.className='status error',a.textContent='[ERROR] Failed to save '+t),setTimeout(()=>a.className='status',3000)}catch(e){document.getElementById('status').className='status error',document.getElementById('status').textContent='Error: '+e.message}}loadConfig();updateSignal()</script></body></html>";
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
  doc["roaming_enabled"] = config.roaming_enabled;
  doc["gsm_network"] = config.gsm_network;
  doc["apn"] = config.apn;
  doc["apn_user"] = config.apn_user;
  doc["apn_password"] = config.apn_password;
  doc["sim_pin"] = config.sim_pin;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleGetLogs() {
  JsonDocument doc;
  JsonArray logsArray = doc["logs"].to<JsonArray>();
  for (int i = 0; i < log_count; i++) {
    JsonObject logObj = logsArray.createNestedObject();
    logObj["timestamp"] = logs[i].timestamp;
    logObj["type"] = logs[i].type;
    logObj["message"] = logs[i].message;
  }
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleGetSignal() {
  JsonDocument doc;
  doc["signal"] = signal_strength;
  doc["bars"] = (signal_strength / 20) + 1;
  doc["quality"] = signal_strength > 70 ? "Excelente" : signal_strength > 50 ? "Bom" : signal_strength > 30 ? "Regular" : "Fraco";
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleGetWiFiScan() {
  scanWiFiNetworks();

  JsonDocument doc;
  JsonArray networks = doc["networks"].to<JsonArray>();

  for (int i = 0; i < available_networks_count; i++) {
    JsonObject net = networks.createNestedObject();
    net["ssid"] = available_networks[i].ssid;
    net["rssi"] = available_networks[i].rssi;
    net["signal"] = map(available_networks[i].rssi, -100, -30, 0, 100);
    net["encryption"] = available_networks[i].encryption;
  }

  doc["count"] = available_networks_count;

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
  if (doc.containsKey("roaming_enabled")) {
    config.roaming_enabled = doc["roaming_enabled"];
  }
  if (doc.containsKey("gsm_network")) {
    strlcpy(config.gsm_network, doc["gsm_network"] | "AUTO", sizeof(config.gsm_network));
  }
  if (doc.containsKey("apn")) {
    strlcpy(config.apn, doc["apn"] | "", sizeof(config.apn));
  }
  if (doc.containsKey("apn_user")) {
    strlcpy(config.apn_user, doc["apn_user"] | "", sizeof(config.apn_user));
  }
  if (doc.containsKey("apn_password")) {
    strlcpy(config.apn_password, doc["apn_password"] | "", sizeof(config.apn_password));
  }
  if (doc.containsKey("sim_pin")) {
    strlcpy(config.sim_pin, doc["sim_pin"] | "", sizeof(config.sim_pin));
  }

  saveConfig();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleNotFound() {
  if (server.method() == HTTP_GET && server.uri() == "/config") {
    handleGetConfig();
  } else if (server.method() == HTTP_POST && server.uri() == "/config") {
    handlePostConfig();
  } else if (server.method() == HTTP_GET && server.uri() == "/logs") {
    handleGetLogs();
  } else if (server.method() == HTTP_GET && server.uri() == "/signal") {
    handleGetSignal();
  } else if (server.method() == HTTP_GET && server.uri() == "/api/wifi/scan") {
    handleGetWiFiScan();
  } else if (server.method() == HTTP_GET && server.uri() == "/") {
    handleRoot();
  } else {
    server.send(404, "text/plain", "Not Found");
  }
}

// --- OTA Setup ---
void setupOTA() {
  ArduinoOTA.setHostname("SIM800L-Gateway");
  ArduinoOTA.setPassword("12345678");

  ArduinoOTA.onStart([]() {
    addLog("OTA", "Atualizacao iniciada");
  });

  ArduinoOTA.onEnd([]() {
    addLog("OTA", "Atualizacao concluida com sucesso");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    addLog("OTA", "Erro durante atualizacao");
  });

  ArduinoOTA.begin();
  addLog("OTA", "Servico OTA ativo");
}

// --- WiFi Setup ---
// --- Auto-configure APN from MCC/MNC ---
String getMCCMNC() {
  sim800l.println("AT+COPS?");
  delay(500);
  String response = "";
  while (sim800l.available()) {
    response += (char)sim800l.read();
  }
  // Parse +COPS: <mode>,<format>,<oper>,<AcT>
  // Example: +COPS: 0,2,"22801",2
  int startIdx = response.indexOf("\"");
  int endIdx = response.lastIndexOf("\"");
  if (startIdx != -1 && endIdx != -1 && startIdx < endIdx) {
    return response.substring(startIdx + 1, endIdx);
  }
  return "";
}

void autoConfigureAPN() {
  if (!config.apn[0]) {  // Only if APN not already configured
    Serial.println("[APN] Auto-configuring APN from network operator...");
    addLog("APN", "Iniciando auto-configuracao");

    String mccmnc = getMCCMNC();
    if (mccmnc.length() > 0) {
      Serial.printf("[APN] Detected MCC/MNC: %s\n", mccmnc.c_str());

      // Map MCC/MNC to APN (Portuguese, French and international operators)
      // --- PORTUGAL ---
      if (mccmnc == "22801" || mccmnc == "22810") {  // Vodafone PT
        strcpy(config.apn, "internet.vodafone.pt");
        strcpy(config.apn_user, "vodafone");
        strcpy(config.apn_password, "vodafone");
        addLog("APN", "Vodafone PT configurado");
      } else if (mccmnc == "22806") {  // Optimus PT
        strcpy(config.apn, "internet");
        strcpy(config.apn_user, "");
        strcpy(config.apn_password, "");
        addLog("APN", "Optimus PT configurado");
      } else if (mccmnc == "22804" || mccmnc == "22805") {  // MEO PT / Altice
        strcpy(config.apn, "meo");
        strcpy(config.apn_user, "meo");
        strcpy(config.apn_password, "meo");
        addLog("APN", "MEO/Altice PT configurado");
      } else if (mccmnc == "22807") {  // CTT Movel PT (defunct, fallback to MEO)
        strcpy(config.apn, "meo");
        strcpy(config.apn_user, "meo");
        strcpy(config.apn_password, "meo");
        addLog("APN", "CTT/MEO PT configurado");
      } else if (mccmnc == "22808") {  // Telemóvel PT (old, fallback)
        strcpy(config.apn, "internet");
        strcpy(config.apn_user, "");
        strcpy(config.apn_password, "");
        addLog("APN", "Telemóvel PT configurado");
      } else if (mccmnc == "22888") {  // FREE Mobile PT
        strcpy(config.apn, "free");
        strcpy(config.apn_user, "free");
        strcpy(config.apn_password, "free");
        addLog("APN", "FREE Mobile PT configurado");
      }
      // --- FRANCE ---
      else if (mccmnc == "20801") {  // Orange France
        strcpy(config.apn, "orange");
        strcpy(config.apn_user, "orange");
        strcpy(config.apn_password, "orange");
        addLog("APN", "Orange FR configurado");
      } else if (mccmnc == "20810") {  // Vodafone France (SFR)
        strcpy(config.apn, "sfr");
        strcpy(config.apn_user, "sfr");
        strcpy(config.apn_password, "sfr");
        addLog("APN", "SFR/Vodafone FR configurado");
      } else if (mccmnc == "20811") {  // SFR France
        strcpy(config.apn, "sfr");
        strcpy(config.apn_user, "sfr");
        strcpy(config.apn_password, "sfr");
        addLog("APN", "SFR FR configurado");
      } else if (mccmnc == "20815") {  // FREE Mobile France
        strcpy(config.apn, "free");
        strcpy(config.apn_user, "free");
        strcpy(config.apn_password, "free");
        addLog("APN", "FREE Mobile FR configurado");
      } else if (mccmnc == "20820") {  // Bouygues France
        strcpy(config.apn, "bouygues.fr");
        strcpy(config.apn_user, "bouygues");
        strcpy(config.apn_password, "bouygues");
        addLog("APN", "Bouygues FR configurado");
      } else if (mccmnc == "20821") {  // Bouygues Alternative
        strcpy(config.apn, "bouygues.fr");
        strcpy(config.apn_user, "bouygues");
        strcpy(config.apn_password, "bouygues");
        addLog("APN", "Bouygues FR configurado");
      }
      // --- OTHER COUNTRIES ---
      else if (mccmnc == "31026") {  // Vodafone ES
        strcpy(config.apn, "vodafone.es");
        strcpy(config.apn_user, "vodafone");
        strcpy(config.apn_password, "vodafone");
        addLog("APN", "Vodafone ES configurado");
      } else if (mccmnc == "21407") {  // Vodafone BR
        strcpy(config.apn, "zap.vivo.com.br");
        strcpy(config.apn_user, "vivo");
        strcpy(config.apn_password, "vivo");
        addLog("APN", "Vodafone BR configurado");
      } else if (mccmnc == "21601") {  // Vivo BR
        strcpy(config.apn, "zap.vivo.com.br");
        strcpy(config.apn_user, "vivo");
        strcpy(config.apn_password, "vivo");
        addLog("APN", "Vivo BR configurado");
      } else if (mccmnc == "21901") {  // Claro BR
        strcpy(config.apn, "claro.com.br");
        strcpy(config.apn_user, "claro");
        strcpy(config.apn_password, "claro");
        addLog("APN", "Claro BR configurado");
      } else if (mccmnc == "24001") {  // Vodafone DE
        strcpy(config.apn, "internet.vodafone.de");
        strcpy(config.apn_user, "vodafone");
        strcpy(config.apn_password, "vodafone");
        addLog("APN", "Vodafone DE configurado");
      } else if (mccmnc == "44000") {  // USA carriers (T-Mobile, AT&T, etc)
        strcpy(config.apn, "super");
        strcpy(config.apn_user, "");
        strcpy(config.apn_password, "");
        addLog("APN", "USA carrier configurado");
      } else {
        Serial.printf("[APN] Unknown operator: %s, keeping default\n", mccmnc.c_str());
        addLog("APN", "Operador desconhecido");
      }

      // Save auto-configured APN
      saveConfig();
      Serial.println("[APN] Auto-configuration complete");
      addLog("APN", "Auto-configuracao concluida");
    } else {
      addLog("APN", "Nao foi possivel obter MCC/MNC");
    }
  }
}

void setupAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.printf("[WiFi] AP Mode: SSID=%s, IP=%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  server.onNotFound(handleNotFound);
  server.begin();
  setupOTA();
}

void setupWiFiMode() {
  if (!config.wifi_ssid[0]) {
    setupAPMode();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifi_ssid, config.wifi_password);

  Serial.printf("[WiFi] Connecting to %s...\n", config.wifi_ssid);
  addLog("WiFi", "Conectando...");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    addLog("WiFi", "Conectado!");
    autoConfigureAPN();
    server.onNotFound(handleNotFound);
    server.begin();
    setupOTA();
  } else {
    Serial.println("\n[WiFi] Failed to connect, entering AP mode");
    addLog("WiFi", "Falha na conexao, entrando em modo AP");
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
  ArduinoOTA.handle();
  checkSMS();
  server.handleClient();

  // Check signal strength every 10 seconds
  if (millis() - last_signal_check > 10000) {
    signal_strength = getSignalStrength();
    last_signal_check = millis();
    Serial.printf("[Signal] Strength: %d%%\n", signal_strength);
  }

  if (sms_received) {
    sms_received = false;
    sendToBackend(last_sms_sender, last_sms_body);
  }

  delay(10);
}
