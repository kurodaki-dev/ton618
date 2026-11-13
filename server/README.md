# TON618 registry server

Accounts, API keys and modules in **one SQLite file**. Python 3.8+ standard library only — nothing to `pip install`. Light enough for Oracle Cloud's free VPS (the 1 GB AMD micro or the Arm Ampere A1 both work).

## Try it locally

```bash
cd server
python3 registry_server.py import ../registry/packages --owner kurodaki-dev   # optional: seed requests + stringutils
python3 registry_server.py serve --db registry.db --port 8080
# in another terminal
ton618 register --server http://localhost:8080 --username me
ton618 publish
```

## Deploy on an Oracle Cloud VPS (Ubuntu)

1. **Get a domain.** A free `yourname.duckdns.org` pointing to the VPS public IP is enough.
2. **Open ports 80 and 443** in two places — Oracle blocks them by default:
   - Cloud console → *Networking → Virtual Cloud Network → your subnet → Security List → Add Ingress Rules* (source `0.0.0.0/0`, TCP, ports 80 and 443).
   - On the VM: `sudo iptables -I INPUT 6 -m state --state NEW -p tcp --dport 80 -j ACCEPT`, same with `443`, then `sudo netfilter-persistent save`.
3. **Install the server:**
   ```bash
   sudo apt update && sudo apt install -y python3 git caddy
   sudo useradd --system --home /opt/ton618-registry --create-home ton618
   sudo -u ton618 git clone https://github.com/kurodaki-dev/ton618 /opt/ton618-registry/app
   sudo cp /opt/ton618-registry/app/server/ton618-registry.service /etc/systemd/system/
   sudo systemctl enable --now ton618-registry
   ```
4. **HTTPS with Caddy** (certificates are automatic). Put this in `/etc/caddy/Caddyfile`, then `sudo systemctl reload caddy`:
   ```
   yourname.duckdns.org {
       reverse_proxy 127.0.0.1:8080
   }
   ```
5. **Seed it and make yourself admin** (first account created is admin automatically):
   ```bash
   cd /opt/ton618-registry/app/server
   sudo -u ton618 python3 registry_server.py import ../registry/packages --db /opt/ton618-registry/registry.db --owner kurodaki-dev
   ```
   The command prints that account's password once; use it with `ton618 login`, or register your own account first and give it admin with `registry_server.py admin <username> --db ...`.
6. **Point the clients at it:** `ton618 login --server https://yourname.duckdns.org`. The address is saved, so `install`, `update`, `search` and `publish` all use it. To make it the default for everybody, change `TON618_PACKAGE_REGISTRY_URL` in `include/Version.hpp` and bump the version.

## Backups

The database is the single file `registry.db`. A consistent copy while the server runs:

```bash
python3 registry_server.py backup /opt/ton618-registry/backup-$(date +%F).db --db /opt/ton618-registry/registry.db
```

Put it in a daily cron job and copy it off the VM now and then.

## Rules the server enforces

- Passwords: scrypt hashes. API keys: only their SHA-256 is stored; a key is shown once, when created.
- 10 failed/new-account attempts per IP per 15 minutes; set `TON618_REGISTRATION=closed` in the service to stop new sign-ups.
- A package belongs to whoever publishes its first version; only the owner (or an admin) adds versions or yanks one.
- Published versions never change. Names, versions, file types and sizes are checked like the static registry's (`scripts/registry.py`).
- Modules run with the full power of ton618: for a public registry, read what gets published (admins can `POST /api/yank/<name>/<version>`).

## API

`GET /registry/index.json`, `GET /registry/packages/<name>-<version>.tonpkg`, `POST /api/register`, `POST /api/login`, `GET /api/whoami`, `GET|POST /api/keys`, `DELETE /api/keys/<id>`, `POST /api/publish` (body: the `.tonpkg`), `POST /api/yank/<name>/<version>`, `GET /healthz`. Keys go in `Authorization: Bearer <key>`.
