#!/usr/bin/env python3
"""Keeps the `atome` registry package in step with atome/module/atome.ton (the file the `atome` command and the kernel use).

    python3 scripts/sync_atome.py           copy the module into registry/sources/atome/
    python3 scripts/sync_atome.py --check   fail if the package source, or the newest built .tonpkg, differs (CI)

After a change: run it, bump `version` in registry/sources/atome/tonproject.toml, then
`cd registry/sources/atome && ../../../ton618 build` and move dist/atome-<version>.tonpkg to registry/packages/.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MODULE = ROOT / "atome" / "module" / "atome.ton"
SOURCE = ROOT / "registry" / "sources" / "atome" / "atome.ton"


def newest_package():
    best = None
    for f in (ROOT / "registry" / "packages").glob("atome-*.tonpkg"):
        v = tuple(int(x) for x in re.findall(r"\d+", f.stem.split("-", 1)[1]))
        if best is None or v > best[0]:
            best = (v, f)
    return best[1] if best else None


def main() -> int:
    text = MODULE.read_text(encoding="utf-8")
    if "--check" not in sys.argv:
        SOURCE.write_text(text, encoding="utf-8")
        print("sync_atome: registry/sources/atome/atome.ton updated")
        return 0
    if not SOURCE.exists() or SOURCE.read_text(encoding="utf-8") != text:
        print("sync_atome: registry/sources/atome/atome.ton differs from atome/module/atome.ton; run: python3 scripts/sync_atome.py", file=sys.stderr)
        return 1
    pkg = newest_package()
    if pkg is None or json.loads(pkg.read_text(encoding="utf-8"))["files"]["atome.ton"] != text:
        print("sync_atome: the newest registry/packages/atome-*.tonpkg is out of date: bump the version, rebuild and replace it (see this file's header)", file=sys.stderr)
        return 1
    print("sync_atome: up to date (" + pkg.name + ")")
    return 0


if __name__ == "__main__":
    sys.exit(main())
