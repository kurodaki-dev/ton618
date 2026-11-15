#!/usr/bin/env python3
"""Keeps the built-in copy of the `atome` module inside exemples/ton618-lite.js in sync with atome/module/atome.ton.

    python3 scripts/embed_atome.py           rewrite the block
    python3 scripts/embed_atome.py --check   fail if the block is out of date (CI)

With the module built into the engine, `IMPORT://atome` works in the playground, the IDE and Acode without
installing anything.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ENGINE = ROOT / "exemples" / "ton618-lite.js"
MODULE = ROOT / "atome" / "module" / "atome.ton"
PATTERN = re.compile(r"(// <atome-module>[^\n]*\n)const ATOME_MODULE_SOURCE = .*?;\n(// </atome-module>)", re.S)


def main() -> int:
    text = ENGINE.read_text(encoding="utf-8")
    block = "const ATOME_MODULE_SOURCE = " + json.dumps(MODULE.read_text(encoding="utf-8"), ensure_ascii=False) + ";\n"
    new, n = PATTERN.subn(lambda m: m.group(1) + block + m.group(2), text)
    if n != 1:
        print("embed_atome: the <atome-module> block is missing in ton618-lite.js", file=sys.stderr)
        return 2
    if "--check" in sys.argv:
        if new != text:
            print("embed_atome: ton618-lite.js is out of date; run: python3 scripts/embed_atome.py", file=sys.stderr)
            return 1
        print("embed_atome: up to date")
        return 0
    ENGINE.write_text(new, encoding="utf-8")
    print("embed_atome: updated" if new != text else "embed_atome: already up to date")
    return 0


if __name__ == "__main__":
    sys.exit(main())
