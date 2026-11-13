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

Everything is in [`deploy/`](deploy/): `setup.sh` (one-command install), `Caddyfile` (HTTPS), `ton618-registry.service` (systemd), `backup.sh` + `ton618-backup.{service,timer}` (daily backup), `update.sh`, `duckdns.sh`.

1. **Create the VM**: Oracle Cloud → Compute → Create instance → image *Ubuntu 22.04/24.04*, shape *VM.Standard.E2.1.Micro* (AMD) or *VM.Standard.A1.Flex* (Arm), keep the SSH key it gives you, note the **public IP**.
2. **Domain**: a free name on duckdns.org, set to the public IP (an A record at your registrar works too).
3. **Open ports 80 and 443 in Oracle's network** (the script handles the VM's own firewall, not this): *Networking → Virtual Cloud Networks → your VCN → your subnet → Security List → Add Ingress Rules*: source `0.0.0.0/0`, protocol TCP, destination port `80`, then again for `443`.
4. **Install** (SSH into the VM):
   ```bash
   ssh ubuntu@PUBLIC_IP
   sudo apt-get update && sudo apt-get install -y git
   git clone https://github.com/kurodaki-dev/ton618 && cd ton618/server/deploy
   sudo bash setup.sh yourname.duckdns.org kurodaki-dev
   ```
   It installs Python, Caddy and the firewall rules, creates the `ton618` user, starts the service, enables the daily backup and loads `requests` + `stringutils`. The seed account's password (if one was created) is in `/opt/ton618-registry/seed.log`.
5. **Use it** from your computer: `ton618 register --server https://yourname.duckdns.org --username you`, then `ton618 publish`. The first account created on a server is its admin.
6. **Update later**: `sudo bash /opt/ton618-registry/app/server/deploy/update.sh`.

Useful: `systemctl status ton618-registry`, `journalctl -u ton618-registry -f`, `systemctl list-timers | grep ton618`.

To make this server the default for everybody, set `TON618_PACKAGE_REGISTRY_URL` to `https://yourname.duckdns.org/registry` in `include/Version.hpp` and release.

## Backups

The database is the single file `registry.db`. A consistent copy while the server runs:

```bash
sudo -u ton618 /usr/local/bin/ton618-backup
```

`setup.sh` already runs it every day at 03:30 (last 14 kept in `/opt/ton618-registry/backups/`). Copy one off the VM now and then (`scp`).

## Rules the server enforces

- Passwords: scrypt hashes. API keys: only their SHA-256 is stored; a key is shown once, when created.
- 10 failed/new-account attempts per IP per 15 minutes; set `TON618_REGISTRATION=closed` in the service to stop new sign-ups.
- A package belongs to whoever publishes its first version; only the owner (or an admin) adds versions or yanks one.
- Published versions never change. Names, versions, file types and sizes are checked like the static registry's (`scripts/registry.py`).
- Modules run with the full power of ton618: for a public registry, read what gets published (admins can `POST /api/yank/<name>/<version>`).

## API

`GET /registry/index.json`, `GET /registry/packages/<name>-<version>.tonpkg`, `POST /api/register`, `POST /api/login`, `GET /api/whoami`, `GET|POST /api/keys`, `DELETE /api/keys/<id>`, `POST /api/publish` (body: the `.tonpkg`), `POST /api/yank/<name>/<version>`, `GET /healthz`. Keys go in `Authorization: Bearer <key>`.
