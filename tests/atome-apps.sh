#!/usr/bin/env bash
# The atome module and its five apps, run by the kernel's own TON interpreter built for a normal computer
# (atome/kernel/host_test.c), driven with scripted keys: fast, and it needs only gcc.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
command -v gcc >/dev/null || { echo "atome apps test skipped (needs gcc)"; exit 0; }
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
gcc -DATOME_HOSTED -O1 -w -I"$ROOT/atome/kernel" -o "$T/host" "$ROOT/atome/kernel/host_test.c" "$ROOT/atome/kernel/tonvm.c" "$ROOT/atome/kernel/libk.c" || { echo "FAIL  the host build failed"; exit 1; }
printf 'atome.boot("TestOS")\natome.run()\n' > "$T/main.ton"
grep -v '^IMPORT://' "$ROOT/atome/module/atome.ton" > "$T/prog.ton"; cat "$T/main.ton" >> "$T/prog.ton"
pass=0; fail=0
ok()  { pass=$((pass + 1)); echo "ok    $1"; }
bad() { fail=$((fail + 1)); echo "FAIL  $1"; }
# screen <keys>: the text screen after the keys, non-ASCII drawing characters shown as #
screen() { "$T/host" "$T/prog.ton" "$1" 2>/dev/null | LC_ALL=C sed 's/[^ -~]/#/g'; }
check() { echo "$3" | grep -qF -- "$2" && ok "$1" || { bad "$1 (expected: $2)"; echo "$3" | head -14 | sed 's/^/        /'; }; }

MENU='<esc>'
check "the system boots with its title"                         "TestOS"                     "$(screen '')"
check "the menu lists the five apps"                            "Calculator"                 "$(screen "$MENU")"
check "Notes: a note is added with Enter"                       "- buy milk"                 "$(screen "$MENU<enter>buy milk<enter>")"
check "Notes: and deleted again"                                "No notes yet."              "$(screen "$MENU<enter>buy milk<enter><down><enter>")"
check "Calculator: 12*(3+4)"                                    "= 84"                       "$(screen "$MENU<down><enter>12*(3+4)<enter>")"
check "Calculator: precedence and negatives"                    "= 7"                        "$(screen "$MENU<down><enter>1+2*3<enter>")"
check "Calculator: division by zero is an error, not a crash"   "error: division by zero"    "$(screen "$MENU<down><enter>5/0<enter>")"
check "Files: lists the home folder"                            "readme.txt"                 "$(screen "$MENU<down><down><enter>")"
check "Files: a folder is created with a trailing /"            "[+] projects"               "$(screen "$MENU<down><down><enter><down><down><down>projects/<enter>")"
check "Files: a file can be opened"                             "Welcome to your system!"    "$(screen "$MENU<down><down><enter><down><down><enter>")"
check "Terminal: ls"                                            "docs/  readme.txt"          "$(screen "$MENU<down><down><down><enter>ls<enter>")"
check "Terminal: mkdir, cd, pwd"                                "/home/x"                    "$(screen "$MENU<down><down><down><enter>mkdir x<enter>cd x<enter>pwd<enter>")"
check "Terminal: unknown commands are explained"                "command not found"          "$(screen "$MENU<down><down><down><enter>nope<enter>")"
check "Settings: shows where it runs"                           "running on kernel"          "$(screen "$MENU<down><down><down><down><enter>")"
check "Tab changes window"                                      "Notes"                      "$(screen "$MENU<enter><esc><down><enter><tab>")"
check "the menu's Power off stops the machine"                  "[poweroff]"                 "$("$T/host" "$T/prog.ton" '<esc><down><down><down><down><down><enter>' 2>&1)"
echo; echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
