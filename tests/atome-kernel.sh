#!/usr/bin/env bash
# Builds the Atome kernel and boots it for real under QEMU: the TON program runs at startup, then the
# shell answers commands typed on the serial port. Skips itself without gcc/nasm/ld/qemu.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
for t in gcc nasm ld qemu-system-i386; do command -v $t >/dev/null || { echo "atome kernel test skipped (needs $t)"; exit 0; }; done
cd "$ROOT/atome/kernel" || exit 1
make clean >/dev/null && make >/dev/null 2>"/tmp/atome-build.$$" || { echo "FAIL  the kernel doesn't build"; cat "/tmp/atome-build.$$"; exit 1; }
pass=0; fail=0
ok()  { pass=$((pass + 1)); echo "ok    $1"; }
bad() { fail=$((fail + 1)); echo "FAIL  $1"; }
OUT="$( (sleep 3; printf 'echo bonjour atome\r'; sleep 1; printf 'mem\r'; sleep 1; printf 'poweroff\r'; sleep 2) \
  | timeout 40 qemu-system-i386 -kernel atome.elf -initrd "$ROOT/atome/examples/hello.ton" -display none -serial stdio -no-reboot 2>&1 | tr -d '\r')"
check() { echo "$OUT" | grep -qF -- "$2" && ok "$1" || { bad "$1 (expected: $2)"; echo "$OUT" | sed 's/^/        /' | head -30; }; }
check "the kernel boots and prints its banner" "Atome kernel 0.1.0"
check "it finds the memory" "[boot] memory:"
check "it runs the TON program: function, recursion, f-string" "Hello from Atome! fib(15) = 610"
check "arrays and method syntax" "2 KIWI"
check "dicts" '{os: "Atome", bits: 32, ok: true} Atome 3 true'
check "loops, compound assignment, split/join, string repeat" 'sum 1..10 = 55 | ababab | ["a", "b", "c"] 1-2-3'
check "continue/break, ternary, integer division and modulo" "k = 6 even 3 -1"
check "the program ends, then the shell answers" "[atome] the program ended"
check "the shell runs a command typed on the serial port" "bonjour atome"
check "the heap is sized from the memory map" "KB used of"
check "poweroff stops the machine" "Powering off..."
rm -f "/tmp/atome-build.$$"
echo; echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
