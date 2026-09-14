---
name: ton618-module-author
description: Write a reusable TON618 (.ton) module meant to be imported by other scripts via IMPORT://, and optionally published to the TON618 module registry so others can fetch it with `ton618 install`. Use this whenever asked to create, package, or publish a TON618 module — as opposed to writing an ordinary script, which the "ton618" skill already covers.
---

# Authoring TON618 modules for AI coding agents

This skill is about **writing a module** — a `.ton` file meant to be `IMPORT://`ed by other scripts, possibly by people other than its author. If you're just writing an ordinary `.ton` script (not a module for reuse), use the general **`ton618`** skill instead; this file assumes you already know the language basics it covers and focuses only on the module-authoring conventions.

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

## Publishing to the TON618 module registry

Once a module is ready, put it in its own **public GitHub repository** with the module's file at the repo root, named exactly `<name>.ton` (e.g. a `stringutils` module needs `stringutils.ton` at the root of its repo — not nested in a subfolder). This is required because `ton618 install <name>` downloads `<name>.ton` straight from the repo's default branch — see `DOCUMENTATION.md` > "Installing modules: `ton618 install`".

The registry itself only stores **`{name, github, version}`** metadata (never the module's code) and is gated: only the registry's owner can add new entries, through a companion website with its own sign-in — an AI agent cannot register a module on someone else's behalf via API calls. If asked to "publish" a module, the correct final step is to tell the user their module is ready and point them at the registry's website to submit `{name, github link, version}` themselves — not to attempt any registry write directly.

Once listed, anyone can pull it down with:
```
ton618 install <name>
```
which fetches the registry entry, downloads `<name>.ton` from the linked GitHub repo into `./modules/<name>.ton`, and leaves it exactly where `IMPORT://<name>` already expects to find it.

## Gotchas specific to module authoring

1. **`IMPORT://name` has no `ton.` prefix** — that prefix means "this is a built-in module", not "this is a user module written by someone named ton". Never write `IMPORT://ton.stringutils` for a user module called `stringutils` — that will look for a built-in that doesn't exist.
2. **The registry install path assumes the file is at the repo root** — `owner/repo/<name>.ton`, not `owner/repo/src/<name>.ton` or `owner/repo/modules/<name>.ton`. A repo can contain other files (README, tests, LICENSE) alongside it; only the root placement and exact filename matter.
3. **A module can't have multiple independent instances.** Everything it declares lands in one shared global scope, so there's exactly one copy of its state per running script — don't design a module assuming callers can create several independently-configured instances of it.
4. **Don't shadow a built-in.** Since bare function calls resolve to whatever's in scope, a module accidentally exporting `push`, `len`, `map`, etc. (unprefixed) would shadow the real native function for every script that imports it — the prefix-everything rule above is what prevents this in practice.
