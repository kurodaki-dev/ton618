#!/usr/bin/env bash
# Sign in with GitHub, end to end, against a fake GitHub (offline): the website flow
# (login -> authorize -> callback -> key in the URL fragment) and `ton618 login --github`.
# Usage: tests/oauth.sh [path/to/ton618]
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$(cd "$(dirname "${1:-$ROOT/ton618}")" && pwd)/$(basename "${1:-$ROOT/ton618}")"
command -v curl >/dev/null && command -v python3 >/dev/null || { echo "oauth tests skipped (need curl and python3)"; exit 0; }
T="$(mktemp -d)"; trap 'kill $SRV $GH 2>/dev/null; rm -rf "$T"' EXIT
pass=0; fail=0
ok()  { pass=$((pass + 1)); echo "ok    $1"; }
bad() { fail=$((fail + 1)); echo "FAIL  $1"; [ -n "${2:-}" ] && echo "$2" | sed 's/^/        /'; }
check() { if echo "$2" | grep -qF -- "$3"; then ok "$1"; else bad "$1" "$2"; fi; }

GHPORT=$((20000 + RANDOM % 10000)); PORT=$((30000 + RANDOM % 10000))
cat > "$T/gh.py" <<PY
import http.server, json, urllib.parse
polls = {"n": 0}
class H(http.server.BaseHTTPRequestHandler):
    def reply(self, obj, status=200):
        b = json.dumps(obj).encode(); self.send_response(status); self.send_header('Content-Type','application/json')
        self.send_header('Content-Length', str(len(b))); self.end_headers(); self.wfile.write(b)
    def form(self):
        n = int(self.headers.get('Content-Length') or 0); return urllib.parse.parse_qs(self.rfile.read(n).decode())
    def do_POST(self):
        f = self.form()
        if self.path == '/login/device/code':
            return self.reply({"device_code": "dev123", "user_code": "ABCD-1234", "verification_uri": "http://127.0.0.1:$GHPORT/device", "expires_in": 60, "interval": 1})
        if self.path == '/login/oauth/access_token':
            if f.get('device_code') == ['dev123']:
                polls["n"] += 1
                return self.reply({"error": "authorization_pending"}) if polls["n"] < 2 else self.reply({"access_token": "tok-device"})
            if f.get('code') == ['goodcode'] and f.get('client_secret') == ['sekret']:
                return self.reply({"access_token": "tok-web"})
            return self.reply({"error": "bad_verification_code", "error_description": "bad code"})
        self.reply({}, 404)
    def do_GET(self):
        if self.path == '/user':
            tok = self.headers.get('Authorization', '')
            who = {"Bearer tok-web": (4242, "Octo-Cat"), "Bearer tok-device": (777, "dev-user")}.get(tok)
            return self.reply({"id": who[0], "login": who[1]}) if who else self.reply({"message": "Bad credentials"}, 401)
        self.reply({}, 404)
    def log_message(self, *a): pass
http.server.HTTPServer(('127.0.0.1', $GHPORT), H).serve_forever()
PY
python3 "$T/gh.py" & GH=$!
export TON618_GITHUB_CLIENT_ID=cid TON618_GITHUB_CLIENT_SECRET=sekret TON618_PUBLIC_URL="http://127.0.0.1:$PORT" \
       TON618_SITE_URLS="https://site.example" TON618_GITHUB_OAUTH_URL="http://127.0.0.1:$GHPORT" TON618_GITHUB_API_URL="http://127.0.0.1:$GHPORT"
python3 "$ROOT/server/registry_server.py" serve --db "$T/r.db" --port "$PORT" >"$T/server.log" 2>&1 & SRV=$!
for i in $(seq 1 20); do curl -s "http://127.0.0.1:$PORT/healthz" >/dev/null && break; sleep 0.3; done
U="http://127.0.0.1:$PORT"

check "config says GitHub sign-in is on" "$(curl -s $U/api/oauth/config)" '"github": true'
H1="$(curl -si "$U/api/oauth/github/login?return=https://site.example/account/")"
check "login redirects to GitHub's authorize page" "$H1" "Location: http://127.0.0.1:$GHPORT/login/oauth/authorize"
STATE="$(echo "$H1" | tr -d '\r' | sed -n 's/^Location:.*state=\([^&]*\).*/\1/p')"
check "a state is issued" "x$STATE" "x"
check "an unknown return address is refused" "$(curl -s "$U/api/oauth/github/login?return=https://evil.example/")" "isn't allowed"
H2="$(curl -si "$U/api/oauth/github/callback?code=goodcode&state=$STATE")"
check "callback sends the key back in the URL fragment" "$H2" "Location: https://site.example/account/#key=ton_"
check "the account is named after the GitHub login" "$H2" "username=octo-cat"
check "a state works only once" "$(curl -s "$U/api/oauth/github/callback?code=goodcode&state=$STATE")" "expired"
H3="$(curl -si "$U/api/oauth/github/login?return=https://site.example/account/")"
STATE2="$(echo "$H3" | tr -d '\r' | sed -n 's/^Location:.*state=\([^&]*\).*/\1/p')"
check "a wrong code is refused" "$(curl -s "$U/api/oauth/github/callback?code=nope&state=$STATE2")" "bad code"
KEY="$(echo "$H2" | tr -d '\r' | sed -n 's/^Location:.*key=\(ton_[^&]*\).*/\1/p')"
check "the key from GitHub sign-in works" "$(curl -s -H "Authorization: Bearer $KEY" $U/api/whoami)" '"username": "octo-cat"'
check "a GitHub account has no usable password" "$(curl -s -X POST -d '{"username":"octo-cat","password":""}' $U/api/login)" "Wrong username or password"

export XDG_CONFIG_HOME="$T/cfg"; unset TON618_PACKAGES_URL
out="$("$BIN" login --github --no-open --server "$U" 2>&1)"
check "ton618 login --github shows the code" "$out" "ABCD-1234"
check "ton618 login --github logs in" "$out" "Logged in as dev-user"
check "and whoami agrees" "$("$BIN" whoami 2>&1)" "dev-user"

echo; echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
