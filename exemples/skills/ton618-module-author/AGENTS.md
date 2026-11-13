# TON618 module authoring — instructions for AI coding agents

This file is about **writing a module** — a `.ton` file meant to be `IMPORT://`ed by other scripts, possibly by people other than its author. If you're just writing an ordinary `.ton` script (not a module for reuse), use the general TON618 `AGENTS.md` instead; this file assumes you already know the language basics it covers and focuses only on the module-authoring conventions.

Full docs: `DOCUMENTATION.md` in the project repo, or `https://github.com/kurodaki-dev/ton618`.

## What a module is

A TON618 module is just a normal `.ton` file. There is no `module`/`export`/`package` keyword and no namespace: every `ton.function` and `ton.<type>` variable a module declares gets pulled straight into the *importing script's global scope* the moment it's imported. `IMPORT://name` (no `ton.` prefix — that's reserved for the built-in modules) looks for `name.ton` in two places, in order:

1. The same directory as the script doing the importing.
2. A `modules/` subdirectory next to that script.

A module is only ever executed the first time it's imported anywhere in a run — importing the same name again is a silent no-op, so it's safe for two unrelated modules to both depend on a third and both `IMPORT://` it.

## The one rule that keeps modules from colliding: prefix everything

Since importing flattens a module's declarations into the caller's global scope with no namespacing, **every function a module exports must be prefixed with the module's own name** — this is the *only* thing standing between two modules (or a module and the main script) silently overwriting each other's functions. A module named `stringutils` should export `stringutils_slugify`, `stringutils_truncate`, etc. — never a bare `slugify`.

Internal helper functions a module doesn't intend callers to use directly should still get the prefix (there's no way to hide them), but consider a second underscore or a clear internal-sounding name (`stringutils__normalize`) so callers can tell "public API" from "implementation detail" at a glance.

## Worked example

`modules/greetings.ton`:
```
// greetings — simple example module. Exported: greetings_hello, greetings_bye.

ton.function greetings_hello(name, greeting = "Hello") {
    return greeting + ", " + name + "!"
}

ton.function greetings_bye(name) {
    return "Goodbye, " + name + "."
}
```

A script using it:
```
IMPORT://greetings

print(greetings_hello("kuro"))            // Hello, kuro!
print(greetings_hello("kuro", "Salut"))    // Salut, kuro!
print(greetings_bye("kuro"))
```

A module can use every language feature an ordinary script can — default/rest parameters, `??`, `in`, `try`/`catch`/`finally`, closures, arrays, dicts — and can `IMPORT://ton.<builtin>` or even `IMPORT://another_user_module` itself. It cannot take constructor arguments or be instantiated multiple times with different config; if a module needs configuration, expose a `thing_configure(options)` function the caller calls once after importing.

## Design checklist before calling a module "done"

1. **Every exported name is prefixed** with the module's own name (see above) — no bare identifiers leak into the caller's scope.
2. **One concern per module.** Compose several small modules (`stringutils`, `validation`, `formatting`) rather than one `utils` grab-bag — smaller modules are easier to prefix consistently and easier for a caller to audit.
3. **No side effects at import time** beyond declaring functions/constants — a module shouldn't read files, hit the network, or start a server just by being imported; make callers opt in explicitly by calling an `_init()`-style function if setup is genuinely needed.
4. **A short header comment** naming the module and listing its exported functions (see the worked example) — this is the only "documentation" a caller (human or AI) sees before reading the whole file.
5. **A companion smoke-test script** (`test_<name>.ton`, not shipped as part of the module itself) that imports the module and exercises every exported function — run it with `ton618 test_<name>.ton` before publishing. This is exactly the pattern `exemples/test_mathutils.ton` and `exemples/test_module_guide.ton` use in the TON618 repo itself.
6. **Runtime errors, not silent wrong answers.** If a function is called with a nonsensical argument (wrong type, out-of-range value), `throw` a clear message rather than returning `nil`/`0`/`""` — callers can `try`/`catch` it, but they can't recover from a wrong answer they didn't know was wrong.

## Publishing to the TON618 package registry (beta-2.1.0+)

A module is shared as a **package**. Turn the module's folder into a project, build it, and publish it:

```
ton618 init mathx          # tonproject.toml + mathx.ton + README.md (use the module's real name)
# edit tonproject.toml: description, authors, license, [dependencies] ...
ton618 build               # checks the manifest and every .ton file's syntax, writes dist/mathx-1.0.0.tonpkg
ton618 register            # once: create an account (saves an API key)
ton618 publish             # builds, checks, uploads to the registry
```

`tonproject.toml` is like `pyproject.toml`: `[project]` has `name` (2-40 chars, `a-z 0-9 - _`, starts with a letter), `version` (`1.2.3`), `description`, `authors`, `license`, `readme`, `main` (the file `IMPORT://<name>` loads, default `<name>.ton`), optional `repository`, `keywords`, `requires` (a ton618 version range such as `">=1.0.0"`); `[dependencies]` maps package names to version ranges (`"^1.0"`, `"~1.2"`, `">=1.0 <2"`, `"1.2.3"`); optional `[build]` has `include` / `exclude` patterns (default: every `.ton` file, README, LICENSE). Package files are text (`.ton .md .txt .json .csv`), at most 200 files, 256 KB each, 1 MB in all. Extra `.ton` files of a package are installed in `modules/<name>/` and imported as `IMPORT://<name>/<file>`.

**Rules for an AI agent:** `ton618 build` and `ton618 publish --dry-run` are safe to run. Creating the account (`ton618 register`) and the real `ton618 publish` belong to the user — it needs their password and uploads under their name; tell them their package is built and ready, and never publish for them. A published version never changes: to fix something, raise `version`.

Anyone installs it with:
```
ton618 install mathx            # newest; also mathx@1.2.0, "mathx@^1.2"
ton618 update                   # newest versions the dependants allow
ton618 uninstall mathx
```
Packages go into `./modules/`, where `IMPORT://mathx` finds them; downloads are checked against the registry's SHA-256. `ton618 search`, `info`, `list --outdated` explore the registry. The older registry (a GitHub link per module, `<name>.ton` at the repo root) is still used as a fallback for names that aren't in the package registry. `ton618 --uninstall-ton` removes the interpreter itself — not the same as `ton618 uninstall <name>`.

## Gotchas specific to module authoring

1. **`IMPORT://name` has no `ton.` prefix** — that prefix means "this is a built-in module", not "this is a user module written by someone named ton". Never write `IMPORT://ton.stringutils` for a user module called `stringutils` — that will look for a built-in that doesn't exist.
2. **The package's `main` file is what `IMPORT://<name>` loads** (default `<name>.ton`); its other `.ton` files are reached as `IMPORT://<name>/<file>`, so inside a package import siblings that way, not by bare file name. (For the older GitHub-link registry, the file must sit at the repo root as `<name>.ton`.)
3. **A module can't have multiple independent instances.** Everything it declares lands in one shared global scope, so there's exactly one copy of its state per running script — don't design a module assuming callers can create several independently-configured instances of it.
4. **Don't shadow a built-in.** Since bare function calls resolve to whatever's in scope, a module accidentally exporting `push`, `len`, `map`, etc. (unprefixed) would shadow the real native function for every script that imports it — the prefix-everything rule above is what prevents this in practice.
