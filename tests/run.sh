#!/usr/bin/env bash
# Runs every tests/*.ton script and compares its output (stdout + stderr)
# with the matching tests/*.expected file. Usage: tests/run.sh [path/to/ton618]
# Regenerate an expected file after an intended change with:
#   ./ton618 tests/name.ton > tests/name.expected 2>&1
set -u
BIN="${1:-./ton618}"
cd "$(dirname "$0")/.."
pass=0; fail=0
for t in tests/*.ton; do
    exp="${t%.ton}.expected"
    [ -f "$exp" ] || continue
    out="$("$BIN" "$t" 2>&1)"
    if [ "$out" == "$(cat "$exp")" ]; then
        pass=$((pass + 1)); echo "ok    $t"
    else
        fail=$((fail + 1)); echo "FAIL  $t"
        diff <(echo "$out") "$exp" | head -20
    fi
done
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
