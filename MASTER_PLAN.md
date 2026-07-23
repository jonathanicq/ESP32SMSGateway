# Master Plan: Android SMS Gateway

**Project:** Android SMS Gateway
**Created:** 2026-06-24
**Last Updated:** 2026-06-25
**Current Phase:** Phase 5 — Hardening, Licensing & Release (in progress); Phases 1–4 functional on test

---

## Project Overview

**Goal:** A native Android SMS gateway (send/receive SMS via HTTP API) with a private server + web backoffice, plus an optional premium WhatsApp bridge (and a planned Telegram channel).

**Success Criteria:**
- [x] Free tier: send/receive SMS via API with webhooks and status tracking. *(inbound verified live on device; outbound queue→device path verified, real outbound send to a live number still to be observed)*
- [x] Backoffice: authenticated dashboard listing/searching all messages and devices.
- [~] Premium tier: inbound SMS → WhatsApp + inbound WhatsApp → SMS via keyword grammar. *(code complete + per-user WAHA server config; live WAHA e2e pending. Telegram channel still to build — issues #1–#3.)*
- [x] Deployed to `test.sms-gateway.thecoordinates.app`.
- [ ] Production via GitHub Actions. *(CI/CD workflow not yet created.)*

---

## Phases Overview

| Phase | Name | Status | Dependencies | Completion |
|-------|------|--------|--------------|------------|
| -1 | Infrastructure | 🔄 Mostly done (test only) | None | 2026-06-24 |
| 0 | Planning & Setup | 🔄 Code/repo done; CI pending | Phase -1 | 2026-06-24 |
| 1 | Backend core + Auth + Backoffice API | ✅ Complete | Phase 0 | 2026-06-24 |
| 2 | Android gateway app (SMS ⇄ API) | ✅ Device-verified (inbound live) | Phase 1 | 2026-06-25 |
| 3 | Web backoffice (message viewer/mgmt) | ✅ Complete + enhanced | Phase 1 | 2026-06-25 |
| 4 | Premium WhatsApp bridge | 🔄 Built + BYO-WAHA config; live e2e pending | Phases 1–3 | 2026-06-25 |
| 5 | Hardening, billing/licensing, release | 🔄 In Progress | Phase 4 | TBD |
| 6 | Telegram channel (forward + transcode) | ⬜ Not Started (issues #1–#3) | Phase 4 | TBD |

**Status Legend:** ⬜ Not Started · 🔄 In Progress · ✅ Complete · ~ Partial · ⏸️ Blocked

---

## Phase -1: Infrastructure

**Status:** 🔄 Test environment provisioned; production infra deferred.

**Deliverables:**
- [x] DNS: `test.sms-gateway.thecoordinates.app` (resolves via public IP 2.81.52.147 → on-prem 192.168.0.116).
- [ ] DNS: `sms-gateway.thecoordinates.app` → 69.169.103.120 (VPS) — **not created yet**.
- [x] GitHub repo `jonathanicq/androidSmsGateway` + labels (standard + Evol/Next Sprint/RoadMap).
- [ ] automationLinkTree card added + deployed — **unconfirmed**.
- [x] Docker host directories prepared (`192.168.0.116:/opt/docker/sms-gateway`).
- [ ] VPS directories prepared — **deferred to Phase 5**.

---

## Phase 0: Planning & Setup

**Status:** 🔄 Foundation done; CI pipeline still missing.

**Deliverables:**
- [x] PROJECT_CONFIG.md created and locked.
- [x] MASTER_PLAN.md created (this file).
- [x] CHANGELOG.md maintained.
- [x] Repository initialized (git) with `dev` + `master` branches.
- [ ] CI/CD pipeline configured (`.github/workflows/deploy-production.yml`) — **NOT created** (deploys are currently manual via `scripts/deploy-test.sh`).
- [x] Project structure (`backend/`, `webfront/`, `android/`) created.

---

## Phase 1: Backend Core + Auth + Backoffice API

**Status:** ✅ Complete (2026-06-24) — deployed to test on port 8050.

**Deliverables:**
- [x] Auth: register/login/refresh/me; master account seeded; impersonation (1h JWT, audited).
- [x] Models: User, Device, Message (status lifecycle), Webhook, BridgeSettings, PairingRequest.
- [x] API: enqueue send (target + source device), list/query (filters), device-facing poll/status/inbound, webhook dispatch, device registration.
- [x] Device pairing flow: claim-code start/poll → device receives API key automatically.
- [x] Device management: create, list, **rename (PATCH)**, **delete (cascades messages + clears bridge ref)**.
- [x] `GET /api/health` (db check); best-effort execution logging to makebdd.execution_logs.
- [x] Dockerized (backend + MySQL); deployed to 192.168.0.116:8050; vhost `/api/` → 8050.

**Acceptance:** ✅ `/api/health` + master login verified through public HTTPS edge; ✅ device auth, rename, delete verified live against test (2026-06-25).

**Notes:** EXEC_LOG_DB_URL not set (exec logging disabled until creds provided). Schema is created via `Base.metadata.create_all` — **no migration tool**; column additions require manual `ALTER TABLE` (see Technical Debt).

---

## Phase 2: Android Gateway App (SMS ⇄ API)

**Status:** ✅ Device-verified (2026-06-25). Inbound proven end-to-end on a real phone.

**Deliverables:**
- [x] SMS send (multipart) + permissions + foreground service (`GatewayService`).
- [x] Inbound SMS receiver → POST inbound to server (`SmsReceiver`).
- [x] Private-server mode: device API-key auth; poll pending; config UI.
- [x] Status tracking (sent/delivered/failed) via PendingIntent result receivers.
- [x] Pairing UI (claim code) — paired against test backoffice on a real device.
- [x] Release signing (v1+v2+v3, minSdk 21) + real launcher icon (fixes "App not installed").
- [x] Install tooling: `scripts/install-apk.sh` (USB/ADB one-command install) + `scripts/udev/51-android.rules`.
- [ ] Local mode (on-device Ktor HTTP server) — deferred (not needed for backoffice flow).
- [ ] APK distribution to NAS + DreamHost FTP — deferred to Phase 5.

**Acceptance:**
- ✅ Real APK installed on Redmi `23053RN02Y` (Android 15) and launched cleanly (v0.1.3).
- ✅ **Inbound SMS verified live**: real SMS → appears in backoffice Messages (2026-06-25).
- ⏳ **Outbound live send**: queue→device pickup proven (polls 200, device fetches pending), but an actual outbound SMS to a live number has **not yet been observed** (test outbound was cancelled). To verify next.

**Build:** AGP 8.7.2 / Kotlin 2.0.21 / Gradle 8.11.1, compileSdk 34 / minSdk 21, shared SDK at `/home/sysadmin/android-sdk`. Release-signed APKs in `releases/android/` (latest, v0.1.2, v0.1.3).

**Known gotchas (documented):** MIUI/HyperOS blocks sideloaded SMS apps — needs "Install via USB" + "USB debugging (Security settings)"; host needs udev rules for ADB; stale device API key → silent 401 (re-pair fixes).

---

## Phase 3: Web Backoffice

**Status:** ✅ Complete (2026-06-24) + enhanced (2026-06-25) — deployed to test on port 3050.

**Deliverables:**
- [x] Auth UI (reusable PasswordInput with show/hide); JWT in localStorage; protected routes.
- [x] Messages table: search + filter by device/status/direction; send form.
- [x] **Auto-refresh** on Messages: interval selector (Off/30s/1m/5m/10m), manual refresh, "last updated" stamp, persisted in localStorage.
- [x] Device management: create/list/**rename (inline)**/**delete (now works — was failing on FK)**; reveal API key; pairing form.
- [x] Webhook management.
- [x] WhatsApp Bridge config page incl. **bring-your-own WAHA server** (URL + API key + session).
- [x] Non-dismissable impersonation banner (decodes `impersonatedBy` claim).

**Acceptance:** ✅ Backoffice loads through public HTTPS; authed round-trips 200; rename/delete/auto-refresh/WAHA-config verified live (2026-06-25).

**Note:** confirm field on password-setting forms still needed when a registration/reset UI is added (currently login-only).

---

## Phase 4: Premium WhatsApp Bridge

**Status:** 🔄 Built + deployed; per-user WAHA server config added (2026-06-25); live WAHA e2e pending.

**Decision:** uses **WAHA** (self-hosted) instead of the official WhatsApp Cloud API — cheaper, no Meta approval. See [[DECISIONS]].

**Deliverables:**
- [x] WAHA client (`send_whatsapp`) + webhook receiver (shared-token auth).
- [x] **Bring-your-own WAHA server per bridge**: `waha_base_url` / `waha_api_key` / `waha_session` on `BridgeSettings`; `resolve_waha()` overrides env defaults; `/whatsapp/status` reports effective vs env config.
- [x] SMS→WhatsApp forwarding (per-user `forward_to_chat`) in `record_inbound`.
- [x] WhatsApp→SMS parser: `@sms <E164> <body>` (configurable trigger); queues SMS on the bridge's device; replies ack.
- [x] Entitlement gating (`tier: premium` / SUPER_ADMIN); free tier → 402.
- [x] Backoffice WhatsApp config page (premium) incl. WAHA server fields.

**Acceptance:** ✅ Per-bridge WAHA config persists + status flips configured (verified live 2026-06-25). ⏳ **Live e2e blocked** on a real WAHA session wired to this backend (point WAHA webhook → `/api/whatsapp/webhook?token=...`, session name must match `waha_session`) + a physical SIM device.

**Open hardening:** WAHA inbound webhook is secured by a single shared `whatsapp_webhook_token` and routed by session name — consider a per-bridge webhook secret (deferred).

---

## Phase 5: Hardening, Licensing & Release

**Status:** 🔄 In Progress.

**Deliverables:**
- [ ] **CI/CD**: `.github/workflows/deploy-production.yml` from template; `VPS_PASSWORD` secret; deploy to `/opt/apps/androidSmsGateway/`.
- [ ] Production DNS + VPS provisioning (`sms-gateway.thecoordinates.app` → 69.169.103.120).
- [ ] **Automated test suite** (currently none — see Technical Debt): backend pytest (auth, devices CRUD+cascade, messages, bridge resolve, webhook token), frontend component tests.
- [ ] Rate limiting + encryption at rest (device API keys / WAHA keys).
- [ ] License key issuance + validation; billing hook.
- [ ] APK release (latest + versioned) → git `releases/`, NAS, DreamHost FTP; automationLinkTree links.
- [ ] Re-pair should **rebind the existing device** instead of creating a new row (root cause of the stale-key 401 bug). *(needs ticket)*
- [ ] Monitoring wired (monitor.thecoordinates.app).

**Dependencies:** Phase 4.

---

## Phase 6: Telegram Channel (planned)

**Status:** ⬜ Not Started — tracked in GitHub issues.

- [ ] **#3** Working session: collect Telegram bot token + chat id (blocks #1).
- [ ] **#1** Forward inbound SMS → Telegram (mirror of the WhatsApp forward path).
- [ ] **#2** Telegram → SMS transcoding (keyword bridge, reverse of #1).

Decisions pending: premium-gated vs free; webhook vs long-poll; shared vs separate trigger keyword.

---

## Testing Status

> ⚠️ **No automated test suite exists yet.** All verification to date is manual / live smoke testing against the running test service. Building a real test suite is a Phase 5 deliverable and current top technical debt (violates the "tests mandatory" standard).

**Verified live (manual, 2026-06-25):**
- ✅ `/api/health` + master login via public HTTPS.
- ✅ Device pairing, API-key auth (401→200 after re-pair), rename (PATCH 200), delete (DELETE 204, FK cascade).
- ✅ **Inbound SMS end-to-end**: real phone → backoffice Messages.
- ✅ Backoffice: auto-refresh, device rename/delete, WhatsApp bridge BYO-WAHA save + status flip.
- ✅ APK install + launch on real device.

**Not yet verified / needs coverage:**
- ⏳ Outbound SMS actually leaving the device to a live number.
- ⏳ WhatsApp bridge live e2e (needs connected WAHA + SIM).
- ⏳ Webhook delivery to a real external endpoint.
- ❌ Automated regression tests (backend + frontend) — to be written.

---

## Overall Progress Tracking

### Completed Milestones
- [x] Phase 1 Complete (backend/API/auth)
- [x] Phase 3 Complete (backoffice)
- [x] Phase 2 Device-verified (inbound live)
- [ ] Phase -1 Complete (production infra pending)
- [ ] Phase 0 Complete (CI pending)
- [ ] Phase 4 Complete (live WAHA pending)
- [ ] Phase 5 Complete
- [ ] Phase 6 (Telegram) Complete

### Current Blockers
- Production deploy blocked: no CI/CD workflow + no production DNS/VPS yet.
- WhatsApp live e2e blocked on a connected WAHA session + SIM device.
- Telegram channel blocked on credentials (issue #3).

### Technical Debt
- **No automated tests** — manual smoke only. (top priority)
- **No CI/CD** — deploys are manual `scripts/deploy-test.sh`.
- **No DB migration tool** — `create_all` only; schema changes need manual `ALTER TABLE` (e.g. bridge WAHA columns added by hand on 2026-06-25).
- **Re-pair orphans the device row** → caused a stale-key 401 (inbound silently dropped). Needs rebind logic + ticket.
- Exec logging disabled (no `EXEC_LOG_DB_URL`).
- WAHA inbound webhook uses one shared token (not per-bridge).

---

## Critical Path
1. Phase -1 → 0 → 1 → 2 → 4 → 5; Phase 6 (Telegram) branches off after Phase 4.

**Parallel:** Phase 3 ran in parallel with Phase 2; test-suite work can start now independent of Phase 6.

---

## Change Log

| Date | Phase | Change Description | Reason |
|------|-------|-------------------|--------|
| 2026-06-24 | 0 | Initial master plan created | Project kickoff |
| 2026-06-24 | 1/3/4 | Backend, backoffice, WhatsApp bridge built + deployed to test | MVP |
| 2026-06-25 | 2 | APK installed on real device; inbound SMS verified end-to-end; install tooling + udev rules added | Device validation |
| 2026-06-25 | 3 | Messages auto-refresh; device rename; device delete FK fix | Backoffice UX/bugfix |
| 2026-06-25 | 4 | Per-user bring-your-own WAHA server (URL/key/session) | Use pre-existing WAHA |
| 2026-06-25 | 6 | Telegram channel scoped into issues #1–#3 | Roadmap |
| 2026-06-25 | — | Plan rewritten to reflect real status, missing steps, and tests | Accuracy |

---

## Notes
Reference implementation: https://github.com/capcom6/android-sms-gateway (Apache-2.0).
Differentiators: web backoffice + premium WhatsApp bridge (+ planned Telegram channel).
