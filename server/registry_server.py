#!/usr/bin/env python3
"""TON618 registry server: accounts, API keys and packages in one SQLite file.

Python 3.8+ standard library only (no pip install). Made to run on a small VPS
(for example Oracle Cloud's free tier) behind Caddy or nginx for HTTPS.

    python3 registry_server.py serve  --db registry.db --port 8080
    python3 registry_server.py import ../registry/packages --owner kurodaki-dev   # seed from .tonpkg files
    python3 registry_server.py admin  kurodaki-dev                                 # make a user admin
    python3 registry_server.py backup backup.db                                    # consistent copy of the database

What it serves (the same layout the ton618 client already reads):

    GET  /registry/index.json                  every package and version, with SHA-256
    GET  /registry/packages/<name>-<ver>.tonpkg
    GET  /api/modules/<name>                   {name, github, version}   (the original registry's answer)
    GET  /                                     home page: search, install and publish instructions

    POST /api/register  {username, password}   -> creates the account and returns a first API key
    POST /api/login     {username, password}   -> returns a new API key
    GET  /api/whoami                           (Authorization: Bearer <key>)
    GET  /api/keys  /  POST /api/keys {name}  /  DELETE /api/keys/<id>
    POST /api/publish   body = a .tonpkg       (Authorization: Bearer <key>)
    POST /api/yank/<name>/<version>            (owner or admin) hide a version from the index

Passwords are stored as scrypt hashes, API keys as SHA-256 hashes (the key is shown once).
A package belongs to whoever publishes its first version; only owners add versions.
Published versions never change. Every package is syntax-checked and size-limited.
"""
from __future__ import annotations

import argparse
import hashlib
import hmac
import html
import json
import os
import re
import secrets
import sqlite3
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlparse, parse_qs

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "scripts"))
import registry as rules  # noqa: E402  (the same checks as the static registry)

USER_RE = re.compile(r"^[a-z0-9][a-z0-9_-]{2,31}$")
MAX_BODY = 3 * 1024 * 1024
SCHEMA = """
CREATE TABLE IF NOT EXISTS users (
  id INTEGER PRIMARY KEY, username TEXT UNIQUE NOT NULL, salt BLOB NOT NULL, pwhash BLOB NOT NULL,
  is_admin INTEGER NOT NULL DEFAULT 0, created_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS api_keys (
  id INTEGER PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  name TEXT NOT NULL, key_hash TEXT UNIQUE NOT NULL, created_at INTEGER NOT NULL, last_used INTEGER);
CREATE TABLE IF NOT EXISTS packages (
  name TEXT PRIMARY KEY, owner_id INTEGER NOT NULL REFERENCES users(id), created_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS versions (
  name TEXT NOT NULL REFERENCES packages(name), version TEXT NOT NULL, sha256 TEXT NOT NULL,
  size INTEGER NOT NULL, data BLOB NOT NULL, yanked INTEGER NOT NULL DEFAULT 0,
  published_by INTEGER REFERENCES users(id), published_at INTEGER NOT NULL, PRIMARY KEY (name, version));
CREATE TABLE IF NOT EXISTS attempts (ip TEXT NOT NULL, at INTEGER NOT NULL);
"""


class Db:
    """One connection per thread; WAL so readers never block the writer."""

    def __init__(self, path: str):
        self.path, self.local = path, threading.local()
        with self.conn() as c:
            c.executescript(SCHEMA)

    def conn(self) -> sqlite3.Connection:
        c = getattr(self.local, "c", None)
        if c is None:
            c = sqlite3.connect(self.path, timeout=15, isolation_level=None)
            c.row_factory = sqlite3.Row
            c.execute("PRAGMA journal_mode=WAL")
            c.execute("PRAGMA foreign_keys=ON")
            self.local.c = c
        return c


def hash_password(password: str, salt: bytes) -> bytes:
    return hashlib.scrypt(password.encode("utf-8"), salt=salt, n=2 ** 14, r=8, p=1, dklen=32)


def key_hash(key: str) -> str:
    return hashlib.sha256(key.encode()).hexdigest()


class Shim:
    """Looks like registry.Package for the checks and the index builder."""

    def __init__(self, name: str, version: str, raw: bytes):
        self.path = Path(f"{name}-{version}.tonpkg")
        self.name, self.version, self.raw = name, version, raw
        self.sha256, self.size = hashlib.sha256(raw).hexdigest(), len(raw)
        self.file = f"packages/{self.path.name}"
        self.data = json.loads(raw.decode("utf-8"))


class Registry:
    def __init__(self, db: Db):
        self.db = db

    # ---- accounts --------------------------------------------------------
    def create_user(self, username: str, password: str) -> int:
        if not USER_RE.match(username):
            raise Fail(400, "The username must be 3-32 characters: lowercase letters, digits, '-' or '_'.")
        if len(password) < 8 or len(password) > 200:
            raise Fail(400, "The password must have at least 8 characters.")
        salt = secrets.token_bytes(16)
        c = self.db.conn()
        first = c.execute("SELECT COUNT(*) FROM users").fetchone()[0] == 0
        try:
            cur = c.execute("INSERT INTO users(username,salt,pwhash,is_admin,created_at) VALUES(?,?,?,?,?)",
                            (username, salt, hash_password(password, salt), 1 if first else 0, int(time.time())))
        except sqlite3.IntegrityError:
            raise Fail(409, "That username is taken.")
        return cur.lastrowid

    def check_password(self, username: str, password: str):
        row = self.db.conn().execute("SELECT * FROM users WHERE username=?", (username,)).fetchone()
        salt = row["salt"] if row else b"\0" * 16   # same work whether or not the user exists
        good = hmac.compare_digest(hash_password(password, salt), row["pwhash"] if row else b"\0" * 32)
        return row if row and good else None

    def new_key(self, user_id: int, name: str) -> str:
        key = "ton_" + secrets.token_urlsafe(32)
        self.db.conn().execute("INSERT INTO api_keys(user_id,name,key_hash,created_at) VALUES(?,?,?,?)",
                               (user_id, name[:60] or "key", key_hash(key), int(time.time())))
        return key

    def user_for_key(self, key: str):
        c = self.db.conn()
        row = c.execute("SELECT u.*, k.id AS key_id FROM api_keys k JOIN users u ON u.id=k.user_id WHERE k.key_hash=?",
                        (key_hash(key),)).fetchone()
        if row:
            c.execute("UPDATE api_keys SET last_used=? WHERE id=?", (int(time.time()), row["key_id"]))
        return row

    def too_many_attempts(self, ip: str) -> bool:
        c = self.db.conn()
        now = int(time.time())
        c.execute("DELETE FROM attempts WHERE at < ?", (now - 900,))
        return c.execute("SELECT COUNT(*) FROM attempts WHERE ip=?", (ip,)).fetchone()[0] >= 10

    def note_attempt(self, ip: str) -> None:
        self.db.conn().execute("INSERT INTO attempts(ip,at) VALUES(?,?)", (ip, int(time.time())))

    # ---- packages --------------------------------------------------------
    def publish(self, user, raw: bytes) -> Shim:
        if len(raw) > MAX_BODY:
            raise Fail(413, "The package is too large.")
        try:
            data = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, ValueError):
            raise Fail(400, "That isn't a .tonpkg file (build one with `ton618 build`).")
        name, version = str(data.get("name", "")) if isinstance(data, dict) else "", str(data.get("version", "")) if isinstance(data, dict) else ""
        if not rules.NAME_RE.match(name) or not rules.VERSION_RE.match(version):
            raise Fail(400, "The package has no valid name and version.")
        pkg = Shim(name, version, raw)
        c = self.db.conn()
        known = {r[0] for r in c.execute("SELECT name FROM packages")} | {name}
        errors = rules.check_package(pkg, known, None)
        if errors:
            raise Fail(422, "; ".join(e.split(": ", 1)[-1] for e in errors))
        c.execute("BEGIN IMMEDIATE")
        try:
            owner = c.execute("SELECT owner_id FROM packages WHERE name=?", (name,)).fetchone()
            if owner and owner["owner_id"] != user["id"] and not user["is_admin"]:
                raise Fail(403, f"'{name}' belongs to another account.")
            if c.execute("SELECT 1 FROM versions WHERE name=? AND version=?", (name, version)).fetchone():
                raise Fail(409, f"{name} {version} is already published, and published versions never change. Raise the version.")
            if not owner:
                c.execute("INSERT INTO packages(name,owner_id,created_at) VALUES(?,?,?)", (name, user["id"], int(time.time())))
            c.execute("INSERT INTO versions(name,version,sha256,size,data,published_by,published_at) VALUES(?,?,?,?,?,?,?)",
                      (name, version, pkg.sha256, pkg.size, raw, user["id"], int(time.time())))
            c.execute("COMMIT")
        except BaseException:
            c.execute("ROLLBACK")
            raise
        return pkg

    def yank(self, user, name: str, version: str) -> None:
        c = self.db.conn()
        owner = c.execute("SELECT owner_id FROM packages WHERE name=?", (name,)).fetchone()
        if not owner or not c.execute("SELECT 1 FROM versions WHERE name=? AND version=?", (name, version)).fetchone():
            raise Fail(404, "No such package version.")
        if owner["owner_id"] != user["id"] and not user["is_admin"]:
            raise Fail(403, "Only the owner (or an admin) can yank a version.")
        c.execute("UPDATE versions SET yanked=1 WHERE name=? AND version=?", (name, version))

    def index(self) -> dict:
        rows = self.db.conn().execute("SELECT v.name, v.version, v.data, v.yanked, u.username FROM versions v "
                                      "JOIN packages p ON p.name=v.name JOIN users u ON u.id=p.owner_id").fetchall()
        shims, owners = [], {}
        for r in rows:
            if r["yanked"]:
                continue
            shims.append(Shim(r["name"], r["version"], bytes(r["data"])))
            owners[r["name"]] = r["username"]
        index = rules.build_index(shims)
        for name, entry in index["packages"].items():
            entry["owner"] = owners.get(name, "")
        return index

    def version_file(self, name: str, version: str):
        row = self.db.conn().execute("SELECT data FROM versions WHERE name=? AND version=?", (name, version)).fetchone()
        return bytes(row["data"]) if row else None


class Fail(Exception):
    def __init__(self, status: int, message: str):
        super().__init__(message)
        self.status, self.message = status, message


HOME = """<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>TON618 packages</title><style>
:root{color-scheme:dark}body{margin:0;background:#0b0b0b;color:#e9e9e9;font:16px/1.65 system-ui,sans-serif}
main{max-width:860px;margin:0 auto;padding:48px 20px}h1{font:600 clamp(34px,7vw,56px)/1 ui-monospace,monospace;margin:0 0 14px;color:#fff}
h2{font-size:13px;letter-spacing:.09em;text-transform:uppercase;color:#9a9a9a;margin:44px 0 14px}a{color:#fff}
code,pre{font-family:ui-monospace,monospace}pre{background:#121212;border:1px solid #2a2a2a;border-radius:6px;padding:14px 16px;overflow-x:auto}
input{width:100%;max-width:420px;font:inherit;color:inherit;background:#0b0b0b;border:1px solid #2a2a2a;border-radius:4px;padding:9px 12px}
.pkg{padding:14px 2px;border-bottom:1px solid #2a2a2a}.pkg b{font:600 18px ui-monospace,monospace}.pkg small{color:#9a9a9a}.pkg p{margin:4px 0}
</style></head><body><main>
<h1>TON618 packages</h1><p>The module registry of the TON618 language. Anyone can install; publishing needs a free account.</p>
<h2>Install</h2><pre>ton618 install requests</pre>
<h2>Publish your own</h2>
<pre>ton618 init mymodule        # tonproject.toml + your code
ton618 register             # create an account (once) and save an API key
ton618 publish              # build and upload</pre>
<p>Every module runs with the full power of ton618: read what you install.</p>
<h2>Modules</h2><input id="q" placeholder="Search..." autocomplete="off"><div id="list"></div>
<script>
fetch("/registry/index.json").then(r=>r.json()).then(ix=>{
 const list=document.getElementById("list"),q=document.getElementById("q"),names=Object.keys(ix.packages).sort();
 function draw(){const t=q.value.toLowerCase();list.textContent="";names.forEach(n=>{const p=ix.packages[n];
  if(t&&!(n+" "+p.description+" "+(p.keywords||[]).join(" ")).toLowerCase().includes(t))return;
  const d=document.createElement("div");d.className="pkg";
  const b=document.createElement("b");b.textContent=n;const s=document.createElement("small");s.textContent=" "+p.latest+" · by "+(p.owner||p.author);
  const x=document.createElement("p");x.textContent=p.description;const c=document.createElement("code");c.textContent="ton618 install "+n;
  d.append(b,s,x,c);list.append(d);});if(!list.children.length)list.textContent="No module matches."}
 q.oninput=draw;draw();});
</script></main></body></html>"""


def make_handler(reg: Registry, trust_proxy: bool):
    class Handler(BaseHTTPRequestHandler):
        server_version = "ton618-registry"
        protocol_version = "HTTP/1.1"

        def log_message(self, fmt, *args):
            sys.stderr.write("%s %s\n" % (self.address_string(), fmt % args))

        # ---- helpers ----
        def ip(self) -> str:
            if trust_proxy and self.headers.get("X-Forwarded-For"):
                return self.headers["X-Forwarded-For"].split(",")[-1].strip()
            return self.client_address[0]

        def send(self, status: int, body, ctype="application/json", extra=None):
            if not isinstance(body, bytes):
                body = (json.dumps(body, ensure_ascii=False) if ctype == "application/json" else str(body)).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", ctype + ("; charset=utf-8" if ctype.startswith("text") or ctype == "application/json" else ""))
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("X-Content-Type-Options", "nosniff")
            for k, v in (extra or {}).items():
                self.send_header(k, v)
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(body)

        def read_body(self) -> bytes:
            n = int(self.headers.get("Content-Length") or 0)
            if n > MAX_BODY:
                raise Fail(413, "The request is too large.")
            return self.rfile.read(n)

        def json_body(self) -> dict:
            try:
                d = json.loads(self.read_body().decode("utf-8") or "{}")
            except (ValueError, UnicodeDecodeError):
                raise Fail(400, "The body must be JSON.")
            if not isinstance(d, dict):
                raise Fail(400, "The body must be a JSON object.")
            return d

        def user(self):
            auth = self.headers.get("Authorization", "")
            if auth.lower().startswith("bearer "):
                row = reg.user_for_key(auth[7:].strip())
                if row:
                    return row
            raise Fail(401, "Missing or invalid API key. Run: ton618 login")

        def guarded(self, fn):
            try:
                fn()
            except Fail as e:
                self.send(e.status, {"error": e.message})
            except Exception as e:  # noqa: BLE001
                sys.stderr.write(f"error: {e!r}\n")
                self.send(500, {"error": "Server error."})

        # ---- routes ----
        def do_OPTIONS(self):
            self.send(204, b"", extra={"Access-Control-Allow-Headers": "Authorization, Content-Type", "Access-Control-Allow-Methods": "GET, POST, DELETE"})

        def do_HEAD(self): self.do_GET()

        def do_GET(self):
            self.guarded(self.get)

        def get(self):
            path = unquote(urlparse(self.path).path)
            if path in ("/", "/index.html"):
                return self.send(200, HOME, "text/html")
            if path == "/registry/index.json":
                return self.send(200, reg.index(), extra={"Cache-Control": "no-cache"})
            m = re.fullmatch(r"/registry/packages/(" + rules.FILE_RE.pattern.strip("^$") + ")", path)
            if m:
                fm = rules.FILE_RE.match(m.group(1))
                raw = reg.version_file(fm.group("name"), fm.group("version"))
                if raw is None:
                    raise Fail(404, "No such package version.")
                return self.send(200, raw, "application/json", {"Cache-Control": "public, max-age=31536000, immutable"})
            m = re.fullmatch(r"/api/modules/([a-z0-9_-]+)", path)
            if m:   # the original registry's answer, so older clients keep working
                idx = reg.index()["packages"].get(m.group(1))
                if not idx:
                    raise Fail(404, "Module not found.")
                return self.send(200, {"name": m.group(1), "github": idx.get("homepage", ""), "version": idx["latest"]})
            if path == "/api/whoami":
                u = self.user()
                return self.send(200, {"username": u["username"], "admin": bool(u["is_admin"])})
            if path == "/api/keys":
                rows = reg.db.conn().execute("SELECT id,name,created_at,last_used FROM api_keys WHERE user_id=?", (self.user()["id"],)).fetchall()
                return self.send(200, {"keys": [dict(r) for r in rows]})
            if path == "/healthz":
                return self.send(200, {"ok": True})
            raise Fail(404, "Not found.")

        def do_POST(self):
            self.guarded(self.post)

        def post(self):
            path = urlparse(self.path).path
            if path in ("/api/register", "/api/login"):
                ip = self.ip()
                if reg.too_many_attempts(ip):
                    raise Fail(429, "Too many attempts. Try again in a few minutes.")
                d = self.json_body()
                username, password = str(d.get("username", "")).strip().lower(), str(d.get("password", ""))
                if path == "/api/register":
                    if os.environ.get("TON618_REGISTRATION") == "closed":
                        raise Fail(403, "Registration is closed on this server.")
                    reg.note_attempt(ip)
                    uid = reg.create_user(username, password)
                else:
                    user = reg.check_password(username, password)
                    if not user:
                        reg.note_attempt(ip)
                        raise Fail(401, "Wrong username or password.")
                    uid = user["id"]
                key = reg.new_key(uid, str(d.get("key_name") or "ton618 cli"))
                return self.send(201 if path == "/api/register" else 200, {"username": username, "api_key": key})
            if path == "/api/keys":
                u = self.user()
                return self.send(201, {"api_key": reg.new_key(u["id"], str(self.json_body().get("name", "key")))})
            if path == "/api/publish":
                u = self.user()
                pkg = reg.publish(u, self.read_body())
                return self.send(201, {"name": pkg.name, "version": pkg.version, "sha256": pkg.sha256, "owner": u["username"]})
            m = re.fullmatch(r"/api/yank/([a-z0-9_-]+)/([0-9A-Za-z.-]+)", path)
            if m:
                reg.yank(self.user(), m.group(1), m.group(2))
                return self.send(200, {"yanked": True})
            raise Fail(404, "Not found.")

        def do_DELETE(self):
            self.guarded(self.delete)

        def delete(self):
            m = re.fullmatch(r"/api/keys/(\d+)", urlparse(self.path).path)
            if not m:
                raise Fail(404, "Not found.")
            u = self.user()
            cur = reg.db.conn().execute("DELETE FROM api_keys WHERE id=? AND user_id=?", (int(m.group(1)), u["id"]))
            if cur.rowcount == 0:
                raise Fail(404, "No such key.")
            self.send(200, {"deleted": True})

    return Handler


def cmd_import(reg: Registry, folder: Path, owner: str) -> None:
    c = reg.db.conn()
    row = c.execute("SELECT * FROM users WHERE username=?", (owner,)).fetchone()
    if not row:
        password = secrets.token_urlsafe(18)
        reg.create_user(owner, password)
        row = c.execute("SELECT * FROM users WHERE username=?", (owner,)).fetchone()
        print(f"created user '{owner}' with password: {password}   (change nothing: log in with this, or make a new key with `login`)")
    for path in sorted(folder.glob("*.tonpkg")):
        try:
            pkg = reg.publish(row, path.read_bytes())
            print(f"imported {pkg.name} {pkg.version}")
        except Fail as e:
            print(f"skipped {path.name}: {e.message}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("serve")
    s.add_argument("--db", default="registry.db")
    s.add_argument("--host", default="127.0.0.1", help="use 0.0.0.0 only without a reverse proxy")
    s.add_argument("--port", type=int, default=8080)
    s.add_argument("--behind-proxy", action="store_true", help="trust X-Forwarded-For (Caddy/nginx in front)")
    i = sub.add_parser("import")
    i.add_argument("folder", type=Path)
    i.add_argument("--db", default="registry.db")
    i.add_argument("--owner", required=True)
    a = sub.add_parser("admin")
    a.add_argument("username")
    a.add_argument("--db", default="registry.db")
    b = sub.add_parser("backup")
    b.add_argument("target")
    b.add_argument("--db", default="registry.db")
    args = ap.parse_args()

    reg = Registry(Db(args.db))
    if args.cmd == "serve":
        httpd = ThreadingHTTPServer((args.host, args.port), make_handler(reg, args.behind_proxy))
        print(f"TON618 registry on http://{args.host}:{args.port}  (database: {args.db})", flush=True)
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            pass
    elif args.cmd == "import":
        cmd_import(reg, args.folder, args.owner)
    elif args.cmd == "admin":
        cur = reg.db.conn().execute("UPDATE users SET is_admin=1 WHERE username=?", (args.username,))
        print("done" if cur.rowcount else "no such user")
    elif args.cmd == "backup":
        dest = sqlite3.connect(args.target)
        reg.db.conn().backup(dest)
        dest.close()
        print(f"backup written to {args.target}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
