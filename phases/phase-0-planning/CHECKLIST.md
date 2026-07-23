# Phase 0 Checklist: Planning & Setup

**Phase:** 0 — Planning & Setup
**Status:** In Progress
**Last Updated:** 2026-06-24

---

## Planning (Docs)
- [x] PROJECT_CONFIG.md created and locked
- [x] MASTER_PLAN.md created with phase roadmap
- [x] CHANGELOG.md created
- [x] phases/phase-0-planning/ with PROMPT.md + CHECKLIST.md
- [ ] README.md with setup instructions
- [ ] docs/adr/ADR-001 — WhatsApp integration method (Cloud API)

## Infrastructure (Phase -1 carry-over)
- [ ] DNS: test.sms-gateway.thecoordinates.app → 192.168.0.116
- [ ] DNS: sms-gateway.thecoordinates.app → 69.169.103.120
- [ ] GitHub repo jonathanicq/androidSmsGateway (private)
- [ ] 5 standard labels (Bug, Evol, Question, Next Sprint, RoadMap)
- [ ] automationLinkTree card + deploy

## Repository Setup
- [ ] git init; first commit (author: Jonathan Lima <jonathan.lima@thecoordinates.xyz>)
- [ ] Branches: master + dev
- [ ] .gitignore (Python, Node, Android, .env)
- [ ] .env.example with all required variables
- [ ] pre-commit config (Ruff/Black, ESLint/Prettier, ktlint)

## Project Structure
- [ ] backend/ FastAPI skeleton + GET /api/health
- [ ] webfront/ Vite + React + TS + Tailwind skeleton
- [ ] android/ Kotlin Gradle project placeholder
- [ ] docker-compose.yml (backend + MySQL)
- [ ] docker-entrypoint.sh with APP_ENV branch selection

## CI/CD
- [ ] .github/workflows/ci.yml (lint + test on PR)
- [ ] .github/workflows/deploy-production.yml (from CI_CD_DEPLOY_TEMPLATE.yml)
- [ ] VPS_PASSWORD secret configured

## Verification (done = verified end-to-end)
- [ ] `docker-compose up` starts backend; GET /api/health returns 200 with status/version/commit/checks
- [ ] Linters pass; pre-commit runs clean
- [ ] No secrets committed (.env gitignored)
- [ ] CI green on a test commit

---

## Sign-off
- [ ] Phase 0 acceptance criteria met
- [ ] Ready for Phase 1
