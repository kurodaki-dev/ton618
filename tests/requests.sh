#!/usr/bin/env bash
# Tests the `requests` package (registry/sources/requests) and ton.requests'
# requests_send against a small local HTTP server. Offline.
# Usage: tests/requests.sh [path/to/ton618]
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$(cd "$(dirname "${1:-$ROOT/ton618}")" && pwd)/$(basename "${1:-$ROOT/ton618}")"
command -v curl >/dev/null && command -v python3 >/dev/null || { echo "requests tests skipped (need curl and python3)"; exit 0; }
T="$(mktemp -d)"; trap 'kill $SRV 2>/dev/null; rm -rf "$T"' EXIT
PORT=$((20000 + RANDOM % 20000))
cat > "$T/srv.py" <<PY
import http.server, json
class H(http.server.BaseHTTPRequestHandler):
    def do_any(self):
        n = int(self.headers.get('Content-Length') or 0)
        body = self.rfile.read(n).decode()
        if self.path == '/redir':
            self.send_response(302); self.send_header('Location', '/echo'); self.end_headers(); return
        out = json.dumps({"method": self.command, "path": self.path, "body": body,
                          "auth": self.headers.get('Authorization'), "ct": self.headers.get('Content-Type')}).encode()
        self.send_response(404 if self.path == '/nope' else 200)
        self.send_header('Content-Type', 'application/json'); self.send_header('X-Test', 'yes')
        self.send_header('Content-Length', str(len(out))); self.end_headers(); self.wfile.write(out)
    do_GET = do_POST = do_PUT = do_PATCH = do_DELETE = do_any
    def log_message(self, *a): pass
http.server.HTTPServer(('127.0.0.1', $PORT), H).serve_forever()
PY
python3 "$T/srv.py" & SRV=$!
for i in 1 2 3 4 5 6 7 8 9 10; do curl -s "http://127.0.0.1:$PORT/" >/dev/null && break; sleep 0.3; done

mkdir -p "$T/app/modules" && cp "$ROOT/registry/sources/requests/requests.ton" "$T/app/modules/" && cd "$T/app" || exit 1
U="http://127.0.0.1:$PORT"
cat > t.ton <<TON
IMPORT://requests
ton.let r = requests.get("$U/echo", {"params": {"q": "a b&c", "n": 2}, "bearer": "tok"})
print(r.status, r.succeeded(), r.headers["x-test"], r.json().path, r.json().auth)
ton.let p = requests.post("$U/p", {"json": {"name": "ton"}, "auth": ["u", "pw"]})
print(p.json().body, p.json().ct, p.json().auth)
ton.let f = requests.post("$U/f", {"data": {"a": "1 2", "b": "é"}})
print(f.json().body, f.json().ct)
print(requests.put("$U/x", {"data": "raw"}).json().method, requests.delete("$U/x").json().method)
print(requests.get("$U/redir").json().path)
ton.let n = requests.get("$U/nope")
print(n.status, n.succeeded())
try { n.raise_for_status() } catch (e) { print("caught:", e) }
ton.let d = requests.get("http://127.0.0.1:1/x", {"timeout": 2})
print(d.ok, d.status)
TON
out="$("$BIN" t.ton 2>&1)"
pass=0; fail=0
check() { if echo "$out" | grep -qF -- "$1"; then pass=$((pass+1)); echo "ok    $2"; else fail=$((fail+1)); echo "FAIL  $2"; echo "$out" | sed 's/^/        /'; fi; }
check '200 true yes /echo?q=a%20b%26c&n=2 Bearer tok' "GET with params, bearer token and response headers"
check '{"name":"ton"} application/json Basic dTpwdw==' "POST JSON with basic auth"
check 'a=1%202&b=%C3%A9 application/x-www-form-urlencoded' "POST form, URL-encoded"
check 'PUT DELETE' "PUT and DELETE"
check '/echo' "redirects are followed"
check '404 false' "a 404 is an answer, not a success"
check 'caught: HTTP 404 for' "raise_for_status throws on 4xx"
check 'false 0' "a refused connection is ok=false, status 0"
echo; echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
