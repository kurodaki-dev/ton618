#!/usr/bin/env python3
"""The TON618 package registry: checks the published packages and builds the site's index.

The registry is just a folder of files in this repository:

    registry/packages/<name>-<version>.tonpkg      one file per published version

A package is added by a pull request that adds such a file (made by
`ton618 build`, proposed by `ton618 publish`). This script is what the pull
request's check and the GitHub Pages build run:

    python3 scripts/registry.py validate [--base origin/main] [--ton618 ./ton618]
    python3 scripts/registry.py build OUT_DIR       # writes OUT_DIR/registry/index.json + the packages

`validate` refuses anything unsafe or malformed, and any change to a version
that was already published (published versions never change).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PACKAGES = ROOT / "registry" / "packages"

NAME_RE = re.compile(r"^[a-z][a-z0-9_-]{1,39}$")
RESERVED = {"ton", "modules", "ton618"}
VERSION_RE = re.compile(r"^(\d{1,9})\.(\d{1,9})\.(\d{1,9})(?:-([0-9A-Za-z.-]+))?$")
FILE_RE = re.compile(r"^(?P<name>[a-z][a-z0-9_-]{1,39})-(?P<version>\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?)\.tonpkg$")
RANGE_TOKEN_RE = re.compile(r"^(?:\*|x|latest|(?:\^|~|>=|<=|>|<|=)?\d+(?:\.(?:\d+|[xX*]))?(?:\.(?:\d+|[xX*]))?(?:-[0-9A-Za-z.-]+)?)$")
ALLOWED_EXT = {".ton", ".md", ".txt", ".json", ".csv"}
MAX_FILE = 256 * 1024
MAX_TOTAL = 1024 * 1024
MAX_FILES = 200


def version_key(version: str):
    """Sorts versions: 1.2.0 < 1.10.0, and 1.0.0-beta < 1.0.0."""
    m = VERSION_RE.match(version)
    if not m:
        return ((0, 0, 0), (0,))
    pre = m.group(4)
    nums = tuple(int(x) for x in m.groups()[:3])
    if pre is None:
        return (nums, (1,))
    parts = tuple((0, int(p), "") if p.isdigit() else (1, 0, p) for p in pre.split("."))
    return (nums, (0,) + parts)


def safe_path(path: str) -> bool:
    if not path or len(path) > 200 or path.startswith("/") or path.endswith("/"):
        return False
    if not re.fullmatch(r"[A-Za-z0-9_./-]+", path):
        return False
    if any(part in ("", ".", "..") or part.startswith(".") for part in path.split("/")):
        return False
    return Path(path).suffix.lower() in ALLOWED_EXT


class Package:
    def __init__(self, path: Path):
        self.path = path
        self.raw = path.read_bytes()
        self.sha256 = hashlib.sha256(self.raw).hexdigest()
        self.size = len(self.raw)
        self.data: dict = {}
        self.name = ""
        self.version = ""

    @property
    def file(self) -> str:
        return f"packages/{self.path.name}"


def load(folder: Path) -> tuple[list[Package], list[str]]:
    packages, errors = [], []
    for path in sorted(folder.glob("*")):
        if path.name == ".gitkeep" or path.name == "README.md":
            continue
        m = FILE_RE.match(path.name)
        if not m:
            errors.append(f"{path.name}: the file name must be <name>-<version>.tonpkg (as made by `ton618 build`)")
            continue
        package = Package(path)
        package.name, package.version = m.group("name"), m.group("version")
        try:
            package.data = json.loads(package.raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            errors.append(f"{path.name}: not valid JSON text ({exc})")
            continue
        packages.append(package)
    return packages, errors


def check_package(p: Package, known: set[str], ton618: str | None) -> list[str]:
    errors: list[str] = []
    fail = lambda message: errors.append(f"{p.path.name}: {message}")  # noqa: E731
    d = p.data
    if not isinstance(d, dict):
        return [f"{p.path.name}: the content must be a JSON object"]
    if d.get("format") != 1:
        fail("unsupported package format (expected 1)")
    if d.get("name") != p.name:
        fail(f"the name inside ({d.get('name')!r}) doesn't match the file name ({p.name!r})")
    if d.get("version") != p.version:
        fail(f"the version inside ({d.get('version')!r}) doesn't match the file name ({p.version!r})")
    if not NAME_RE.match(p.name) or p.name in RESERVED:
        fail("invalid package name (2-40 characters: lowercase letters, digits, '-' or '_', starting with a letter)")
    if not VERSION_RE.match(p.version):
        fail("the version must look like 1.2.3 or 1.2.3-beta.1")
    if not isinstance(d.get("description"), str) or not d["description"].strip():
        fail("a description is required")
    elif len(d["description"]) > 300:
        fail("the description is too long (300 characters at most)")
    if not isinstance(d.get("license"), str) or not d["license"].strip():
        fail("a license is required")
    if p.size > 2 * MAX_TOTAL:
        fail("the file is too large")

    files = d.get("files")
    main = d.get("main")
    if not isinstance(files, dict) or not files:
        fail("no files")
        return errors
    if len(files) > MAX_FILES:
        fail(f"too many files (more than {MAX_FILES})")
    total = 0
    for path, content in files.items():
        if not safe_path(path):
            fail(f"'{path}' is not an allowed file name (plain names, one of {', '.join(sorted(ALLOWED_EXT))})")
        elif not isinstance(content, str):
            fail(f"'{path}' must be text")
        else:
            total += len(content.encode("utf-8"))
            if len(content.encode("utf-8")) > MAX_FILE:
                fail(f"'{path}' is larger than {MAX_FILE // 1024} KB")
    if total > MAX_TOTAL:
        fail(f"the files add up to more than {MAX_TOTAL // 1024} KB")
    if not isinstance(main, str) or main not in files or not main.endswith(".ton"):
        fail("'main' must name one of the .ton files")

    deps = d.get("dependencies", {})
    if not isinstance(deps, dict):
        fail("'dependencies' must be an object")
    else:
        for dep, rng in deps.items():
            if not NAME_RE.match(dep):
                fail(f"dependency '{dep}' isn't a valid package name")
            elif dep == p.name:
                fail("a package can't depend on itself")
            elif dep not in known:
                fail(f"dependency '{dep}' isn't in the registry")
            if not isinstance(rng, str) or not all(RANGE_TOKEN_RE.match(t) for t in rng.replace(",", " ").split()):
                fail(f"the version range for '{dep}' isn't valid")

    if ton618 and not errors:  # syntax-check every .ton file with the real interpreter
        with tempfile.TemporaryDirectory() as tmp:
            for path, content in files.items():
                if path.endswith(".ton"):
                    target = Path(tmp) / path.replace("/", "_")
                    target.write_text(content, encoding="utf-8")
                    result = subprocess.run([ton618, "--check", str(target)], capture_output=True, text=True)
                    if result.returncode != 0:
                        fail(f"syntax error in {path}: {result.stderr.strip().splitlines()[-1] if result.stderr.strip() else 'invalid'}")
    return errors


def check_history(base: str | None) -> list[str]:
    """Versions already on the base branch must not be changed or removed."""
    if not base:
        return []
    result = subprocess.run(["git", "diff", "--name-status", f"{base}...HEAD", "--", "registry/packages"],
                            capture_output=True, text=True, cwd=ROOT)
    if result.returncode != 0:
        print(f"warning: couldn't compare with {base} ({result.stderr.strip()}); skipping the immutability check", file=sys.stderr)
        return []
    errors = []
    for line in result.stdout.splitlines():
        status, _, name = line.partition("\t")
        if status != "A" and name.endswith(".tonpkg"):
            errors.append(f"{Path(name).name}: a published version can't be changed or deleted (publish a new version instead)")
    return errors


def validate(folder: Path, base: str | None, ton618: str | None) -> tuple[list[Package], list[str]]:
    packages, errors = load(folder)
    known = {p.name for p in packages}
    for p in packages:
        errors += check_package(p, known, ton618)
    errors += check_history(base)
    return packages, errors


def build_index(packages: list[Package]) -> dict:
    by_name: dict[str, list[Package]] = {}
    for p in packages:
        by_name.setdefault(p.name, []).append(p)
    index: dict = {"format": 1, "packages": {}}
    for name in sorted(by_name):
        versions = sorted(by_name[name], key=lambda p: version_key(p.version))
        stable = [p for p in versions if "-" not in p.version] or versions
        latest = stable[-1]
        d = latest.data
        authors = d.get("authors") or []
        index["packages"][name] = {
            "description": d.get("description", ""),
            "author": ", ".join(authors) if isinstance(authors, list) else str(authors),
            "license": d.get("license", ""),
            "homepage": d.get("homepage", ""),
            "keywords": d.get("keywords", []),
            "latest": latest.version,
            "versions": {
                p.version: {
                    "sha256": p.sha256,
                    "size": p.size,
                    "file": p.file,
                    "dependencies": p.data.get("dependencies", {}),
                    "requires": p.data.get("requires", ""),
                }
                for p in versions
            },
        }
    return index


def write_site(packages: list[Package], site: Path) -> None:
    """Writes SITE/registry/index.json and SITE/registry/packages/*.tonpkg."""
    out = site / "registry"
    (out / "packages").mkdir(parents=True, exist_ok=True)
    for p in packages:
        (out / "packages" / p.path.name).write_bytes(p.raw)
    (out / "index.json").write_text(json.dumps(build_index(packages), indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("validate", "build"):
        s = sub.add_parser(name)
        s.add_argument("--packages", type=Path, default=PACKAGES, help="folder of .tonpkg files (default: registry/packages)")
        s.add_argument("--ton618", help="path of the ton618 interpreter, to syntax-check every .ton file")
        s.add_argument("--base", help="git ref of the main branch, to refuse changes to published versions")
        if name == "build":
            s.add_argument("out", type=Path, help="site folder; the registry goes in OUT/registry/")
    args = parser.parse_args()

    packages, errors = validate(args.packages, args.base, args.ton618)
    if errors:
        print("The registry has problems:", file=sys.stderr)
        for e in errors:
            print(f"  - {e}", file=sys.stderr)
        return 1
    if args.command == "validate":
        print(f"{len(packages)} package version(s) checked: all good.")
        return 0

    write_site(packages, args.out)
    print(f"registry/index.json: {len({p.name for p in packages})} package(s), {len(packages)} version(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
