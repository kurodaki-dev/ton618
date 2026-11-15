#!/usr/bin/env bash
# The `atome` command end to end: new, check, build for the web, the kernel and a bootable ISO, then boot the
# ISO under QEMU and drive the system with simulated keys (reading the screen from the VM's video memory).
# Parts that need a tool that isn't installed are skipped.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ATOME="python3 $ROOT/atome/cli/atome.py"
T="$(mktemp -d)"; trap 'rm -rf "$T"; [ -n "${QPID:-}" ] && kill $QPID 2>/dev/null' EXIT
pass=0; fail=0; skipped=0
ok()   { pass=$((pass + 1)); echo "ok    $1"; }
bad()  { fail=$((fail + 1)); echo "FAIL  $1"; [ -n "${2:-}" ] && echo "$2" | sed 's/^/        /' | head -20; }
skip() { skipped=$((skipped + 1)); echo "skip  $1"; }
has()  { echo "$2" | grep -qF -- "$3" && ok "$1" || bad "$1 (expected: $3)" "$2"; }
command -v python3 >/dev/null || { echo "atome tests skipped (need python3)"; exit 0; }

cd "$T" || exit 1
out="$($ATOME new demo --title "Demo OS" 2>&1)"; has "atome new creates a project" "$out" "Created demo/"
[ -f demo/atome.toml ] && [ -f demo/main.ton ] && ok "with atome.toml and main.ton" || bad "files missing"
out="$($ATOME new demo 2>&1)"; has "it refuses to overwrite a folder" "$out" "already exists"
cd demo || exit 1
out="$($ATOME check 2>&1)"; has "atome check" "$out" "demo: OK"
printf 'ton.function broken( {\n' > broken.ton
if command -v "$ROOT/ton618" >/dev/null 2>&1 || command -v ton618 >/dev/null 2>&1; then
  PATH="$ROOT:$PATH" out="$($ATOME check 2>&1)" && bad "check should fail on a syntax error" || has "check reports a syntax error" "$out" "broken.ton"
else skip "syntax errors (needs the ton618 binary)"; fi
rm -f broken.ton

out="$($ATOME build 2>&1)"; has "atome build (web)" "$out" "dist/web/index.html"
page=dist/web/index.html
grep -q "Ton618Lite" $page && grep -q "Demo OS" $page && ok "the page holds the engine and the title" || bad "the page is incomplete"
out="$(cd "$ROOT" && node -e '
require("./exemples/ton618-lite.js");
const fs=require("fs"), out=[];
const r=Ton618Lite.runTon618(fs.readFileSync(process.argv[1]+"/main.ton","utf8"),{onUi:m=>out.push(m)});
const last=()=>out.filter(m=>m.type==="render").pop().html;
const win=/data-win=.(w\d+)/.exec(last())[1];
r.session.dispatch({type:"click",id:"a:press",win,value:""});
console.log(r.ok, last().includes("pressed the button 1 times"));' "$T/demo" 2>&1)"
has "the generated system runs and answers a click" "$out" "true true"
out="$($ATOME build --target app 2>&1)"; has "atome build --target app generates a Tauri project" "$out" "Generated"
python3 -c "import json;json.load(open('dist/app/src-tauri/tauri.conf.json'));json.load(open('dist/app/package.json'))" 2>/dev/null && ok "tauri.conf.json and package.json are valid JSON" || bad "the app's JSON files are invalid"
grep -q "pub fn run" dist/app/src-tauri/src/lib.rs && grep -q "demo_lib::run" dist/app/src-tauri/src/main.rs && ok "the Rust side is a Tauri 2 app with a mobile entry point" || bad "lib.rs / main.rs wrong"
python3 -c "import yaml" 2>/dev/null && { python3 -c "import yaml;d=yaml.safe_load(open('.github/workflows/atome-build.yml'));assert set(d['jobs'])=={'desktop','android'}" 2>/dev/null && ok "the generated workflow is valid YAML" || bad "the generated workflow is not valid YAML"; } || skip "workflow YAML check (needs PyYAML)"
[ -f .github/workflows/atome-build.yml ] && python3 -c "import struct;d=open('dist/app/icon.png','rb').read();assert d[:8]==b'\x89PNG\r\n\x1a\n'" && ok "and a valid icon and a GitHub workflow for the Android/Windows/Linux/macOS apps" || bad "workflow missing"

if command -v gcc >/dev/null && command -v nasm >/dev/null && command -v ld >/dev/null; then
  out="$($ATOME build --target kernel 2>&1)"; has "atome build --target kernel" "$out" "atome.elf"
  [ -s dist/kernel/atome.elf ] && [ -s dist/kernel/program.ton ] && ok "the kernel and the bundled program exist" || bad "kernel outputs missing"
  grep -q "atome_boot" dist/kernel/program.ton && ! grep -q "^IMPORT://" dist/kernel/program.ton && ok "imports are inlined for the kernel" || bad "bundle wrong"
else skip "the kernel (needs gcc, nasm, ld)"; fi

if command -v gcc >/dev/null && command -v nasm >/dev/null && command -v grub-mkrescue >/dev/null && command -v xorriso >/dev/null && command -v qemu-system-i386 >/dev/null; then
  out="$($ATOME build --target image 2>&1)"; has "atome build --target image" "$out" "demo.iso"
  [ -s dist/demo.iso ] && ok "the ISO exists" || bad "no ISO"
  rm -f "$T/qm.sock"
  qemu-system-i386 -cdrom dist/demo.iso -display none -serial null -monitor unix:"$T/qm.sock",server,nowait -no-reboot >/dev/null 2>&1 &
  QPID=$!
  sleep 9
  SCREEN="$(python3 - "$T/qm.sock" "$ROOT/tests/vga_dump.py" <<'PY'
import sys, socket, time, importlib.util
spec = importlib.util.spec_from_file_location("vga_dump", sys.argv[2]); m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1]); time.sleep(0.3); s.recv(4096)
print("\n".join(m.screen(s)))
print("=====")
s.send(b"sendkey ret\n"); time.sleep(0.7)
for k in ["down", "b", "o", "b"]:
    s.send(("sendkey %s\n" % k).encode()); time.sleep(0.3)
time.sleep(0.5)
print("\n".join(m.screen(s)))
s.send(b"quit\n")
PY
)"
  has "the ISO boots under QEMU into the system" "$SCREEN" "Demo OS"
  has "its window is drawn" "$SCREEN" "You pressed the button 0 times."
  has "Enter on the button runs the app's action" "$SCREEN" "You pressed the button 1 times."
  has "typing fills the input field" "$SCREEN" "Your name (then Enter): bob_"
  has "and the app answers" "$SCREEN" "Hi bob"
else skip "the bootable image (needs gcc, nasm, grub-mkrescue, xorriso, qemu-system-i386)"; fi

echo; echo "$pass passed, $fail failed, $skipped skipped"
[ "$fail" -eq 0 ]
