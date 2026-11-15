#!/usr/bin/env python3
"""atome — build an operating system in TON.

    atome new myos            a new project (atome.toml + main.ton)
    atome check               syntax-check the project
    atome run                 open it in the browser (a graphical desktop)
    atome run --target kernel boot it on the Atome kernel in QEMU
    atome build               dist/web/index.html: the system as one web page / app shell
    atome build --target kernel   dist/kernel/atome.elf + program.ton
    atome build --target image    dist/<name>.iso: a bootable image (GRUB + the Atome kernel + your program)
    atome build --target app      dist/app: a Tauri project and a GitHub workflow that builds Android, Windows,
                                  Linux and macOS apps in the cloud
    atome doctor              what this computer can build

One program, two screens: the web target draws windows (ton.ui); the kernel and image targets run the very same
program in text mode on the Atome kernel. Standard library only.
"""
from __future__ import annotations

import argparse
import http.server
import json
import os
import re
import shutil
import socketserver
import subprocess
import sys
import tempfile
import threading
import webbrowser
from pathlib import Path

VERSION = "1.0.0"
HERE = Path(__file__).resolve().parent


# ------------------------------------------------------------------ where the parts of Atome are
def find_resources() -> dict:
    """The kernel sources, the atome module and the JavaScript engine: next to this file (a packaged
    atome) or in the repository layout."""
    candidates = [HERE / "res", HERE.parent, HERE.parent.parent]
    for base in candidates:
        kernel = base / "kernel" if (base / "kernel" / "boot.asm").exists() else None
        module = base / "module" / "atome.ton" if (base / "module" / "atome.ton").exists() else (base / "atome.ton" if (base / "atome.ton").exists() else None)
        engine = base / "ton618-lite.js" if (base / "ton618-lite.js").exists() else (base.parent / "exemples" / "ton618-lite.js" if (base.parent / "exemples" / "ton618-lite.js").exists() else None)
        if base.name == "atome" and engine is None and (base.parent / "exemples" / "ton618-lite.js").exists():
            engine = base.parent / "exemples" / "ton618-lite.js"
        if kernel and module and engine:
            return {"kernel": kernel, "module": module, "engine": engine}
    sys.exit("atome: can't find its own files (kernel, module, engine). Reinstall atome or run it from the repository.")


# ------------------------------------------------------------------ the project
def read_toml_project(path: Path) -> dict:
    text = path.read_text(encoding="utf-8")
    try:
        import tomllib  # Python 3.11+
        return tomllib.loads(text).get("project", {})
    except ImportError:
        data, inside = {}, False
        for raw in text.splitlines():
            line = raw.split("#", 1)[0].strip()
            if line.startswith("["):
                inside = line == "[project]"
            elif inside and "=" in line:
                k, v = line.split("=", 1)
                data[k.strip()] = v.strip().strip('"')
        return data


class Project:
    def __init__(self, root: Path):
        self.root = root
        manifest = root / "atome.toml"
        if not manifest.exists():
            sys.exit(f"atome: no atome.toml in {root} (run `atome new <name>` first, or go to your project's folder)")
        meta = read_toml_project(manifest)
        self.name = meta.get("name", root.name)
        self.title = meta.get("title", self.name)
        self.main = meta.get("main", "main.ton")
        if not re.fullmatch(r"[A-Za-z0-9_.\-]+", self.name):
            sys.exit("atome: the project name may only have letters, digits, '-', '_' and '.'")
        if not (root / self.main).exists():
            sys.exit(f"atome: the main file {self.main} doesn't exist")

    def ton_files(self) -> dict:
        files = {}
        for p in sorted(self.root.rglob("*.ton")):
            rel = p.relative_to(self.root).as_posix()
            if rel.startswith(("dist/", ".")):
                continue
            files[rel] = p.read_text(encoding="utf-8")
        return files


IMPORT_RE = re.compile(r"^\s*IMPORT://([A-Za-z0-9_./]+)(?:\s+as\s+\w+)?\s*$", re.M)


def bundle(project: Project, res: dict) -> str:
    """One TON source for the kernel (which has no imports): every imported module first, each once,
    then the main file. `ton.*` built-ins are dropped: the kernel has the functions it needs built in."""
    files = project.ton_files()
    order, seen = [], set()

    def lookup(name: str) -> str:
        if name == "atome":
            return res["module"].read_text(encoding="utf-8")
        for rel in (f"{name}.ton", f"modules/{name}.ton"):
            if rel in files:
                return files[rel]
        sys.exit(f"atome: IMPORT://{name} — no such module in the project")

    def visit(label: str, source: str):
        for m in IMPORT_RE.finditer(source):
            name = m.group(1)
            if name.startswith("ton.") or name in seen:
                continue
            seen.add(name)
            visit(name, lookup(name))
        order.append((label, source))

    visit(project.main, files[project.main])
    parts = []
    for label, source in order:
        parts.append(f"// ---- {label}\n{IMPORT_RE.sub('', source)}")
    return "\n".join(parts) + "\n"


def check_project(project: Project, res: dict) -> bool:
    """Syntax check with the real interpreter if it is installed (`ton618 --check`), else a light check."""
    ton = shutil.which("ton618")
    ok = True
    for rel, source in project.ton_files().items():
        if ton:
            with tempfile.NamedTemporaryFile("w", suffix=".ton", delete=False, encoding="utf-8") as f:
                f.write(source)
            r = subprocess.run([ton, "--check", f.name], capture_output=True, text=True)
            os.unlink(f.name)
            if r.returncode != 0:
                print(f"  {rel}: {r.stderr.strip().splitlines()[-1] if r.stderr.strip() else 'syntax error'}")
                ok = False
        elif source.count("{") != source.count("}") or source.count("(") != source.count(")"):
            print(f"  {rel}: unbalanced brackets")
            ok = False
    return ok


# ------------------------------------------------------------------ the web target
WEB_PAGE = """<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>__TITLE__</title>
<style>html,body{margin:0;height:100%;background:#000;overflow:hidden}#screen{position:fixed;inset:0}
#err{position:fixed;left:0;right:0;bottom:0;max-height:40%;overflow:auto;margin:0;padding:10px 14px;background:#7a1f1f;color:#fff;font:13px/1.5 monospace;white-space:pre-wrap;display:none}</style>
</head><body><div id="screen"></div><pre id="err"></pre>
<script>__ENGINE__</script>
<script>
(function () {
  var FILES = __FILES__, MAIN = __MAIN__, KEY = "atome:" + __NAME__;
  var storage = {};
  try { storage = JSON.parse(localStorage.getItem(KEY) || "{}"); } catch (e) {}
  var err = document.getElementById("err");
  function fail(text) { err.style.display = "block"; err.textContent += text + "\\n"; }
  var screen = Ton618Lite.mountUi(document.getElementById("screen"), function (ev) { if (session) session.dispatch(ev); });
  var session = null;
  var r = Ton618Lite.runTon618(FILES[MAIN], {
    files: FILES, storage: storage,
    onPrint: function (t) { if (window.console) console.log(t); },
    onUi: function (m) {
      if (m.type === "render") screen.render(m.html);
      else if (m.type === "title") document.title = m.text;
      else if (m.type === "storage") {
        if (m.value === null) delete storage[m.key]; else storage[m.key] = m.value;
        try { localStorage.setItem(KEY, JSON.stringify(storage)); } catch (e) {}
      } else if (m.type === "error") fail(m.text);
    }
  });
  if (!r.ok) fail(r.output);
  session = r.session;
  if (session) session.start();
})();
</script></body></html>
"""


def build_web(project: Project, res: dict, out: Path) -> Path:
    files = project.ton_files()
    files["atome.ton"] = res["module"].read_text(encoding="utf-8")
    page = (WEB_PAGE.replace("__TITLE__", project.title.replace("<", "&lt;"))
            .replace("__ENGINE__", res["engine"].read_text(encoding="utf-8").replace("</script", "<\\/script"))
            .replace("__FILES__", json.dumps(files).replace("</", "<\\/"))
            .replace("__MAIN__", json.dumps(project.main))
            .replace("__NAME__", json.dumps(project.name)))
    target = out / "web"
    target.mkdir(parents=True, exist_ok=True)
    (target / "index.html").write_text(page, encoding="utf-8")
    return target / "index.html"


# ------------------------------------------------------------------ the kernel and the image
def need(*tools: str):
    missing = [t for t in tools if not shutil.which(t)]
    if missing:
        sys.exit("atome: this target needs " + ", ".join(missing) + " (see `atome doctor`)")


def build_kernel(project: Project, res: dict, out: Path) -> tuple[Path, Path]:
    need("gcc", "nasm", "ld", "make")
    work = out / ".kbuild"
    if work.exists():
        shutil.rmtree(work)
    shutil.copytree(res["kernel"], work, ignore=shutil.ignore_patterns("*.o", "*.elf", "host_test.c"))
    r = subprocess.run(["make", "-C", str(work)], capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("atome: the kernel doesn't build:\n" + r.stdout + r.stderr)
    target = out / "kernel"
    target.mkdir(parents=True, exist_ok=True)
    shutil.copy(work / "atome.elf", target / "atome.elf")
    (target / "program.ton").write_text(bundle(project, res), encoding="utf-8")
    shutil.rmtree(work, ignore_errors=True)
    return target / "atome.elf", target / "program.ton"


GRUB_CFG = """set timeout=0
set default=0
menuentry "%s" {
    multiboot /boot/atome.elf
    module /boot/program.ton program
    boot
}
"""


def build_image(project: Project, res: dict, out: Path) -> Path:
    need("grub-mkrescue", "xorriso")
    elf, prog = build_kernel(project, res, out)
    iso_root = out / ".iso"
    if iso_root.exists():
        shutil.rmtree(iso_root)
    (iso_root / "boot" / "grub").mkdir(parents=True)
    shutil.copy(elf, iso_root / "boot" / "atome.elf")
    shutil.copy(prog, iso_root / "boot" / "program.ton")
    (iso_root / "boot" / "grub" / "grub.cfg").write_text(GRUB_CFG % project.title.replace('"', ""), encoding="utf-8")
    iso = out / f"{project.name}.iso"
    r = subprocess.run(["grub-mkrescue", "-o", str(iso), str(iso_root)], capture_output=True, text=True)
    shutil.rmtree(iso_root, ignore_errors=True)
    if r.returncode != 0 or not iso.exists():
        sys.exit("atome: grub-mkrescue failed:\n" + r.stdout + r.stderr)
    return iso


# ------------------------------------------------------------------ the app target (Tauri)
TAURI_CONF = {
    "$schema": "https://schema.tauri.app/config/2",
    "productName": "__TITLE__",
    "mainBinaryName": "__ID__",
    "version": "1.0.0",
    "identifier": "dev.atome.__ID__",
    "build": {"frontendDist": "../web"},
    "app": {"windows": [{"label": "main", "title": "__TITLE__", "width": 1100, "height": 760, "minWidth": 360, "minHeight": 400}], "security": {"csp": None}},
    "bundle": {"active": True, "targets": "all", "icon": ["icons/32x32.png", "icons/128x128.png", "icons/128x128@2x.png", "icons/icon.icns", "icons/icon.ico"]},
}
CARGO_TOML = """[package]
name = "__ID__"
version = "1.0.0"
edition = "2021"

[lib]
name = "__ID___lib"
crate-type = ["staticlib", "cdylib", "rlib"]

[build-dependencies]
tauri-build = { version = "2", features = [] }

[dependencies]
tauri = { version = "2", features = [] }

[profile.release]
opt-level = "s"
lto = true
codegen-units = 1
strip = true
panic = "abort"
"""
LIB_RS = """#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .run(tauri::generate_context!())
        .expect("error while running the app");
}
"""
MAIN_RS = """// Prevents an extra console window on Windows in release builds.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

fn main() {
    __ID___lib::run()
}
"""
PACKAGE_JSON = {"name": "__ID__", "private": True, "version": "1.0.0", "scripts": {"tauri": "tauri"}, "devDependencies": {"@tauri-apps/cli": "^2.2.0"}}
# The same recipe as the TON618 IDE's own release (which builds the Windows, Linux, macOS and Android apps).
WORKFLOW = """name: Build the apps
# Builds your system as an app for Windows, Linux, macOS and Android in the cloud (GitHub Actions).
# Run it from the Actions tab ("Run workflow"): the apps come out as downloadable artifacts.
on: { workflow_dispatch: {} }
jobs:
  desktop:
    strategy:
      fail-fast: false
      matrix: { os: [ubuntu-22.04, windows-latest, macos-latest] }
    runs-on: ${{ matrix.os }}
    defaults: { run: { working-directory: dist/app } }
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: 22 }
      - uses: dtolnay/rust-toolchain@stable
      - if: matrix.os == 'ubuntu-22.04'
        run: sudo apt-get update && sudo apt-get install -y libwebkit2gtk-4.1-dev libgtk-3-dev libayatana-appindicator3-dev librsvg2-dev patchelf
      - run: npm install
      - run: npx tauri icon icon.png
      - run: npx tauri build
      - uses: actions/upload-artifact@v4
        with:
          name: app-${{ matrix.os }}
          path: dist/app/src-tauri/target/release/bundle/**
          if-no-files-found: error
  android:
    runs-on: ubuntu-latest
    defaults: { run: { working-directory: dist/app } }
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: 22 }
      - uses: actions/setup-java@v4
        with: { distribution: temurin, java-version: 17 }
      - uses: dtolnay/rust-toolchain@stable
        with: { targets: "aarch64-linux-android,armv7-linux-androideabi" }
      - name: Android NDK
        run: |
          NDK="${ANDROID_NDK_LATEST_HOME:-${ANDROID_NDK_HOME:-}}"
          if [ -z "$NDK" ] || [ ! -d "$NDK" ]; then
            yes | "$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager" "ndk;27.2.12479018" > /dev/null
            NDK="$ANDROID_HOME/ndk/27.2.12479018"
          fi
          echo "NDK_HOME=$NDK" >> "$GITHUB_ENV"
      - run: |
          npm install
          npx tauri icon icon.png
          npx tauri android init --ci
      - run: npx tauri android build --apk --ci --target aarch64 armv7
      - name: Sign the APK with a throwaway key (use your own key to publish)
        run: |
          APK=$(find src-tauri/gen/android -name "*-release-unsigned.apk" | head -1)
          [ -n "$APK" ] || APK=$(find src-tauri/gen/android -name "*-release*.apk" | head -1)
          BT=$(ls -d "$ANDROID_HOME"/build-tools/* | sort -V | tail -1)
          keytool -genkeypair -keystore release.keystore -storepass atomekey -keypass atomekey -alias atome -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Atome app"
          mkdir -p out
          "$BT/zipalign" -f -p 4 "$APK" aligned.apk
          "$BT/apksigner" sign --ks release.keystore --ks-pass pass:atomekey --out out/app-android.apk aligned.apk
      - uses: actions/upload-artifact@v4
        with:
          name: app-android
          path: dist/app/out/*.apk
          if-no-files-found: error
"""


def make_icon_png(size: int = 1024) -> bytes:
    """A PNG icon made on the spot (a light ring on a dark square), so the generated app has an icon without shipping an image."""
    import struct
    import zlib
    rows = []
    c = (size - 1) / 2
    for y in range(size):
        row = bytearray([0])
        for x in range(size):
            d = ((x - c) ** 2 + (y - c) ** 2) ** 0.5
            ring = abs(d - size * 0.30) < size * 0.07
            dot = ((x - c * 1.25) ** 2 + (y - c * 0.75) ** 2) ** 0.5 < size * 0.06
            row += bytes((235, 235, 245, 255)) if ring or dot else bytes((14, 16, 24, 255))
        rows.append(bytes(row))

    def chunk(kind: bytes, data: bytes) -> bytes:
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b""))


def build_app(project: Project, res: dict, out: Path) -> Path:
    html = build_web(project, res, out)
    app = out / "app"
    if app.exists():
        shutil.rmtree(app)
    (app / "src-tauri" / "src").mkdir(parents=True)
    ident = re.sub(r"[^a-z0-9]", "", project.name.lower()) or "atomeos"
    if ident[0].isdigit():
        ident = "app" + ident
    conf = json.loads(json.dumps(TAURI_CONF).replace("__TITLE__", project.title.replace('"', "")).replace("__ID__", ident))
    (app / "src-tauri" / "tauri.conf.json").write_text(json.dumps(conf, indent=2), encoding="utf-8")
    (app / "src-tauri" / "Cargo.toml").write_text(CARGO_TOML.replace("__ID__", ident), encoding="utf-8")
    (app / "src-tauri" / "build.rs").write_text("fn main() { tauri_build::build() }\n", encoding="utf-8")
    (app / "src-tauri" / "src" / "lib.rs").write_text(LIB_RS, encoding="utf-8")
    (app / "src-tauri" / "src" / "main.rs").write_text(MAIN_RS.replace("__ID__", ident), encoding="utf-8")
    (app / "package.json").write_text(json.dumps(json.loads(json.dumps(PACKAGE_JSON).replace("__ID__", ident)), indent=2), encoding="utf-8")
    (app / "icon.png").write_bytes(make_icon_png())     # `tauri icon` makes every size from it
    (app / "src-tauri" / "icons").mkdir()
    web = app / "web"
    web.mkdir()
    shutil.copy(html, web / "index.html")
    wf = project.root / ".github" / "workflows"
    wf.mkdir(parents=True, exist_ok=True)
    (wf / "atome-build.yml").write_text(WORKFLOW, encoding="utf-8")
    return app


# ------------------------------------------------------------------ commands
TEMPLATE_MAIN = '''// __TITLE__: an operating system in TON, made with Atome.
// Press Play in Acode or the TON618 IDE, or run `atome run`.
IMPORT://atome

// An app is a few functions: init (optional), view (what is on screen), act (what buttons do).
ton.function hello_init(win) { win["s"]["n"] = 0 }
ton.function hello_view(win) {
    return [
        atome.title("Hello!"),
        atome.label("You pressed the button " + str(win["s"]["n"]) + " times."),
        atome.button("Press me", "press"),
        atome.input("name", "Your name (then Enter)"),
        atome.label("Hi " + atome.value(win, "name"))
    ]
}
ton.function hello_act(win, action, value) {
    if action == "press" { win["s"]["n"] += 1 }
}

atome.boot("__TITLE__")
atome.app({"id": "hello", "title": "Hello", "icon": "H", "view": hello_view, "init": hello_init, "act": hello_act})
atome.open("hello")
atome.run()
'''


def cmd_new(args):
    root = Path(args.name)
    if root.exists() and any(root.iterdir()):
        sys.exit(f"atome: {root} already exists and isn't empty")
    name = root.name
    if not re.fullmatch(r"[A-Za-z0-9_.\-]+", name):
        sys.exit("atome: the name may only have letters, digits, '-', '_' and '.'")
    root.mkdir(parents=True, exist_ok=True)
    title = args.title or name
    (root / "atome.toml").write_text(f'[project]\nname = "{name}"\ntitle = "{title}"\nmain = "main.ton"\n', encoding="utf-8")
    (root / "main.ton").write_text(TEMPLATE_MAIN.replace("__TITLE__", title), encoding="utf-8")
    (root / ".gitignore").write_text("dist/\n", encoding="utf-8")
    (root / "README.md").write_text(f"# {title}\n\nAn operating system in TON, made with [Atome](https://github.com/kurodaki-dev/ton618).\n\n"
                                    "```\natome run                    # the browser\natome run --target kernel    # boots on the Atome kernel in QEMU\n"
                                    "atome build --target image   # dist/%s.iso\n```\n" % name, encoding="utf-8")
    print(f"Created {root}/  (atome.toml, main.ton)\n  cd {root} && atome run")


def cmd_check(args):
    p, res = Project(Path(args.path)), find_resources()
    if check_project(p, res):
        print(f"{p.name}: OK")
    else:
        sys.exit(1)


class _Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a):
        pass


def cmd_run(args):
    p, res = Project(Path(args.path)), find_resources()
    out = Path(args.path) / "dist"
    if args.target == "web":
        html = build_web(p, res, out)
        os.chdir(html.parent)
        with socketserver.TCPServer(("127.0.0.1", args.port), _Handler) as srv:
            url = f"http://127.0.0.1:{srv.server_address[1]}/"
            print(f"{p.title} is running at {url}  (Ctrl+C to stop)")
            if not args.no_open:
                threading.Timer(0.4, lambda: webbrowser.open(url)).start()
            try:
                srv.serve_forever()
            except KeyboardInterrupt:
                pass
    elif args.target == "kernel":
        need("qemu-system-i386")
        elf, prog = build_kernel(p, res, out)
        cmd = ["qemu-system-i386", "-kernel", str(elf), "-initrd", str(prog)] + (["-display", "none", "-serial", "stdio"] if args.nographic else [])
        print("Booting " + p.title + " (close the QEMU window or press Ctrl+C to stop)")
        subprocess.call(cmd)
    elif args.target == "image":
        need("qemu-system-i386")
        iso = build_image(p, res, out)
        print("Booting " + str(iso))
        subprocess.call(["qemu-system-i386", "-cdrom", str(iso)] + (["-display", "none", "-serial", "stdio"] if args.nographic else []))
    else:
        sys.exit("atome: run supports --target web, kernel and image")


def cmd_build(args):
    p, res = Project(Path(args.path)), find_resources()
    if not check_project(p, res):
        sys.exit("atome: fix the errors above first")
    out = Path(args.path) / "dist"
    out.mkdir(exist_ok=True)
    if args.target == "web":
        print("Built", build_web(p, res, out), " (one self-contained page: host it anywhere, open it from a phone)")
    elif args.target == "kernel":
        elf, prog = build_kernel(p, res, out)
        print(f"Built {elf} and {prog}\n  qemu-system-i386 -kernel {elf} -initrd {prog}")
    elif args.target == "image":
        iso = build_image(p, res, out)
        print(f"Built {iso}  ({iso.stat().st_size // 1024} KB)\n  qemu-system-i386 -cdrom {iso}   (or write it to a USB stick / attach it to a VM)")
    elif args.target == "app":
        app = build_app(p, res, out)
        print(f"Generated {app}\n  A GitHub workflow was written to .github/workflows/atome-build.yml: push the project to GitHub and run it\n"
              f"  (Actions > Build the apps > Run workflow) to get the Windows, Linux, macOS and Android apps.\n"
              f"  Locally, with Node and Rust installed: cd {app} && npm install && npx tauri icon icon.png && npx tauri build")
    else:
        sys.exit("atome: unknown target")


def cmd_doctor(args):
    rows = [
        ("web (browser, Acode, IDE)", []),
        ("kernel", ["gcc", "nasm", "ld", "make"]),
        ("run --target kernel", ["qemu-system-i386"]),
        ("image (bootable ISO)", ["gcc", "nasm", "ld", "make", "grub-mkrescue", "xorriso"]),
        ("app (Tauri project)", []),
        ("building the apps here", ["cargo"]),
    ]
    for label, tools in rows:
        miss = [t for t in tools if not shutil.which(t)]
        print(f"  {'ok     ' if not miss else 'missing'}  {label}" + (f"   needs: {', '.join(miss)}" if miss else ""))
    res = find_resources()
    print("\nparts:", {k: str(v) for k, v in res.items()})


def main() -> int:
    ap = argparse.ArgumentParser(prog="atome", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--version", action="version", version="atome " + VERSION)
    sub = ap.add_subparsers(dest="command", required=True)
    s = sub.add_parser("new"); s.add_argument("name"); s.add_argument("--title")
    s.set_defaults(fn=cmd_new)
    s = sub.add_parser("check"); s.add_argument("path", nargs="?", default=".")
    s.set_defaults(fn=cmd_check)
    s = sub.add_parser("run"); s.add_argument("path", nargs="?", default=".")
    s.add_argument("--target", default="web", choices=["web", "kernel", "image"])
    s.add_argument("--port", type=int, default=0); s.add_argument("--no-open", action="store_true"); s.add_argument("--nographic", action="store_true")
    s.set_defaults(fn=cmd_run)
    s = sub.add_parser("build"); s.add_argument("path", nargs="?", default=".")
    s.add_argument("--target", default="web", choices=["web", "kernel", "image", "app"])
    s.set_defaults(fn=cmd_build)
    s = sub.add_parser("doctor"); s.set_defaults(fn=cmd_doctor)
    args = ap.parse_args()
    args.fn(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
