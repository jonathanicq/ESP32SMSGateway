# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added
- Phase 0 planning scaffold: PROJECT_CONFIG.md (locked), MASTER_PLAN.md, CHANGELOG.md.
- phases/phase-0-planning/ with PROMPT.md and CHECKLIST.md.
- Product definition: Free (SMS ⇄ API) + Premium (WhatsApp bridge) tiers.
- Resolved stack decisions: FastAPI backend, React+Tailwind backoffice, Kotlin/Ktor/Room Android app, MySQL, WhatsApp Business Cloud API.
- **Phase 1 backend**: FastAPI app — JWT auth (register/login/refresh/me), master account seed, SUPER_ADMIN impersonation (1h JWT, audited).
- Models User/Device/Message/Webhook; send/query messages with filters; device-facing endpoints (poll pending, status update, inbound); webhook dispatch.
- `GET /api/health` with DB check; best-effort execution logging to makebdd.execution_logs.
- Dockerfile + docker-compose (backend + MySQL); deployed to test on `192.168.0.116:8050` behind `test.sms-gateway.thecoordinates.app` (per-host LE cert; `/api/` → 8050, `/` → placeholder).
- **Phase 3 backoffice**: React + TypeScript + Tailwind app (`webfront/`) — login (reusable PasswordInput show/hide), messages table with search + device/status/direction filters + send form, device management (create/list/delete, reveal API key), webhook management, non-dismissable impersonation banner.
- Frontend nginx (SPA fallback) + Dockerfile; added `frontend` service to compose; deployed to test on `192.168.0.116:3050` (replaces placeholder; `/` → 3050).
- `scripts/deploy-test.sh` — rsync (excludes `.env`) + compose build/up to the test VM.
- `scripts/install-apk.sh` — one-command USB/ADB install of the APK onto a phone (auto-locates `adb`, checks device state, installs, verifies version, launches; MIUI-aware error hints).
- `scripts/udev/51-android.rules` — host udev rules (Xiaomi/Google/Samsung) so `adb` stops reporting "no permissions"; install once to make USB access permanent.
- README: "Install / update over USB (ADB)" section documenting one-time device + host setup, after which updates are a single `scripts/install-apk.sh`.
- **Phase 2 Android app**: native Kotlin gateway (`android/`) — foreground `GatewayService` (poll pending → SmsManager multipart send → sent/delivered/failed status), `SmsReceiver` (inbound → server), `MainActivity` config (server URL + device API key, permissions, start/stop). `ApiClient` over HttpURLConnection + org.json (no external deps).
- Android build: AGP 8.7.2 / Kotlin 2.0.21 / Gradle 8.11.1, compileSdk 34 / minSdk 24. Debug APK committed to `releases/android/` (latest + v0.1.0-debug). Runtime SMS not yet verified on hardware.
- **Phase 4 WhatsApp bridge (premium)**: `BridgeSettings` model; premium-gated `GET/PUT /api/whatsapp/bridge`; WAHA client + `@sms <E164> <body>` keyword parser; `POST /api/whatsapp/webhook` (shared-token) → enqueue transcoded SMS + ack; inbound SMS → WhatsApp forward in `record_inbound`; `/api/whatsapp/status`; backoffice WhatsApp config page. Free tier → 402.

- **Device pairing flow**: app generates a 6-char claim code (`POST /api/pair/start`), user enters it in the backoffice (`POST /api/devices/claim`), app polls (`POST /api/pair/poll`) and receives its API key automatically — no manual key copying. Backoffice "Pair a phone" box on Devices; Android pairing screen. APK bumped to v0.1.1.

- **Release signing**: dedicated keystore + Gradle release `signingConfig` (reads gitignored `keystore.properties`). APK v0.1.1 is now **release-signed (v2), non-debuggable** — fixes "App not installed" from debug-signed sideloads. Artifacts renamed `sms-gateway-v0.1.1.apk` (dropped `-debug`).

### Fixed
- **APK v0.1.3 — "App not installed" root cause**: the manifest set `android:icon="@android:drawable/sym_def_app_icon"`, a framework drawable (`0x01080093`) that no longer exists in modern Android SDKs. The package manager failed to resolve the app icon (`aapt`: *ERROR getting 'android:icon' attribute: attribute value reference does not exist*), so installation was rejected and no launchable activity resolved. Diagnosed by comparing badging against the working ChurchManagementApp APK (which ships a real `@mipmap/ic_launcher`). Fix: added a proper launcher icon — adaptive icon (`mipmap-anydpi-v26/ic_launcher{,_round}.xml`) over a blue background + white SMS-bubble vector foreground, with raster `ic_launcher{,_round}.png` fallbacks (mdpi–xxxhdpi) for pre-API-26 devices; manifest now uses `@mipmap/ic_launcher` + `@mipmap/ic_launcher_round`. Verified: badging error gone, `launchable-activity` resolves, release-signed (v1+v2+v3).

### Changed
- WhatsApp integration switched from official Cloud API to **WAHA** (self-hosted) — cheaper, no Meta approval.

---

## Version Numbering Guide

**MAJOR.MINOR.PATCH** (e.g., 1.2.3)
- **MAJOR:** Incompatible API or major functionality changes
- **MINOR:** Backwards-compatible new functionality
- **PATCH:** Backwards-compatible bug fixes

---

## Notes
- Each version gets a release date in YYYY-MM-DD format.
- Reference issues/PRs when applicable.
- Update this file with every significant commit.
