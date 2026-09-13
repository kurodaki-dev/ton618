# ton618

A small interpreted programming language, written in C++. A programming language created by AI.

`.ton` scripts, the `ton.` prefix, a built-in HTTP server, a debugger, and a standard library — all in a single dependency-free binary that auto-detects whether it's running on Linux, Termux (Android), or Windows.

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
ton618 --version          # current version, platform, architecture
ton618 --update           # update to the latest release
ton618 --update beta-1.0.1 # pin to a specific release
ton618 --uninstall
```

No compiler required. Contributing to the interpreter itself and need to build from source instead? See [DOCUMENTATION.md](DOCUMENTATION.md#architecture--how-the-interpreter-works).

## Documentation

The full bilingual (EN/FR) reference — language basics, types, the standard library, the built-in HTTP server, the debugger, and how to extend the interpreter — lives in [DOCUMENTATION.md](DOCUMENTATION.md), also viewable as a static page at [`exemples/documentation.html`](exemples/documentation.html) (open it directly in a browser, no server needed).

Want it hosted at a public URL instead of a local file? See [Publishing the docs to GitHub Pages](DOCUMENTATION.md#publishing-the-docs-to-github-pages).

## Try it without installing

Open [`exemples/playground.html`](exemples/playground.html) in any browser for a live code editor — no install, no server, nothing sent anywhere.

## License

Apache 2.0 — see [LICENSE](LICENSE).
