# ESP32 SIM800L Gateway — Changelog

All notable changes to the ESP32 firmware (`esp/`) are documented here.
Format based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [1.3.1]

### Fixed
- Received SMS were forwarded to the backend minutes late when the Wi-Fi link dropped: the outbox
  only drains while Wi-Fi is up, and recovery depended on the core's slow auto-reconnect. Now, in
  station mode, a link that stays down for 30 s is re-established by walking the priority list
  again (at most once a minute).
- Every Wi-Fi disconnect is logged with its reason code, so drops can be diagnosed.
- Wi-Fi modem power-save is turned off after connecting; it caused latency spikes and DNS timeouts.
- Modem UART RX buffer raised from 256 B to 2 KB, so an SMS arriving while loop() is busy in a slow
  HTTPS call is not truncated.

## [1.3.0]

### Added
- Several Wi-Fi networks (up to 5) tried in priority order; the first that connects wins, and the
  gateway only opens its own AP if all of them fail. One scan first sizes the timeout (about 10 s for
  a network seen in the scan, about 4 s for one not seen); none is skipped, since a scan can miss
  hidden SSIDs. Configs saved by older firmware migrate to a single-network list on load.
- Portal Wi-Fi tab: list with move up/down and remove, add from scan or manually. Passwords stay
  masked; a masked password sent back keeps the stored one for that SSID.
- `GET /api/gsm/status` (read-only, same Basic auth): raw and parsed CSQ, registration (CREG/CGREG),
  operator, modem supply voltage, last reset reason (`brownout` = supply dip), uptime, Wi-Fi RSSI and
  free heap. Tells a bad reading from no registration from a power problem.

### Fixed
- DNS: the DHCP-provided resolvers are now used as handed out. The previous forced override to
  8.8.8.8 / 1.1.1.1 broke name resolution (NTP, backend) on networks that block outbound DNS to
  external servers. The DNS probe now resolves the backend host instead of google.com.
- Zero-signal warning now also mentions power: a supply or USB cable under 2 A gives no GSM signal.

### Changed
- Network scan names in the portal are inserted as text, not HTML.

## [1.2.0]

### Security
- Backend TLS verifies the server certificate (ISRG Root X1); `setInsecure()` removed.
- Web portal requires HTTP Basic auth (user `admin`, existing AP password) on every route except the page itself.
- `GET /config` returns secrets masked (`********`); a masked value sent back means "keep the stored value".
- Phone numbers are masked in the system log (last 4 digits kept).

### Added
- Outbox: each received SMS is stored before delivery and removed only after the backend accepts it.
- Delivered-message log keeps only HMAC-SHA256 digests (per-device random key), never message text.
- Pending messages are delivered when the backend comes back online and every 10 s while pending.
- `GET /api/outbox/status` (counts only), `GET /api/backend/status`, `POST /api/backend/test`.
- NTP time (WET/WEST) for log timestamps.

### Fixed
- Two SMS arriving in the same loop no longer overwrite each other.
- Task watchdog set to 60 s with `esp_task_wdt_reconfigure` and fed during setup (5 s default caused boot loops).
- Backend HTTP calls have a 10 s timeout.

### Changed
- Web UI taken from the 1.1.2 device page (includes the backend test tab).

### Not changed (by decision)
- WiFi, OTA and AP passwords are unchanged.

## [1.1.0]

### Added
- **Full backend pairing handshake** with `sms-gateway.thecoordinates.app`: the device calls
  `POST /pair/start`, shows the resulting claim code on the local portal (API tab) and serial
  log, then polls `POST /pair/poll` until the user links it from the backoffice's Devices page —
  same flow the Android app uses. Once linked it shows up as a normal paired device.
- **Real backend message sync**: received SMS are forwarded via `POST /device/messages/inbound`
  (which the backend fans out to webhooks and the account's configured Telegram/WhatsApp bridge);
  outbound SMS queued from the backoffice/Telegram are pulled via `GET /device/messages/pending`
  and reported back via `POST /device/messages/{id}/status`.
- **Self-healing pairing**: if the stored API key is rejected (401/403 — device deleted, DB
  reset, etc.), the firmware drops it and re-pairs automatically instead of polling forever with
  a dead key.
- **Manual phone number field** (GSM tab): fallback for SIMs whose carrier never provisioned the
  MSISDN (`AT+CNUM` returns empty — common on prepaid SIMs). Used for the "Numero" display and
  for naming the device (`ESP-<numero>`) when pairing.
- **"Teste" tab**: send a test SMS to any number directly from the portal
  (`POST /api/sms/test`), without needing a phone in hand.
- **Version banner** (`v1.1.0`) shown in the portal header (visible on every tab, since it's
  outside the tab-switched content) and in the serial boot banner.
- Antenna warning: signal bar shows a red "Verifique se ha antena conectada" alert whenever
  reported signal is 0%.
- SMS-received log entries now include the sender and the first 40 characters of the message
  body, not just "received from X".

### Fixed
- **WiFi DNS silently broken on some routers**: several networks hand out a DNS server (seen:
  OpenDNS) that refuses to resolve the backend's domain, which failed with no visible error
  besides a low-level `hostByName()` log line. Fixed by re-applying public DNS (8.8.8.8 / 1.1.1.1)
  right after DHCP hands out an address — `WiFi.config()` needs a real (non-zero) local IP to
  succeed on this Arduino core, so the fix reuses the DHCP-assigned IP/gateway/subnet rather than
  a hardcoded one, which keeps it working on any network.
- **`api_url` missing its `/api` suffix silently broke every backend call**: a URL without the
  suffix hits the webfront's SPA fallback (HTTP 200 + HTML body) instead of a 404, so every
  request "succeeded" while actually returning a web page that fails to parse as JSON — with
  zero error logged. `loadConfig()` now normalizes the stored URL (ensures `/api` suffix, migrates
  the old test/LAN-IP defaults to production) and the pending-message poll now logs raw HTTP
  status / unparseable-JSON bodies instead of failing silently.
- **Accented characters broke inbound SMS forwarding entirely**: SIM800L delivers Western
  accented characters (é, ã, ç, ñ...) as single Latin-1 bytes, which are invalid UTF-8 — embedding
  them in the JSON body sent to the backend made the whole request fail
  (`"There was an error parsing the body"`), not just garble the character. Fixed with a
  Latin-1→UTF-8 re-encode on receive.
- **Some messages still arrived as raw UCS2 hex**: the *network*, not our own charset config,
  decides per-message whether an SMS with many/mixed accents gets sent as UCS2 instead of a
  single byte per character — a manual GSM/UCS2 toggle wouldn't actually control this. Replaced
  with auto-detection: if the body looks like a valid hex string (multiple of 4 digits, min 8),
  decode it as UCS2 (with surrogate-pair support); otherwise treat it as Latin-1. No user-facing
  setting needed.
- **Signal strength could read over 100%**: the CSQ→percentage conversion had broken breakpoints
  that produced values up to 130% for common mid-to-good signal levels. Replaced with a simple
  linear `csq * 100 / 31` mapping, always clamped to 0–100.
- **Manual phone number required a reboot to show up**: the displayed "Numero" was resolved once
  at boot, before the user could have entered anything in the portal yet. `/signal` now falls back
  to the live config value on every request instead of a boot-time cache.
- ESP32 boot crash (`assert failed: xQueueSemaphoreTake`) traced to a stale `-DBOARD_HAS_PSRAM`
  build flag on a board without PSRAM; removed.
- Fragile hand-rolled `+CMT` sender parsing (comma-index based) replaced with quote-delimited
  parsing, and receive is now driven by TinyGSM instead of ad-hoc AT read loops.

### Changed
- **Removed the on-device Telegram bot** (local polling + `/sms` command handling): Telegram
  forwarding now happens server-side via the backend's existing per-account bridge
  (`BridgeSettings`), configured once in the backoffice instead of per-device — so changing which
  Telegram chat a device forwards to no longer requires reflashing.
- API Key field in the portal is now masked (`type="password"`), matching how the WiFi password
  field is already handled — it's a sensitive credential and shouldn't stay visible after saving.

## [1.0.1]

### Added
- Initial working firmware for the **TTGO T-Call V1.4** board (SIM800L on-board, IP5306 PMU):
  IP5306 boost-keep-on init over I2C, PWRKEY power-on sequence, correct UART pins (26/27) — the
  module previously looked completely dead (no LED, no AT reply) because the PMU cuts power to it
  without a battery unless told otherwise, which this project's earlier revisions never did (they
  assumed a generic ESP32 + loose SIM800L breakout).
- WiFi AP captive portal for initial setup (SSID `SIM800L-Gateway`) + STA mode for normal
  operation, with GSM status/config, WiFi scan, and a live log tab.
- Actual SMS transmission (`AT+CMGS`) — earlier revisions only stored the config field and never
  sent anything.
