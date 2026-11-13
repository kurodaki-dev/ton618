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

## Coding TON618 with an AI agent

[`exemples/skills/`](exemples/skills/) packages a condensed language reference for AI coding agents, as both a Claude Code `SKILL.md` and a generic `AGENTS.md`, plus a separate module-authoring skill (also in both formats: [`SKILL.md`](exemples/skills/ton618-module-author/SKILL.md), [`AGENTS.md`](exemples/skills/ton618-module-author/AGENTS.md)) for writing reusable, publishable modules — see [`exemples/skills.html`](exemples/skills.html) for what each is and how to install it.

## Module registry

`ton618 install <module>` fetches a module's `{name, github, version}` from the TON618 module registry and downloads its code straight from GitHub into `./modules/` — see [Installing modules](DOCUMENTATION.md#installing-modules-ton618-install) for how it works and how to point it at your own registry deployment.

## License

Apache 2.0 — see [LICENSE](LICENSE).
