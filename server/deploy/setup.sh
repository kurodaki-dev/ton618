#!/usr/bin/env bash
# Installs the TON618 registry on a fresh Ubuntu/Debian VPS (Oracle Cloud included).
#   sudo bash setup.sh your-domain.example [admin-username]
# Idempotent: run it again to repair or update. Needs the domain's DNS A record to
# already point at this VPS (Caddy gets the HTTPS certificate on first request).
set -euo pipefail

DOMAIN="${1:-}"
[ -n "$DOMAIN" ] || { echo "Usage: sudo bash setup.sh your-domain.example [seed-owner]"; exit 1; }
OWNER="${2:-kurodaki-dev}"
[ "$(id -u)" -eq 0 ] || { echo "Run as root (sudo)."; exit 1; }

BASE=/opt/ton618-registry
APP=$BASE/app
REPO=https://github.com/kurodaki-dev/ton618

echo "==> Packages"
export DEBIAN_FRONTEND=noninteractive
apt-get update -y
apt-get install -y python3 git curl ca-certificates gnupg debian-keyring debian-archive-keyring apt-transport-https iptables-persistent netfilter-persistent
if ! command -v caddy >/dev/null; then
  curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/gpg.key' | gpg --dearmor -o /usr/share/keyrings/caddy-stable-archive-keyring.gpg
  curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/debian.deb.txt' > /etc/apt/sources.list.d/caddy-stable.list
  apt-get update -y
  apt-get install -y caddy
fi

echo "==> Firewall on the VM (Oracle images block everything but SSH)"
for port in 80 443; do
  iptables -C INPUT -p tcp --dport "$port" -m state --state NEW -j ACCEPT 2>/dev/null ||
    iptables -I INPUT 5 -p tcp --dport "$port" -m state --state NEW -j ACCEPT
done
netfilter-persistent save

echo "==> Service user and code"
id ton618 >/dev/null 2>&1 || useradd --system --home "$BASE" --shell /usr/sbin/nologin ton618
mkdir -p "$BASE" "$BASE/backups"
if [ -d "$APP/.git" ]; then git -C "$APP" pull --ff-only; else git clone --depth 1 "$REPO" "$APP"; fi
chown -R ton618:ton618 "$BASE"

echo "==> systemd service and daily backup"
install -m 644 "$APP/server/deploy/ton618-registry.service" /etc/systemd/system/ton618-registry.service
install -m 644 "$APP/server/deploy/ton618-backup.service" /etc/systemd/system/ton618-backup.service
install -m 644 "$APP/server/deploy/ton618-backup.timer" /etc/systemd/system/ton618-backup.timer
install -m 755 "$APP/server/deploy/backup.sh" /usr/local/bin/ton618-backup
systemctl daemon-reload
systemctl enable --now ton618-registry ton618-backup.timer
systemctl restart ton618-registry

echo "==> Caddy (HTTPS)"
sed "s/YOUR_DOMAIN/$DOMAIN/g" "$APP/server/deploy/Caddyfile" > /etc/caddy/Caddyfile
systemctl reload caddy || systemctl restart caddy

echo "==> Seed the registry with requests and stringutils (first run only)"
if [ ! -s "$BASE/.seeded" ]; then
  sudo -u ton618 python3 "$APP/server/registry_server.py" import "$APP/registry/packages" --db "$BASE/registry.db" --owner "$OWNER" | tee "$BASE/seed.log"
  touch "$BASE/.seeded"; chown ton618:ton618 "$BASE/.seeded" "$BASE/seed.log"; chmod 600 "$BASE/seed.log"
  echo "    (the password of '$OWNER', if one was created, is in $BASE/seed.log)"
fi

sleep 2
echo "==> Check"
curl -fsS http://127.0.0.1:8080/healthz && echo
echo
echo "Done. Open https://$DOMAIN/ (the certificate can take a minute on the first visit)."
echo "On your computer:  ton618 register --server https://$DOMAIN --username YOU"
echo "Then make yourself admin if you aren't the first account:"
echo "  sudo -u ton618 python3 $APP/server/registry_server.py admin YOU --db $BASE/registry.db"
