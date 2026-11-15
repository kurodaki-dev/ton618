#!/usr/bin/env bash
# Boots a real Atome image under QEMU with a screen and a mouse, clicks and drags with the (virtual) mouse and checks
# the pixels of the screenshots. Skips itself without gcc/nasm/grub-mkrescue/xorriso/qemu.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
for t in gcc nasm ld make grub-mkrescue xorriso qemu-system-i386 python3; do command -v $t >/dev/null || { echo "atome graphics test skipped (needs $t)"; exit 0; }; done
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
ATOME="python3 $ROOT/atome/cli/atome.py"
pass=0; fail=0
ok()  { pass=$((pass + 1)); echo "ok    $1"; }
bad() { fail=$((fail + 1)); echo "FAIL  $1"; }
# px FILE X Y -> "r,g,b" of a pixel of a P6 screenshot
px() { python3 - "$@" <<'PY'
import sys
d = open(sys.argv[1], "rb").read()
parts = d.split(b"\n", 3); w, h = (int(v) for v in parts[1].split()); body = parts[3]
x, y = int(sys.argv[2]), int(sys.argv[3]); o = (y * w + x) * 3   # P6: three bytes per pixel
print("%d,%d,%d" % tuple(body[o:o + 3]))
PY
}
expect() { [ "$(px "$2" "$3" "$4")" = "$5" ] && ok "$1" || bad "$1 (pixel $3,$4 is $(px "$2" "$3" "$4"), expected $5)"; }
differs() { [ "$(px "$2" "$3" "$4")" != "$5" ] && ok "$1" || bad "$1 (pixel $3,$4 is still $5)"; }
DRIVE="python3 $ROOT/tests/qemu_drive.py"

cd "$T" && $ATOME new desk >/dev/null && cd desk || exit 1
$ATOME build --target image >/dev/null 2>"$T/build.log" || { bad "the image builds"; cat "$T/build.log"; exit 1; }
ISO=dist/desk.iso

# Windows style: the Hello window open, a taskbar, a Start menu
$DRIVE $ISO "$T/w" "wait:14;shot:boot;move:30,740;click;wait:1;shot:menu;move:200,42;press;move:500,200;move:600,300;release;wait:1;shot:drag" >/dev/null 2>&1
expect "the taskbar is drawn on the real screen"          "$T/w/boot.ppm" 500 750 "22,24,34"
expect "the focused window has a blue title bar"          "$T/w/boot.ppm" 200 42 "0,95,184"
expect "the selected button is highlighted"               "$T/w/boot.ppm" 70 137 "0,120,215"
expect "a mouse click on Start opens the menu"            "$T/w/menu.ppm" 100 490 "0,120,215"
expect "dragging the title bar moves the window"          "$T/w/drag.ppm" 700 300 "0,95,184"
differs "the window left its old place"                   "$T/w/drag.ppm" 200 42 "0,95,184"
grep -q "graphical screen" "$T/w/serial.txt" && ok "the kernel reports the graphical screen" || bad "the kernel reports the graphical screen"

# Android style: a phone-shaped screen, the home screen first
sed -i 's/atome.run()/atome.style("android")\natome.run()/' main.ton
$ATOME build --target image >/dev/null 2>&1
$DRIVE $ISO "$T/a" "wait:14;shot:app;move:340,80;click;wait:1;shot:opened" >/dev/null 2>&1
expect "the phone has a status bar"                       "$T/a/app.ppm" 300 5 "22,24,34"
expect "the phone has a navigation bar"                   "$T/a/app.ppm" 400 760 "22,24,34"
echo; echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
