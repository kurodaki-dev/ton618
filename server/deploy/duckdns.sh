#!/usr/bin/env bash
# Optional: keeps a DuckDNS name pointing at this VPS (useful if the IP changes).
# Cron: */5 * * * * /usr/local/bin/duckdns.sh
SUBDOMAIN="yourname"            # without .duckdns.org
TOKEN="your-duckdns-token"
curl -fsS "https://www.duckdns.org/update?domains=$SUBDOMAIN&token=$TOKEN&ip=" >/dev/null
