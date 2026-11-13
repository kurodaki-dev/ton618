#!/usr/bin/env bash
# Consistent copy of the registry database, keeps the last 14.
set -euo pipefail
BASE=/opt/ton618-registry
OUT="$BASE/backups/registry-$(date +%F).db"
python3 "$BASE/app/server/registry_server.py" backup "$OUT" --db "$BASE/registry.db"
find "$BASE/backups" -name 'registry-*.db' -mtime +14 -delete
