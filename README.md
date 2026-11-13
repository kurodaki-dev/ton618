<p align="center"><img src="exemples/logo.svg" width="120" height="120" alt="TON618 logo"></p>

# ton618

A small interpreted programming language, written in C++. A programming language created by AI.

`.ton` scripts, the `ton.` prefix, a built-in HTTP server, a debugger, and a standard library — all in a single dependency-free binary that auto-detects whether it's running on Linux, Termux (Android), or Windows.

📖 **Documentation: https://c-ton618.duckdns.org/** (English & français) — built with [Claide](https://github.com/kurodaki-dev/Claide) from [DOCUMENTATION.md](DOCUMENTATION.md).

```
ton.class Player {
    ton.int score = 0
    ton.function init(name) { self.name = name }
    ton.function add(points) { self.score += points; return self }
}

ton.let players = ["kuro", "rusher", "natlep"].map(n => Player(n))
players[0].add(30).add(12)
for p in players.sorted(p => -p.score) {
    print(f"{p.name.padEnd(8)} {p.score}")
}
```

**New in beta-2.0.0:** classes with inheritance, method syntax on every value (`"hi".upper()`, `arr.filter(...).map(...)`), arrow functions, f-strings, ranges and slices, destructuring, `ton.let`/`ton.const`, the pipe operator `|>`, optional chaining `?.`, ~45 new standard functions, an interactive REPL, error messages with tracebacks — and an interpreter about **6x faster**. See the [changelog](DOCUMENTATION.md#changelog).

## Install

Precompiled binaries are built by [GitHub Actions](.github/workflows/release.yml) and published on [Releases](https://github.com/kurodaki-dev/ton618/releases). The installer auto-detects your platform:

```bash
# Linux / Termux
curl -fsSL https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.sh | bash
```

```powershell
# Windows
powershell -c "irm https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.ps1 | iex"
```

Then:

```bash
ton618 --help
ton618 script.ton
ton618                     # interactive REPL
ton618 -e 'print(2 ** 10)' # run a one-liner
ton618 --version          # current version, platform, architecture
ton618 --update           # update to the latest release
ton618 --update beta-1.0.4 # pin to a specific release
ton618 install <module>    # fetch a module from the TON618 module registry
ton618 uninstall <module>  # remove a module fetched with install
ton618 --uninstall-ton
```

No compiler required. Contributing to the interpreter itself and need to build from source instead? See [DOCUMENTATION.md](DOCUMENTATION.md#architecture--how-the-interpreter-works) — `make` builds it, `make test` runs the test suite.

## Documentation

The full bilingual (EN/FR) reference — language basics, types, classes, the standard library, the built-in HTTP server, the debugger, and how to extend the interpreter — lives in [DOCUMENTATION.md](DOCUMENTATION.md) and is published as a website at **https://c-ton618.duckdns.org/**.

The website is generated from that file by [`website/app.py`](website/app.py), a [Claide](https://github.com/kurodaki-dev/Claide) app (`pip install claide-web`, then `python website/app.py` to preview it), and deployed by [`.github/workflows/pages.yml`](.github/workflows/pages.yml) — see [Publishing the docs to GitHub Pages](DOCUMENTATION.md#publishing-the-docs-to-github-pages).

## Try it without installing

Open the [playground](https://c-ton618.duckdns.org/playground.html) (or [`exemples/playground.html`](exemples/playground.html) locally) for a live code editor — no install, no server, nothing sent anywhere. It supports the whole 2.0 language; every example in the documentation has a **Run** button that opens it there.

## IDE & editor extensions

Download from the [latest release](https://github.com/kurodaki-dev/ton618/releases/latest):

- **TON618 IDE** — Windows (`.exe`/`.msi`), Linux (`.AppImage`/`.deb`), macOS (`.dmg`), Android (`.apk`). A ~2 MB app ([Tauri](https://tauri.app)) with the interpreter built in: editor, ▶ Run, output, several files. Source: [`ide/`](ide/).
- **VS Code** — `ton618-vscode-*.vsix`: highlighting, snippets, run, REPL, syntax errors. Source: [`editors/vscode/`](editors/vscode/).
- **Acode** (Android) — `ton618-acode-plugin-*.zip`: highlighting, run with an output panel. Source: [`editors/acode/`](editors/acode/).

Everything — interpreter binaries for Linux, Windows, macOS and Termux included — is built by [GitHub Actions](.github/workflows/release.yml) and released automatically when `TON618_VERSION` in [`include/Version.hpp`](include/Version.hpp) changes.

## Coding TON618 with an AI agent

[`exemples/skills/`](exemples/skills/) packages a condensed language reference for AI coding agents, as both a Claude Code `SKILL.md` and a generic `AGENTS.md`, plus a separate module-authoring skill (also in both formats: [`SKILL.md`](exemples/skills/ton618-module-author/SKILL.md), [`AGENTS.md`](exemples/skills/ton618-module-author/AGENTS.md)) for writing reusable, publishable modules — see [`exemples/skills.html`](exemples/skills.html) for what each is and how to install it.

## Packages

`ton618` is also a package manager, like pip or npm:

```bash
ton618 install mathx          # from the registry, into ./modules/  (also: update, uninstall, list, search, info)
ton618 init mymodule          # start a module: creates tonproject.toml
ton618 build                  # -> dist/mymodule-0.1.0.tonpkg
ton618 publish                # share it (free account: ton618 register)
```

The registry is a small server with a SQLite database ([`server/`](server/README.md), made for a free VPS): accounts, API keys and packages (`ton618 register`, `ton618 publish`). Downloads are verified with SHA-256, versions support ranges (`^1.2`, `~1.2`, `>=1 <2`), and dependencies are installed with the package. Try `ton618 install requests`. See [Packages](DOCUMENTATION.md#packages-install-build--publish).

## License

The source code is under the **MIT** license ([LICENSE-MIT](LICENSE-MIT)) **or** Apache 2.0 ([LICENSE](LICENSE)), whichever you prefer.

The TON618 logo is **© 2026 kurodaki-dev, all rights reserved** and is not covered by those licenses — see [LOGO-LICENSE.md](LOGO-LICENSE.md).
