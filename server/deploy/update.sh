#!/usr/bin/env bash
# Updates the server code to the latest version (the database is untouched).
#   sudo bash /opt/ton618-registry/app/server/deploy/update.sh
set -euo pipefail
sudo -u ton618 git -C /opt/ton618-registry/app pull --ff-only
systemctl restart ton618-registry
sleep 1
curl -fsS http://127.0.0.1:8080/healthz && echo " updated"
