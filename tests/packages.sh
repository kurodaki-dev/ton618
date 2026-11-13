#!/usr/bin/env bash
# End-to-end test of the package manager, entirely offline: builds packages with
# `ton618 build`, turns them into a registry with scripts/registry.py, serves it
# from a file:// URL and runs install / update / uninstall / ... against it.
# Usage: tests/packages.sh [path/to/ton618]
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$(cd "$(dirname "${1:-$ROOT/ton618}")" && pwd)/$(basename "${1:-$ROOT/ton618}")"
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
pass=0; fail=0
ok()   { pass=$((pass + 1)); echo "ok    $1"; }
bad()  { fail=$((fail + 1)); echo "FAIL  $1"; [ -n "${2:-}" ] && echo "$2" | sed 's/^/        /'; }
expect_ok()   { local d="$1"; shift; local out; if out="$("$@" 2>&1)"; then ok "$d"; else bad "$d" "$out"; fi; LAST="$out"; }
expect_fail() { local d="$1"; shift; local out; if out="$("$@" 2>&1)"; then bad "$d (should have failed)" "$out"; else ok "$d"; fi; LAST="$out"; }
has()  { echo "$LAST" | grep -qF -- "$1" && ok "  says: $1" || bad "  expected to say: $1" "$LAST"; }

# --- three packages: alpha 1.0.0 / 1.1.0 / 2.0.0, beta (needs alpha ^1.0) ---------------------
mkpkg() { # name version [dependency-lines]
  local d="$T/src/$1"; mkdir -p "$d"
  cat > "$d/tonproject.toml" <<TOML
[project]
name = "$1"
version = "$2"
description = "Test package $1."
license = "MIT"
authors = ["tester"]
main = "$1.ton"

[dependencies]
${3:-}
TOML
  printf 'ton.function %s_version() { return "%s %s" }\n' "$1" "$1" "$2" > "$d/$1.ton"
  (cd "$d" && "$BIN" build >/dev/null 2>&1) && mkdir -p "$T/registry/packages" && cp "$d/dist/$1-$2.tonpkg" "$T/registry/packages/"
}
mkpkg alpha 1.0.0; mkpkg alpha 1.1.0; mkpkg alpha 2.0.0
mkpkg beta 1.0.0 'alpha = "^1.0"'
[ -f "$T/registry/packages/beta-1.0.0.tonpkg" ] && ok "build: made 4 package files" || bad "build: package files missing"

python3 "$ROOT/scripts/registry.py" build "$T/site" --packages "$T/registry/packages" --ton618 "$BIN" >/dev/null 2>&1 && ok "registry.py builds the index" || bad "registry.py failed"
export TON618_PACKAGES_URL="file://$T/site/registry"
export TON618_REGISTRY_URL="file://$T/nowhere"   # the original registry is unreachable in this test

mkdir -p "$T/app" && cd "$T/app" || exit 1

expect_ok "search finds alpha" "$BIN" search alpha; has "alpha 2.0.0"
expect_ok "info shows versions" "$BIN" info alpha; has "versions: 2.0.0, 1.1.0, 1.0.0"

expect_ok "install beta (and its dependency)" "$BIN" install beta; has "beta needs alpha ^1.0"
[ -f modules/beta.ton ] && [ -f modules/alpha.ton ] && ok "files are in modules/" || bad "files missing"
grep -q "alpha 1.1.0" modules/alpha.ton && ok "the dependency resolved to the newest 1.x (1.1.0)" || bad "wrong alpha version" "$(cat modules/alpha.ton)"
printf 'IMPORT://alpha\nIMPORT://beta\nprint(alpha_version(), beta_version())\n' > use.ton
expect_ok "a script can import the packages" "$BIN" use.ton; has "alpha 1.1.0 beta 1.0.0"

expect_ok "list" "$BIN" list; has "beta 1.0.0"
expect_ok "install again says already installed" "$BIN" install beta; has "already installed"
expect_ok "update keeps alpha < 2 (beta needs ^1.0)" "$BIN" update; has "needs ^1.0"
expect_fail "installing alpha 2 would break beta" "$BIN" install alpha@2.0.0; has "beta needs alpha ^1.0"
expect_ok "install an exact older version with --force" "$BIN" install alpha@1.0.0 --force; has "Installed alpha 1.0.0"
expect_ok "update brings alpha back to 1.1.0" "$BIN" update alpha; has "Updated alpha: 1.0.0 -> 1.1.0"
expect_ok "list --outdated" "$BIN" list --outdated; has "alpha"
expect_fail "uninstall refuses while beta needs alpha" "$BIN" uninstall alpha; has "needed by beta"
expect_ok "uninstall beta" "$BIN" uninstall beta
expect_ok "uninstall alpha" "$BIN" uninstall alpha
[ ! -e modules/alpha.ton ] && [ ! -e modules/beta.ton ] && ok "files removed" || bad "files left behind"

expect_fail "unknown package" "$BIN" install nosuchthing; has "Not found"
expect_fail "bad range" "$BIN" install alpha@banana; has "isn't a valid version range"
expect_fail "unsafe name is refused" "$BIN" install 'x;touch pwned'
[ ! -e pwned ] && ok "no shell injection" || bad "INJECTION"

# tampered package: the checksum must catch it
cp -r "$T/site" "$T/evil"; sed -i 's/alpha 1.1.0/alpha 6.6.6/' "$T/evil/registry/packages/alpha-1.1.0.tonpkg"
TON618_PACKAGES_URL="file://$T/evil/registry" expect_fail "tampered package is refused" "$BIN" install alpha@1.1.0; has "Checksum mismatch"
[ ! -e modules/alpha.ton ] && ok "nothing installed from a bad download" || bad "installed a tampered package"

# dependencies from tonproject.toml, and --save
mkdir -p "$T/proj" && cd "$T/proj" || exit 1
expect_ok "init" "$BIN" init my-tool; has "Created tonproject.toml"
expect_ok "the new project builds" "$BIN" build; has "my-tool-0.1.0.tonpkg"
expect_ok "install --save" "$BIN" install alpha --save; has "Saved alpha"
grep -q 'alpha = "^2.0.0"' tonproject.toml && ok "dependency saved in tonproject.toml" || bad "not saved" "$(cat tonproject.toml)"
rm -rf modules; expect_ok "install with no name reads tonproject.toml" "$BIN" install; [ -f modules/alpha.ton ] && ok "  and installs it" || bad "  not installed"
expect_fail "publish refuses the placeholder description" "$BIN" publish --dry-run --no-open; has "real description"

# update and modules installed by the original installer (not recorded in .installed.json)
mkdir -p "$T/old/modules" && cd "$T/old" || exit 1
printf 'ton.function legacymod_hi() { return "old" }\n' > modules/legacymod.ton
expect_ok "update looks at modules that were never recorded" "$BIN" update; has "legacymod: not tracked"
grep -q old modules/legacymod.ton && ok "an unreachable registry leaves the module intact" || bad "legacymod.ton was damaged"
printf '{"format":1,"packages":{"legacymod":{"version":"1","source":"legacy","github":"x/y","dependencies":{},"files":["modules/legacymod.ton"]}}}' > modules/.installed.json
expect_ok "update handles a recorded legacy module" "$BIN" update; has "original registry can't be reached"
grep -q old modules/legacymod.ton && ok "and keeps it" || bad "legacymod.ton was damaged"

# the index is fetched fresh: a new version published after the first run shows up
mkdir -p "$T/fresh" && cd "$T/fresh" || exit 1
rebuild() { python3 "$ROOT/scripts/registry.py" build "$T/site" --packages "$T/registry/packages" >/dev/null 2>&1; }
mkpkg gamma 1.0.0; rebuild
expect_ok "install gamma 1.0.0" "$BIN" install gamma
mkpkg gamma 1.1.0; rebuild
expect_ok "update sees the version published after install" "$BIN" update gamma; has "Updated gamma: 1.0.0 -> 1.1.0"

# build errors
mkdir -p "$T/broken" && cd "$T/broken" || exit 1
printf '[project]\nname = "Bad Name"\nversion = "1.0.0"\n' > tonproject.toml
expect_fail "invalid name is rejected" "$BIN" build; has "project.name"
printf '[project]\nname = "okname"\nversion = "1.0.0"\n' > tonproject.toml; printf 'print(1\n' > okname.ton
expect_fail "syntax errors stop the build" "$BIN" build; has "Fix the syntax errors"
printf 'print(1)\n' > okname.ton; printf '[project]\nname = "okname"\nversion = "1.0"\n' > tonproject.toml
expect_fail "bad version is rejected" "$BIN" build; has "project.version"
printf '[project\n' > tonproject.toml
expect_fail "bad TOML reports the line" "$BIN" build; has "tonproject.toml line 1"

# publish: an already published version is refused, a new one is accepted (dry run)
cd "$T/src/alpha" && sed -i 's/version = "2.0.0"/version = "2.0.0"/' tonproject.toml
expect_fail "publishing an existing version is refused" "$BIN" publish --dry-run --no-open; has "already in the registry"
sed -i 's/version = "2.0.0"/version = "2.1.0"/' tonproject.toml
expect_ok "publishing a new version (dry run)" "$BIN" publish --dry-run --no-open; has "ready to be published"

echo; echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
