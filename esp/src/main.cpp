#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HardwareSerial.h>
#include <ArduinoOTA.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <esp_idf_version.h>
#include "ui_html.h"
#include "ca_isrg_x1.h"
#include <mbedtls/md.h>
#include <esp_system.h>
#include <TinyGsmClient.h>
#include <Wire.h>

// SIM800L configuration (UART on TTGO T-Call V1.4 board)
// TinyGSM drives the modem over this UART, replacing the hand-rolled
// AT-command parsing that used to hang silently whenever the module was
// slow to respond (e.g. brownout during a TX current spike).
HardwareSerial sim800l(2);
TinyGsm modem(sim800l);
const int RX_PIN = 26;
const int TX_PIN = 27;
const int RST_PIN = 5;
const int PWRKEY_PIN = 4;
const int POWER_ON_PIN = 23;
const int I2C_SDA_PIN = 21;
const int I2C_SCL_PIN = 22;

// IP5306 power management IC (keeps the 5V boost rail feeding the SIM800L
// alive; by default it shuts that rail off when it doesn't see a battery,
// which is why the modem can look completely dead - no LED, no AT reply -
// while the ESP32 itself boots and runs fine off a separate supply).
#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

bool setupPMU() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  Wire.write(0x37);  // enable boost-output-keep-on (bit1)
  return Wire.endTransmission() == 0;
}

// Pulses PWRKEY to power on the modem, per TTGO T-Call boot sequence.
void sim800l_powerOn() {
  pinMode(POWER_ON_PIN, OUTPUT);
  digitalWrite(POWER_ON_PIN, HIGH);

  pinMode(PWRKEY_PIN, OUTPUT);
  digitalWrite(PWRKEY_PIN, HIGH);
  delay(100);
  digitalWrite(PWRKEY_PIN, LOW);
  delay(1000);
  digitalWrite(PWRKEY_PIN, HIGH);
}

// Log structure
struct LogEntry {
  char timestamp[32];
  char type[16];
  char message[256];
};

// Wi-Fi networks, tried in priority order (index 0 = highest priority).
#define MAX_WIFI_NETWORKS 5
struct WifiNetwork {
  char ssid[64];
  char password[128];
};

// Configuration storage
struct Config {
  WifiNetwork wifi_nets[MAX_WIFI_NETWORKS];
  char api_url[256];
  char api_key[128];
  char whatsapp_number[32];
  bool whatsapp_enabled;
  bool roaming_enabled;
  char gsm_network[64];
  char apn[128];
  char apn_user[64];
  char apn_password[64];
  char sim_pin[8];
  char manual_phone_number[32];
} config;

// Log system
#define MAX_LOGS 100
LogEntry logs[MAX_LOGS];
int log_count = 0;

// WiFi and Web Server
WebServer server(80);
const char* AP_SSID = "SIM800L-Gateway";
const char* AP_PASSWORD = "12345678";
const char* FIRMWARE_VERSION = "v1.3.0";

volatile bool sms_received = false;
String last_sms_sender;
String last_sms_body;

// Signal strength
int signal_strength = 0;
unsigned long last_signal_check = 0;

// SIM identity - own phone number (if provisioned) and ICCID as a fallback
// unique label when it isn't.
String sim_phone_number = "";

// Backend (sms-gateway) integration state
WiFiClientSecure backend_client;
unsigned long last_backend_poll = 0;
const unsigned long BACKEND_POLL_INTERVAL_MS = 5000;

// Pairing state: while config.api_key is empty, the device asks the backend
// for a claim code, shows it on the local portal / serial log, and polls
// until the user links it to their account in the backoffice.
String pairing_claim_code = "";
String pairing_poll_token = "";
String pairing_status = "unpaired";  // unpaired | pending | linked | error
bool pairing_active = false;
unsigned long last_pairing_action = 0;
const unsigned long PAIRING_POLL_INTERVAL_MS = 5000;
const unsigned long OUTBOX_RETRY_MS = 10000;   // retry period while messages are pending
#define OUTBOX_FILE "/outbox.json"              // pending messages (plaintext until delivered)
#define SENTLOG_FILE "/sentlog.json"            // HMAC-SHA256 digests of delivered messages only
#define HMAC_KEY_FILE "/hmac.key"
#define OUTBOX_MAX 200
#define SENTLOG_MAX 500
const unsigned long PAIRING_RETRY_INTERVAL_MS = 20000;

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
void saveConfig();  // defined below; loadConfig()'s URL migration needs it

// Masks digit runs of 7+ digits (phone numbers) keeping the last 4, e.g. +351912345678 -> +351*******5678.
static String maskPhoneNumbers(const char* in) {
  String src(in), out;
  int i = 0, n = src.length();
  while (i < n) {
    if (isDigit(src[i]) || (src[i] == '+' && i + 1 < n && isDigit(src[i + 1]))) {
      int start = i;
      if (src[i] == '+') i++;
      int digits = 0, j = i;
      while (j < n && isDigit(src[j])) { j++; digits++; }
      if (digits >= 7) {
        out += src.substring(start, i);
        for (int k = 0; k < digits - 4; k++) out += '*';
        out += src.substring(j - 4, j);
      } else {
        out += src.substring(start, j);
      }
      i = j;
    } else {
      out += src[i++];
    }
  }
  return out;
}

void addLog(const char* type, const char* message) {
  String masked = maskPhoneNumbers(message);
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
  strlcpy(logs[log_count].message, masked.c_str(), sizeof(logs[log_count].message));
  log_count++;
  Serial.printf("[LOG] %s | %s | %s\n", logs[log_count-1].timestamp, type, message);
}

void loadConfig() {
  File file = SPIFFS.open("/config.json", "r");
  if (!file) {
    memset(&config, 0, sizeof(config));
    strcpy(config.api_url, "https://sms-gateway.thecoordinates.app/api");
    strcpy(config.api_key, "");
    strcpy(config.whatsapp_number, "");
    strcpy(config.gsm_network, "AUTO");
    strcpy(config.apn, "");
    strcpy(config.apn_user, "");
    strcpy(config.apn_password, "");
    strcpy(config.manual_phone_number, "");
    config.whatsapp_enabled = true;
    config.roaming_enabled = false;
    addLog("BOOT", "Sistema iniciado");
    return;
  }

  JsonDocument doc;
  deserializeJson(doc, file);
  file.close();

  // Priority list; configs saved by older firmware only have wifi_ssid/wifi_password,
  // which become the single (highest-priority) network.
  memset(config.wifi_nets, 0, sizeof(config.wifi_nets));
  JsonArray nets = doc["wifi_networks"].as<JsonArray>();
  if (!nets.isNull()) {
    int n = 0;
    for (JsonObject o : nets) {
      if (n >= MAX_WIFI_NETWORKS) break;
      const char* s = o["ssid"] | "";
      if (!s[0]) continue;
      strlcpy(config.wifi_nets[n].ssid, s, sizeof(config.wifi_nets[n].ssid));
      strlcpy(config.wifi_nets[n].password, o["password"] | "", sizeof(config.wifi_nets[n].password));
      n++;
    }
  } else {
    strlcpy(config.wifi_nets[0].ssid, doc["wifi_ssid"] | "", sizeof(config.wifi_nets[0].ssid));
    strlcpy(config.wifi_nets[0].password, doc["wifi_password"] | "", sizeof(config.wifi_nets[0].password));
  }
  strlcpy(config.api_url, doc["api_url"] | "https://sms-gateway.thecoordinates.app/api", sizeof(config.api_url));
  strlcpy(config.api_key, doc["api_key"] | "", sizeof(config.api_key));
  strlcpy(config.whatsapp_number, doc["whatsapp_number"] | "", sizeof(config.whatsapp_number));
  strlcpy(config.gsm_network, doc["gsm_network"] | "AUTO", sizeof(config.gsm_network));
  strlcpy(config.apn, doc["apn"] | "", sizeof(config.apn));
  strlcpy(config.apn_user, doc["apn_user"] | "", sizeof(config.apn_user));
  strlcpy(config.apn_password, doc["apn_password"] | "", sizeof(config.apn_password));
  strlcpy(config.sim_pin, doc["sim_pin"] | "", sizeof(config.sim_pin));
  strlcpy(config.manual_phone_number, doc["manual_phone_number"] | "", sizeof(config.manual_phone_number));
  config.whatsapp_enabled = doc["whatsapp_enabled"] | true;
  config.roaming_enabled = doc["roaming_enabled"] | false;

  // One-time migration: earlier builds saved a LAN dev URL or the "test."
  // convenience subdomain, neither of which resolves from an arbitrary WiFi
  // network - that left pairing silently stuck on DNS failures. Force
  // production so a device that's already been configured still pairs.
  String storedUrl = String(config.api_url);
  if (storedUrl.indexOf("test.sms-gateway") >= 0 || storedUrl.indexOf("192.168.0.116") >= 0) {
    storedUrl = "https://sms-gateway.thecoordinates.app/api";
  }

  // Safety net regardless of host: every backend call below appends its own
  // "/device/..." or "/pair/..." path assuming api_url already ends in
  // "/api". Missing that suffix doesn't fail loudly - the reverse proxy
  // serves the webfront's SPA (HTTP 200, HTML body) for any unmatched path,
  // so the mistake looked like total silence instead of an error.
  while (storedUrl.endsWith("/")) storedUrl.remove(storedUrl.length() - 1);
  if (!storedUrl.endsWith("/api")) storedUrl += "/api";

  if (storedUrl != String(config.api_url)) {
    strlcpy(config.api_url, storedUrl.c_str(), sizeof(config.api_url));
    saveConfig();
    addLog("CONFIG", "API URL corrigida/migrada");
  }

  addLog("CONFIG", "Configuracoes carregadas");
  Serial.printf("[CONFIG] api_url=%s  api_key_len=%d\n", config.api_url, (int)strlen(config.api_key));
}

void saveConfig() {
  JsonDocument doc;
  JsonArray nets = doc["wifi_networks"].to<JsonArray>();
  for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
    if (!config.wifi_nets[i].ssid[0]) continue;
    JsonObject o = nets.add<JsonObject>();
    o["ssid"] = config.wifi_nets[i].ssid;
    o["password"] = config.wifi_nets[i].password;
  }
  // Highest-priority network also under the legacy keys, so a downgrade keeps working.
  doc["wifi_ssid"] = config.wifi_nets[0].ssid;
  doc["wifi_password"] = config.wifi_nets[0].password;
  doc["api_url"] = config.api_url;
  doc["api_key"] = config.api_key;
  doc["whatsapp_number"] = config.whatsapp_number;
  doc["whatsapp_enabled"] = config.whatsapp_enabled;
  doc["roaming_enabled"] = config.roaming_enabled;
  doc["gsm_network"] = config.gsm_network;
  doc["apn"] = config.apn;
  doc["apn_user"] = config.apn_user;
  doc["apn_password"] = config.apn_password;
  doc["sim_pin"] = config.sim_pin;
  doc["manual_phone_number"] = config.manual_phone_number;

  File file = SPIFFS.open("/config.json", "w");
  serializeJson(doc, file);
  file.close();
  addLog("CONFIG", "Configuracoes salvas");
}

// --- SIM800L Communication (via TinyGSM) ---

// Pulses the RST line low. Only useful as a last resort: on this wiring RST
// is pulled up externally, so this is the one way software can force the
// module to reboot when it stops answering AT commands entirely.
void sim800l_hardReset() {
  Serial.println("[SIM800L] Hardware reset pulse...");
  digitalWrite(RST_PIN, LOW);
  delay(300);
  digitalWrite(RST_PIN, HIGH);
  delay(3000);
}

// Reads the SIM's own MSISDN (subscriber number) via AT+CNUM. Many prepaid
// SIMs never have this provisioned by the carrier, so "" is a normal,
// expected result here, not a failure.
String readSimPhoneNumber() {
  modem.sendAT(GF("+CNUM"));
  String resp;
  modem.waitResponse(10000, resp);

  // Response looks like: +CNUM: "","+5511987654321",145
  // Two quoted fields - alpha tag (usually empty), then the number.
  int start = resp.indexOf("+CNUM:");
  if (start < 0) return "";
  int q1 = resp.indexOf('"', start);
  int q2 = (q1 < 0) ? -1 : resp.indexOf('"', q1 + 1);
  int q3 = (q2 < 0) ? -1 : resp.indexOf('"', q2 + 1);
  int q4 = (q3 < 0) ? -1 : resp.indexOf('"', q3 + 1);
  if (q4 < 0) return "";
  String number = resp.substring(q3 + 1, q4);
  number.trim();
  return number;
}

// Fallback unique label when the SIM has no MSISDN on file: the ICCID
// printed on the card itself, read via AT+CCID/AT+ICCID depending on
// firmware. Parsed by stripping to digits rather than trusting an exact
// response shape, since that varies across SIM800 firmware revisions.
String readSimICCID() {
  modem.sendAT(GF("+CCID"));
  String resp;
  modem.waitResponse(10000, resp);
  String digits;
  for (size_t i = 0; i < resp.length(); i++) {
    if (isDigit(resp.charAt(i))) digits += resp.charAt(i);
  }
  return digits;
}

void sim800l_init() {
  Serial.println("\n\n[SIM800L] Starting initialization...");
  Serial.printf("[SIM800L] RST_PIN=%d, RX_PIN=%d, TX_PIN=%d, PWRKEY_PIN=%d\n",
                RST_PIN, RX_PIN, TX_PIN, PWRKEY_PIN);

  if (!setupPMU()) {
    Serial.println("[SIM800L] Warning: IP5306 PMU not acknowledged (I2C write failed)");
  }

  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, HIGH);

  sim800l_powerOn();
  delay(3000);  // Wait for any pending initialization

  sim800l.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(3000);  // SIM800L boot time after power-up

  addLog("SIM800L", "Testando conexao...");
  bool connected = modem.testAT(5000);

  // First attempt failed - try one hardware reset pulse before giving up,
  // this recovers the module if it locked up (e.g. brownout) without
  // requiring a full ESP32 reboot.
  if (!connected) {
    addLog("SIM800L", "Sem resposta, repetindo power-on (PWRKEY)...");
    sim800l_powerOn();
    delay(3000);
    connected = modem.testAT(5000);
  }

  // Still nothing - try a hardware reset pulse as a last resort.
  if (!connected) {
    addLog("SIM800L", "Sem resposta, tentando reset de hardware...");
    sim800l_hardReset();
    connected = modem.testAT(10000);
  }

  if (!connected) {
    addLog("SIM800L", "Conexao falhou!");
    return;
  }
  addLog("SIM800L", "Conectado!");

  // Check and unlock SIM if PIN configured
  SimStatus simStatus = modem.getSimStatus();
  if (simStatus == SIM_LOCKED) {
    if (config.sim_pin[0]) {
      addLog("SIM800L", "Desbloqueando PIN da SIM...");
      if (modem.simUnlock(config.sim_pin)) {
        addLog("SIM800L", "PIN desbloqueado com sucesso!");
        delay(2000);  // Wait for SIM to be ready
      } else {
        addLog("SIM800L", "Erro ao desbloquear PIN!");
      }
    } else {
      addLog("SIM800L", "SIM bloqueada e nenhum PIN configurado!");
    }
  } else if (simStatus == SIM_READY) {
    addLog("SIM800L", "SIM ja desbloqueada");
  } else {
    addLog("SIM800L", "SIM nao detectada!");
  }

  sim_phone_number = readSimPhoneNumber();
  if (sim_phone_number.length()) {
    addLog("SIM800L", ("Numero da SIM: " + sim_phone_number).c_str());
  } else if (config.manual_phone_number[0]) {
    // Carrier didn't provision the MSISDN on the SIM - use what the user
    // declared manually on the portal instead.
    sim_phone_number = config.manual_phone_number;
    addLog("SIM800L", ("Numero declarado manualmente: " + sim_phone_number).c_str());
  } else {
    addLog("SIM800L", "Numero da SIM indisponivel (nao gravado pela operadora)");
  }

  // Configure SMS mode: text mode, direct URC delivery of incoming SMS
  addLog("SIM800L", "Configurando SMS...");
  modem.sendAT(GF("+CMGF=1"));
  modem.waitResponse();

  // Character set for AT+CMGR/+CMT text - accented Western European chars
  // arrive as raw Latin-1 bytes, decoded via latin1ToUtf8 on receive.
  modem.sendAT(GF("+CSCS=\"GSM\""));
  modem.waitResponse();

  // mode 2 = URC not dropped while a command is in flight, mt 2 = deliver
  // incoming SMS directly as "+CMT:" instead of just an index to look up
  modem.sendAT(GF("+CNMI=2,2,0,0,0"));
  modem.waitResponse();

  modem.sendAT(GF("+CSMP=17,167,0,0"));
  modem.waitResponse();

  // Configure roaming if enabled
  if (config.roaming_enabled) {
    addLog("SIM800L", "Ativando roaming internacional...");
    modem.sendAT(GF("+CROAMING=1"));
    modem.waitResponse();
  }

  // Set network if configured
  if (config.gsm_network[0] && strcmp(config.gsm_network, "AUTO") != 0) {
    addLog("SIM800L", "Configurando rede GSM...");
    modem.sendAT(GF("+COPS=1,2,"), config.gsm_network);
    modem.waitResponse(10000);
  }

  // Set APN if configured
  if (config.apn[0] && config.apn_user[0] && config.apn_password[0]) {
    addLog("SIM800L", "Configurando APN...");
    modem.sendAT(GF("+SAPBR=3,1,Contype,GPRS"));
    modem.waitResponse();
    modem.sendAT(GF("+SAPBR=3,1,APN,"), config.apn);
    modem.waitResponse();
    modem.sendAT(GF("+SAPBR=3,1,USER,"), config.apn_user);
    modem.waitResponse();
    modem.sendAT(GF("+SAPBR=3,1,PWD,"), config.apn_password);
    modem.waitResponse();
  }

  addLog("SIM800L", "Inicializacao concluida");
}

// --- Actual SMS transmission via SIM800L ---
bool sendSMS(const String& number, const String& text) {
  bool ok = modem.sendSMS(number, text);
  Serial.printf("[SMS] Send result: %s\n", ok ? "OK" : "FAIL");
  addLog("SMS", ok ? ("Enviado para " + number).c_str() : ("Falha ao enviar para " + number).c_str());
  return ok;
}

void enqueueSMS(const String& sender, const String& body);
void flushOutbox();
bool sendToBackend(String sender, String body);

// --- Outbox and delivered-message log --------------------------------------
// A received SMS is written to the outbox before anything else, and removed only after the backend
// accepted it. The sent log stores HMAC-SHA256(key, sender|text|timestamp) for every delivered
// message, so it can answer "was this already sent?" without containing any message text. The key
// is random per device and never leaves the device.
// Last backend call, shown in the portal (Teste tab). Body is never stored: it can contain SMS text.
struct LastBackendCall {
  bool has = false;
  String when, method, url, error;
  int http_code = 0;
  unsigned long ms = 0;
};
LastBackendCall lastBackend;

uint8_t hmac_key[32];
int outbox_count = 0;
unsigned long last_flush_attempt = 0;
bool backend_online = false;

bool loadJson(const char* path, JsonDocument& doc) {
  File f = SPIFFS.open(path, "r");
  if (!f) return false;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  return !err;
}

bool saveJson(const char* path, const JsonDocument& doc) {
  File f = SPIFFS.open(path, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

JsonArray itemsOf(JsonDocument& doc, const char* key) {
  if (doc[key].isNull()) return doc[key].to<JsonArray>();
  return doc[key].as<JsonArray>();
}

void initOutbox() {
  // Random per-device HMAC key, created once.
  File f = SPIFFS.open(HMAC_KEY_FILE, "r");
  bool loaded = false;
  if (f && f.size() == 64) {
    String hex = f.readString();
    loaded = true;
    for (int i = 0; i < 32; i++) {
      char b[3] = {hex[i * 2], hex[i * 2 + 1], 0};
      hmac_key[i] = (uint8_t)strtoul(b, nullptr, 16);
    }
  }
  if (f) f.close();
  if (!loaded) {
    esp_fill_random(hmac_key, sizeof hmac_key);
    File w = SPIFFS.open(HMAC_KEY_FILE, "w");
    for (int i = 0; i < 32; i++) w.printf("%02x", hmac_key[i]);
    w.close();
    addLog("Outbox", "Chave de registo de envios criada");
  }
  JsonDocument box;
  if (loadJson(OUTBOX_FILE, box)) outbox_count = itemsOf(box, "items").size();
}

String messageDigest(const char* from, const char* text, uint32_t ts) {
  String input = String(from) + "\n" + text + "\n" + String(ts);
  unsigned char out[32];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), hmac_key, sizeof hmac_key,
                  reinterpret_cast<const unsigned char*>(input.c_str()), input.length(), out);
  String hex;
  for (int i = 0; i < 32; i++) {
    char b[3];
    snprintf(b, sizeof b, "%02x", out[i]);
    hex += b;
  }
  return hex;
}

void sentLogAdd(JsonArray digests, const String& digest) {
  while (digests.size() >= SENTLOG_MAX) digests.remove(0);
  digests.add(digest);
}

void enqueueSMS(const String& sender, const String& body) {
  JsonDocument box;
  loadJson(OUTBOX_FILE, box);
  JsonArray items = itemsOf(box, "items");
  if (items.size() >= OUTBOX_MAX) {
    items.remove(0);
    addLog("Outbox", "Fila cheia: mensagem mais antiga descartada");
  }
  JsonObject it = items.add<JsonObject>();
  it["from"] = sender;
  it["text"] = body;
  it["ts"] = (uint32_t)time(nullptr);
  saveJson(OUTBOX_FILE, box);
  outbox_count = items.size();
}

// Backend reachable (any HTTP answer below 500) means online. Going from offline to online
// immediately delivers everything that is still pending.
void noteBackendResult(int code) {
  bool online = code >= 200 && code < 500;
  if (online && !backend_online) {
    backend_online = true;
    addLog("Backend", "Ligado ao backend: a enviar mensagens pendentes");
    flushOutbox();
  } else if (!online && backend_online) {
    backend_online = false;
    addLog("Backend", "Backend inacessivel: mensagens ficam em fila");
  }
}

// Sends every pending message that is not already in the sent log, oldest first.
// Stops at the first transport/5xx failure (retried later); drops messages the backend
// permanently rejects (4xx other than auth/rate limits) so they cannot block the queue.
void flushOutbox() {
  if (!WiFi.isConnected() || !config.api_key[0]) return;
  JsonDocument box;
  if (!loadJson(OUTBOX_FILE, box)) { outbox_count = 0; return; }
  JsonArray items = itemsOf(box, "items");
  if (items.size() == 0) { outbox_count = 0; return; }

  JsonDocument logDoc;
  loadJson(SENTLOG_FILE, logDoc);
  JsonArray digests = itemsOf(logDoc, "digests");
  bool logChanged = false;
  bool changed = false;

  for (size_t i = 0; i < items.size();) {
    const char* from = items[i]["from"] | "";
    const char* text = items[i]["text"] | "";
    uint32_t ts = items[i]["ts"] | 0;
    String digest = messageDigest(from, text, ts);

    bool already = false;
    for (JsonVariant v : digests) if (digest == (v | "")) { already = true; break; }
    if (already) {
      items.remove(i);
      changed = true;
      addLog("Outbox", "Mensagem ja enviada anteriormente: removida da fila");
      continue;
    }

    bool ok = sendToBackend(String(from), String(text));
    int code = lastBackend.http_code;
    if (ok) {
      sentLogAdd(digests, digest);
      logChanged = true;
      items.remove(i);
      changed = true;
      continue;
    }
    if (code >= 400 && code < 500 && code != 401 && code != 403 && code != 408 && code != 429) {
      addLog("Outbox", ("Backend rejeitou a mensagem (HTTP " + String(code) + "): descartada").c_str());
      items.remove(i);
      changed = true;
      continue;
    }
    if (code < 0 || code >= 500) backend_online = false;  // no flush here: avoids recursion
    break;
  }

  if (changed) saveJson(OUTBOX_FILE, box);
  if (logChanged) saveJson(SENTLOG_FILE, logDoc);
  outbox_count = items.size();
}

String outboxStatusJson() {
  JsonDocument doc;
  JsonDocument box;
  loadJson(OUTBOX_FILE, box);
  JsonDocument logDoc;
  loadJson(SENTLOG_FILE, logDoc);
  doc["pending"] = itemsOf(box, "items").size();
  doc["delivered_digests"] = itemsOf(logDoc, "digests").size();
  doc["backend_online"] = backend_online;
  String out;
  serializeJson(doc, out);
  return out;
}

void handleSMSReceived(String sender, String body) {
  Serial.printf("[SMS] From: %s | Body: %s\n", sender.c_str(), body.c_str());

  String preview = body.length() > 40 ? body.substring(0, 40) + "..." : body;
  addLog("SMS", ("Recebido de " + sender + ": " + preview).c_str());

  // Persist first: a message is never dropped because the backend was down or a second SMS arrived.
  enqueueSMS(sender, body);
  flushOutbox();
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
  int csq = modem.getSignalQuality();  // raw 0-31, or 99 if unknown/not detectable
  if (csq < 0 || csq > 31) return 0;
  return (csq * 100) / 31;  // linear 0-31 -> 0-100%, always in range
}

// SIM800L text-mode SMS delivery sends accented/special characters as raw
// Latin-1 bytes (single byte >= 0x80), not UTF-8. Left alone, those bytes
// are invalid UTF-8 and break the JSON body sent to the backend outright
// ("There was an error parsing the body") - not just garble the character.
String latin1ToUtf8(const String& in) {
  String out;
  out.reserve(in.length());
  for (size_t i = 0; i < in.length(); i++) {
    uint8_t c = (uint8_t)in.charAt(i);
    if (c < 0x80) {
      out += (char)c;
    } else {
      out += (char)(0xC0 | (c >> 6));
      out += (char)(0x80 | (c & 0x3F));
    }
  }
  return out;
}

// The network - not our own charset config - decides per-message whether an
// SMS with "too many"/mixed accents gets sent as UCS2 instead of a single
// Latin-1-ish byte per character; when that happens the body arrives as a
// plain hex string of 16-bit code units regardless of anything we set.
// Auto-detect that shape and decode it, rather than requiring a manual
// mode switch that wouldn't actually control this anyway.
bool looksLikeUcs2Hex(const String& s) {
  if (s.length() < 8 || s.length() % 4 != 0) return false;
  for (size_t i = 0; i < s.length(); i++) {
    if (!isHexadecimalDigit(s.charAt(i))) return false;
  }
  return true;
}

String ucs2HexToUtf8(const String& hex) {
  String out;
  size_t n = hex.length() / 4;
  out.reserve(n * 3);
  for (size_t i = 0; i < n; i++) {
    uint32_t codepoint = strtol(hex.substring(i * 4, i * 4 + 4).c_str(), nullptr, 16);

    if (codepoint >= 0xD800 && codepoint <= 0xDBFF && i + 1 < n) {
      long low = strtol(hex.substring((i + 1) * 4, (i + 1) * 4 + 4).c_str(), nullptr, 16);
      if (low >= 0xDC00 && low <= 0xDFFF) {
        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
        i++;
      }
    }

    if (codepoint < 0x80) {
      out += (char)codepoint;
    } else if (codepoint < 0x800) {
      out += (char)(0xC0 | (codepoint >> 6));
      out += (char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
      out += (char)(0xE0 | (codepoint >> 12));
      out += (char)(0x80 | ((codepoint >> 6) & 0x3F));
      out += (char)(0x80 | (codepoint & 0x3F));
    } else {
      out += (char)(0xF0 | (codepoint >> 18));
      out += (char)(0x80 | ((codepoint >> 12) & 0x3F));
      out += (char)(0x80 | ((codepoint >> 6) & 0x3F));
      out += (char)(0x80 | (codepoint & 0x3F));
    }
  }
  return out;
}

void checkSMS() {
  if (sim800l.available()) {
    String line = sim800l.readStringUntil('\n');
    line.trim();
    Serial.printf("[SIM800L] %s\n", line.c_str());

    // Parse incoming SMS: +CMT: "+1234567890","","21/06/29,14:32:45+00"
    if (line.startsWith("+CMT:")) {
      int quote1 = line.indexOf('"');
      int quote2 = line.indexOf('"', quote1 + 1);

      if (quote1 >= 0 && quote2 > quote1) {
        String sender = line.substring(quote1 + 1, quote2);

        // Body arrives as the next full line
        String body = sim800l.readStringUntil('\n');
        body.trim();
        String decodedBody = looksLikeUcs2Hex(body) ? ucs2HexToUtf8(body) : latin1ToUtf8(body);
        handleSMSReceived(sender, decodedBody);
      }
    }
  }
}

// --- Backend (sms-gateway) integration ---
// Device pairing and message sync reuse the same X-API-Key device API the
// Android gateway app uses, so this ESP32 shows up as an ordinary paired
// device in the backoffice - including its Telegram/WhatsApp forwarding,
// which the backend already does server-side per account (see BridgeSettings).


static void recordBackendCall(const char* method, const String& url, int code, unsigned long startMs) {
  lastBackend.has = true;
  lastBackend.method = method;
  lastBackend.url = url;
  lastBackend.http_code = code;
  lastBackend.ms = millis() - startMs;
  time_t now = time(nullptr);
  char buf[24];
  strftime(buf, sizeof buf, "%d/%m %H:%M:%S", localtime(&now));
  lastBackend.when = buf;
  if (code < 0) lastBackend.error = "Falha de ligacao";
  else if (code < 200 || code >= 300) lastBackend.error = "HTTP " + String(code);
  else lastBackend.error = "";
}

bool backendPost(const String& path, const String& payload, String& responseOut) {
  backend_client.setCACert(BACKEND_CA_PEM);  // verify the server certificate (ISRG Root X1)
  HTTPClient https;
  String url = String(config.api_url) + path;
  unsigned long t0 = millis();
  if (!https.begin(backend_client, url)) { recordBackendCall("POST", url, -1, t0); return false; }
  https.setTimeout(10000);
  https.addHeader("Content-Type", "application/json");
  if (config.api_key[0]) https.addHeader("X-API-Key", config.api_key);
  int code = https.POST(payload);
  responseOut = https.getString();
  https.end();
  recordBackendCall("POST", url, code, t0);
  return code >= 200 && code < 300;
}

// Returns the HTTP status code (or -1 if the connection itself failed) so
// callers can tell "server said no" (e.g. 401 = revoked key) apart from
// "network/DNS trouble, try again later".
int backendGetCode(const String& path, String& responseOut) {
  backend_client.setCACert(BACKEND_CA_PEM);  // verify the server certificate (ISRG Root X1)
  HTTPClient https;
  String url = String(config.api_url) + path;
  unsigned long t0 = millis();
  if (!https.begin(backend_client, url)) { recordBackendCall("GET", url, -1, t0); return -1; }
  https.setTimeout(10000);
  if (config.api_key[0]) https.addHeader("X-API-Key", config.api_key);
  int code = https.GET();
  responseOut = https.getString();
  https.end();
  recordBackendCall("GET", url, code, t0);
  return code;
}

// Forward a received SMS to the backend; it fans it out to webhooks and to
// the account's configured Telegram/WhatsApp chat on its own.
bool sendToBackend(String sender, String body) {
  if (!WiFi.isConnected() || !config.api_key[0]) {
    return false;
  }

  JsonDocument doc;
  doc["phone_number"] = sender;
  doc["text"] = body;
  String payload;
  serializeJson(doc, payload);

  String response;
  bool ok = backendPost("/device/messages/inbound", payload, response);
  Serial.printf("[API] Inbound forward %s: %s\n", ok ? "OK" : "FAIL", response.c_str());
  addLog("API", ok ? "SMS encaminhado ao backend" : "Falha ao encaminhar SMS ao backend");
  return ok;
}

void reportMessageStatus(long messageId, const char* status, const char* error = nullptr) {
  JsonDocument doc;
  doc["status"] = status;
  if (error) doc["error"] = error;
  String payload, response;
  serializeJson(doc, payload);
  backendPost("/device/messages/" + String(messageId) + "/status", payload, response);
}

// Poll the backend for SMS the user queued from the backoffice/Telegram and
// send them out over the SIM800L, same flow the Android app's poll loop uses.
void pollPendingMessages() {
  if (!WiFi.isConnected() || !config.api_key[0]) return;

  String response;
  int code = backendGetCode("/device/messages/pending", response);
  noteBackendResult(code);

  if (code == 401 || code == 403) {
    // The key doesn't work anymore (device deleted in the backoffice, DB
    // reset, etc.) - drop it so loop() falls back to pairing on its own
    // instead of silently polling forever with a dead key.
    addLog("Pairing", "API key rejeitada pelo backend, repareando...");
    config.api_key[0] = '\0';
    saveConfig();
    pairing_status = "unpaired";
    return;
  }
  if (code != 200) {
    Serial.printf("[API] pending poll HTTP %d\n", code);
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, response)) {
    Serial.printf("[API] pending poll: invalid JSON (HTTP 200): %.100s\n", response.c_str());
    return;
  }

  for (JsonObject msg : doc.as<JsonArray>()) {
    long id = msg["id"] | 0;
    String phone = msg["phone_number"] | "";
    String text = msg["text"] | "";
    if (!id || !phone.length()) continue;

    bool ok = sendSMS(phone, text);
    reportMessageStatus(id, ok ? "sent" : "failed", ok ? nullptr : "SIM800L send failed");
  }
}

// --- Device pairing with the backend ---
void startPairing() {
  if (!WiFi.isConnected()) return;

  // Name the device after the SIM's own number (with country code) so it's
  // identifiable in the backoffice; fall back to the ICCID, then the MAC,
  // for SIMs that don't have a number provisioned.
  String label = sim_phone_number;
  if (!label.length() && config.manual_phone_number[0]) label = config.manual_phone_number;
  if (!label.length()) label = readSimICCID();
  if (!label.length()) label = WiFi.macAddress().substring(9);

  JsonDocument doc;
  doc["device_name"] = "ESP-" + label;
  String payload;
  serializeJson(doc, payload);

  String response;
  bool ok = backendPost("/pair/start", payload, response);
  if (!ok) {
    addLog("Pairing", "Falha ao iniciar pareamento");
    pairing_status = "error";
    last_pairing_action = millis();
    return;
  }

  JsonDocument res;
  if (deserializeJson(res, response)) {
    addLog("Pairing", "Resposta invalida do backend");
    pairing_status = "error";
    last_pairing_action = millis();
    return;
  }

  pairing_claim_code = res["claim_code"] | "";
  pairing_poll_token = res["poll_token"] | "";
  pairing_status = "pending";
  pairing_active = true;
  last_pairing_action = millis();
  addLog("Pairing", ("Codigo de pareamento: " + pairing_claim_code).c_str());
  String frontendUrl = String(config.api_url);
  int apiSuffix = frontendUrl.lastIndexOf("/api");
  if (apiSuffix > 0) frontendUrl = frontendUrl.substring(0, apiSuffix);
  Serial.printf("[Pairing] Entre em %s/devices e use o codigo: %s\n",
                frontendUrl.c_str(), pairing_claim_code.c_str());
}

void pollPairing() {
  JsonDocument doc;
  doc["poll_token"] = pairing_poll_token;
  String payload, response;
  serializeJson(doc, payload);

  if (!backendPost("/pair/poll", payload, response)) {
    last_pairing_action = millis();
    return;
  }

  JsonDocument res;
  if (deserializeJson(res, response)) {
    last_pairing_action = millis();
    return;
  }

  String status = res["status"] | "";
  if (status == "linked") {
    strlcpy(config.api_key, (res["api_key"] | ""), sizeof(config.api_key));
    saveConfig();
    pairing_status = "linked";
    pairing_active = false;
    addLog("Pairing", "Pareado com sucesso!");
  } else if (status == "expired") {
    addLog("Pairing", "Codigo expirado, gerando um novo...");
    pairing_active = false;
    startPairing();
  }
  // "pending" - keep waiting, nothing to do.
  last_pairing_action = millis();
}

// --- Web Server Handlers ---
void handleRoot() {
  String html = String(UI_HTML);
  html.replace("__FW_VERSION__", FIRMWARE_VERSION);
  server.send(200, "text/html", html);
}

// Secrets are never returned in clear. The UI sends the mask back unchanged, which means "keep".
static const char* SECRET_MASK = "********";
static String maskSecret(const char* v) { return v[0] ? String(SECRET_MASK) : String(""); }
static bool isMaskedValue(JsonVariantConst v) {
  return v.is<const char*>() && strcmp(v.as<const char*>(), SECRET_MASK) == 0;
}

void handleGetConfig() {
  JsonDocument doc;
  JsonArray nets = doc["wifi_networks"].to<JsonArray>();
  for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
    if (!config.wifi_nets[i].ssid[0]) continue;
    JsonObject o = nets.add<JsonObject>();
    o["ssid"] = config.wifi_nets[i].ssid;
    o["password"] = maskSecret(config.wifi_nets[i].password);
  }
  doc["api_url"] = config.api_url;
  doc["api_key"] = maskSecret(config.api_key);
  doc["whatsapp_number"] = config.whatsapp_number;
  doc["whatsapp_enabled"] = config.whatsapp_enabled;
  doc["roaming_enabled"] = config.roaming_enabled;
  doc["gsm_network"] = config.gsm_network;
  doc["apn"] = config.apn;
  doc["apn_user"] = config.apn_user;
  doc["apn_password"] = maskSecret(config.apn_password);
  doc["sim_pin"] = maskSecret(config.sim_pin);
  doc["manual_phone_number"] = config.manual_phone_number;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleGetLogs() {
  JsonDocument doc;
  JsonArray logsArray = doc["logs"].to<JsonArray>();
  for (int i = 0; i < log_count; i++) {
    JsonObject logObj = logsArray.add<JsonObject>();
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
  // sim_phone_number is only resolved once at boot (before the user could
  // have typed a manual number in the portal yet), so fall back live here
  // instead of caching it - otherwise a manual number saved after boot never
  // shows up until the next reboot.
  doc["sim_number"] = sim_phone_number.length() ? sim_phone_number : String(config.manual_phone_number);
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleGetWiFiScan() {
  scanWiFiNetworks();

  JsonDocument doc;
  JsonArray networks = doc["networks"].to<JsonArray>();

  for (int i = 0; i < available_networks_count; i++) {
    JsonObject net = networks.add<JsonObject>();
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

  if (doc["wifi_networks"].is<JsonArray>()) {
    // The posted list replaces the stored one, in the posted order (= priority).
    // A masked password means "keep the stored one for this SSID".
    WifiNetwork old[MAX_WIFI_NETWORKS];
    memcpy(old, config.wifi_nets, sizeof(old));
    memset(config.wifi_nets, 0, sizeof(config.wifi_nets));
    int n = 0;
    for (JsonObject o : doc["wifi_networks"].as<JsonArray>()) {
      if (n >= MAX_WIFI_NETWORKS) break;
      const char* s = o["ssid"] | "";
      if (!s[0]) continue;
      bool dup = false;
      for (int j = 0; j < n; j++) if (strcmp(config.wifi_nets[j].ssid, s) == 0) dup = true;
      if (dup) continue;
      strlcpy(config.wifi_nets[n].ssid, s, sizeof(config.wifi_nets[n].ssid));
      if (isMaskedValue(o["password"])) {
        for (int j = 0; j < MAX_WIFI_NETWORKS; j++) {
          if (strcmp(old[j].ssid, s) == 0) {
            strlcpy(config.wifi_nets[n].password, old[j].password, sizeof(config.wifi_nets[n].password));
            break;
          }
        }
      } else {
        strlcpy(config.wifi_nets[n].password, o["password"] | "", sizeof(config.wifi_nets[n].password));
      }
      n++;
    }
  } else if (!doc["wifi_ssid"].isNull()) {
    // Legacy single-network form: sets the highest-priority network.
    strlcpy(config.wifi_nets[0].ssid, doc["wifi_ssid"] | "", sizeof(config.wifi_nets[0].ssid));
    if (!doc["wifi_password"].isNull() && !isMaskedValue(doc["wifi_password"])) {
      strlcpy(config.wifi_nets[0].password, doc["wifi_password"] | "", sizeof(config.wifi_nets[0].password));
    }
  }
  if (!doc["api_url"].isNull()) {
    strlcpy(config.api_url, doc["api_url"] | "", sizeof(config.api_url));
  }
  if (!doc["api_key"].isNull() && !isMaskedValue(doc["api_key"])) {
    strlcpy(config.api_key, doc["api_key"] | "", sizeof(config.api_key));
  }
  if (!doc["whatsapp_number"].isNull()) {
    strlcpy(config.whatsapp_number, doc["whatsapp_number"] | "", sizeof(config.whatsapp_number));
  }
  if (!doc["whatsapp_enabled"].isNull()) {
    config.whatsapp_enabled = doc["whatsapp_enabled"];
  }
  if (!doc["roaming_enabled"].isNull()) {
    config.roaming_enabled = doc["roaming_enabled"];
  }
  if (!doc["gsm_network"].isNull()) {
    strlcpy(config.gsm_network, doc["gsm_network"] | "AUTO", sizeof(config.gsm_network));
  }
  if (!doc["apn"].isNull()) {
    strlcpy(config.apn, doc["apn"] | "", sizeof(config.apn));
  }
  if (!doc["apn_user"].isNull()) {
    strlcpy(config.apn_user, doc["apn_user"] | "", sizeof(config.apn_user));
  }
  if (!doc["apn_password"].isNull() && !isMaskedValue(doc["apn_password"])) {
    strlcpy(config.apn_password, doc["apn_password"] | "", sizeof(config.apn_password));
  }
  if (!doc["sim_pin"].isNull() && !isMaskedValue(doc["sim_pin"])) {
    strlcpy(config.sim_pin, doc["sim_pin"] | "", sizeof(config.sim_pin));
  }
  if (!doc["manual_phone_number"].isNull()) {
    strlcpy(config.manual_phone_number, doc["manual_phone_number"] | "", sizeof(config.manual_phone_number));
  }

  saveConfig();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handlePostTestSMS() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No body\"}");
    return;
  }

  JsonDocument doc;
  deserializeJson(doc, server.arg("plain"));
  String phone = doc["phone"] | "";
  String text = doc["text"] | "";
  if (!phone.length() || !text.length()) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"phone e text sao obrigatorios\"}");
    return;
  }

  bool ok = sendSMS(phone, text);
  JsonDocument res;
  res["ok"] = ok;
  if (!ok) res["error"] = "SIM800L nao confirmou o envio (ver Log)";
  String out;
  serializeJson(res, out);
  server.send(ok ? 200 : 500, "application/json", out);
}

void handleGetPairing() {
  JsonDocument doc;
  doc["paired"] = config.api_key[0] != 0;
  doc["status"] = pairing_status;
  doc["claim_code"] = pairing_claim_code;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleGetBackendStatus() {
  JsonDocument doc;
  doc["has_call"] = lastBackend.has;
  doc["key_set"] = config.api_key[0] != 0;
  doc["paired"] = config.api_key[0] != 0;
  doc["when"] = lastBackend.when;
  doc["method"] = lastBackend.method;
  doc["url"] = lastBackend.url;
  doc["http_code"] = lastBackend.http_code;
  doc["error"] = lastBackend.error;
  doc["ms"] = lastBackend.ms;
  doc["body"] = "";  // never exposed: bodies can contain SMS text
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handlePostBackendTest() {
  String resp;
  backendGetCode("/device/messages/pending", resp);  // same authenticated call the poller makes
  handleGetBackendStatus();
}

// --- GSM diagnostics (read-only) ---
// Sends one AT query to the modem and returns its reply on a single line.
static String gsmQuery(const char* cmd, uint32_t timeoutMs = 2000) {
  modem.sendAT(cmd);
  String r;
  modem.waitResponse(timeoutMs, r);
  r.replace("\r", " ");
  r.replace("\n", " ");
  r.trim();
  return r;
}

static const char* resetReasonName() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "poweron";
    case ESP_RST_EXT:       return "external";
    case ESP_RST_SW:        return "software";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "int_watchdog";
    case ESP_RST_TASK_WDT:  return "task_watchdog";
    case ESP_RST_WDT:       return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT:  return "brownout";
    default:                return "other";
  }
}

// GET /api/gsm/status: raw modem state, to tell a bad reading from a real "no signal"
// (modem not registered) from a power problem (low/unstable supply voltage, brownout resets).
// Contains no secrets: no SIM number, no message content, no keys.
void handleGetGsmStatus() {
  JsonDocument doc;

  String csq = gsmQuery("+CSQ");
  String creg = gsmQuery("+CREG?");
  String cgreg = gsmQuery("+CGREG?");
  String cops = gsmQuery("+COPS?");
  String cbc = gsmQuery("+CBC");

  doc["csq_raw"] = csq;
  doc["creg_raw"] = creg;
  doc["cgreg_raw"] = cgreg;
  doc["cops_raw"] = cops;
  doc["cbc_raw"] = cbc;

  // +CSQ: <rssi 0-31, 99 = unknown>,<ber>
  int rssi = -1;
  int p = csq.indexOf("+CSQ:");
  if (p >= 0) sscanf(csq.c_str() + p + 5, " %d", &rssi);
  doc["csq"] = rssi;
  doc["csq_ok"] = rssi >= 1 && rssi <= 31;

  // +CREG: <n>,<stat>  (stat 1 = home network, 5 = roaming, 2 = searching, 3 = denied, 0 = not registered)
  int n = -1, stat = -1;
  p = creg.indexOf("+CREG:");
  if (p >= 0) sscanf(creg.c_str() + p + 6, " %d,%d", &n, &stat);
  doc["creg_stat"] = stat;
  doc["registered"] = (stat == 1 || stat == 5);

  // +CBC: <charge state>,<percent>,<supply voltage in mV>
  int chg = -1, pct = -1, mv = -1;
  p = cbc.indexOf("+CBC:");
  if (p >= 0) sscanf(cbc.c_str() + p + 5, " %d,%d,%d", &chg, &pct, &mv);
  doc["supply_mv"] = mv;
  // SIM800L needs about 3.4 V or more at its supply, with current peaks up to 2 A.
  doc["supply_low"] = (mv > 0 && mv < 3500);

  doc["signal_percent"] = signal_strength;  // value shown in the portal bar
  doc["uptime_s"] = (uint32_t)(millis() / 1000);
  doc["reset_reason"] = resetReasonName();  // "brownout" = supply voltage dipped
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["free_heap"] = ESP.getFreeHeap();

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleNotFound() {
  // Everything except the page itself requires the portal password (Basic auth, user "admin").
  // The password is the existing AP password, so no password is added or removed.
  if (server.uri() != "/" && !server.authenticate("admin", AP_PASSWORD)) {
    server.requestAuthentication(BASIC_AUTH, "SIM800L Gateway");
    return;
  }
  if (server.method() == HTTP_GET && server.uri() == "/api/outbox/status") {
    server.send(200, "application/json", outboxStatusJson());  // counts only, never message content
    return;
  } else if (server.method() == HTTP_GET && server.uri() == "/api/backend/status") {
    handleGetBackendStatus();
    return;
  } else if (server.method() == HTTP_GET && server.uri() == "/api/gsm/status") {
    handleGetGsmStatus();
    return;
  } else if (server.method() == HTTP_POST && server.uri() == "/api/backend/test") {
    handlePostBackendTest();
    return;
  }
  if (server.method() == HTTP_GET && server.uri() == "/config") {
    handleGetConfig();
  } else if (server.method() == HTTP_POST && server.uri() == "/config") {
    handlePostConfig();
  } else if (server.method() == HTTP_POST && server.uri() == "/api/sms/test") {
    handlePostTestSMS();
  } else if (server.method() == HTTP_GET && server.uri() == "/pairing") {
    handleGetPairing();
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
  // Force numeric operator format (2) so +COPS? returns the MCC/MNC code
  // instead of the carrier's display name.
  modem.sendAT(GF("+COPS=3,2"));
  modem.waitResponse();

  String response;
  modem.sendAT(GF("+COPS?"));
  modem.waitResponse(5000, response);

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
      } else if (mccmnc == "22808") {  // Telemovel PT (old, fallback)
        strcpy(config.apn, "internet");
        strcpy(config.apn_user, "");
        strcpy(config.apn_password, "");
        addLog("APN", "Telemovel PT configurado");
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

// Tries one network for up to maxAttempts*0.5 s (default ~10 s). Feeds the watchdog while waiting, since
// several networks in a row can take longer than a single wait.
static bool tryConnectWiFi(const char* ssid, const char* password, int maxAttempts = 20) {
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.printf("[WiFi] Connecting to %s...\n", ssid);
  addLog("WiFi", (String("Conectando a ") + ssid + "...").c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    esp_task_wdt_reset();
    attempts++;
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

// Walks the configured networks in priority order and stops at the first that connects.
// One scan first, only to size the timeout: a network seen in the scan gets the full ~10 s,
// one that was not seen gets ~4 s. It is never skipped, because a single ESP32 scan can miss
// networks (hidden SSIDs, busy channels) that still connect fine. If none is seen at all,
// every network gets the full timeout.
static bool connectByPriority() {
  bool seen[MAX_WIFI_NETWORKS] = {};
  bool anySeen = false;
  WiFi.mode(WIFI_STA);
  int found = WiFi.scanNetworks();
  for (int i = 0; i < found; i++) {
    String s = WiFi.SSID(i);
    for (int j = 0; j < MAX_WIFI_NETWORKS; j++) {
      if (config.wifi_nets[j].ssid[0] && s == config.wifi_nets[j].ssid) {
        seen[j] = true;
        anySeen = true;
      }
    }
  }
  WiFi.scanDelete();
  esp_task_wdt_reset();
  Serial.printf("[WiFi] Scan: %d networks, %s of the configured ones in range\n", found,
                anySeen ? "some" : "none");

  for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
    if (!config.wifi_nets[i].ssid[0]) continue;
    int attempts = (anySeen && !seen[i]) ? 8 : 20;
    if (tryConnectWiFi(config.wifi_nets[i].ssid, config.wifi_nets[i].password, attempts)) {
      addLog("WiFi", (String("Rede ") + (i + 1) + " (prioridade): " + config.wifi_nets[i].ssid).c_str());
      return true;
    }
  }
  return false;
}

void setupWiFiMode() {
  if (!config.wifi_nets[0].ssid[0]) {
    setupAPMode();
    return;
  }

  if (connectByPriority()) {
    Serial.printf("[WiFi] Connected to %s, IP: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    addLog("WiFi", "Conectado!");

    // DNS: use exactly what the network's DHCP server handed out. Do not override it with
    // public resolvers - some networks block outbound DNS to external servers (8.8.8.8 and
    // 1.1.1.1 time out there), which silently broke name resolution for NTP and the backend.
    Serial.printf("[WiFi] DNS (DHCP) 1: %s  2: %s\n",
                  WiFi.dnsIP(0).toString().c_str(), WiFi.dnsIP(1).toString().c_str());

    // Probe the host the gateway actually needs (the backend), not an unrelated domain.
    String probeHost = String(config.api_url);
    int schemeEnd = probeHost.indexOf("://");
    if (schemeEnd >= 0) probeHost = probeHost.substring(schemeEnd + 3);
    int slash = probeHost.indexOf('/');
    if (slash >= 0) probeHost = probeHost.substring(0, slash);
    if (probeHost.isEmpty()) probeHost = "google.com";

    IPAddress probeIp;
    bool probeOk = WiFi.hostByName(probeHost.c_str(), probeIp);
    Serial.printf("[WiFi] DNS probe %s -> %s\n", probeHost.c_str(),
                  probeOk ? probeIp.toString().c_str() : "FAILED");
    if (!probeOk) addLog("WiFi", (String("DNS nao resolve ") + probeHost).c_str());
    esp_task_wdt_reset();

    configTzTime("WET0WEST,M3.5.0/1,M10.5.0", "pool.ntp.org", "time.google.com");  // log timestamps
    autoConfigureAPN();
    server.onNotFound(handleNotFound);
    server.begin();
    setupOTA();
  } else {
    Serial.println("[WiFi] Failed to connect to any network, entering AP mode");
    addLog("WiFi", "Falha na conexao, entrando em modo AP");
    setupAPMode();
  }
}

// --- Main Setup ---
// The core already initialises the task watchdog with a 5 s timeout, so esp_task_wdt_init() is a no-op
// here: the timeout has to be changed with esp_task_wdt_reconfigure(). Without this, setup() (WiFi,
// DNS, modem AT commands) trips the watchdog and the gateway reboots in a loop.
static void initWatchdog() {
#if ESP_IDF_VERSION_MAJOR >= 5 && ESP_IDF_VERSION_MINOR >= 1
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = 60 * 1000;
  cfg.idle_core_mask = 0;
  cfg.trigger_panic = true;
  esp_task_wdt_reconfigure(&cfg);
#else
  esp_task_wdt_init(60, true);
#endif
  esp_task_wdt_add(NULL);  // ignored error if already subscribed
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  initWatchdog();
  Serial.printf("\n\n=== SIM800L SMS Gateway %s ===\n\n", FIRMWARE_VERSION);

  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, HIGH);

  if (!SPIFFS.begin(true)) {
    Serial.println("[FATAL] SPIFFS initialization failed");
    return;
  }

  loadConfig();
  initOutbox();
  esp_task_wdt_reset();
  setupWiFiMode();
  esp_task_wdt_reset();
  sim800l_init();
  esp_task_wdt_reset();

  server.onNotFound(handleNotFound);
  server.begin();
}

// --- Main Loop ---
void loop() {
  esp_task_wdt_reset();
  ArduinoOTA.handle();
  checkSMS();
  server.handleClient();

  // Check signal strength every 10 seconds
  if (millis() - last_signal_check > 10000) {
    signal_strength = getSignalStrength();
    last_signal_check = millis();
    Serial.printf("[Signal] Strength: %d%%\n", signal_strength);
  }

  if (WiFi.isConnected()) {
    if (!config.api_key[0]) {
      // Not paired yet: kick off pairing, then poll until claimed.
      if (!pairing_active && millis() - last_pairing_action > PAIRING_RETRY_INTERVAL_MS) {
        startPairing();
      } else if (pairing_active && millis() - last_pairing_action > PAIRING_POLL_INTERVAL_MS) {
        pollPairing();
      }
    } else if (millis() - last_backend_poll > BACKEND_POLL_INTERVAL_MS) {
      pollPendingMessages();
      last_backend_poll = millis();
    }
  }

  if (outbox_count > 0 && millis() - last_flush_attempt > OUTBOX_RETRY_MS) {
    last_flush_attempt = millis();
    flushOutbox();
  }

  delay(10);
}
