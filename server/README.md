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

## Deploy

Run it behind a reverse proxy that provides HTTPS (Caddy or nginx) with `registry_server.py serve --db registry.db --host 127.0.0.1 --port 8080 --behind-proxy`, under a service manager such as systemd, as a non-root user. Open ports 80 and 443.

## Backups

The database is the single file `registry.db`. A consistent copy while the server runs:

```bash
python3 registry_server.py backup backup.db --db registry.db
```

Run it daily (cron or a systemd timer) and copy a backup off the machine now and then.

## Rules the server enforces

- Passwords: scrypt hashes. API keys: only their SHA-256 is stored; a key is shown once, when created.
- 10 failed/new-account attempts per IP per 15 minutes; set `TON618_REGISTRATION=closed` in the service to stop new sign-ups.
- A package belongs to whoever publishes its first version; only the owner (or an admin) adds versions or yanks one.
- Published versions never change. Names, versions, file types and sizes are checked like the static registry's (`scripts/registry.py`).
- Modules run with the full power of ton618: for a public registry, read what gets published (admins can `POST /api/yank/<name>/<version>`).

## API

`GET /registry/index.json`, `GET /registry/packages/<name>-<version>.tonpkg`, `POST /api/register`, `POST /api/login`, `GET /api/whoami`, `GET|POST /api/keys`, `DELETE /api/keys/<id>`, `POST /api/publish` (body: the `.tonpkg`), `POST /api/yank/<name>/<version>`, `GET /healthz`. Keys go in `Authorization: Bearer <key>`.

## Sign in with GitHub (optional)

Accounts stay in SQLite; GitHub only proves who someone is. Create an OAuth App at github.com/settings/developers: callback URL `<public url>/api/oauth/github/callback`, tick *Enable Device Flow*. Then give the service its settings (never put them in the repository):

```
TON618_GITHUB_CLIENT_ID=...
TON618_GITHUB_CLIENT_SECRET=...
TON618_PUBLIC_URL=https://your-registry.example
TON618_SITE_URLS=https://your-docs-site.example     # where the website lives (comma separated)
```

A GitHub account gets its own account named after its login (never merged with a password account), with no password.
