# Phase 0: Planning & Setup

**Phase Directory:** `phases/phase-0-planning/`
**Status:** In Progress
**Started:** 2026-06-24
**Completed:** N/A

---

## Phase Objective

Establish the project foundation: lock PROJECT_CONFIG.md, create the repository and base structure, and wire CI/CD — so Phase 1 can start coding against settled decisions.

**This phase is complete when:** the repo is initialized with `dev`/`master` branches, the base `backend/`, `webfront/`, `android/` structure exists, CI runs on a test commit, and all decisions are recorded and locked in PROJECT_CONFIG.md.

---

## Context

**What was completed before this phase:**
- Phase -1 infra (DNS, GitHub repo, automationLinkTree) — in progress.
- Product scope and stack decisions resolved (see PROJECT_CONFIG.md and Obsidian DECISIONS.md).

**What depends on this phase:**
- Phase 1 (backend core), Phase 2 (Android app), Phase 3 (backoffice), Phase 4 (WhatsApp bridge).

**Related Documentation:**
- PROJECT_CONFIG.md (all sections — locked 2026-06-24)
- MASTER_PLAN.md (Phases overview)
- Obsidian: `01-projects/android-sms-gateway/` CONTEXT/DECISIONS

---

## Requirements

### Functional Requirements
1. Repository initialized with branch strategy (`master`, `dev`).
2. Monorepo structure: `backend/` (FastAPI), `webfront/` (React+TS+Tailwind), `android/` (Kotlin).
3. CI/CD scaffold (lint + test on PR; `deploy-production.yml` for VPS).

### Non-Functional Requirements
- **Security:** `.env.example` only; no secrets committed; `.env` gitignored.
- **Maintainability:** pre-commit hooks (Ruff/Black, ESLint/Prettier, ktlint) configured.

---

## Deliverables

### Code/Repo Deliverables
- [ ] Git repo initialized, `dev` + `master` branches, `.gitignore`.
  - Location: repo root
- [ ] `backend/` FastAPI skeleton with `GET /api/health`.
- [ ] `webfront/` Vite + React + TS + Tailwind skeleton.
- [ ] `android/` empty Gradle project (Kotlin) placeholder.
- [ ] `docker-compose.yml` + `docker-entrypoint.sh` (APP_ENV branch logic).
- [ ] `.github/workflows/` lint+test + `deploy-production.yml`.
- [ ] `.env.example` with all required vars.

### Documentation Deliverables
- [x] PROJECT_CONFIG.md (locked)
- [x] MASTER_PLAN.md
- [x] CHANGELOG.md
- [ ] README.md with setup instructions
- [ ] docs/adr/ADR-001 (WhatsApp integration method)

---

## Technical Approach

### Architecture
Monorepo with three deployables. Backend is the source of truth (MySQL); Android app and backoffice are clients. WhatsApp bridge is a backend module gated by entitlement.

### Data Flow
```
API client / Backoffice → Backend (FastAPI) → Android device (gateway) → SMS
Inbound SMS → Android → Backend → webhook / WhatsApp (premium)
WhatsApp inbound → Meta webhook → Backend parser → Android → SMS (premium)
```

### Technologies Used
- FastAPI, SQLAlchemy, MySQL, Alembic — backend.
- React, Vite, Tailwind — backoffice.
- Kotlin, Ktor, Room — Android.

---

## Implementation Tasks

See `CHECKLIST.md`.

**High-level steps:**
1. Init repo + branches + .gitignore + README.
2. Scaffold backend/webfront/android skeletons.
3. Add docker-compose + entrypoint + CI workflows.
4. Add .env.example; configure pre-commit.

---

## Acceptance Criteria

1. **Functionality:** repo clones and `docker-compose up` starts backend; `/api/health` returns 200.
2. **Code Quality:** linters pass; no secrets committed.
3. **Documentation:** README + locked PROJECT_CONFIG present.
4. **Security:** `.env` gitignored; `.env.example` complete.

---

## Risks & Mitigation

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| WhatsApp Cloud API approval delay | Med | High | Start Meta app review during Phase 1; bridge is Phase 4 |
| Android SMS permission/policy limits | Med | Med | Foreground service + clear user consent; test on real device early |

---

## Environment Variables

See PROJECT_CONFIG.md § Environment Variables. Mirror into `.env.example`.

---

## Sign-off

**Phase completed by:** [Name/Date]
**Verified by:** [Name/Date]
**Final status:** ⬜ In Progress
