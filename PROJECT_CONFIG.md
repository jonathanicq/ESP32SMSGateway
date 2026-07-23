# Project Configuration

**Project Name:** Android SMS Gateway
**Created:** 2026-06-24
**Last Updated:** 2026-06-24
**Status:** Planning

---

## Overview

**Project Description:**
A native Android app that turns a phone into an SMS gateway (send/receive SMS via HTTP API), paired with a private server + web backoffice to view and manage messages. Modeled on the open-source `capcom6/android-sms-gateway`, with two differentiators: (1) a web dashboard, and (2) an optional **premium WhatsApp bridge** that forwards inbound SMS to WhatsApp and transcodes inbound WhatsApp messages into outbound SMS via a keyword grammar.

**Project Goals:**
- Free tier: reliable SMS ⇄ HTTP API gateway with webhooks and message status.
- Web backoffice to view/search/manage all messages and devices.
- Premium tier: WhatsApp ⇄ SMS bridge (forward + keyword-driven transcoding).

---

## Technology Stack

### Backend (private server + backoffice API)
- **Language:** Python 3.12
- **Framework:** FastAPI
- **Version:** latest stable

### Frontend (backoffice)
- **Language:** TypeScript
- **Framework:** React + Tailwind CSS
- **Version:** latest stable (Vite)

### Mobile (gateway app)
- **Language:** Kotlin
- **Frameworks:** Ktor (on-device HTTP server), Room (local DB)
- **Target:** Android 5.0+ (API 21+)

### Database
- **Primary Database:** MySQL 8
- **Additional Datastores:** Redis (queues / rate limiting) — optional, add when needed

### Infrastructure & Deployment
- **Cloud Provider:** Self-hosted — Docker on 192.168.0.116 (test) + VPS 69.169.103.120 (prod)
- **Container Platform:** Docker / docker-compose
- **CI/CD:** GitHub Actions (production deploy via `deploy-production.yml`)
- **Hosting:** test.sms-gateway.thecoordinates.app (on-prem) / sms-gateway.thecoordinates.app (VPS)

### Third-Party Services & APIs
- WhatsApp Business Cloud API (Meta) — premium bridge (compliant, official)
- SMTP (smtp.dreamhost.com) — transactional email from do-not-reply@thecoordinates.app
- makebdd.execution_logs — execution logging

---

## Code Style & Standards

### Naming Conventions
- **Files:** snake_case (Python), kebab-case (frontend), PascalCase (Kotlin classes)
- **Variables/Functions:** snake_case (Python), camelCase (TS/Kotlin)
- **Classes:** PascalCase
- **Constants:** SCREAMING_SNAKE_CASE

### Formatting
- **Formatter:** Black + isort (Python), Prettier (frontend), ktlint (Kotlin)
- **Max Line Length:** 100
- **Indentation:** 4 spaces (Python), 2 spaces (frontend)

### Linting
- **Linter:** Ruff (Python), ESLint (frontend), ktlint (Kotlin)
- **Pre-commit Hooks:** Yes (pre-commit)

---

## Testing Strategy

### Test Types
- [x] Unit Tests
- [x] Integration Tests
- [x] End-to-End Tests
- [ ] Contract Tests
- [ ] Performance Tests

### Testing Framework
- **Framework:** Pytest (backend), Vitest + React Testing Library (frontend), JUnit/Espresso (Android)
- **Coverage Tool:** Coverage.py
- **Coverage Threshold:** 80%

### Test Commands
```bash
# Backend
pytest                          # all tests
pytest tests/unit               # unit only
pytest tests/integration        # integration only
pytest --cov=app --cov-report=term-missing

# Frontend
npm test
```

---

## Documentation Requirements

### Required Documentation
- [x] README.md with setup instructions
- [x] API documentation (OpenAPI via FastAPI /docs)
- [x] Architecture Decision Records (ADRs)
- [x] CHANGELOG.md
- [x] Inline code comments (where necessary)

### API Documentation
- **Tool:** OpenAPI (FastAPI auto-generated)
- **Location:** `/docs`

---

## Logging & Monitoring

### Logging
- **Framework:** Loguru (backend)
- **Format:** JSON
- **Log Levels:** DEBUG, INFO, WARN, ERROR
- **Sensitive Data Policy:** Never log SMS bodies, phone numbers, tokens, or PII at INFO+

### Monitoring
- **Uptime Monitoring:** monitor.thecoordinates.app
- **Health endpoint:** `GET /api/health` (no auth) → status/version/commit/checks/timestamp

---

## Security Standards

### Authentication & Authorization
- **Method:** JWT (login/register/refresh). HTTP Basic for device→server API (per reference compatibility).
- **Library/Service:** FastAPI security utils + custom

### Master Account (MANDATORY)
- **Email:** `admin@thecoordinates.xyz` (env var: `SUPER_ADMIN_EMAIL`)
- **Password:** `ChangeMe123!` (env var: `SUPER_ADMIN_PASSWORD`)
- **Role:** `SUPER_ADMIN`
- Seed only if no super admin exists — never overwrite.
- [ ] Master account seeded on startup
- [ ] `SUPER_ADMIN_EMAIL` / `SUPER_ADMIN_PASSWORD` in `.env.example`
- [ ] Startup warning logged if default password detected in production
- [ ] Impersonation: `POST /api/admin/impersonate/:userId` → 1h JWT with `impersonatedBy`, audited, non-dismissable banner

### Security Checklist
- [ ] All secrets in environment variables (never hardcoded)
- [ ] .env.example with placeholder values
- [ ] .env in .gitignore
- [ ] Input validation on all external inputs
- [ ] HTTPS enforced
- [ ] CORS scoped to sms-gateway.thecoordinates.app
- [ ] SQL injection prevention (parameterized queries / ORM)
- [ ] XSS prevention
- [ ] Rate limiting on send endpoints
- [ ] SMS content encrypted at rest (align with reference's E2E approach)

---

## Dependency Management

### Package Manager
- **Tool:** uv/pip (Python), npm (frontend), Gradle (Android)
- **Lockfile:** uv.lock / requirements.txt, package-lock.json, gradle.lockfile

### Dependency Policy
- Commit lockfiles. Monthly updates. Audit: `pip-audit`, `npm audit`.

---

## Git Workflow

### Branch Strategy
- **Main Branch:** `master` (production)
- **Development Branch:** `dev`
- **Feature Branches:** `feature/descriptive-name`
- **Fix Branches:** `fix/descriptive-name`

### Environment-Based Branch Selection (Docker)
| APP_ENV | Branch | Purpose |
|---------|--------|---------|
| `production` | `master` | Production releases |
| `development` | `dev` | Active development |

### Commit Standards
- Author: `Jonathan Lima <jonathan.lima@thecoordinates.xyz>`
- Commit on: new module, feature, version, bug fix, doc update.

### Repository
- **Type:** Private
- **Host:** GitHub
- **URL:** https://github.com/jonathanicq/androidSmsGateway

---

## Environment Variables

### Required Variables
```bash
# Environment Mode (controls Docker branch selection)
APP_ENV=development

# Database
DATABASE_URL=mysql+pymysql://user:password@db:3306/sms_gateway

# Auth
JWT_SECRET=your_secret_here
SUPER_ADMIN_EMAIL=admin@thecoordinates.xyz
SUPER_ADMIN_PASSWORD=ChangeMe123!

# Email
SMTP_HOST=smtp.dreamhost.com
SMTP_PORT=587
SMTP_PASSWORD=__from_shared_config__

# WhatsApp (premium)
WHATSAPP_CLOUD_API_TOKEN=
WHATSAPP_PHONE_NUMBER_ID=
WHATSAPP_VERIFY_TOKEN=
WHATSAPP_TRIGGER_KEYWORD=@sms     # WhatsApp→SMS transcoding trigger
```

---

## Build & Deployment

### Build Commands
```bash
# Backend
uv sync                          # install deps
uvicorn app.main:app --reload    # dev server

# Frontend
npm install && npm run dev       # dev
npm run build                    # production build

# Android
./gradlew assembleRelease        # build APK
```

### Deployment Strategy
- **Default target:** test (`test.sms-gateway.thecoordinates.app`)
- **Production:** GitHub Actions only (`deploy-production.yml`), never manual SSH.

---

## Performance Considerations
- Index `messages` by (device_id, status, created_at) for backoffice queries.
- Outbound send queue with retry/backoff; per-device rate limiting.
- WhatsApp webhook handling must be idempotent (Meta retries).

---

## Additional Notes

### Product Tiers
- **Free:** SMS ⇄ API, webhooks, status, backoffice viewing.
- **Premium:** WhatsApp bridge — (a) forward inbound SMS → WhatsApp; (b) transcode inbound WhatsApp → SMS.

### WhatsApp→SMS Keyword Grammar (default)
```
@sms <E164_recipient> <message body...>
```
- Trigger keyword configurable via `WHATSAPP_TRIGGER_KEYWORD` (default `@sms`).
- First token after trigger = recipient (E.164, e.g. +447700900123).
- Remainder of the message = SMS body.
- Invalid/unparseable messages get a WhatsApp reply explaining the format.

### Premium Entitlement
- License key issued per account; backend validates and stamps JWT with `tier: premium`.
- WhatsApp bridge routes/features gated by `tier == premium`. Free tier returns 402/feature-locked.

---

## Decision Lock
**These decisions are locked for this project.** Any changes require explicit approval and an ADR.

**Locked on:** 2026-06-24
**Approved by:** Jonathan Lima
