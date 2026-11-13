#!/usr/bin/env bash
# End-to-end test of the registry server (server/registry_server.py, SQLite) with the
# real client: register, login, publish, install, ownership, immutability. Offline.
# Usage: tests/server.sh [path/to/ton618]
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$(cd "$(dirname "${1:-$ROOT/ton618}")" && pwd)/$(basename "${1:-$ROOT/ton618}")"
command -v curl >/dev/null && command -v python3 >/dev/null || { echo "server tests skipped (need curl and python3)"; exit 0; }
T="$(mktemp -d)"; trap 'kill $SRV 2>/dev/null; rm -rf "$T"' EXIT
pass=0; fail=0
ok()  { pass=$((pass + 1)); echo "ok    $1"; }
bad() { fail=$((fail + 1)); echo "FAIL  $1"; [ -n "${2:-}" ] && echo "$2" | sed 's/^/        /'; }
expect_ok()   { local d="$1"; shift; local out; if out="$("$@" 2>&1)"; then ok "$d"; else bad "$d" "$out"; fi; LAST="$out"; }
expect_fail() { local d="$1"; shift; local out; if out="$("$@" 2>&1)"; then bad "$d (should have failed)" "$out"; else ok "$d"; fi; LAST="$out"; }
has() { echo "$LAST" | grep -qF -- "$1" && ok "  says: $1" || bad "  expected to say: $1" "$LAST"; }

PORT=$((20000 + RANDOM % 20000)); URL="http://127.0.0.1:$PORT"
python3 "$ROOT/server/registry_server.py" serve --db "$T/r.db" --port "$PORT" >"$T/server.log" 2>&1 & SRV=$!
for i in $(seq 1 20); do curl -s "$URL/healthz" >/dev/null && break; sleep 0.3; done
export XDG_CONFIG_HOME="$T/cfg"; unset TON618_PACKAGES_URL

mkdir -p "$T/me/mathx" && cd "$T/me/mathx" || exit 1
expect_fail "publishing without an account is refused" bash -c "cd '$T/me/mathx' && '$BIN' init mathx >/dev/null && sed -i 's/^description = .*/description = \"Maths helpers for the test.\"/' tonproject.toml && '$BIN' publish"; has "ton618 register"
expect_ok "register" bash -c "echo 'correct horse battery' | '$BIN' register --server $URL --username alice --password-stdin"; has "Account created"
expect_ok "whoami" "$BIN" whoami; has "Logged in as alice"
[ "$(stat -c %a "$T/cfg/ton618/credentials.json" 2>/dev/null)" = "600" ] && ok "the key file is private (600)" || bad "credentials.json isn't mode 600"
expect_fail "a second account can't take the same username" bash -c "echo 'whatever123' | '$BIN' register --server $URL --username alice --password-stdin"; has "taken"
expect_fail "a wrong password is refused" bash -c "echo 'nope nope nope' | '$BIN' login --server $URL --username alice --password-stdin"; has "Wrong username or password"
expect_ok "publish" "$BIN" publish; has "Published mathx 0.1.0"
expect_fail "the same version can't be published twice" "$BIN" publish; has "never change"
curl -s "$URL/registry/index.json" | grep -q '"mathx"' && ok "the index lists mathx" || bad "index misses mathx"

mkdir -p "$T/user" && cd "$T/user" || exit 1
export TON618_PACKAGES_URL="$URL/registry"
expect_ok "anyone installs it" "$BIN" install mathx; has "Installed mathx 0.1.0"
[ -f modules/mathx.ton ] && ok "the file is in modules/" || bad "mathx.ton missing"
expect_ok "search" "$BIN" search math; has "mathx"
unset TON618_PACKAGES_URL

# another account can't publish a new version of alice's package
export XDG_CONFIG_HOME="$T/cfg2"
expect_ok "register bob" bash -c "echo 'bobs password 1' | '$BIN' register --server $URL --username bob --password-stdin"
cd "$T/me/mathx" && sed -i 's/^version = .*/version = "0.2.0"/' tonproject.toml
expect_fail "bob can't publish alice's package" "$BIN" publish; has "belongs to another account"
export XDG_CONFIG_HOME="$T/cfg"
expect_ok "alice publishes 0.2.0" "$BIN" publish; has "Published mathx 0.2.0"
expect_ok "logout" "$BIN" logout
sed -i 's/^version = .*/version = "0.3.0"/' tonproject.toml
expect_fail "after logout publishing needs an account again" "$BIN" publish; has "ton618 register"

echo; echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
