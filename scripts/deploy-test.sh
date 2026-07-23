#!/usr/bin/env bash
# Deploy the current working tree to the TEST environment (192.168.0.116).
# Production deploys go through GitHub Actions only — never this script.
set -euo pipefail

VM="sysadmin@192.168.0.116"
DEST="/opt/docker/sms-gateway"
SRC="$(cd "$(dirname "$0")/.." && pwd)/"

# IMPORTANT: never delete the VM's .env (holds DB password / JWT secret).
# Without this exclude, `rsync --delete` wipes .env and the DB password
# stops matching the initialized MySQL volume -> backend 502.
rsync -az --delete \
  --exclude '.git' --exclude '.env' --exclude '*.db' \
  --exclude '__pycache__' --exclude 'node_modules' --exclude 'dist' --exclude '*.apk' \
  "$SRC" "$VM:$DEST/"

ssh "$VM" "cd $DEST && GIT_COMMIT=$(git rev-parse --short HEAD) docker compose up -d --build"
ssh "$VM" "sleep 5 && curl -s http://127.0.0.1:8050/api/health"
echo
echo "Deployed. Test: https://test.sms-gateway.thecoordinates.app"
