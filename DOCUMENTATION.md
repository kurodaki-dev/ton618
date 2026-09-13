# TON618 — Documentation

*A simple interpreted programming language, written in C++.*
*Un langage de programmation interprété simple, écrit en C++.*

---

## 🇬🇧 English

### Table of contents
1. [What is TON618](#what-is-ton618)
2. [Building the interpreter](#building-the-interpreter)
3. [Running a script](#running-a-script)
4. [The browser playground](#the-browser-playground)
5. [Language basics](#language-basics)
6. [Types](#types)
7. [Operators](#operators)
8. [Control flow](#control-flow)
9. [Functions](#functions)
10. [Arrays](#arrays)
11. [Dictionaries](#dictionaries)
12. [Error handling: try / catch / throw](#error-handling-try--catch--throw)
13. [Built-in native functions](#built-in-native-functions)
14. [HTML type](#html-type)
15. [Modules (IMPORT://)](#modules-import)
16. [Creating a TON618 module — full guide](#creating-a-ton618-module--full-guide)
17. [Built-in system modules (ton.sys, ton.os, ton.requests, ton.random, ton.time, ton.json, ton.mathutils, ton.strings)](#built-in-system-modules-tonsys-tonos-tonrequests-tonrandom-tontime-tonjson-tonmathutils-tonstrings)
18. [Local server & routes (API)](#local-server--routes-api)
19. [The debugger](#the-debugger)
20. [Architecture — how the interpreter works](#architecture--how-the-interpreter-works)
21. [Extending the interpreter itself](#extending-the-interpreter-itself)
22. [Publishing the docs to GitHub Pages](#publishing-the-docs-to-github-pages)
23. [Known limitations](#known-limitations)
24. [Changelog](#changelog)

---

### What is TON618

TON618 is a small interpreted language, similar in spirit to how Python or HolyC work: you write a `.ton` file, run it directly with the `ton618` interpreter, and it executes immediately — no separate compilation step for your scripts.

The interpreter itself (`ton618`) is written in C++ and must be compiled once per platform (Linux, Termux/Android, macOS, Windows). Once compiled, the resulting binary needs nothing else installed to run `.ton` scripts.

The language's defining feature is the **`ton.` prefix**: every variable and function is declared and used with `ton.` in front of it (`ton.x`, `ton.myFunction()`). This makes TON618 code immediately recognizable and keeps a consistent, explicit style.

TON618 now also supports **error handling** (`try`/`catch`/`throw`), a **dictionary type** (`ton.dict`), a **`for ... in` loop** for iterating arrays and dicts, **compound assignment** (`+=`, `-=`, ...), **increment/decrement** (`++`, `--`), a **ternary operator** (`cond ? a : b`), **function expressions** (anonymous callbacks you can pass to other functions), and a much larger standard library (math, string, array and dict helpers) — see the [Changelog](#changelog) for the full list.

---

### Building the interpreter

Requirements: a C++17 compiler (`g++` or `clang++`) and `make`.

```bash
# Termux
pkg install clang make

# Debian/Ubuntu
sudo apt install g++ make

# macOS (with Homebrew)
brew install make
```

Then, from the project root:

```bash
make
```

This produces a `ton618` executable in the project folder. `make clean` removes it.

If you edit this file (`DOCUMENTATION.md`), run `make docs` afterwards to regenerate `exemples/documentation.html` — that page embeds a copy of this file directly so it renders instantly with no server or network fetch, and `make docs` is what keeps that copy in sync.

If you're modifying the interpreter's C++ source, see [Architecture](#architecture--how-the-interpreter-works) and [Extending the interpreter itself](#extending-the-interpreter-itself) below — they explain how the pieces fit together and where to make specific kinds of changes.

**Or install a precompiled binary** — no compiler needed. `.github/workflows/release.yml` cross-compiles Linux x64, Windows x64, and Termux/Android arm64 binaries and publishes them on [GitHub Releases](https://github.com/kurodaki-dev/ton618/releases). The installers auto-detect which of the three you're on (Termux is told apart from a regular Linux via its `$PREFIX` environment variable) and add `ton618` to your `PATH`:

```bash
# Linux / Termux
curl -fsSL https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.sh | bash
```
```powershell
# Windows
powershell -c "irm https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.ps1 | iex"
```

Uninstall any time with `ton618 --uninstall`, or the matching `uninstall.sh`/`uninstall.ps1` script.

---

### Running a script

```bash
./ton618 path/to/script.ton
```

Options:
| Flag | Effect |
|---|---|
| `--debug` | Start in step-by-step debug mode |
| `--break=<line>` | Add a breakpoint at a given line (repeatable) |
| `-h`, `--help` | Show usage |
| `-v`, `--version` | Print the version, auto-detected platform (`linux`/`termux`/`windows`/`macos`), and architecture |
| `--update [version]` | Re-download the binary matching this platform from [GitHub Releases](https://github.com/kurodaki-dev/ton618/releases) and replace the running one — the latest release if `version` (a release tag) is omitted |
| `--uninstall` | Remove this ton618 install (binary + PATH entry) |

Example:
```bash
./ton618 exemples/test.ton --break=10 --break=20
./ton618 --update             # update to the latest release
./ton618 --update beta-1.0.0  # pin to a specific release tag
```

---

### The browser playground

Don't want to compile anything just to try a snippet? Open `exemples/playground.html` in any browser (no server needed) for a live code editor with a "Run" button and example snippets to load.

The playground runs **`ton618-lite.js`**, a JavaScript re-implementation of the language, entirely client-side — nothing is sent anywhere, and no installation is required. It covers the core language faithfully (types, operators, control flow including `for ... in`, functions and function expressions, arrays, dicts, `try`/`catch`/`throw`) plus the `ton.random`, `ton.time`, and `ton.json` built-in modules, since none of those need anything a browser can't already do.

What it can't do, because a browser sandbox has no filesystem, sockets, or CLI arguments to back them with:
- `ton.sys`, `ton.os`, `ton.requests` — importing any of these gives a clear error explaining why, instead of silently behaving differently from the real interpreter
- `IMPORT://<file>` for your own module files (no filesystem to read them from)
- `get`/`post`/`serve`, `readfile` (no HTTP server, no disk access)

For anything beyond quick experiments — and for the full standard library — install the real interpreter (see [Building the interpreter](#building-the-interpreter) above).

---

### Language basics

Every variable declaration requires an explicit type, prefixed with `ton.`:

```
ton.int x = 5
ton.string name = "kuro"
ton.bool active = true
```

Using a variable afterwards also requires the `ton.` prefix:

```
print(ton.x)
ton.x = ton.x + 1
```

Comments:
```
// single-line comment
/* multi-line
   comment */
```

---

### Types

| Type keyword | Meaning | Example |
|---|---|---|
| `ton.int` | Whole or decimal number (internally the same as float) | `ton.int x = 5` |
| `ton.float` | Decimal number (alias of int internally) | `ton.float pi = 3.14` |
| `ton.string` | Text | `ton.string s = "hello"` |
| `ton.bool` | `true` / `false` | `ton.bool ok = true` |
| `ton.array` | Ordered list of values | `ton.array a = [1, 2, 3]` |
| `ton.dict` | Key/value map, insertion-ordered | `ton.dict d = {name: "kuro"}` |
| `ton.html` | HTML content (for web responses) | `ton.html page = "<h1>Hi</h1>"` |

**Type checking is strict**: assigning a value of the wrong type raises a runtime error immediately.

```
ton.int x = "not a number"   // Error at line 1: Invalid type for 'ton.x': expected int/float but got string.
```

A plain string literal assigned to a `ton.html` variable is automatically treated as HTML — no need to convert it manually.

`ton.function` values (including function expressions, see [Functions](#functions)) are themselves a valid kind of value: you can hold one in a variable, pass it as an argument, or return it from another function.

---

### Operators

```
+  -  *  /  %              // arithmetic (+ also concatenates strings/html)
== !=  < <= > >=           // comparison
and  or  !                 // logical
=                          // assignment
+= -= *= /= %=             // compound assignment: ton.x += 1  is  ton.x = ton.x + 1
++  --                     // increment / decrement (postfix): ton.x++  is  ton.x += 1
?  :                       // ternary: cond ? whenTrue : whenFalse
```

Compound assignment and `++`/`--` work on plain variables and on indexed targets (`ton.arr[0]++`, `ton.dict["count"] += 1`).

The ternary operator lets you write a short conditional expression instead of a full `if`/`else`:

```
ton.int y = ton.x > 5 ? 100 : 200
```

---

### Control flow

**if / else**
```
if ton.x < 10 {
    print("small")
} else if ton.x < 100 {
    print("medium")
} else {
    print("big")
}
```
Parentheses around the condition are optional.

**while**
```
ton.int i = 0
while ton.i < 5 {
    print(ton.i)
    ton.i++
}
```

**for** (C-style)
```
for ton.int i = 0; i < 5; i++ {
    print(i)
}
```

**for ... in** (iterate an array or a dict)
```
ton.array names = ["kuro", "rusher", "natlep"]
for ton.n in names {
    print(n)
}

ton.dict scores = {kuro: 10, rusher: 20}
for ton.key in scores {
    print(key + " -> " + str(scores[key]))
}
```
Iterating a `ton.dict` walks its **keys**, in the order they were written; use `scores[key]` to read the matching value.

`break` and `continue` are supported inside all three loop forms.

---

### Functions

**Declared as a statement** (the classic form, gives the function a name in the enclosing scope):
```
ton.function add(a, b) {
    return a + b
}

print(ton.add(2, 3))   // 5
```

**Declared as an expression** (a "function literal"/callback, most useful when passing a function straight into another one, e.g. `map`/`filter`/`reduce`, or `ton.get`/`ton.post`):
```
ton.array doubled = map(numbers, ton.function(v) { return v * 2 })
```
A function expression may optionally carry a name (`ton.function double(v) { ... }`) purely for readability/debugging — that name is *not* bound as a variable the way a top-level declaration's name is.

- Called with `ton.name(args)`
- Functions are closures: they remember the environment they were defined in.
- Zero-parameter functions are allowed: `ton.function hello() { return "hi" }`

---

### Arrays

```
ton.array numbers = [1, 2, 3]
push(numbers, 4)          // adds an element
print(numbers[0])          // indexing
numbers[0] = 99             // mutation
print(len(numbers))         // size
```

Beyond `push`/`len`, the standard library has a full set of array helpers — see [Built-in native functions](#built-in-native-functions) for `pop`, `shift`, `unshift`, `slice`, `join`, `sort`, `reverse`, `indexOf`, `contains`, `map`, `filter`, `reduce`.

> Note: native functions like `push`/`len` currently work with or without the `ton.` prefix — using `ton.push(...)` / `ton.len(...)` is the recommended, consistent style.

---

### Dictionaries

`ton.dict` is a key/value map. Keys are always strings under the hood; a bare identifier key in a literal (`name: "kuro"`) is just shorthand for the string key `"name"` — use a quoted string key (`"first name": "kuro"`) when the key isn't a valid identifier.

```
ton.dict person = {name: "kuro", age: 21}

print(person["name"])     // kuro
person["age"] = 22        // mutation
person["city"] = "Paris"  // adding a new key

for ton.k in person {
    print(k + " = " + str(person[k]))
}

print(keys(person))        // [name, age, city]
print(values(person))      // [kuro, 22, Paris]
print(has(person, "age"))  // true
print(json(person))        // {"name":"kuro","age":22,"city":"Paris"}
```

Dicts print and JSON-encode in insertion order, so output is predictable and stable across runs.

---

### Error handling: try / catch / throw

Any runtime error — a native one (dividing by zero, indexing out of bounds, calling something that isn't a function...) or one you raise yourself with `throw` — can be caught with `try`/`catch` instead of crashing the whole script:

```
try {
    ton.int x = 10 / 0
} catch (ton.err) {
    print("Something went wrong: " + err)
}

ton.function safeDivide(a, b) {
    if b == 0 {
        throw "cannot divide by zero"
    }
    return a / b
}

try {
    print(ton.safeDivide(4, 0))
} catch (ton.e) {
    print("caught: " + e)
}
```

- `throw <expression>` raises an error carrying that expression's string form.
- The `catch (ton.name)` (or just `catch (name)`) variable always receives the error message as a `ton.string`.
- An uncaught error still stops the program and prints the error, exactly as before — `try`/`catch` is purely opt-in.
- `break`, `continue` and `return` inside a `try` block still work normally and are *not* intercepted by an enclosing `catch`.

---

### Built-in native functions

All of these are called through `ton.` (e.g. `ton.json(myArray)`), consistent with the rest of the language, though most also work without the prefix.

**Core**
| Function | Description |
|---|---|
| `print(x)` | Prints a value to stdout |
| `type(x)` | Returns the type name of a value as a string (`"int/float"`, `"string"`, `"array"`, `"dict"`, ...) |
| `str(x)` | Converts a value to string |
| `num(x)` | Converts a value to number |
| `json(x)` | Converts a value (array, dict, or scalar) to a JSON string |
| `assert(cond, [msg])` | Throws (catchable) if `cond` is falsy |
| `input([prompt])` | Prints an optional prompt, reads and returns one line from stdin |
| `readfile(path)` | Reads a file's content as `ton.html` (resolved relative to the script's directory too) |

**Math**
| Function | Description |
|---|---|
| `sqrt(x)`, `pow(x, y)`, `abs(x)` | Square root, power, absolute value |
| `floor(x)`, `ceil(x)`, `round(x)` | Rounding |
| `min(...)`, `max(...)` | Smallest/largest of any number of arguments |
| `random()` | A float in `[0, 1)` |
| `random(max)` | An integer in `[0, max)` |
| `random(min, max)` | An integer in `[min, max)` |

**Strings**
| Function | Description |
|---|---|
| `len(s)` | Length |
| `upper(s)`, `lower(s)` | Case conversion |
| `trim(s)` | Strips leading/trailing whitespace |
| `split(s, sep)` | Splits into a `ton.array` of strings |
| `join(arr, sep)` | Joins an array into a string (see also under Arrays) |
| `replace(s, search, repl)` | Replaces every occurrence of `search` with `repl` |
| `substring(s, start, [length])` | Extracts a substring |
| `contains(s, sub)` | Whether `sub` occurs in `s` |
| `indexOf(s, sub)` | Index of the first occurrence, or `-1` |

**Arrays**
| Function | Description |
|---|---|
| `len(arr)` | Number of elements |
| `push(arr, v)` | Appends a value (mutates in place) |
| `pop(arr)` | Removes and returns the last element |
| `shift(arr)` | Removes and returns the first element |
| `unshift(arr, v)` | Inserts a value at the front |
| `slice(arr, start, [end])` | Returns a new sub-array |
| `join(arr, sep)` | Joins elements into a string |
| `sort(arr)` | Sorts in place (numeric or alphabetical) and returns it |
| `reverse(arr)` | Reverses in place and returns it |
| `contains(arr, item)` | Whether `item` is an element |
| `indexOf(arr, item)` | Index of the first matching element, or `-1` |
| `map(arr, fn)` | New array of `fn(element)` for each element |
| `filter(arr, fn)` | New array keeping only elements where `fn(element)` is truthy |
| `reduce(arr, fn, initial)` | Folds the array via `fn(accumulator, element)` |
| `find(arr, fn)` | First element where `fn(element)` is truthy, or `nil` |
| `any(arr, fn)` | `true` if `fn(element)` is truthy for at least one element |
| `all(arr, fn)` | `true` if `fn(element)` is truthy for every element |

**Dicts**
| Function | Description |
|---|---|
| `len(d)` | Number of keys |
| `keys(d)` | Array of keys, in insertion order |
| `values(d)` | Array of values, in insertion order |
| `has(d, key)` | Whether `key` exists |
| `json(d)` | JSON-encodes the dict |

**HTTP server**
| Function | Description |
|---|---|
| `get(path, fn)` | Registers a GET route (see [Local server](#local-server--routes-api)) |
| `post(path, fn)` | Registers a POST route |
| `serve(port)` / `serve(port, content)` | Starts the local HTTP server |

---

### HTML type

`ton.html` marks content meant to be served as a web page:

```
ton.html page = "<h1>Welcome</h1><p>This is TON618.</p>"
```

A `ton.function` returning HTML will automatically be served with `Content-Type: text/html` when used as a route handler (see below).

---

### Modules (IMPORT://)

You can split code across files and import them:

```
IMPORT://math
```

This looks for `math.ton` next to your script, then in a `modules/` subfolder. All declarations from the module (variables, functions) become available in your script, still accessed with `ton.`:

**math.ton**
```
ton.function square(n) {
    return n * n
}
```

**main.ton**
```
IMPORT://math

print(ton.square(4))   // 16
```

Imports must appear before other code and are only loaded once (re-importing the same module is a no-op).

For a complete, hands-on guide to writing your own modules (conventions, a worked example, common pitfalls), see the next section.

---

### Creating a TON618 module — full guide

A **TON618 module** is nothing more than a regular `.ton` file, written so that other scripts can `IMPORT://` it and reuse what it declares. There is no special "module" keyword or wrapper — any `.ton` file can be imported, and any `.ton` file can `IMPORT://` another one.

This guide is about writing your *own* modules. TON618 also ships with a set of **built-in system modules** (`ton.sys`, `ton.os`, `ton.requests`, `ton.random`, `ton.time`, `ton.json`, `ton.mathutils`, `ton.strings`) that live inside the interpreter rather than on disk — see [Built-in system modules](#built-in-system-modules-tonsys-tonos-tonrequests-tonrandom-tontime-tonjson-tonmathutils-tonstrings) further down. They're imported the same way (`IMPORT://ton.sys`), and the same naming convention (prefix every function with the module's name) that this guide recommends is exactly the convention they follow.

#### 1. Where modules live

When a script runs `IMPORT://name`, the interpreter looks for the module in exactly two places, in this order (see `Interpreter::runImport` in `src/Interpreter.cpp`):

1. `<directory of the running script>/name.ton`
2. `<directory of the running script>/modules/name.ton`

So a project laid out like this:
```
myapp/
├── main.ton
└── modules/
    └── stringutils.ton
```
lets `main.ton` do `IMPORT://stringutils` and it will be found in `modules/`. A module placed directly next to the script (no `modules/` folder) works too — that's the first candidate path.

There is no nested module path (e.g. `IMPORT://utils/strings` doesn't work) and no package registry — modules are just files you keep in your project (or copy from somewhere else) and reference by their filename, without the `.ton` extension.

#### 2. What a module can declare

A module is executed once, into the **global scope**, exactly as if its statements had been pasted at the top of the importing script. That means a module can declare:

- `ton.function` declarations — the most common thing to share.
- `ton.int`/`ton.string`/`ton.bool`/`ton.array`/`ton.dict`/`ton.html` variables — shared constants or shared mutable state.
- Nested `IMPORT://` statements of its own (a module can depend on another module).

A module is **not** a separate namespace: everything it declares lands directly in the importing script's global scope, under the same `ton.` prefix as everything else. This means:

- Name collisions are possible. If your module declares `ton.function log(...)` and the importing script also declares `ton.function log(...)`, whichever runs last (in source order) wins. **Convention**: prefix your module's functions/variables with the module's own name to avoid collisions, e.g. a `stringutils` module should declare `ton.function stringutils_capitalize(s)` rather than a bare `ton.function capitalize(s)`.
- A module is only ever loaded once per program run, even if several files `IMPORT://` it (the interpreter tracks already-imported module names) — so it's safe for both `main.ton` and another module it depends on to import the same shared module without re-running its side effects twice.

#### 3. A worked example

**`modules/stringutils.ton`** — a small reusable string-helpers module:
```
// stringutils.ton — reusable string helpers.
// Convention: every name here is prefixed with "stringutils_" to avoid
// clashing with whatever the importing script (or another module) declares.

ton.function stringutils_capitalize(s) {
    if len(s) == 0 {
        return s
    }
    return upper(substring(s, 0, 1)) + substring(s, 1)
}

ton.function stringutils_titleCase(s) {
    ton.array words = split(s, " ")
    ton.array out = map(words, ton.function(w) { return stringutils_capitalize(w) })
    return join(out, " ")
}
```

**`main.ton`**:
```
IMPORT://stringutils

print(ton.stringutils_capitalize("kuro"))       // Kuro
print(ton.stringutils_titleCase("hello world")) // Hello World
```

Notice the module freely uses the rest of the standard library (`len`, `substring`, `upper`, `split`, `map`, `join`) — a module is ordinary TON618 code, nothing more.

#### 4. Best practices for writing a module

- **Prefix every declaration** with the module's name (`modulename_thing`) to avoid clobbering names in whatever script imports it.
- **Put it in `modules/`** if it's meant to be reused across several scripts in the project; keep it next to the script if it's a one-off split for readability.
- **Document the module's public functions** with a short comment above each one, stating what it expects and returns — since TON618 has no type-checked function signatures, this is the only contract a caller has.
- **Avoid heavy side effects at import time** (e.g. starting an HTTP server, printing banners) — a module should mostly *declare* things, and let the importing script decide when to actually call them. An exception is a module that defines shared constant data (e.g. `ton.dict config = {...}`), which is a reasonable thing to declare directly.
- **Keep `IMPORT://` statements at the very top of the file**, before any other statement — the parser doesn't strictly enforce this everywhere, but it matches how the language is meant to read and avoids surprises about what's in scope where.
- **Test a module standalone** by writing a tiny script next to it that imports it and calls each function once — since there's no unit-test framework, this is the practical way to catch mistakes.

---

### Built-in system modules (ton.sys, ton.os, ton.requests, ton.random, ton.time, ton.json, ton.mathutils, ton.strings)

Besides the module files you write yourself (see the guide above), TON618 ships with a handful of **built-in system modules**. They aren't files on disk — they live inside the interpreter — but you still have to `IMPORT://` them before using them, exactly like a user module:

```
IMPORT://ton.sys
IMPORT://ton.os
IMPORT://ton.requests
IMPORT://ton.random
IMPORT://ton.time
IMPORT://ton.json
```

Note the syntax: a **user** module is `IMPORT://name` (a `name.ton` file); a **built-in** module is `IMPORT://ton.name` (the leading `ton.` is how the parser tells the two apart — it's never a file lookup). Calling any of the functions below without first importing its module fails with an "undefined function" error — this is intentional: a script only pays for (and only exposes) the built-ins it actually asked for.

Every built-in module follows the same naming convention recommended for user modules in the guide above: every function is prefixed with the module's own name (`sys_...`, `os_...`, `requests_...`, `random_...`, `time_...`, `json_...`).

#### ton.sys — runtime & process information

| Function | Description |
|---|---|
| `sys_args()` | Array of extra command-line arguments passed after the script path (`./ton618 script.ton foo bar` → `["foo", "bar"]`) |
| `sys_platform()` | `"windows"`, `"termux"`, `"macos"`, or `"linux"` — auto-detected (Termux is told apart from plain Linux via its `$PREFIX` environment variable) |
| `sys_arch()` | `"x64"`, `"arm64"`, `"arm"`, `"x86"`, or `"unknown"` |
| `sys_version()` | The interpreter's own version string (e.g. `"beta-1.0.0"`), same as `ton618 --version` |
| `sys_exit(code)` | Stops the whole program immediately with the given exit code |
| `sys_sleep(ms)` | Pauses execution for the given number of milliseconds |

#### ton.os — environment variables & filesystem

| Function | Description |
|---|---|
| `os_name()` | `"nt"` on Windows, `"posix"` elsewhere |
| `os_getenv(name)` | An environment variable's value, or `nil` if unset |
| `os_setenv(name, value)` | Sets an environment variable for this process; returns whether it succeeded |
| `os_cwd()` | The current working directory |
| `os_exists(path)` | Whether a file or directory exists |
| `os_mkdir(path)` | Creates a directory (and any missing parent directories) |
| `os_remove(path)` | Removes a file or an empty directory |
| `os_listdir(path)` | Array of entry names directly inside a directory (not recursive) |
| `os_tempdir()` | The system's temporary-files directory |
| `os_copy(src, dst)` | Copies a file, overwriting `dst` if it already exists |

#### ton.requests — a minimal HTTP client

A small, dependency-free HTTP client (raw sockets, same spirit as the built-in server). **Limitation: `http://` only, no TLS/`https://`** — there is no bundled SSL library, so it can talk to local services, other TON618 servers, or any plain-HTTP endpoint, but not to an HTTPS-only API.

| Function | Description |
|---|---|
| `requests_get(url)` | Performs a GET request |
| `requests_post(url, [body])` | Performs a POST request with an optional string body |
| `requests_request(method, url, [body])` | Performs a request with any HTTP method |

All three return a `ton.dict` with the same shape:
```
{
    ok: true,          // whether a response was received at all
    status: 200,        // the HTTP status code (0 if the request failed outright)
    body: "...",         // the raw response body
    error: ""             // a human-readable reason when ok is false
}
```

Example:
```
IMPORT://ton.requests

ton.dict res = requests_get("http://localhost:8081/api/users")
if res["ok"] {
    print("Status: " + str(res["status"]))
    print(res["body"])
} else {
    print("Request failed: " + res["error"])
}
```

#### ton.random — extra randomness helpers

Complements the always-available `random()` (see [Built-in native functions](#built-in-native-functions)) with a few common patterns:

| Function | Description |
|---|---|
| `random_int(min, max)` | An integer in `[min, max]`, inclusive on both ends |
| `random_float()` | A float in `[0, 1)` |
| `random_choice(arr)` | A uniformly random element from a non-empty array |
| `random_shuffle(arr)` | Shuffles an array in place (Fisher-Yates) and returns it |
| `random_seed(n)` | Reseeds the random generator deterministically — useful for reproducible tests |

#### ton.time — clocks & timestamps

| Function | Description |
|---|---|
| `time_now()` | Seconds since the Unix epoch |
| `time_millis()` | Milliseconds since the Unix epoch — handy for measuring elapsed time |
| `time_string([timestamp])` | A human-readable local time string; defaults to now |
| `time_sleep(ms)` | Pauses execution for the given number of milliseconds (same as `sys_sleep`) |

#### ton.json — JSON parsing

The always-available `json(x)` native (see [Built-in native functions](#built-in-native-functions)) already turns a value *into* JSON. `IMPORT://ton.json` adds the other direction — parsing JSON text *into* a value — plus an explicitly-named alias for encoding:

| Function | Description |
|---|---|
| `json_parse(text)` | Parses a JSON string into a `ton.dict`/`ton.array`/scalar |
| `json_stringify(value)` | Same as `json(value)`, provided under a matching name for scripts that import this module |
| `json_pretty(value)` | Same as `json_stringify`, indented across multiple lines for human-readable output |

```
IMPORT://ton.json
IMPORT://ton.requests

ton.dict res = requests_get("http://localhost:8081/api/users")
ton.array users = json_parse(res["body"])
print(len(users))
```

#### ton.mathutils — trig, logs & stats helpers

Complements the always-available `sqrt`/`pow`/`abs`/`floor`/`ceil`/`round`/`min`/`max` (see [Built-in native functions](#built-in-native-functions)) with trigonometry, logarithms, and small statistics/number-theory helpers:

| Function | Description |
|---|---|
| `mathutils_pi()` / `mathutils_e()` | The constants π and e |
| `mathutils_sin/cos/tan(x)`, `mathutils_asin/acos/atan(x)`, `mathutils_atan2(y, x)` | Trigonometry, in radians |
| `mathutils_log(x)` / `mathutils_log2(x)` / `mathutils_log10(x)` / `mathutils_exp(x)` | Natural, base-2, base-10 logarithms, and e^x |
| `mathutils_hypot(x, y)` | `sqrt(x*x + y*y)`, computed without overflow |
| `mathutils_degrees(rad)` / `mathutils_radians(deg)` | Angle unit conversion |
| `mathutils_clamp(x, lo, hi)` | Restricts `x` to `[lo, hi]` |
| `mathutils_lerp(a, b, t)` | Linear interpolation between `a` and `b` at `t` (`0..1`) |
| `mathutils_sign(x)` | `-1`, `0`, or `1` |
| `mathutils_gcd(a, b)` / `mathutils_lcm(a, b)` | Greatest common divisor / least common multiple of two integers |
| `mathutils_factorial(n)` | `n!` for a non-negative integer `n` |
| `mathutils_is_prime(n)` | `true` if `n` is prime |
| `mathutils_sum(arr)` / `mathutils_mean(arr)` / `mathutils_median(arr)` / `mathutils_stddev(arr)` | Sum, mean, median, and population standard deviation of a numeric array |

```
IMPORT://ton.mathutils

print(mathutils_gcd(48, 18))          // 6
print(mathutils_mean([1, 2, 3, 4]))   // 2.5
print(mathutils_clamp(15, 0, 10))     // 10
```

#### ton.strings — extra string helpers

Complements the always-available `upper`/`lower`/`trim`/`split`/`replace`/`contains`/`substring` (see [Built-in native functions](#built-in-native-functions)):

| Function | Description |
|---|---|
| `strings_starts_with(s, prefix)` / `strings_ends_with(s, suffix)` | Prefix/suffix tests |
| `strings_repeat(s, n)` | Repeats `s` `n` times |
| `strings_reverse(s)` | Reverses a string (the always-available `reverse()` only works on arrays) |
| `strings_capitalize(s)` | Uppercases the first letter, lowercases the rest |
| `strings_pad_left(s, width, [ch])` / `strings_pad_right(s, width, [ch])` | Pads `s` up to `width` with `ch` (default `" "`) |
| `strings_count(s, sub)` | Number of non-overlapping occurrences of `sub` in `s` |
| `strings_trim_start(s)` / `strings_trim_end(s)` | Strip whitespace from only the start/end of `s` (`trim()` strips both) |

```
IMPORT://ton.strings

print(strings_capitalize("hello world"))  // "Hello world"
print(strings_pad_left("7", 3, "0"))      // "007"
```

---

### Local server & routes (API)

TON618 has a built-in HTTP server, no external dependencies (raw sockets).

**Simple mode** — same content for every request:
```
ton.serve(8080, "<h1>Hello from TON618!</h1>")
```

**Routing mode** — register handlers per path/method, then `serve(port)`:
```
ton.function home() {
    return "<h1>Home page</h1>"
}

ton.function api_users() {
    ton.array users = ["kuro", "rusher"]
    return ton.json(users)
}

ton.get("/", ton.home)
ton.get("/api/users", ton.api_users)

ton.serve(8081)
```

- `ton.get(path, handlerFunction)` — registers a GET route
- `ton.post(path, handlerFunction)` — registers a POST route
- Handlers take no arguments (for now) and return a string, html value, or JSON string
- Unregistered paths automatically return `404 Not Found`
- `serve()` blocks the program — it runs until you stop it (Ctrl+C)

---

### The debugger

Run with `--debug` or `--break=<line>` to activate it.

```bash
./ton618 script.ton --debug
./ton618 script.ton --break=12
```

Commands once paused:

| Command | Effect |
|---|---|
| `s`, `step` | Execute the next line (steps into function calls) |
| `c`, `continue` | Run until the next breakpoint |
| `b <line>` | Add a breakpoint |
| `rb <line>` | Remove a breakpoint |
| `p <var>` | Print a variable's value and type |
| `vars` | List all local variables |
| `bt` | Show the call stack |
| `l` | Show source code around the current line |
| `q` | Quit the program |

---

### Architecture — how the interpreter works

TON618 is a classic **tree-walking interpreter**, made of four stages that run in a straight pipeline:

```
source text (.ton file)
        │
        ▼
┌───────────────┐   turns characters into a flat list of Tokens
│     Lexer     │   (numbers, strings, identifiers, keywords, operators)
└───────┬───────┘
        ▼  tokens
┌───────────────┐   recursive-descent parsing: groups Tokens into an
│     Parser    │   Abstract Syntax Tree (a tree of Stmt/Expr nodes)
└───────┬───────┘   according to the language's grammar
        ▼  AST
┌───────────────┐   walks the AST directly and executes it — no bytecode,
│  Interpreter  │   no separate compilation step; each node is evaluated
└───────────────┘   the moment it's reached
```

```
ton618/
├── Makefile
├── include/              # headers (.hpp) — declarations and design notes
│   ├── Token.hpp           — the language's vocabulary (TokenType, Token)
│   ├── Lexer.hpp            — text -> tokens
│   ├── Ast.hpp                — the AST node shapes (Expr, Stmt)
│   ├── Parser.hpp               — tokens -> AST
│   ├── Value.hpp                  — runtime value representation
│   ├── Environment.hpp              — variable scopes (name -> Value, chained)
│   ├── Interpreter.hpp                — walks the AST, runs the program
│   ├── Debugger.hpp                    — breakpoints / step mode
│   ├── LocalServer.hpp                   — raw-socket HTTP server (ton.serve/get/post)
│   ├── HttpClient.hpp                      — raw-socket HTTP client (the ton.requests module)
│   └── JsonParser.hpp                        — JSON text -> Value (the ton.json module's json_parse)
├── src/                  # implementations (.cpp), one per header above,
│                           plus main.cpp (CLI entry point: reads a file,
│                           runs the Lexer -> Parser -> Interpreter pipeline,
│                           and wires up --debug / --break / --uninstall)
├── exemples/             # example .ton scripts, including a set that
│                           exercises every language feature (test_new_features.ton),
│                           plus documentation.html and playground.html (see below)
├── scripts/doc_templates/ # the head/tail HTML wrapped around DOCUMENTATION.md
│                           by `make docs` to produce exemples/documentation.html
└── modules/              # place shared .ton modules here (see the module guide above)
```

Every header starts with a short comment block explaining its role in the pipeline and, where relevant, exactly what to touch first if you want to extend that part of the language — read those before diving into the `.cpp` files.

Two design choices worth calling out because they shape almost every other file:

- **AST/Value nodes are "fat structs", not a class hierarchy.** `Expr` and `Stmt` (Ast.hpp) and `Value` (Value.hpp) each hold every field any of their variants might need, with a `type` tag saying which ones apply. This trades a bit of memory for a codebase where `evaluate()`/`execute()` are single `switch` statements instead of a scattered visitor pattern — much easier to read end-to-end for a language this size. Some node kinds *reuse* another kind's fields (documented next to the `enum`) instead of adding new ones, to keep the structs from growing unbounded.
- **Control flow uses C++ exceptions.** `break`, `continue`, and `return` are implemented as tiny exception types (`BreakException`, `ContinueException`, `ReturnException`, see Interpreter.hpp) thrown from `execute()` and caught by the nearest enclosing loop or function call. This mirrors, almost one-to-one, how those constructs unwind nested blocks in the AST — no manual "did we break?" flag needs to be threaded through every recursive call.

---

### Extending the interpreter itself

This section is for people modifying the C++ interpreter (not writing `.ton` scripts) — e.g. contributors.

**Adding a native function** (the easiest kind of extension — no grammar changes needed):
1. Open `src/Interpreter.cpp`, find `defineNatives()`.
2. Call the `def("name", [...](std::vector<Value>& args) -> Value { ... })` helper with your function's logic. Look at the existing math/string/array/dict sections for the pattern.
3. That's it — your function is immediately callable from any script as `ton.name(...)` (and, like the rest, without the prefix too).

**Adding a new built-in system module** (an `IMPORT://ton.<name>` module, like `ton.sys`/`ton.os`/`ton.requests`/`ton.random`/`ton.time`/`ton.json`) — the opt-in version of the above:
1. Declare a `void registerBuiltinX();` private method on `Interpreter` (`include/Interpreter.hpp`).
2. Implement it in `src/Interpreter.cpp`, near the other `registerBuiltin*` functions: it's the same `def("name_thing", ...)` pattern as a native function, just prefixed with the module's name and living in its own method instead of `defineNatives()`.
3. Wire it into the `moduleName.rfind("ton.", 0) == 0` branch of `Interpreter::runImport` so `IMPORT://ton.<name>` calls it.
4. Document it in the [Built-in system modules](#built-in-system-modules-tonsys-tonos-tonrequests-tonrandom-tontime-tonjson-tonmathutils-tonstrings) section above.

**Adding a new operator** (e.g. `**` for exponentiation):
1. Add a `TokenType` in `include/Token.hpp`.
2. Teach the Lexer to produce it in `src/Lexer.cpp` (`scanToken()`).
3. Hook it into the right precedence level in `src/Parser.cpp` (see the precedence chain documented at the top of `Parser.hpp`) and into the `BINARY`/`UNARY` handling in `Interpreter::evaluate` (`src/Interpreter.cpp`).

**Adding a new statement kind** (e.g. a `switch` statement):
1. Add a `StmtType` in `include/Ast.hpp`, and either reuse existing `Stmt` fields (preferred, see the comments next to the existing reused fields for examples) or add new ones if nothing fits.
2. Add a parsing method in `src/Parser.cpp` and dispatch to it from `statement()`.
3. Add the corresponding `case` in `Interpreter::execute` (`src/Interpreter.cpp`).

**Adding a new value/type kind** (e.g. a `ton.set`):
1. Add a `ValueType` and storage field in `include/Value.hpp`; extend `typeName()`, `toString()`, `toJson()`.
2. Add a `VarKind` in `include/Ast.hpp` and a `TokenType`/keyword (`Token.hpp`, `Lexer.cpp`) if it needs its own `ton.<type>` declaration syntax.
3. Extend `Interpreter::checkType` (`src/Interpreter.cpp`) so `ton.<type> x = ...` enforces it.
4. Add literal syntax in `Parser::primary()` if the type needs one (see how `ARRAY`/`DICT` literals are parsed there), and evaluation support in `Interpreter::evaluate`.

After any change, run `make` and sanity-check with the scripts in `exemples/` (in particular `exemples/test_new_features.ton`, which exercises most of the standard library and control-flow features) before relying on the change.

---

### Publishing the docs to GitHub Pages

`exemples/documentation.html` is fully static and self-contained (it embeds this very file, see `make docs` above) — it needs nothing but a web server, which makes GitHub Pages a good fit for hosting it at a public URL instead of only as a local file.

1. **GitHub → this repository → Settings → Pages.**
2. Under **Build and deployment**, set **Source** to **Deploy from a branch**.
3. Pick the **`main`** branch and the **`/ (root)`** folder, then **Save**.
4. GitHub publishes the whole repository as a static site at `https://<user>.github.io/ton618/` (adjust the org/user in that URL to whichever account owns this repo). The docs land at:
   ```
   https://<user>.github.io/ton618/exemples/documentation.html
   ```
5. *(Optional)* Add a root `index.html` that redirects to `exemples/documentation.html`, so the docs are reachable at the bare `https://<user>.github.io/ton618/` too:
   ```html
   <!doctype html><meta http-equiv="refresh" content="0; url=exemples/documentation.html">
   ```

Whenever `DOCUMENTATION.md` changes, run `make docs` and commit the regenerated `exemples/documentation.html` — GitHub Pages simply serves whatever is on `main`, so the published page updates on the next push.

Rather use GitHub Actions to build and deploy the page instead of serving the branch directly (e.g. to publish only `exemples/` at the site root)? Use the **"GitHub Pages Jekyll"**-free **"Static HTML"** starter workflow from the **Actions** tab, or the `actions/upload-pages-artifact` + `actions/deploy-pages` actions, pointing them at `exemples/` — see [GitHub's own guide](https://docs.github.com/en/pages) for that variant.

---

### Known limitations

- Route handlers can't yet read POST body or URL parameters (e.g. `/user/:id`)
- `serve()` is single-threaded and blocking
- `ton.int` and `ton.float` are currently the same internal number type (no integer-only enforcement)
- Dict keys are always strings (no numeric or nested-structural keys)
- `IMPORT://` only resolves a flat module name, no subfolders/namespacing
- No user-defined types/classes — only the built-in scalar/array/dict/html/function kinds
- `ton.requests` only speaks plain `http://` — no TLS/`https://`, no redirects, no chunked transfer-encoding

---

### Changelog

**beta-1.0.0** — first GitHub-distributed release:

- **Precompiled binaries are now built and published by GitHub Actions** (`.github/workflows/release.yml`), cross-compiling Linux x64, Windows x64, and Termux/Android arm64 on every tagged release, instead of on a privately-run server
- **`ton618 --update [version]`**: re-downloads the binary matching the current platform from GitHub Releases and replaces the running one in place — omit `version` for the latest release, or pass a release tag to pin/roll back
- **`ton618 --version` / `-v`**: prints the version, auto-detected platform, and architecture
- **Platform auto-detection**: `sys_platform()` now also recognizes `"termux"` (previously indistinguishable from `"linux"`); the new `sys_arch()` reports the CPU architecture and `sys_version()` the interpreter's own version — see [Platform.hpp](#extending-the-interpreter-itself)
- **New always-available array helpers**: `find(arr, fn)`, `any(arr, fn)`, `all(arr, fn)`
- **New filesystem helpers**: `os_tempdir()`, `os_copy(src, dst)`
- **New string helpers**: `strings_trim_start(s)`, `strings_trim_end(s)`
- **`json_pretty(value)`**: multi-line indented JSON output, alongside the existing `json_stringify`
- `scripts/install.sh` / `install.ps1` install from GitHub Releases instead of a self-hosted server; a `.github/workflows/ci.yml` builds and smoke-tests every push/PR

Recent additions to the language and standard library:

- **Compound assignment**: `+=`, `-=`, `*=`, `/=`, `%=`
- **Increment/decrement**: `++`, `--` (postfix, on variables and indexed targets)
- **Ternary operator**: `cond ? a : b`
- **`for ... in`** loop over arrays and dicts
- **`try` / `catch` / `throw`** error handling
- **`ton.dict`** type, with `{key: value, ...}` literals, `[]` indexing, and `keys()`/`values()`/`has()`
- **Function expressions**: `ton.function(params) { ... }` as an inline value, for callbacks
- **Much larger standard library**: `type`, `assert`, `input`, `sqrt`, `pow`, `abs`, `floor`, `ceil`, `round`, `min`, `max`, `random`, `upper`, `lower`, `trim`, `split`, `replace`, `substring`, `contains`, `indexOf`, `pop`, `shift`, `unshift`, `slice`, `join`, `sort`, `reverse`, `map`, `filter`, `reduce`, `keys`, `values`, `has`
- **A documented module-writing guide** (see [Creating a TON618 module](#creating-a-ton618-module--full-guide))
- **Eight built-in system modules**, each opt-in via `IMPORT://ton.<name>`: `ton.sys`, `ton.os`, `ton.requests` (a minimal `http://` client), `ton.random`, `ton.time`, `ton.json` (adds `json_parse` on top of the always-available `json()` encoder), `ton.mathutils` (trig/log/stats helpers), `ton.strings` (extra string helpers) — see [Built-in system modules](#built-in-system-modules-tonsys-tonos-tonrequests-tonrandom-tontime-tonjson-tonmathutils-tonstrings)
- **Header-level architecture comments** across every `.hpp` file, explaining each file's role and where to extend it
- **A browser playground** (`exemples/playground.html`, backed by `exemples/ton618-lite.js`) — a live code editor that runs TON618 scripts entirely client-side, no install or server required — see [The browser playground](#the-browser-playground)
- **`documentation.html` is now a live Markdown viewer**: it renders this very file directly (embedded at edit time via `make docs`) instead of a hand-maintained HTML transcript, so the page can no longer drift out of sync with the real docs

---
---

## 🇫🇷 Français

### Table des matières
1. [Qu'est-ce que TON618](#quest-ce-que-ton618)
2. [Compiler l'interpréteur](#compiler-linterpréteur)
3. [Lancer un script](#lancer-un-script)
4. [Le playground dans le navigateur](#le-playground-dans-le-navigateur)
5. [Bases du langage](#bases-du-langage)
6. [Types](#types-1)
7. [Opérateurs](#opérateurs)
8. [Structures de contrôle](#structures-de-contrôle)
9. [Fonctions](#fonctions)
10. [Tableaux](#tableaux)
11. [Dictionnaires](#dictionnaires)
12. [Gestion des erreurs : try / catch / throw](#gestion-des-erreurs--try--catch--throw)
13. [Fonctions natives](#fonctions-natives)
14. [Type HTML](#type-html)
15. [Modules (IMPORT://)](#modules-import-1)
16. [Créer un module TON618 — guide complet](#créer-un-module-ton618--guide-complet)
17. [Modules système intégrés (ton.sys, ton.os, ton.requests, ton.random, ton.time, ton.json, ton.mathutils, ton.strings)](#modules-système-intégrés-tonsys-tonos-tonrequests-tonrandom-tontime-tonjson-tonmathutils-tonstrings)
18. [Serveur local & routes (API)](#serveur-local--routes-api)
19. [Le débogueur](#le-débogueur)
20. [Architecture — comment fonctionne l'interpréteur](#architecture--comment-fonctionne-linterpréteur)
21. [Étendre l'interpréteur lui-même](#étendre-linterpréteur-lui-même)
22. [Publier la doc sur GitHub Pages](#publier-la-doc-sur-github-pages)
23. [Limitations connues](#limitations-connues)
24. [Changelog](#changelog-1)

---

### Qu'est-ce que TON618

TON618 est un petit langage interprété, dans le même esprit que Python ou HolyC : tu écris un fichier `.ton`, tu le lances directement avec l'interpréteur `ton618`, et il s'exécute immédiatement — pas d'étape de compilation séparée pour tes scripts.

L'interpréteur lui-même (`ton618`) est écrit en C++ et doit être compilé une fois par plateforme (Linux, Termux/Android, macOS, Windows). Une fois compilé, le binaire obtenu n'a besoin de rien d'autre pour exécuter des scripts `.ton`.

La caractéristique principale du langage est le **préfixe `ton.`** : chaque variable et fonction se déclare et s'utilise avec `ton.` devant (`ton.x`, `ton.maFonction()`). Ça rend le code TON618 immédiatement reconnaissable et garde un style cohérent et explicite.

TON618 supporte maintenant aussi la **gestion d'erreurs** (`try`/`catch`/`throw`), un **type dictionnaire** (`ton.dict`), une **boucle `for ... in`** pour parcourir tableaux et dictionnaires, l'**assignation composée** (`+=`, `-=`, ...), l'**incrémentation/décrémentation** (`++`, `--`), un **opérateur ternaire** (`cond ? a : b`), les **expressions de fonction** (callbacks anonymes passables à d'autres fonctions), et une bibliothèque standard bien plus large (math, string, array) — voir le [Changelog](#changelog-1) pour la liste complète.

---

### Compiler l'interpréteur

Prérequis : un compilateur C++17 (`g++` ou `clang++`) et `make`.

```bash
# Termux
pkg install clang make

# Debian/Ubuntu
sudo apt install g++ make

# macOS (avec Homebrew)
brew install make
```

Puis, depuis la racine du projet :

```bash
make
```

Ça produit un exécutable `ton618` dans le dossier du projet. `make clean` le supprime.

Si tu modifies ce fichier (`DOCUMENTATION.md`), lance `make docs` ensuite pour régénérer `exemples/documentation.html` — cette page embarque une copie de ce fichier directement pour s'afficher instantanément sans serveur ni requête réseau, et `make docs` est ce qui garde cette copie synchronisée.

Si tu modifies le code source C++ de l'interpréteur, voir [Architecture](#architecture--comment-fonctionne-linterpréteur) et [Étendre l'interpréteur lui-même](#étendre-linterpréteur-lui-même) plus bas — ces sections expliquent comment les pièces s'assemblent et où faire quel genre de modification.

**Ou installe un binaire précompilé** — aucun compilateur requis. `.github/workflows/release.yml` compile en croisé les binaires Linux x64, Windows x64 et Termux/Android arm64, et les publie sur les [GitHub Releases](https://github.com/kurodaki-dev/ton618/releases). Les installeurs détectent automatiquement lequel des trois tu utilises (Termux est distingué d'un Linux classique via sa variable d'environnement `$PREFIX`) et ajoutent `ton618` à ton `PATH` :

```bash
# Linux / Termux
curl -fsSL https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.sh | bash
```
```powershell
# Windows
powershell -c "irm https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.ps1 | iex"
```

Désinstalle à tout moment avec `ton618 --uninstall`, ou les scripts `uninstall.sh`/`uninstall.ps1` correspondants.

---

### Lancer un script

```bash
./ton618 chemin/vers/script.ton
```

Options :
| Option | Effet |
|---|---|
| `--debug` | Démarre en mode débogage pas-à-pas |
| `--break=<ligne>` | Ajoute un point d'arrêt à une ligne donnée (répétable) |
| `-h`, `--help` | Affiche l'aide |
| `-v`, `--version` | Affiche la version, la plateforme détectée (`linux`/`termux`/`windows`/`macos`) et l'architecture |
| `--update [version]` | Retélécharge le binaire correspondant à cette plateforme depuis les [GitHub Releases](https://github.com/kurodaki-dev/ton618/releases) et remplace celui en cours d'exécution — la dernière release si `version` (un tag de release) est omis |
| `--uninstall` | Désinstalle ton618 (binaire + entrée PATH) |

Exemple :
```bash
./ton618 exemples/test.ton --break=10 --break=20
./ton618 --update             # met à jour vers la dernière release
./ton618 --update beta-1.0.0  # épingle une release précise
```

---

### Le playground dans le navigateur

Pas envie de compiler quoi que ce soit juste pour essayer un extrait de code ? Ouvre `exemples/playground.html` dans n'importe quel navigateur (pas besoin de serveur) pour un éditeur de code en direct avec un bouton "Run" et des exemples prêts à charger.

Le playground exécute **`ton618-lite.js`**, une réimplémentation JavaScript du langage, entièrement côté client — rien n'est envoyé nulle part, et aucune installation n'est nécessaire. Il couvre fidèlement le cœur du langage (types, opérateurs, structures de contrôle dont `for ... in`, fonctions et expressions de fonction, tableaux, dictionnaires, `try`/`catch`/`throw`) ainsi que les modules intégrés `ton.random`, `ton.time`, et `ton.json`, puisqu'aucun d'eux n'a besoin de quoi que ce soit qu'un navigateur ne puisse déjà faire.

Ce qu'il ne peut pas faire, parce qu'un bac à sable navigateur n'a ni système de fichiers, ni sockets, ni arguments de ligne de commande pour les alimenter :
- `ton.sys`, `ton.os`, `ton.requests` — importer l'un d'eux donne une erreur claire expliquant pourquoi, plutôt que de se comporter silencieusement différemment du vrai interpréteur
- `IMPORT://<fichier>` pour tes propres fichiers de modules (pas de système de fichiers pour les lire)
- `get`/`post`/`serve`, `readfile` (pas de serveur HTTP, pas d'accès disque)

Pour tout ce qui dépasse de rapides expérimentations — et pour la bibliothèque standard complète — installe le vrai interpréteur (voir [Compiler l'interpréteur](#compiler-linterpréteur) ci-dessus).

---

### Bases du langage

Chaque déclaration de variable nécessite un type explicite, préfixé par `ton.` :

```
ton.int x = 5
ton.string name = "kuro"
ton.bool active = true
```

Utiliser une variable ensuite nécessite aussi le préfixe `ton.` :

```
print(ton.x)
ton.x = ton.x + 1
```

Commentaires :
```
// commentaire sur une ligne
/* commentaire
   multi-lignes */
```

---

### Types

| Mot-clé de type | Signification | Exemple |
|---|---|---|
| `ton.int` | Nombre entier ou décimal (même type que float en interne) | `ton.int x = 5` |
| `ton.float` | Nombre décimal (alias de int en interne) | `ton.float pi = 3.14` |
| `ton.string` | Texte | `ton.string s = "salut"` |
| `ton.bool` | `true` / `false` | `ton.bool ok = true` |
| `ton.array` | Liste ordonnée de valeurs | `ton.array a = [1, 2, 3]` |
| `ton.dict` | Table clé/valeur, ordonnée par insertion | `ton.dict d = {name: "kuro"}` |
| `ton.html` | Contenu HTML (pour les réponses web) | `ton.html page = "<h1>Salut</h1>"` |

**La vérification de type est stricte** : assigner une valeur du mauvais type déclenche immédiatement une erreur à l'exécution.

```
ton.int x = "pas un nombre"   // Error at line 1: Invalid type for 'ton.x': expected int/float but got string.
```

Une string littérale assignée à une variable `ton.html` est automatiquement traitée comme du HTML — pas besoin de la convertir manuellement.

Les valeurs `ton.function` (y compris les expressions de fonction, voir [Fonctions](#fonctions)) sont elles-mêmes un type de valeur à part entière : tu peux la stocker dans une variable, la passer en argument, ou la retourner depuis une autre fonction.

---

### Opérateurs

```
+  -  *  /  %              // arithmétique (+ concatène aussi les strings/html)
== !=  < <= > >=           // comparaison
and  or  !                 // logique
=                          // assignation
+= -= *= /= %=             // assignation composée : ton.x += 1  vaut  ton.x = ton.x + 1
++  --                     // incrémentation / décrémentation (postfixe) : ton.x++ vaut ton.x += 1
?  :                       // ternaire : cond ? siVrai : siFaux
```

L'assignation composée et `++`/`--` marchent sur de simples variables ainsi que sur des cibles indexées (`ton.arr[0]++`, `ton.dict["count"] += 1`).

L'opérateur ternaire permet d'écrire une courte expression conditionnelle sans un `if`/`else` complet :

```
ton.int y = ton.x > 5 ? 100 : 200
```

---

### Structures de contrôle

**if / else**
```
if ton.x < 10 {
    print("petit")
} else if ton.x < 100 {
    print("moyen")
} else {
    print("grand")
}
```
Les parenthèses autour de la condition sont optionnelles.

**while**
```
ton.int i = 0
while ton.i < 5 {
    print(ton.i)
    ton.i++
}
```

**for** (façon C)
```
for ton.int i = 0; i < 5; i++ {
    print(i)
}
```

**for ... in** (parcourt un tableau ou un dictionnaire)
```
ton.array names = ["kuro", "rusher", "natlep"]
for ton.n in names {
    print(n)
}

ton.dict scores = {kuro: 10, rusher: 20}
for ton.key in scores {
    print(key + " -> " + str(scores[key]))
}
```
Parcourir un `ton.dict` parcourt ses **clés**, dans l'ordre où elles ont été écrites ; utilise `scores[key]` pour lire la valeur associée.

`break` et `continue` sont supportés dans les trois formes de boucle.

---

### Fonctions

**Déclarée comme une instruction** (la forme classique, donne un nom à la fonction dans la portée englobante) :
```
ton.function add(a, b) {
    return a + b
}

print(ton.add(2, 3))   // 5
```

**Déclarée comme une expression** (un "littéral de fonction"/callback, surtout utile pour passer une fonction directement à une autre, par ex. `map`/`filter`/`reduce`, ou `ton.get`/`ton.post`) :
```
ton.array doubled = map(numbers, ton.function(v) { return v * 2 })
```
Une expression de fonction peut porter un nom optionnel (`ton.function double(v) { ... }`) uniquement pour la lisibilité/le débogage — ce nom n'est *pas* lié comme variable, contrairement au nom d'une déclaration de premier niveau.

- Appelées avec `ton.nom(args)`
- Les fonctions sont des closures : elles se souviennent de l'environnement dans lequel elles ont été définies.
- Les fonctions sans paramètre sont autorisées : `ton.function hello() { return "hi" }`

---

### Tableaux

```
ton.array numbers = [1, 2, 3]
push(numbers, 4)          // ajoute un élément
print(numbers[0])          // indexation
numbers[0] = 99             // modification
print(len(numbers))         // taille
```

Au-delà de `push`/`len`, la bibliothèque standard offre un ensemble complet d'outils pour les tableaux — voir [Fonctions natives](#fonctions-natives) pour `pop`, `shift`, `unshift`, `slice`, `join`, `sort`, `reverse`, `indexOf`, `contains`, `map`, `filter`, `reduce`.

> Remarque : les fonctions natives comme `push`/`len` fonctionnent actuellement avec ou sans le préfixe `ton.` — utiliser `ton.push(...)` / `ton.len(...)` est le style recommandé et cohérent.

---

### Dictionnaires

`ton.dict` est une table clé/valeur. Les clés sont toujours des strings en interne ; une clé identifiant nue dans un littéral (`name: "kuro"`) est juste un raccourci pour la clé string `"name"` — utilise une clé string entre guillemets (`"first name": "kuro"`) quand la clé n'est pas un identifiant valide.

```
ton.dict person = {name: "kuro", age: 21}

print(person["name"])     // kuro
person["age"] = 22        // modification
person["city"] = "Paris"  // ajout d'une nouvelle clé

for ton.k in person {
    print(k + " = " + str(person[k]))
}

print(keys(person))        // [name, age, city]
print(values(person))      // [kuro, 22, Paris]
print(has(person, "age"))  // true
print(json(person))        // {"name":"kuro","age":22,"city":"Paris"}
```

Les dictionnaires s'affichent et se sérialisent en JSON dans l'ordre d'insertion, donc la sortie est prévisible et stable d'une exécution à l'autre.

---

### Gestion des erreurs : try / catch / throw

Toute erreur d'exécution — native (division par zéro, index hors limites, appel de quelque chose qui n'est pas une fonction...) ou levée soi-même avec `throw` — peut être capturée avec `try`/`catch` au lieu de faire planter tout le script :

```
try {
    ton.int x = 10 / 0
} catch (ton.err) {
    print("Un problème est survenu : " + err)
}

ton.function safeDivide(a, b) {
    if b == 0 {
        throw "division par zero impossible"
    }
    return a / b
}

try {
    print(ton.safeDivide(4, 0))
} catch (ton.e) {
    print("attrape : " + e)
}
```

- `throw <expression>` lève une erreur portant la forme string de cette expression.
- La variable `catch (ton.nom)` (ou juste `catch (nom)`) reçoit toujours le message d'erreur sous forme de `ton.string`.
- Une erreur non capturée arrête toujours le programme et affiche l'erreur, exactement comme avant — `try`/`catch` est purement optionnel.
- `break`, `continue` et `return` à l'intérieur d'un bloc `try` fonctionnent normalement et ne sont *pas* interceptés par un `catch` englobant.

---

### Fonctions natives

Toutes s'appellent via `ton.` (ex : `ton.json(monTableau)`), cohérent avec le reste du langage, bien que la plupart fonctionnent aussi sans le préfixe.

**Cœur**
| Fonction | Description |
|---|---|
| `print(x)` | Affiche une valeur sur la sortie standard |
| `type(x)` | Retourne le nom du type d'une valeur sous forme de string (`"int/float"`, `"string"`, `"array"`, `"dict"`, ...) |
| `str(x)` | Convertit une valeur en string |
| `num(x)` | Convertit une valeur en nombre |
| `json(x)` | Convertit une valeur (tableau, dict, ou scalaire) en string JSON |
| `assert(cond, [msg])` | Lève une erreur (attrapable) si `cond` est fausse |
| `input([prompt])` | Affiche un prompt optionnel, lit et retourne une ligne depuis stdin |
| `readfile(chemin)` | Lit le contenu d'un fichier comme `ton.html` (résolu aussi relativement au dossier du script) |

**Math**
| Fonction | Description |
|---|---|
| `sqrt(x)`, `pow(x, y)`, `abs(x)` | Racine carrée, puissance, valeur absolue |
| `floor(x)`, `ceil(x)`, `round(x)` | Arrondis |
| `min(...)`, `max(...)` | Le plus petit/grand parmi un nombre quelconque d'arguments |
| `random()` | Un float dans `[0, 1)` |
| `random(max)` | Un entier dans `[0, max)` |
| `random(min, max)` | Un entier dans `[min, max)` |

**Strings**
| Fonction | Description |
|---|---|
| `len(s)` | Longueur |
| `upper(s)`, `lower(s)` | Conversion de casse |
| `trim(s)` | Retire les espaces en début/fin |
| `split(s, sep)` | Découpe en un `ton.array` de strings |
| `join(arr, sep)` | Assemble un tableau en une string (voir aussi Tableaux) |
| `replace(s, recherche, remplacement)` | Remplace toutes les occurrences |
| `substring(s, debut, [longueur])` | Extrait une sous-chaîne |
| `contains(s, sous-chaine)` | Si `sous-chaine` apparaît dans `s` |
| `indexOf(s, sous-chaine)` | Index de la première occurrence, ou `-1` |

**Tableaux**
| Fonction | Description |
|---|---|
| `len(arr)` | Nombre d'éléments |
| `push(arr, v)` | Ajoute une valeur (modifie en place) |
| `pop(arr)` | Retire et retourne le dernier élément |
| `shift(arr)` | Retire et retourne le premier élément |
| `unshift(arr, v)` | Insère une valeur au début |
| `slice(arr, debut, [fin])` | Retourne un nouveau sous-tableau |
| `join(arr, sep)` | Assemble les éléments en une string |
| `sort(arr)` | Trie en place (numérique ou alphabétique) et le retourne |
| `reverse(arr)` | Inverse en place et le retourne |
| `contains(arr, item)` | Si `item` est un élément |
| `indexOf(arr, item)` | Index du premier élément correspondant, ou `-1` |
| `map(arr, fn)` | Nouveau tableau de `fn(élément)` pour chaque élément |
| `filter(arr, fn)` | Nouveau tableau ne gardant que les éléments où `fn(élément)` est vrai |
| `reduce(arr, fn, initial)` | Replie le tableau via `fn(accumulateur, élément)` |
| `find(arr, fn)` | Premier élément où `fn(élément)` est vrai, ou `nil` |
| `any(arr, fn)` | `true` si `fn(élément)` est vrai pour au moins un élément |
| `all(arr, fn)` | `true` si `fn(élément)` est vrai pour tous les éléments |

**Dictionnaires**
| Fonction | Description |
|---|---|
| `len(d)` | Nombre de clés |
| `keys(d)` | Tableau des clés, dans l'ordre d'insertion |
| `values(d)` | Tableau des valeurs, dans l'ordre d'insertion |
| `has(d, clé)` | Si `clé` existe |
| `json(d)` | Sérialise le dict en JSON |

**Serveur HTTP**
| Fonction | Description |
|---|---|
| `get(chemin, fn)` | Enregistre une route GET (voir [Serveur local](#serveur-local--routes-api)) |
| `post(chemin, fn)` | Enregistre une route POST |
| `serve(port)` / `serve(port, contenu)` | Démarre le serveur HTTP local |

---

### Type HTML

`ton.html` marque un contenu destiné à être servi comme page web :

```
ton.html page = "<h1>Bienvenue</h1><p>Ceci est TON618.</p>"
```

Une `ton.function` qui retourne du HTML sera automatiquement servie avec `Content-Type: text/html` quand elle est utilisée comme gestionnaire de route (voir ci-dessous).

---

### Modules (IMPORT://)

Tu peux répartir ton code entre plusieurs fichiers et les importer :

```
IMPORT://math
```

Ça cherche `math.ton` à côté de ton script, puis dans un sous-dossier `modules/`. Toutes les déclarations du module (variables, fonctions) deviennent disponibles dans ton script, toujours accessibles via `ton.` :

**math.ton**
```
ton.function square(n) {
    return n * n
}
```

**main.ton**
```
IMPORT://math

print(ton.square(4))   // 16
```

Les imports doivent apparaître avant le reste du code et ne sont chargés qu'une seule fois (réimporter le même module ne fait rien).

Pour un guide complet et pratique sur l'écriture de tes propres modules (conventions, exemple concret, pièges courants), voir la section suivante.

---

### Créer un module TON618 — guide complet

Un **module TON618** n'est rien de plus qu'un fichier `.ton` ordinaire, écrit pour que d'autres scripts puissent l'importer avec `IMPORT://` et réutiliser ce qu'il déclare. Il n'existe aucun mot-clé ou wrapper spécial "module" — n'importe quel fichier `.ton` peut être importé, et n'importe quel fichier `.ton` peut faire `IMPORT://` d'un autre.

#### 1. Où vivent les modules

Quand un script exécute `IMPORT://nom`, l'interpréteur cherche le module à exactement deux endroits, dans cet ordre (voir `Interpreter::runImport` dans `src/Interpreter.cpp`) :

1. `<dossier du script en cours>/nom.ton`
2. `<dossier du script en cours>/modules/nom.ton`

Donc un projet organisé ainsi :
```
monapp/
├── main.ton
└── modules/
    └── stringutils.ton
```
permet à `main.ton` de faire `IMPORT://stringutils`, qui sera trouvé dans `modules/`. Un module placé directement à côté du script (sans dossier `modules/`) fonctionne aussi — c'est le premier chemin candidat.

Il n'y a pas de chemin de module imbriqué (par ex. `IMPORT://utils/strings` ne fonctionne pas) et pas de registre de paquets — les modules sont juste des fichiers que tu gardes dans ton projet (ou copies depuis ailleurs) et référence par leur nom de fichier, sans l'extension `.ton`.

#### 2. Ce qu'un module peut déclarer

Un module est exécuté une fois, dans la **portée globale**, exactement comme si ses instructions avaient été collées en haut du script qui l'importe. Ça veut dire qu'un module peut déclarer :

- Des déclarations `ton.function` — la chose la plus souvent partagée.
- Des variables `ton.int`/`ton.string`/`ton.bool`/`ton.array`/`ton.dict`/`ton.html` — constantes ou état mutable partagés.
- Ses propres instructions `IMPORT://` imbriquées (un module peut dépendre d'un autre module).

Ce guide parle d'écrire tes *propres* modules. TON618 fournit aussi un ensemble de **modules système intégrés** (`ton.sys`, `ton.os`, `ton.requests`, `ton.random`, `ton.time`, `ton.json`, `ton.mathutils`, `ton.strings`) qui vivent dans l'interpréteur plutôt que sur le disque — voir [Modules système intégrés](#modules-système-intégrés-tonsys-tonos-tonrequests-tonrandom-tontime-tonjson-tonmathutils-tonstrings) plus bas. Ils s'importent de la même façon (`IMPORT://ton.sys`), et la même convention de nommage (préfixer chaque fonction par le nom du module) recommandée dans ce guide est exactement celle qu'ils suivent.

Un module n'est **pas** un espace de noms séparé : tout ce qu'il déclare atterrit directement dans la portée globale du script importateur, sous le même préfixe `ton.` que tout le reste. Ça implique :

- Des collisions de noms sont possibles. Si ton module déclare `ton.function log(...)` et que le script importateur déclare aussi `ton.function log(...)`, celle exécutée en dernier (dans l'ordre du code source) l'emporte. **Convention** : préfixe les fonctions/variables de ton module avec le nom du module pour éviter les collisions, par ex. un module `stringutils` devrait déclarer `ton.function stringutils_capitalize(s)` plutôt qu'un `ton.function capitalize(s)` nu.
- Un module n'est chargé qu'une seule fois par exécution du programme, même si plusieurs fichiers font `IMPORT://` dessus (l'interpréteur garde la trace des modules déjà importés) — donc c'est sûr pour `main.ton` et un autre module dont il dépend d'importer tous les deux le même module partagé sans réexécuter ses effets de bord deux fois.

#### 3. Un exemple concret

**`modules/stringutils.ton`** — un petit module réutilisable d'outils de strings :
```
// stringutils.ton — outils de strings réutilisables.
// Convention : chaque nom ici est préfixé par "stringutils_" pour éviter
// les collisions avec ce que le script importateur (ou un autre module) déclare.

ton.function stringutils_capitalize(s) {
    if len(s) == 0 {
        return s
    }
    return upper(substring(s, 0, 1)) + substring(s, 1)
}

ton.function stringutils_titleCase(s) {
    ton.array words = split(s, " ")
    ton.array out = map(words, ton.function(w) { return stringutils_capitalize(w) })
    return join(out, " ")
}
```

**`main.ton`** :
```
IMPORT://stringutils

print(ton.stringutils_capitalize("kuro"))       // Kuro
print(ton.stringutils_titleCase("hello world")) // Hello World
```

Remarque : le module utilise librement le reste de la bibliothèque standard (`len`, `substring`, `upper`, `split`, `map`, `join`) — un module est du code TON618 ordinaire, rien de plus.

#### 4. Bonnes pratiques pour écrire un module

- **Préfixe chaque déclaration** avec le nom du module (`nommodule_chose`) pour éviter d'écraser des noms dans le script qui l'importe.
- **Place-le dans `modules/`** s'il est destiné à être réutilisé par plusieurs scripts du projet ; garde-le à côté du script si c'est juste une séparation ponctuelle pour la lisibilité.
- **Documente les fonctions publiques du module** avec un court commentaire au-dessus de chacune, indiquant ce qu'elle attend et retourne — puisque TON618 n'a pas de signatures de fonction vérifiées par type, c'est le seul contrat qu'un appelant a.
- **Évite les effets de bord lourds au moment de l'import** (par ex. démarrer un serveur HTTP, afficher des bannières) — un module devrait surtout *déclarer* des choses, et laisser le script importateur décider quand les appeler réellement. Une exception est un module qui définit des données constantes partagées (par ex. `ton.dict config = {...}`), ce qui est raisonnable à déclarer directement.
- **Garde les instructions `IMPORT://` tout en haut du fichier**, avant toute autre instruction — le parseur ne l'impose pas strictement partout, mais ça correspond à la façon dont le langage est censé se lire et évite des surprises sur ce qui est en portée où.
- **Teste un module de façon isolée** en écrivant un petit script à côté qui l'importe et appelle chaque fonction une fois — puisqu'il n'y a pas de framework de tests unitaires, c'est la façon pratique d'attraper les erreurs.

---

### Modules système intégrés (ton.sys, ton.os, ton.requests, ton.random, ton.time, ton.json, ton.mathutils, ton.strings)

En plus des modules que tu écris toi-même (voir le guide ci-dessus), TON618 embarque une poignée de **modules système intégrés**. Ce ne sont pas des fichiers sur le disque — ils vivent dans l'interpréteur — mais tu dois quand même faire `IMPORT://` dessus avant de les utiliser, exactement comme un module utilisateur :

```
IMPORT://ton.sys
IMPORT://ton.os
IMPORT://ton.requests
IMPORT://ton.random
IMPORT://ton.time
IMPORT://ton.json
```

Remarque la syntaxe : un module **utilisateur** est `IMPORT://nom` (un fichier `nom.ton`) ; un module **intégré** est `IMPORT://ton.nom` (le `ton.` en tête est ce qui permet au parseur de distinguer les deux — ce n'est jamais une recherche de fichier). Appeler une des fonctions ci-dessous sans avoir d'abord importé son module échoue avec une erreur "fonction non définie" — c'est voulu : un script ne paie (et n'expose) que les modules intégrés qu'il a réellement demandés.

Chaque module intégré suit la même convention de nommage recommandée pour les modules utilisateur dans le guide ci-dessus : chaque fonction est préfixée par le nom du module (`sys_...`, `os_...`, `requests_...`, `random_...`, `time_...`, `json_...`).

#### ton.sys — informations sur l'exécution & le processus

| Fonction | Description |
|---|---|
| `sys_args()` | Tableau des arguments de ligne de commande passés après le chemin du script (`./ton618 script.ton foo bar` → `["foo", "bar"]`) |
| `sys_platform()` | `"windows"`, `"termux"`, `"macos"`, ou `"linux"` — détecté automatiquement (Termux est distingué d'un Linux classique via la variable d'environnement `$PREFIX`) |
| `sys_arch()` | `"x64"`, `"arm64"`, `"arm"`, `"x86"`, ou `"unknown"` |
| `sys_version()` | La version de l'interpréteur lui-même (ex. `"beta-1.0.0"`), identique à `ton618 --version` |
| `sys_exit(code)` | Arrête immédiatement tout le programme avec le code de sortie donné |
| `sys_sleep(ms)` | Met en pause l'exécution pendant le nombre de millisecondes donné |

#### ton.os — variables d'environnement & système de fichiers

| Fonction | Description |
|---|---|
| `os_name()` | `"nt"` sous Windows, `"posix"` ailleurs |
| `os_getenv(nom)` | La valeur d'une variable d'environnement, ou `nil` si non définie |
| `os_setenv(nom, valeur)` | Définit une variable d'environnement pour ce processus ; retourne si ça a réussi |
| `os_cwd()` | Le dossier de travail courant |
| `os_exists(chemin)` | Si un fichier ou dossier existe |
| `os_mkdir(chemin)` | Crée un dossier (et les dossiers parents manquants) |
| `os_remove(chemin)` | Supprime un fichier ou un dossier vide |
| `os_listdir(chemin)` | Tableau des noms d'entrées directement dans un dossier (non récursif) |
| `os_tempdir()` | Le dossier temporaire du système |
| `os_copy(src, dst)` | Copie un fichier, en écrasant `dst` s'il existe déjà |

#### ton.requests — un client HTTP minimal

Un petit client HTTP sans dépendance (sockets bruts, dans le même esprit que le serveur intégré). **Limitation : `http://` uniquement, pas de TLS/`https://`** — il n'y a pas de bibliothèque SSL embarquée, donc il peut parler à des services locaux, d'autres serveurs TON618, ou n'importe quel point d'accès en HTTP simple, mais pas à une API exclusivement HTTPS.

| Fonction | Description |
|---|---|
| `requests_get(url)` | Effectue une requête GET |
| `requests_post(url, [corps])` | Effectue une requête POST avec un corps optionnel (string) |
| `requests_request(méthode, url, [corps])` | Effectue une requête avec n'importe quelle méthode HTTP |

Les trois retournent un `ton.dict` avec la même forme :
```
{
    ok: true,          // si une réponse a été reçue du tout
    status: 200,        // le code de statut HTTP (0 si la requête a totalement échoué)
    body: "...",         // le corps brut de la réponse
    error: ""             // une raison lisible quand ok vaut false
}
```

Exemple :
```
IMPORT://ton.requests

ton.dict res = requests_get("http://localhost:8081/api/users")
if res["ok"] {
    print("Statut : " + str(res["status"]))
    print(res["body"])
} else {
    print("Requête échouée : " + res["error"])
}
```

#### ton.random — outils de hasard supplémentaires

Complète le `random()` toujours disponible (voir [Fonctions natives](#fonctions-natives)) avec quelques usages courants :

| Fonction | Description |
|---|---|
| `random_int(min, max)` | Un entier dans `[min, max]`, inclusif des deux côtés |
| `random_float()` | Un float dans `[0, 1)` |
| `random_choice(arr)` | Un élément uniformément aléatoire d'un tableau non vide |
| `random_shuffle(arr)` | Mélange un tableau en place (Fisher-Yates) et le retourne |
| `random_seed(n)` | Réinitialise le générateur aléatoire de façon déterministe — utile pour des tests reproductibles |

#### ton.time — horloges & timestamps

| Fonction | Description |
|---|---|
| `time_now()` | Secondes depuis l'epoch Unix |
| `time_millis()` | Millisecondes depuis l'epoch Unix — pratique pour mesurer un temps écoulé |
| `time_string([timestamp])` | Une string de temps local lisible ; par défaut l'instant présent |
| `time_sleep(ms)` | Met en pause l'exécution pendant le nombre de millisecondes donné (identique à `sys_sleep`) |

#### ton.json — parsing JSON

Le natif `json(x)` toujours disponible (voir [Fonctions natives](#fonctions-natives)) transforme déjà une valeur *en* JSON. `IMPORT://ton.json` ajoute l'autre sens — parser du texte JSON *en* une valeur — plus un alias explicitement nommé pour l'encodage :

| Fonction | Description |
|---|---|
| `json_parse(texte)` | Parse une string JSON en un `ton.dict`/`ton.array`/scalaire |
| `json_stringify(valeur)` | Identique à `json(valeur)`, fourni sous un nom cohérent pour les scripts qui importent ce module |
| `json_pretty(valeur)` | Identique à `json_stringify`, indenté sur plusieurs lignes pour une sortie lisible |

```
IMPORT://ton.json
IMPORT://ton.requests

ton.dict res = requests_get("http://localhost:8081/api/users")
ton.array users = json_parse(res["body"])
print(len(users))
```

#### ton.mathutils — trigonométrie, logs & stats

Complète les fonctions toujours disponibles `sqrt`/`pow`/`abs`/`floor`/`ceil`/`round`/`min`/`max` (voir [Fonctions natives](#fonctions-natives)) avec de la trigonométrie, des logarithmes, et quelques aides de statistiques/arithmétique :

| Fonction | Description |
|---|---|
| `mathutils_pi()` / `mathutils_e()` | Les constantes π et e |
| `mathutils_sin/cos/tan(x)`, `mathutils_asin/acos/atan(x)`, `mathutils_atan2(y, x)` | Trigonométrie, en radians |
| `mathutils_log(x)` / `mathutils_log2(x)` / `mathutils_log10(x)` / `mathutils_exp(x)` | Logarithmes naturel, base 2, base 10, et e^x |
| `mathutils_hypot(x, y)` | `sqrt(x*x + y*y)`, calculé sans dépassement |
| `mathutils_degrees(rad)` / `mathutils_radians(deg)` | Conversion d'unité d'angle |
| `mathutils_clamp(x, lo, hi)` | Restreint `x` à `[lo, hi]` |
| `mathutils_lerp(a, b, t)` | Interpolation linéaire entre `a` et `b` à `t` (`0..1`) |
| `mathutils_sign(x)` | `-1`, `0`, ou `1` |
| `mathutils_gcd(a, b)` / `mathutils_lcm(a, b)` | PGCD / PPCM de deux entiers |
| `mathutils_factorial(n)` | `n!` pour un entier `n` positif ou nul |
| `mathutils_is_prime(n)` | `true` si `n` est premier |
| `mathutils_sum(arr)` / `mathutils_mean(arr)` / `mathutils_median(arr)` / `mathutils_stddev(arr)` | Somme, moyenne, médiane et écart-type d'un tableau numérique |

```
IMPORT://ton.mathutils

print(mathutils_gcd(48, 18))          // 6
print(mathutils_mean([1, 2, 3, 4]))   // 2.5
print(mathutils_clamp(15, 0, 10))     // 10
```

#### ton.strings — aides supplémentaires sur les chaînes

Complète les fonctions toujours disponibles `upper`/`lower`/`trim`/`split`/`replace`/`contains`/`substring` (voir [Fonctions natives](#fonctions-natives)) :

| Fonction | Description |
|---|---|
| `strings_starts_with(s, préfixe)` / `strings_ends_with(s, suffixe)` | Tests de préfixe/suffixe |
| `strings_repeat(s, n)` | Répète `s` `n` fois |
| `strings_reverse(s)` | Inverse une string (le `reverse()` toujours disponible ne marche que sur les tableaux) |
| `strings_capitalize(s)` | Met la première lettre en majuscule, le reste en minuscule |
| `strings_pad_left(s, largeur, [car])` / `strings_pad_right(s, largeur, [car])` | Complète `s` jusqu'à `largeur` avec `car` (par défaut `" "`) |
| `strings_count(s, sous)` | Nombre d'occurrences non chevauchantes de `sous` dans `s` |
| `strings_trim_start(s)` / `strings_trim_end(s)` | Supprime les espaces seulement au début/à la fin de `s` (`trim()` supprime les deux) |

```
IMPORT://ton.strings

print(strings_capitalize("hello world"))  // "Hello world"
print(strings_pad_left("7", 3, "0"))      // "007"
```

---

### Serveur local & routes (API)

TON618 a un serveur HTTP intégré, sans dépendance externe (sockets bruts).

**Mode simple** — même contenu pour chaque requête :
```
ton.serve(8080, "<h1>Salut depuis TON618 !</h1>")
```

**Mode routage** — enregistre des handlers par chemin/méthode, puis `serve(port)` :
```
ton.function home() {
    return "<h1>Page d'accueil</h1>"
}

ton.function api_users() {
    ton.array users = ["kuro", "rusher"]
    return ton.json(users)
}

ton.get("/", ton.home)
ton.get("/api/users", ton.api_users)

ton.serve(8081)
```

- `ton.get(chemin, fonctionHandler)` — enregistre une route GET
- `ton.post(chemin, fonctionHandler)` — enregistre une route POST
- Les handlers ne prennent pas de paramètre (pour l'instant) et retournent une string, une valeur html, ou une string JSON
- Les chemins non enregistrés retournent automatiquement `404 Not Found`
- `serve()` bloque le programme — il tourne jusqu'à ce que tu l'arrêtes (Ctrl+C)

---

### Le débogueur

Lance avec `--debug` ou `--break=<ligne>` pour l'activer.

```bash
./ton618 script.ton --debug
./ton618 script.ton --break=12
```

Commandes une fois en pause :

| Commande | Effet |
|---|---|
| `s`, `step` | Exécute la ligne suivante (entre dans les appels de fonction) |
| `c`, `continue` | Continue jusqu'au prochain point d'arrêt |
| `b <ligne>` | Ajoute un point d'arrêt |
| `rb <ligne>` | Retire un point d'arrêt |
| `p <var>` | Affiche la valeur et le type d'une variable |
| `vars` | Liste toutes les variables locales |
| `bt` | Affiche la pile d'appels |
| `l` | Affiche le code source autour de la ligne actuelle |
| `q` | Quitte le programme |

---

### Architecture — comment fonctionne l'interpréteur

TON618 est un **interpréteur "tree-walking"** classique, composé de quatre étapes qui s'enchaînent en pipeline :

```
texte source (fichier .ton)
        │
        ▼
┌───────────────┐   transforme les caractères en une liste plate de Tokens
│     Lexer     │   (nombres, strings, identifiants, mots-clés, opérateurs)
└───────┬───────┘
        ▼  tokens
┌───────────────┐   analyse récursive descendante : regroupe les Tokens en
│     Parser    │   un arbre syntaxique abstrait (un arbre de nœuds Stmt/Expr)
└───────┬───────┘   selon la grammaire du langage
        ▼  AST
┌───────────────┐   parcourt l'AST directement et l'exécute — pas de bytecode,
│  Interpreter  │   pas d'étape de compilation séparée ; chaque nœud est évalué
└───────────────┘   au moment où il est atteint
```

```
ton618/
├── Makefile
├── include/              # en-têtes (.hpp) — déclarations et notes de conception
│   ├── Token.hpp           — le vocabulaire du langage (TokenType, Token)
│   ├── Lexer.hpp            — texte -> tokens
│   ├── Ast.hpp                — la forme des nœuds de l'AST (Expr, Stmt)
│   ├── Parser.hpp               — tokens -> AST
│   ├── Value.hpp                  — représentation des valeurs à l'exécution
│   ├── Environment.hpp              — portées des variables (nom -> Value, chaînées)
│   ├── Interpreter.hpp                — parcourt l'AST, exécute le programme
│   ├── Debugger.hpp                    — points d'arrêt / mode pas-à-pas
│   ├── LocalServer.hpp                   — serveur HTTP en sockets bruts (ton.serve/get/post)
│   ├── HttpClient.hpp                      — client HTTP en sockets bruts (le module ton.requests)
│   └── JsonParser.hpp                        — texte JSON -> Value (json_parse du module ton.json)
├── src/                  # implémentations (.cpp), une par en-tête ci-dessus,
│                           plus main.cpp (point d'entrée CLI : lit un fichier,
│                           lance le pipeline Lexer -> Parser -> Interpreter,
│                           et gère --debug / --break / --uninstall)
├── exemples/             # scripts .ton d'exemple, dont un qui exerce toutes
│                           les fonctionnalités du langage (test_new_features.ton),
│                           plus documentation.html et playground.html (voir plus bas)
├── scripts/doc_templates/ # le head/tail HTML autour duquel `make docs` enveloppe
│                           DOCUMENTATION.md pour produire exemples/documentation.html
└── modules/              # place tes modules .ton partagés ici (voir le guide ci-dessus)
```

Chaque en-tête commence par un court bloc de commentaire expliquant son rôle dans le pipeline et, si pertinent, exactement quoi toucher en premier pour étendre cette partie du langage — lis-les avant de plonger dans les fichiers `.cpp`.

Deux choix de conception valent la peine d'être soulignés car ils façonnent presque tous les autres fichiers :

- **Les nœuds AST/Value sont des "fat structs", pas une hiérarchie de classes.** `Expr` et `Stmt` (Ast.hpp) et `Value` (Value.hpp) contiennent chacun tous les champs dont n'importe laquelle de leurs variantes pourrait avoir besoin, avec une étiquette `type` disant lesquels s'appliquent. Ça sacrifie un peu de mémoire pour une base de code où `evaluate()`/`execute()` sont de simples `switch` au lieu d'un visitor pattern éparpillé — bien plus facile à lire de bout en bout pour un langage de cette taille. Certains types de nœuds *réutilisent* les champs d'un autre type (documenté à côté de l'`enum`) plutôt que d'en ajouter de nouveaux, pour empêcher les structs de grossir sans limite.
- **Le flux de contrôle utilise les exceptions C++.** `break`, `continue`, et `return` sont implémentés comme de petits types d'exception (`BreakException`, `ContinueException`, `ReturnException`, voir Interpreter.hpp) levés depuis `execute()` et attrapés par la boucle ou l'appel de fonction englobant le plus proche. Ça reflète, presque un-à-un, comment ces constructions déroulent les blocs imbriqués dans l'AST — aucun drapeau manuel "est-ce qu'on a break" à faire transiter manuellement à travers chaque appel récursif.

---

### Étendre l'interpréteur lui-même

Cette section est pour les personnes qui modifient l'interpréteur C++ (pas celles qui écrivent des scripts `.ton`) — par ex. les contributeurs.

**Ajouter une fonction native** (le type d'extension le plus simple — aucun changement de grammaire nécessaire) :
1. Ouvre `src/Interpreter.cpp`, trouve `defineNatives()`.
2. Appelle l'aide `def("nom", [...](std::vector<Value>& args) -> Value { ... })` avec la logique de ta fonction. Regarde les sections math/string/array/dict existantes pour le modèle.
3. C'est tout — ta fonction est immédiatement appelable depuis n'importe quel script en tant que `ton.nom(...)` (et, comme le reste, aussi sans le préfixe).

**Ajouter un nouveau module système intégré** (un module `IMPORT://ton.<nom>`, comme `ton.sys`/`ton.os`/`ton.requests`/`ton.random`/`ton.time`/`ton.json`) — la version optionnelle de ce qui précède :
1. Déclare une méthode privée `void registerBuiltinX();` sur `Interpreter` (`include/Interpreter.hpp`).
2. Implémente-la dans `src/Interpreter.cpp`, à côté des autres fonctions `registerBuiltin*` : c'est le même modèle `def("nom_chose", ...)` qu'une fonction native, juste préfixé par le nom du module et vivant dans sa propre méthode plutôt que dans `defineNatives()`.
3. Branche-la dans la partie `moduleName.rfind("ton.", 0) == 0` de `Interpreter::runImport` pour que `IMPORT://ton.<nom>` l'appelle.
4. Documente-la dans la section [Modules système intégrés](#modules-système-intégrés-tonsys-tonos-tonrequests-tonrandom-tontime-tonjson-tonmathutils-tonstrings) ci-dessus.

**Ajouter un nouvel opérateur** (par ex. `**` pour l'exponentiation) :
1. Ajoute un `TokenType` dans `include/Token.hpp`.
2. Apprends au Lexer à le produire dans `src/Lexer.cpp` (`scanToken()`).
3. Branche-le au bon niveau de priorité dans `src/Parser.cpp` (voir la chaîne de priorité documentée en haut de `Parser.hpp`) et dans la gestion `BINARY`/`UNARY` de `Interpreter::evaluate` (`src/Interpreter.cpp`).

**Ajouter un nouveau type d'instruction** (par ex. une instruction `switch`) :
1. Ajoute un `StmtType` dans `include/Ast.hpp`, et soit réutilise des champs existants de `Stmt` (préféré, voir les commentaires à côté des champs réutilisés existants pour des exemples), soit en ajoute de nouveaux si rien ne convient.
2. Ajoute une méthode de parsing dans `src/Parser.cpp` et redirige vers elle depuis `statement()`.
3. Ajoute le `case` correspondant dans `Interpreter::execute` (`src/Interpreter.cpp`).

**Ajouter un nouveau type de valeur** (par ex. un `ton.set`) :
1. Ajoute un `ValueType` et un champ de stockage dans `include/Value.hpp` ; étends `typeName()`, `toString()`, `toJson()`.
2. Ajoute un `VarKind` dans `include/Ast.hpp` et un `TokenType`/mot-clé (`Token.hpp`, `Lexer.cpp`) s'il a besoin de sa propre syntaxe de déclaration `ton.<type>`.
3. Étends `Interpreter::checkType` (`src/Interpreter.cpp`) pour que `ton.<type> x = ...` l'impose.
4. Ajoute la syntaxe littérale dans `Parser::primary()` si le type en a besoin (regarde comment les littéraux `ARRAY`/`DICT` y sont parsés), et le support d'évaluation dans `Interpreter::evaluate`.

Après tout changement, lance `make` et vérifie avec les scripts de `exemples/` (en particulier `exemples/test_new_features.ton`, qui exerce la majorité de la bibliothèque standard et des fonctionnalités de contrôle de flux) avant de te fier au changement.

---

### Publier la doc sur GitHub Pages

`exemples/documentation.html` est entièrement statique et autonome (elle embarque ce fichier même, voir `make docs` plus haut) — elle n'a besoin que d'un serveur web, ce qui fait de GitHub Pages un bon moyen de l'héberger à une URL publique plutôt qu'uniquement comme fichier local.

1. **GitHub → ce dépôt → Settings → Pages.**
2. Sous **Build and deployment**, mets **Source** sur **Deploy from a branch**.
3. Choisis la branche **`main`** et le dossier **`/ (root)`**, puis **Save**.
4. GitHub publie tout le dépôt comme site statique à `https://<utilisateur>.github.io/ton618/` (adapte l'org/l'utilisateur de cette URL à celui qui possède ce dépôt). La doc se trouve alors à :
   ```
   https://<utilisateur>.github.io/ton618/exemples/documentation.html
   ```
5. *(Optionnel)* Ajoute un `index.html` à la racine qui redirige vers `exemples/documentation.html`, pour que la doc soit aussi accessible via la simple `https://<utilisateur>.github.io/ton618/` :
   ```html
   <!doctype html><meta http-equiv="refresh" content="0; url=exemples/documentation.html">
   ```

Chaque fois que `DOCUMENTATION.md` change, lance `make docs` et commite `exemples/documentation.html` régénéré — GitHub Pages sert simplement ce qui est sur `main`, donc la page publiée se met à jour au prochain push.

Tu préfères utiliser une GitHub Action pour construire et déployer la page plutôt que de servir la branche directement (par ex. pour ne publier que `exemples/` à la racine du site) ? Utilise le workflow de démarrage **"Static HTML"** depuis l'onglet **Actions**, ou les actions `actions/upload-pages-artifact` + `actions/deploy-pages` pointées vers `exemples/` — voir le [guide officiel de GitHub](https://docs.github.com/en/pages) pour cette variante.

---

### Limitations connues

- Les gestionnaires de route ne peuvent pas encore lire le corps d'une requête POST ni des paramètres d'URL (ex : `/user/:id`)
- `serve()` est mono-thread et bloquant
- `ton.int` et `ton.float` sont actuellement le même type numérique en interne (pas de contrainte "entier uniquement")
- Les clés de dict sont toujours des strings (pas de clés numériques ou structurelles imbriquées)
- `IMPORT://` ne résout qu'un nom de module plat, pas de sous-dossiers/espaces de noms
- Pas de types/classes définis par l'utilisateur — seulement les types scalaires/tableau/dict/html/fonction intégrés
- `ton.requests` ne parle que du `http://` simple — pas de TLS/`https://`, pas de redirections, pas de transfer-encoding chunked

---

### Changelog

**beta-1.0.0** — première release distribuée via GitHub :

- **Les binaires précompilés sont maintenant construits et publiés par GitHub Actions** (`.github/workflows/release.yml`), qui compile en croisé Linux x64, Windows x64 et Termux/Android arm64 à chaque release taguée, au lieu d'un serveur privé
- **`ton618 --update [version]`** : retélécharge le binaire correspondant à la plateforme actuelle depuis les GitHub Releases et remplace celui en cours d'exécution — omets `version` pour la dernière release, ou passe un tag pour épingler/revenir en arrière
- **`ton618 --version` / `-v`** : affiche la version, la plateforme détectée et l'architecture
- **Détection automatique de la plateforme** : `sys_platform()` reconnaît maintenant aussi `"termux"` (auparavant indistinguable de `"linux"`) ; le nouveau `sys_arch()` renvoie l'architecture CPU et `sys_version()` la version de l'interpréteur — voir [Platform.hpp](#étendre-linterpréteur-lui-même)
- **Nouvelles aides tableaux toujours disponibles** : `find(arr, fn)`, `any(arr, fn)`, `all(arr, fn)`
- **Nouvelles aides fichiers** : `os_tempdir()`, `os_copy(src, dst)`
- **Nouvelles aides chaînes** : `strings_trim_start(s)`, `strings_trim_end(s)`
- **`json_pretty(valeur)`** : sortie JSON indentée sur plusieurs lignes, en complément de `json_stringify`
- `scripts/install.sh` / `install.ps1` installent depuis les GitHub Releases au lieu d'un serveur auto-hébergé ; un `.github/workflows/ci.yml` compile et teste chaque push/PR

Ajouts récents au langage et à la bibliothèque standard :

- **Assignation composée** : `+=`, `-=`, `*=`, `/=`, `%=`
- **Incrémentation/décrémentation** : `++`, `--` (postfixe, sur variables et cibles indexées)
- **Opérateur ternaire** : `cond ? a : b`
- **Boucle `for ... in`** sur tableaux et dictionnaires
- **Gestion d'erreurs `try` / `catch` / `throw`**
- **Type `ton.dict`**, avec littéraux `{cle: valeur, ...}`, indexation `[]`, et `keys()`/`values()`/`has()`
- **Expressions de fonction** : `ton.function(params) { ... }` comme valeur inline, pour les callbacks
- **Bibliothèque standard bien plus grande** : `type`, `assert`, `input`, `sqrt`, `pow`, `abs`, `floor`, `ceil`, `round`, `min`, `max`, `random`, `upper`, `lower`, `trim`, `split`, `replace`, `substring`, `contains`, `indexOf`, `pop`, `shift`, `unshift`, `slice`, `join`, `sort`, `reverse`, `map`, `filter`, `reduce`, `keys`, `values`, `has`
- **Un guide documenté pour écrire des modules** (voir [Créer un module TON618](#créer-un-module-ton618--guide-complet))
- **Huit modules système intégrés**, chacun optionnel via `IMPORT://ton.<nom>` : `ton.sys`, `ton.os`, `ton.requests` (un client `http://` minimal), `ton.random`, `ton.time`, `ton.json` (ajoute `json_parse` en plus du `json()` toujours disponible), `ton.mathutils` (trigonométrie/logs/stats), `ton.strings` (aides supplémentaires sur les chaînes) — voir [Modules système intégrés](#modules-système-intégrés-tonsys-tonos-tonrequests-tonrandom-tontime-tonjson-tonmathutils-tonstrings)
- **Commentaires d'architecture au niveau des en-têtes** dans chaque fichier `.hpp`, expliquant le rôle de chaque fichier et où l'étendre
- **Un playground dans le navigateur** (`exemples/playground.html`, propulsé par `exemples/ton618-lite.js`) — un éditeur de code en direct qui exécute des scripts TON618 entièrement côté client, sans installation ni serveur — voir [Le playground dans le navigateur](#le-playground-dans-le-navigateur)
- **`documentation.html` est maintenant un vrai lecteur Markdown** : elle affiche ce fichier directement (embarqué au moment de l'édition via `make docs`) au lieu d'une transcription HTML maintenue à la main, donc la page ne peut plus se désynchroniser des vrais docs
