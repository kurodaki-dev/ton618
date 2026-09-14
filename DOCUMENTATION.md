# TON618 — Documentation

*A simple interpreted programming language, written in C++.*
*Un langage de programmation interprété simple, écrit en C++.*

---

## 🇬🇧 English

### Table of contents
1. [What is TON618](#what-is-ton618)
2. [Installing & building the interpreter](#installing--building-the-interpreter)
3. [Running a script](#running-a-script)
4. [The browser playground](#the-browser-playground)
5. [Language basics](#language-basics)
6. [Types](#types)
7. [String formatting](#string-formatting)
8. [Operators](#operators)
9. [Control flow](#control-flow)
10. [Functions](#functions)
11. [Arrays](#arrays)
12. [Dictionaries](#dictionaries)
13. [Error handling: try / catch / throw / finally](#error-handling-try--catch--throw--finally)
14. [Built-in native functions](#built-in-native-functions)
15. [HTML type](#html-type)
16. [Modules (IMPORT://)](#modules-import)
17. [Creating a TON618 module — full guide](#creating-a-ton618-module--full-guide)
18. [Built-in system modules](#built-in-system-modules)
19. [Local server & routes (API)](#local-server--routes-api)
20. [The debugger](#the-debugger)
21. [Architecture — how the interpreter works](#architecture--how-the-interpreter-works)
22. [Extending the interpreter itself](#extending-the-interpreter-itself)
23. [Publishing the docs to GitHub Pages](#publishing-the-docs-to-github-pages)
24. [Known limitations](#known-limitations)
25. [Changelog](#changelog)

---

### What is TON618

TON618 is a small interpreted language, similar in spirit to how Python or HolyC work: you write a `.ton` file, run it directly with the `ton618` interpreter, and it executes immediately — no separate compilation step for your scripts.

The interpreter itself (`ton618`) is written in C++17 and ships as a single, dependency-free binary. It's cross-compiled for Linux, Termux (Android), and Windows (see [Installing & building](#installing--building-the-interpreter)), and auto-detects which of those it's running on at startup.

The language's defining feature is the **`ton.` prefix**: declaring a variable or a function always uses it (`ton.int x = 5`, `ton.function f() { ... }`). Once something is declared, though, referring to it again — reading it, reassigning it, or naming a function parameter — can use either `ton.name` or the bare `name`; both resolve to the same thing. This documentation uses `ton.` everywhere for clarity, except for short-lived names like loop counters and function parameters, matching the style already used throughout `exemples/`. See [Language basics](#language-basics) for exactly where the prefix is required versus optional.

TON618 supports **error handling** (`try`/`catch`/`throw`), a **dictionary type** (`ton.dict`), **`for ... in`** and C-style `for` loops, a **`switch`/`case`** statement, a **`format()`** helper for building strings without a chain of `+`, **compound assignment** (`+=`, `-=`, ...), **increment/decrement** (`++`, `--`), a **ternary operator** (`cond ? a : b`), **function expressions** (anonymous callbacks), and a fairly large standard library covering math, strings, arrays, dicts, files, JSON, regular expressions, and base64/hex/URL encoding — see the [Changelog](#changelog) for the full history.

---

### Installing & building the interpreter

**Precompiled binaries** (no compiler needed) are built by [GitHub Actions](.github/workflows/release.yml) — cross-compiling Linux x64, Windows x64, and Termux/Android arm64 — and published on [GitHub Releases](https://github.com/kurodaki-dev/ton618/releases). The installers auto-detect which platform you're on (Termux is told apart from a regular Linux via its `$PREFIX` environment variable) and add `ton618` to your `PATH`:

```bash
# Linux / Termux
curl -fsSL https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.sh | bash
```
```powershell
# Windows
powershell -c "irm https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.ps1 | iex"
```

Install a specific release instead of the latest one by setting `TON618_VERSION` (`install.sh`) or `$env:TON618_VERSION` (`install.ps1`) to a release tag (e.g. `beta-1.0.1`) before running the command above. Uninstall any time with `ton618 --uninstall`, or the matching `uninstall.sh`/`uninstall.ps1` script.

No compiler needed for any of this — a precompiled binary is all you ever run. If you're contributing to the interpreter itself and need to build it from source, see [Architecture](#architecture--how-the-interpreter-works) and [Extending the interpreter itself](#extending-the-interpreter-itself) below (the project just needs a C++17 compiler and `make` — `make` builds `./ton618`, `make docs` regenerates `exemples/documentation.html` from this file after an edit).

---

### Running a script

```bash
ton618 path/to/script.ton
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

Anything after the script path that isn't one of these flags is passed through to the script itself, readable via `sys_args()` (see [ton.sys](#built-in-system-modules)).

Example:
```bash
ton618 exemples/test.ton --break=10 --break=20
ton618 --update             # update to the latest release
ton618 --update beta-1.0.1  # pin to a specific release tag
```

---

### The browser playground

Don't want to install anything just to try a snippet? Open `exemples/playground.html` in any browser (no server needed) for a live code editor with a "Run" button and example snippets to load.

The playground runs **`ton618-lite.js`**, a JavaScript re-implementation of the language, entirely client-side — nothing is sent anywhere, and no installation is required. It mirrors the real interpreter closely: the full grammar (including `switch`/`case`), and the `ton.random`, `ton.time`, `ton.json`, `ton.mathutils`, and `ton.strings` built-in modules, since none of those need anything a browser can't already do.

What it can't do, because a browser sandbox has no filesystem, sockets, or CLI arguments to back them with:
- `ton.sys`, `ton.os`, `ton.requests` — real process/filesystem/network access; importing any of these gives a clear error explaining why, instead of silently behaving differently from the real interpreter
- `ton.encoding`, `ton.regex`, `ton.path` — not reimplemented in JavaScript yet (they exist in the real interpreter)
- `IMPORT://<file>` for your own module files (no filesystem to read them from)
- `get`/`post`/`serve` (no HTTP server), `readfile`/`writefile` (no disk access)

One cosmetic difference: printing a float can show more decimal digits in the playground than in the real interpreter (JavaScript's and C++'s default number-to-string conversions round differently) — the *value* is identical either way, only its printed form differs.

For anything beyond quick experiments — and for the full standard library — install the real interpreter (see [Installing & building](#installing--building-the-interpreter) above).

---

### Language basics

Every statement ends where the next one begins — semicolons are optional and rarely used in practice. Comments come in two forms:

```
// a line comment, runs to the end of the line

/* a block comment,
   can span multiple lines */
```

**Declaring** something always needs the `ton.` prefix, followed by a type keyword and a name:

```
ton.int age = 25
ton.function greet(name) {
    print("Hello, " + name)
}
```

**Referring to** something already declared — reading a variable, reassigning it, calling a function, or naming a function parameter — accepts either the prefixed or the bare form; the parser treats `ton.x` and `x` identically once `x` exists in scope:

```
ton.int age = 25
print(ton.age)   // ok
print(age)        // also ok — same variable
age = age + 1      // also ok
```

Function parameters are conventionally written without the prefix (`ton.function greet(name) { ... }`, not `ton.function greet(ton.name)`), matching every example in this document and in `exemples/`.

A bare assignment can only *reassign* an existing variable — `age = 5` fails with "was never declared" unless something already declared `age` earlier. To introduce a brand-new variable you always need the full `ton.<type> name = value` form.

---

### Types

| Type | Declaration | Example |
|---|---|---|
| Integer/float | `ton.int` / `ton.float` | `ton.int x = 5` |
| String | `ton.string` | `ton.string s = "hi"` |
| Boolean | `ton.bool` | `ton.bool ok = true` |
| Array | `ton.array` | `ton.array a = [1, 2, 3]` |
| Dictionary | `ton.dict` | `ton.dict d = {a: 1}` |
| HTML | `ton.html` | `ton.html page = "<h1>Hi</h1>"` |
| Function | `ton.function` | `ton.function f() { ... }` |

`ton.int` and `ton.float` are both backed by the same internal double-precision number — TON618 doesn't enforce integer-only storage (see [Known limitations](#known-limitations)). Assigning a plain string literal to a `ton.html` variable automatically coerces it to HTML (a convenience for route handlers that return markup).

`type(x)` returns a value's type name as a string: `"int/float"`, `"string"`, `"html"`, `"bool"`, `"array"`, `"dict"`, `"nil"`, or `"function"`.

---

### String formatting

String literals are plain and simple — no embedded expression syntax. To build a string out of variables without a long chain of `+`, use `format(template, ...args)`: it replaces each `{}` in `template`, left to right, with the matching argument converted to text the same way `str()` would.

```
ton.string name = "kuro"
ton.int age = 5
print(format("Hello {}, you'll be {} next year!", name, age + 1))
// Hello kuro, you'll be 6 next year!
```

An argument can be any value — a number, a string, the result of a function call, anything `str()` can convert. Extra `{}` placeholders with no matching argument are left as-is (`{}`); extra arguments beyond the number of placeholders are simply unused.

---

### Operators

| Category | Operators |
|---|---|
| Arithmetic | `+` `-` `*` `/` `%` |
| Comparison | `==` `!=` `<` `<=` `>` `>=` |
| Logical | `&&`/`and`, `\|\|`/`or`, `!` |
| Nil-coalescing | `??` |
| Membership | `in` |
| Assignment | `=` `+=` `-=` `*=` `/=` `%=` |
| Increment/decrement | `++` `--` (postfix) |
| Ternary | `cond ? a : b` |

Notes:
- `+` on two numbers adds them; if either side is a string or `ton.html`, it concatenates (converting the other side to text first) — the result is `ton.html` if either operand was.
- `<`, `<=`, `>`, `>=` compare two numbers numerically, or two strings/`ton.html` values lexicographically; comparing any other combination (or an array, a dict, a bool) is a runtime error rather than a silently meaningless result.
- `==`/`!=` use structural equality: numbers/bools/nil compare by value, a string and an html value with the same text are equal, and arrays/dicts compare element-by-element (recursively) rather than by reference — `[1, 2] == [1, 2]` is `true`.
- `/` and `%` both raise "Division by zero" for a zero right-hand side, rather than producing `inf`/`NaN` silently.
- `and`/`or` are exact synonyms for `&&`/`||` — use whichever reads better.
- `??` ("nil-coalescing") evaluates its left side and returns it as-is unless it's `nil`, in which case it evaluates and returns the right side instead — the right side is never evaluated otherwise, so it's safe to put a fallback that has side effects. Handy with dict lookups: `dict["missing"] ?? "default"`.
- `in` tests membership: `value in array` checks the array's elements (using the same equality rules as `==`), `value in dict` checks the dict's **keys**, and `value in string` checks for a substring. The right-hand side must be an array, dict, or string/`ton.html` — anything else is a runtime error.

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
while i < 5 {
    print(i)
    i++
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

**switch / case** — no fallthrough: each matching case (or `default`, if none match) runs its own block and the switch is done.
```
switch ton.status {
    case 200, 201: {
        print("ok")
    }
    case 404: {
        print("not found")
    }
    default: {
        print("other: " + str(ton.status))
    }
}
```
A case can list several comma-separated values. Matching uses the same equality rules as `==`. A stray `break;` inside a case (a habit from other languages) is harmless — it just exits the switch early, since there's no fallthrough to break out of in the first place.

`break` and `continue` are supported inside `while`, `for`, and `for ... in`.

---

### Functions

```
ton.function add(a, b) {
    return a + b
}
print(add(2, 3))   // 5
```

Functions are values: they can be stored in variables, passed as arguments, and returned from other functions. **Function expressions** create one inline, most often as a callback:

```
ton.array doubled = map([1, 2, 3], ton.function(n) { return n * 2 })
```

A function expression can optionally carry a name (`ton.function label(n) { ... }`) used only for friendlier debugger/error output — it isn't bound as a variable by that name.

**Default parameters** — give a parameter `= expression` to make it optional; the default is evaluated at call time (in order, so a later default can reference an earlier parameter) and only when the argument is omitted:

```
ton.function greet(name, greeting = "Hello") {
    return greeting + ", " + name + "!"
}
print(greet("kuro"))            // Hello, kuro!
print(greet("kuro", "Salut"))   // Salut, kuro!

ton.function pair(a, b = a + 1) {
    return str(a) + "-" + str(b)
}
print(pair(5))   // 5-6
```

**Rest parameters** — prefix the last parameter with `...` to collect any extra arguments into an array:

```
ton.function sum(...nums) {
    ton.int total = 0
    for ton.n in nums { total += n }
    return total
}
print(sum(1, 2, 3, 4))   // 10
print(sum())              // 0
```

A function can mix fixed and rest parameters (`function label(prefix, ...items) { ... }`), but `...` may only appear on the last one.

Calling a function with too few required arguments (or too many, when there's no rest parameter) is a runtime error — the message adapts to whether the function has defaults/rest (`'ton.f' expects 2 argument(s) but got 1.`, `'ton.f' expects between 1 and 2 argument(s) but got 0.`, `'ton.f' expects at least 1 argument(s) but got 0.`).

Closures work as expected: a function remembers the variables in scope where it was declared, even after that scope has otherwise finished.

---

### Arrays

```
ton.array nums = [1, 2, 3]
push(nums, 4)          // [1, 2, 3, 4]
print(nums[0])          // 1
nums[0] = 10             // [10, 2, 3, 4]
print(len(nums))          // 4
```

See [Built-in native functions](#built-in-native-functions) for the full set of array operations (`push`/`pop`/`shift`/`unshift`/`slice`/`join`/`sort`/`reverse`/`contains`/`indexOf`/`map`/`filter`/`reduce`/`find`/`any`/`all`). Indexing out of bounds is a runtime error, not `nil`.

Strings also support read-only indexing — `s[0]` is the first character, as a 1-character string. Strings are immutable: `s[0] = "x"` is a runtime error; build a new string with `replace()`/`substring()`/the `ton.strings` module instead.

---

### Dictionaries

```
ton.dict user = {name: "kuro", age: 22}
print(user["name"])     // kuro
user["age"] = 23
user["city"] = "paris"   // adds a new key
print(keys(user))         // [name, age, city]
print(has(user, "age"))    // true
```

Dict keys are always strings (see [Known limitations](#known-limitations)): a bare identifier key (`name: ...`) is treated as its own name, and a non-identifier key needs a string literal (`"first name": ...`). Reading a missing key returns `nil` rather than erroring; writing to a new key adds it. `keys()`/`values()` iterate in insertion order.

---

### Error handling: try / catch / throw / finally

```
try {
    throw "something went wrong"
} catch (ton.err) {
    print("caught: " + err)
}
```

Any runtime error — a native error (division by zero, wrong argument count, index out of bounds, ...) or an explicit `throw` — is catchable. The caught value is always the error's message as a string, bound to the name given in `catch (name)`. `break`, `continue`, and `return` pass straight through an enclosing `try` untouched (they aren't errors).

`finally` adds a block that always runs after the `try` (and its `catch`, if any) — whether the block completed normally, threw, or hit a `return`:

```
ton.function readSafely() {
    try {
        return risky()
    } finally {
        print("cleanup ran")
    }
}
```

`try` needs at least one of `catch`/`finally`; `catch` alone (as above), `finally` alone (`try { ... } finally { ... }`, useful for cleanup when you want the error to keep propagating), or both together are all valid.

---

### Built-in native functions

All of these work with or without the `ton.` prefix (see [Language basics](#language-basics)).

**Core**
| Function | Description |
|---|---|
| `print(x)` | Prints a value to stdout |
| `type(x)` | Returns the type name of a value as a string |
| `str(x)` | Converts a value to string |
| `num(x)` | Converts a value to number (`0` if it can't be parsed) |
| `format(template, ...args)` | Replaces each `{}` in `template` with the matching argument (see [String formatting](#string-formatting)) |
| `json(x)` | Converts a value (array, dict, or scalar) to a JSON string |
| `assert(cond, [msg])` | Throws (catchable) if `cond` is falsy |
| `input([prompt])` | Prints an optional prompt, reads and returns one line from stdin |
| `readfile(path)` | Reads a file's content as `ton.html` (resolved relative to the script's directory too) |
| `writefile(path, content)` | Writes (overwriting) a file with `content`; returns whether it succeeded |

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
| `s[i]` | The character at index `i`, as a 1-character string (read-only — see [Arrays](#arrays)) |

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
| `contains(arr, item)` | Whether `item` is an element (structural equality) |
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

`ton.html` is a string-like type meant for markup: it prints and concatenates like a string, but `+`-concatenating it with anything else keeps the result `ton.html`, and a route handler (see [Local server](#local-server--routes-api)) returning `ton.html` gets served with `Content-Type: text/html` instead of `text/plain`.

```
ton.html page = "<h1>Hello</h1>"
ton.string name = "kuro"
ton.html greeting = page + "<p>Hi " + name + "</p>"
```

A plain string literal assigned directly to a `ton.html` variable is coerced automatically — you rarely need to think about the distinction until you're returning something from a route handler.

---

### Modules (IMPORT://)

```
IMPORT://mathutils      // loads mathutils.ton (a user module — see the guide below)
IMPORT://ton.mathutils  // loads the *built-in* ton.mathutils module — note the "ton." prefix
```

The leading `ton.` is how the parser tells a **built-in** module apart from a **user** module — it's never a filesystem lookup. See [Built-in system modules](#built-in-system-modules) for the full list of built-ins, and the guide below for writing your own.

---

### Creating a TON618 module — full guide

#### 1. Where modules live

`IMPORT://name` looks for `name.ton` in two places, in order:
1. The same directory as the script that imports it.
2. A `modules/` subdirectory next to that script.

So a project can keep one-off helpers next to the script, and shared code under `modules/`.

#### 2. What a module can declare

A module file is just a normal `.ton` file: it can declare variables and functions (`ton.function`), and everything it declares becomes available in the importing script's global scope once imported. A module isn't a separate namespace — there's no `mathutils.add(...)` syntax; you call `add(...)` directly (see the naming convention below).

#### 3. A worked example

`modules/greetings.ton`:
```
ton.function greetings_hello(name) {
    return "Hello, " + name + "!"
}

ton.function greetings_bye(name) {
    return "Goodbye, " + name + "."
}
```

Using it:
```
IMPORT://greetings

print(greetings_hello("kuro"))
print(greetings_bye("kuro"))
```

#### 4. Best practices for writing a module

- **Prefix every function with the module's own name** (`greetings_hello`, not `hello`) — since importing flattens everything into one global scope, this is the only thing preventing two modules (or a module and the main script) from silently overwriting each other's functions.
- Keep a module focused on one concern; compose several small modules rather than one large one.
- A module is only executed the first time it's imported — importing the same name twice (from anywhere) is a no-op the second time, so it's safe for two different modules to both import a third one they both depend on.

---

### Built-in system modules

TON618 ships with eleven **built-in system modules**. They aren't files on disk — they live inside the interpreter — but you still `IMPORT://ton.<name>` them before using them, exactly like a user module:

```
IMPORT://ton.sys
IMPORT://ton.os
IMPORT://ton.requests
IMPORT://ton.random
IMPORT://ton.time
IMPORT://ton.json
IMPORT://ton.mathutils
IMPORT://ton.strings
IMPORT://ton.encoding
IMPORT://ton.regex
IMPORT://ton.path
```

Calling any of the functions below without first importing its module fails with an "undefined function" error — this is intentional: a script only pays for (and only exposes) the built-ins it actually asked for. Every module follows the naming convention recommended for user modules above: every function is prefixed with the module's own name.

#### ton.sys — runtime & process information

| Function | Description |
|---|---|
| `sys_args()` | Array of extra command-line arguments passed after the script path (`ton618 script.ton foo bar` → `["foo", "bar"]`) |
| `sys_platform()` | `"windows"`, `"termux"`, `"macos"`, or `"linux"` — auto-detected (Termux is told apart from plain Linux via its `$PREFIX` environment variable) |
| `sys_arch()` | `"x64"`, `"arm64"`, `"arm"`, `"x86"`, or `"unknown"` |
| `sys_version()` | The interpreter's own version string (e.g. `"beta-1.0.1"`), same as `ton618 --version` |
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
| `os_isfile(path)` / `os_isdir(path)` | Whether `path` exists and is that kind of entry |
| `os_mkdir(path)` | Creates a directory (and any missing parent directories) |
| `os_remove(path)` | Removes a file or an empty directory |
| `os_rename(src, dst)` | Renames/moves a file or directory |
| `os_copy(src, dst)` | Copies a file, overwriting `dst` if it already exists |
| `os_listdir(path)` | Array of entry names directly inside a directory (not recursive) |
| `os_readlines(path)` | Array of the file's lines (no trailing newlines) |
| `os_appendfile(path, content)` | Appends text to a file, creating it if needed |
| `os_tempdir()` | The system's temporary-files directory |

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
    ok: true,      // whether a response was received at all
    status: 200,     // the HTTP status code (0 if the request failed outright)
    body: "...",       // the raw response body
    error: ""            // a human-readable reason when ok is false
}
```

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

Complements the always-available `random()` with a few common patterns:

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

#### ton.json — JSON parsing & pretty-printing

The always-available `json(x)` native already turns a value *into* compact JSON. `IMPORT://ton.json` adds the other direction, plus a multi-line format:

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

Complements the always-available `sqrt`/`pow`/`abs`/`floor`/`ceil`/`round`/`min`/`max` with trigonometry, logarithms, and small statistics/number-theory helpers:

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
| `mathutils_min_of(arr)` / `mathutils_max_of(arr)` | Smallest/largest number in an array (unlike `min()`/`max()`, which take separate arguments) |

```
IMPORT://ton.mathutils

print(mathutils_gcd(48, 18))          // 6
print(mathutils_mean([1, 2, 3, 4]))   // 2.5
print(mathutils_clamp(15, 0, 10))     // 10
```

#### ton.strings — extra string helpers

Complements the always-available `upper`/`lower`/`trim`/`split`/`replace`/`contains`/`substring`:

| Function | Description |
|---|---|
| `strings_starts_with(s, prefix)` / `strings_ends_with(s, suffix)` | Prefix/suffix tests |
| `strings_repeat(s, n)` | Repeats `s` `n` times |
| `strings_reverse(s)` | Reverses a string (the always-available `reverse()` only works on arrays) |
| `strings_capitalize(s)` | Uppercases the first letter, lowercases the rest |
| `strings_pad_left(s, width, [ch])` / `strings_pad_right(s, width, [ch])` | Pads `s` up to `width` with `ch` (default `" "`) |
| `strings_center(s, width, [ch])` | Centers `s` within `width`, padding both sides with `ch` |
| `strings_count(s, sub)` | Number of non-overlapping occurrences of `sub` in `s` |
| `strings_trim_start(s)` / `strings_trim_end(s)` | Strip whitespace from only the start/end of `s` (`trim()` strips both) |
| `strings_words(s)` | Array of whitespace-separated words (unlike `split()`, which needs an exact separator) |
| `strings_replace_first(s, search, repl)` | Replaces only the first occurrence of `search` |
| `strings_snake_case(s)` | `"Hello World"` → `"hello_world"` |
| `strings_camel_case(s)` | `"hello_world"` → `"helloWorld"` |

```
IMPORT://ton.strings

print(strings_capitalize("hello world"))  // "Hello world"
print(strings_pad_left("7", 3, "0"))      // "007"
```

#### ton.encoding — base64, hex, and URL percent-encoding

Dependency-free encoders/decoders for the formats scripts most often need when talking to web APIs:

| Function | Description |
|---|---|
| `base64_encode(s)` / `base64_decode(s)` | Standard base64 |
| `hex_encode(s)` / `hex_decode(s)` | Lowercase hexadecimal |
| `url_encode(s)` / `url_decode(s)` | Percent-encoding (also used internally to parse a local server request's query string) |

```
IMPORT://ton.encoding

print(base64_encode("hello world"))  // aGVsbG8gd29ybGQ=
print(url_encode("a b/c"))            // a%20b%2Fc
```

#### ton.regex — pattern matching

Backed by C++'s standard `<regex>` library (ECMAScript syntax) — no external dependency, and the same regex flavor used by JavaScript.

| Function | Description |
|---|---|
| `regex_test(s, pattern)` | Whether `pattern` matches anywhere in `s` |
| `regex_match(s, pattern)` | `[fullMatch, group1, group2, ...]` for the first match, or `nil` |
| `regex_find_all(s, pattern)` | Array of every full match found in `s` |
| `regex_replace(s, pattern, repl)` | `s` with every match replaced; `repl` can use `$1`, `$2`... for capture groups |
| `regex_split(s, pattern)` | Array of substrings, splitting `s` on every match |

```
IMPORT://ton.regex

print(regex_test("hello123", "[0-9]+"))                          // true
print(regex_replace("2024-01-15", "(\\d+)-(\\d+)-(\\d+)", "$3/$2/$1"))  // 15/01/2024
```

#### ton.path — filesystem path manipulation

Pure string/path manipulation — no disk access (see `ton.os` for that).

| Function | Description |
|---|---|
| `path_join(a, b, ...)` | Joins any number of path segments with the platform separator |
| `path_basename(p)` | The filename with its extension |
| `path_dirname(p)` | The parent directory |
| `path_extension(p)` | The extension including the dot, or `""` if none |
| `path_stem(p)` | The filename without its extension |
| `path_absolute(p)` | An absolute path (resolved against the current working directory) |

```
IMPORT://ton.path

print(path_join("a", "b", "c.txt"))     // a/b/c.txt
print(path_extension("/foo/bar.txt"))    // .txt
```

---

### Local server & routes (API)

TON618 has a built-in HTTP server, no external dependencies (raw sockets).

**Simple mode** — serve the same content on every request:
```
ton.serve(8080, "<h1>Hello from TON618!</h1>")
```

**Routing mode** — register handlers per path and method, then call `serve(port)` with no second argument. Every handler takes a single argument, a `ton.dict` describing the request:

```
{
    method: "GET",           // the HTTP method
    path: "/user/42",          // the request path, without the query string
    query: {a: "1"},             // parsed query-string parameters, as a dict
    params: {id: "42"},            // ":name" segments captured from the route pattern
    body: ""                         // the raw request body (POST/PUT/... payload)
}
```

A route's path can contain `:name` segments, captured into `req["params"]`:

```
ton.function userById(req) {
    ton.string id = req["params"]["id"]
    return json({id: id})
}

ton.function search(req) {
    ton.dict query = req["query"]
    ton.string q = has(query, "q") ? query["q"] : ""
    return "You searched for: " + q
}

ton.function createUser(req) {
    print("Received body: " + req["body"])
    return json({ok: true})
}

ton.get("/user/:id", ton.userById)
ton.get("/search", ton.search)          // GET /search?q=kuro -> req["query"]["q"]
ton.post("/users", ton.createUser)

ton.serve(8081)
```

A handler returning `ton.html` is served with `Content-Type: text/html`; anything else is served as `text/plain` (use `json(...)` to build a JSON response body, which is still plain text over the wire — set your client to expect `application/json` if you need that exact header). A path with no matching route or method returns `404`. The server is single-threaded and blocking — it handles one request at a time (see [Known limitations](#known-limitations)).

---

### The debugger

Start a script with `--debug` (or `--break=<line>`) to drop into an interactive step-debugger the moment execution reaches a breakpoint (or from the very first statement, with `--debug` alone):

```bash
ton618 script.ton --debug
ton618 script.ton --break=12 --break=20
```

Commands at the `(ton618-dbg)` prompt:

| Command | Effect |
|---|---|
| `s`, `step` | Run the next line (steps into function calls) |
| `c`, `continue` | Continue until the next breakpoint |
| `b <line>` | Add a breakpoint |
| `rb <line>` | Remove a breakpoint |
| `p <var>` | Print a variable's value and type |
| `vars`, `locals` | Print all local variables in the current scope |
| `bt`, `backtrace`, `stack` | Print the call stack |
| `l`, `list` | Show more source context around the current line |
| `h`, `help`, `?` | Show this list |
| `q`, `quit`, `exit` | Stop the program |

---

### Architecture — how the interpreter works

TON618 is a classic **tree-walking interpreter**, made of three stages that run in a straight pipeline:

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
├── include/                 # headers (.hpp) — declarations and design notes
│   ├── Token.hpp              — the language's vocabulary (TokenType, Token)
│   ├── Lexer.hpp                — text -> tokens
│   ├── Ast.hpp                    — the AST node shapes (Expr, Stmt)
│   ├── Parser.hpp                   — tokens -> AST
│   ├── Value.hpp                      — runtime value representation
│   ├── Environment.hpp                  — variable scopes (name -> Value, chained)
│   ├── Interpreter.hpp                    — walks the AST, runs the program
│   ├── Debugger.hpp                         — breakpoints / step mode
│   ├── LocalServer.hpp                        — raw-socket HTTP server (ton.serve/get/post)
│   ├── HttpClient.hpp                           — raw-socket HTTP client (the ton.requests module)
│   ├── JsonParser.hpp                             — JSON text -> Value (ton.json's json_parse)
│   ├── Platform.hpp                                 — OS/Termux/architecture auto-detection
│   └── Version.hpp                                    — the interpreter's own version string
├── src/                     # implementations (.cpp), one per header above,
│                              plus main.cpp (CLI entry point: reads a file, runs the
│                              Lexer -> Parser -> Interpreter pipeline, and wires up
│                              --debug / --break / --version / --update / --uninstall)
├── exemples/                # example .ton scripts, including one that exercises most
│                              of the language (test_new_features.ton), plus
│                              documentation.html, playground.html, and ton618-lite.js
│                              (a JS reimplementation used only by the playground)
├── scripts/                 # install.sh/.ps1/.bat, uninstall.sh/.ps1/.bat, rebuild.sh,
│                              zig-strip.sh, and doc_templates/ (see `make docs` above)
└── modules/                 # shared .ton modules (see the module guide above)
```

Every header starts with a short comment block explaining its role in the pipeline and, where relevant, exactly what to touch first if you want to extend that part of the language — read those before diving into the `.cpp` files.

Two design choices worth calling out because they shape almost every other file:

- **AST/Value nodes are "fat structs", not a class hierarchy.** `Expr` and `Stmt` (Ast.hpp) and `Value` (Value.hpp) each hold every field any of their variants might need, with a `type` tag saying which ones apply. This trades a bit of memory for a codebase where `evaluate()`/`execute()` are single `switch` statements instead of a scattered visitor pattern — much easier to read end-to-end for a language this size. Some node kinds *reuse* another kind's fields (documented next to the `enum`) instead of adding new ones, to keep the structs from growing unbounded — `SWITCH`, for instance, reuses `IF`'s `condition`/`elseBranch` fields for its subject expression and default block.
- **Control flow uses C++ exceptions.** `break`, `continue`, and `return` are implemented as tiny exception types (`BreakException`, `ContinueException`, `ReturnException`, see Interpreter.hpp) thrown from `execute()` and caught by the nearest enclosing loop, `switch`, or function call. This mirrors, almost one-to-one, how those constructs unwind nested blocks in the AST — no manual "did we break?" flag needs to be threaded through every recursive call.

A third thing worth knowing if you're touching the standard library: `valuesEqual()` (a free function near the top of `src/Interpreter.cpp`) is the single source of truth for what "equal" means — the `==`/`!=` operators, `switch`/`case` matching, and the `contains()`/`indexOf()` natives all call it, so a fix or a new value kind only needs handling in one place.

---

### Extending the interpreter itself

**Adding a new native function** (the most common change): pick the right `registerBuiltin*()` in `src/Interpreter.cpp` (or `defineNatives()` for something that should always be available, no `IMPORT://` needed), and add a `def("name", [...](std::vector<Value>& args) -> Value { ... })` call. Look at a neighboring function for the argument-checking convention (throw `std::runtime_error` with a clear message on the wrong type/count). Then document it in this file, and — if it's pure computation with no filesystem/network/process access — consider porting it to `exemples/ton618-lite.js` too, so the browser playground stays in sync (see that file's own header comment for exactly what it can and can't support).

**Adding a new operator or keyword**: add a `TokenType` (Token.hpp), teach the Lexer to produce it (`src/Lexer.cpp` — the `keywords` map for a word, `scanToken()` for punctuation), then teach the Parser to consume it at the right precedence level (see Parser.hpp's comment on the `expression -> assignment -> ternary -> ...` chain) and the Interpreter to evaluate it.

**Adding a new statement kind** (like `switch` was): add a `StmtType` in Ast.hpp (reusing existing fields where they fit, rather than growing the struct), a parsing method in Parser.cpp dispatched from `statement()`, and a `case` in `Interpreter::execute()`.

**Adding a new value/type kind** (e.g. a `ton.set`):
1. Add a `ValueType` and storage field in `include/Value.hpp`; extend `typeName()`, `toString()`, `toJson()`.
2. Add a `VarKind` in `include/Ast.hpp` and a `TokenType`/keyword (`Token.hpp`, `Lexer.cpp`) if it needs its own `ton.<type>` declaration syntax.
3. Extend `Interpreter::checkType` (`src/Interpreter.cpp`) so `ton.<type> x = ...` enforces it.
4. Add literal syntax in `Parser::primary()` if the type needs one (see how `ARRAY`/`DICT` literals are parsed there), and evaluation support in `Interpreter::evaluate`.

After any change, run `make` and sanity-check with the scripts in `exemples/` (in particular `exemples/test_new_features.ton`) before relying on the change.

---

### Publishing the docs to GitHub Pages

`exemples/` is fully static and self-contained — `documentation.html` embeds this very file (see `make docs` above), and `playground.html` runs entirely client-side via `ton618-lite.js`. `.github/workflows/pages.yml` publishes that whole folder to GitHub Pages as-is (not the rest of the repository): it copies `exemples/` verbatim, plus `documentation.html` as `index.html` at the site root so the docs load at the bare Pages URL, with `playground.html` and `ton618-lite.js` reachable right next to it exactly as `documentation.html` already links to them.

One-time setup:
1. **GitHub → this repository → Settings → Pages.**
2. Under **Build and deployment**, set **Source** to **GitHub Actions** (not "Deploy from a branch").

That's it — `.github/workflows/pages.yml` runs on every push to `main` that touches `exemples/` (or manually from the **Actions** tab), and publishes to:
```
https://<user>.github.io/ton618/                       — documentation.html (as index.html)
https://<user>.github.io/ton618/playground.html         — the playground
https://<user>.github.io/ton618/ton618-lite.js          — the playground's engine
```
(adjust the org/user in those URLs to whichever account owns this repo.)

Whenever `DOCUMENTATION.md` changes, run `make docs`, commit the regenerated `exemples/documentation.html`, and push — the workflow re-publishes automatically.

**Using a custom domain** (e.g. a free [DuckDNS](https://www.duckdns.org) subdomain instead of `github.io`): this needs no server of your own — GitHub hosts the site, not you.

1. On DuckDNS, point your subdomain's IP at one of GitHub Pages' fixed addresses instead of any server you run: `185.199.108.153`, `185.199.109.153`, `185.199.110.153`, or `185.199.111.153` (DuckDNS only accepts one IP per subdomain, so pick just one). If that subdomain is also kept updated by a dynamic-DNS script running somewhere (cron on a VPS, a router, etc.), that script will overwrite this the next time it runs — use a subdomain that nothing else auto-updates.
2. `exemples/CNAME` in this repo already holds the domain the docs are pinned to; `.github/workflows/pages.yml` publishes it as-is, which is what tells GitHub Pages to serve that domain.
3. **GitHub → this repository → Settings → Pages** should then show the custom domain under **Custom domain** (DNS propagation can take a few minutes) — once it verifies, tick **Enforce HTTPS**.

---

### Known limitations

- `serve()` is single-threaded and blocking — one request at a time, no concurrency
- `ton.int` and `ton.float` are currently the same internal number type (no integer-only enforcement)
- Dict keys are always strings (no numeric or nested-structural keys)
- `IMPORT://` only resolves a flat module name, no subfolders/namespacing
- No user-defined types/classes — only the built-in scalar/array/dict/html/function kinds
- `ton.requests` only speaks plain `http://` — no TLS/`https://`, no redirects, no chunked transfer-encoding
- The browser playground (`ton618-lite.js`) doesn't implement `ton.encoding`/`ton.regex`/`ton.path` yet, and can print floats with more decimal digits than the real interpreter (a JavaScript-vs-C++ number-formatting difference, not a value difference)

---

### Changelog

**beta-1.0.3** — new operators, flexible function parameters, and `finally`:

- **Default parameters**: `function greet(name, greeting = "Hello") { ... }` — the default is evaluated at call time, in order, so a later default can reference an earlier parameter.
- **Rest parameters**: `function sum(...nums) { ... }` collects any extra arguments into an array; may be mixed with fixed parameters as long as `...` is last. Arity-error messages now adapt (`expects between 1 and 2 argument(s)`, `expects at least 1 argument(s)`).
- **`??` (nil-coalescing operator)**: `value ?? fallback` returns `value` unless it's `nil`, in which case it returns `fallback` — the fallback is never evaluated otherwise (short-circuiting). See [Operators](#operators).
- **`in` (membership operator)**: `value in array`, `value in dict` (tests keys), `value in string` (substring test). See [Operators](#operators).
- **`finally`**: `try`/`catch` gained a `finally` block that always runs — on success, on a caught error, or through a `return` — and `catch` is now optional as long as `finally` is present (`try { ... } finally { ... }`). See [Error handling](#error-handling-try--catch--throw--finally).

**beta-1.0.2** — simpler string formatting, and an AI-coding skill:

- **String interpolation (`"Hello ${name}!"`) has been removed** in favor of the much simpler **`format(template, ...args)`**: `format("Hello {}!", name)`. Interpolation required the lexer to recursively re-lex arbitrary expressions embedded inside string literals (tracking nested quotes and braces) — a lot of machinery for what a single string-processing function does just as well, with far less to reason about. See [String formatting](#string-formatting).
- **An AI-coding skill for TON618**, in both a Claude Code `SKILL.md` and a generic `AGENTS.md`, so an AI agent can write correct `.ton` scripts without re-deriving the language from this document — see the **"Skills for AI"** link next to the playground, or `exemples/skills/`.

**beta-1.0.1** — bug fixes, new syntax, and a much larger standard library:

*Correctness fixes:*
- **`<`, `<=`, `>`, `>=` now type-check and support strings**: previously they silently compared the internal number field regardless of type, so e.g. `"apple" < "banana"` evaluated as `0 < 0` (`false`) instead of comparing lexicographically. They now compare numbers numerically, strings/html lexicographically, and error on any other combination.
- **`==`/`!=` now do structural equality for arrays and dicts** (recursively, element-by-element) instead of always being "not equal" to each other regardless of content; a string and an html value with the same text now also compare equal.
- **`%` now errors on division by zero** instead of silently producing `NaN`, matching `/`.

*New syntax:*
- ~~String interpolation~~ — added here, replaced by `format()` in beta-1.0.2 (see above)
- **`switch` / `case` / `default`**, with no fallthrough and support for multiple values per case
- **String indexing**: `s[0]` reads a character (strings stay immutable — assigning to an index is an error)

*New standard library:*
- Three new built-in modules: **`ton.encoding`** (base64/hex/URL), **`ton.regex`** (backed by `<regex>`), **`ton.path`** (path manipulation)
- New always-available natives: `find`, `any`, `all`, `writefile`
- New `ton.os`: `os_appendfile`, `os_readlines`, `os_isfile`, `os_isdir`, `os_rename`, `os_tempdir`, `os_copy`
- New `ton.strings`: `strings_trim_start`, `strings_trim_end`, `strings_words`, `strings_center`, `strings_replace_first`, `strings_snake_case`, `strings_camel_case`
- New `ton.mathutils`: `mathutils_min_of`, `mathutils_max_of`
- New `ton.json`: `json_pretty`
- New `ton.sys`: `sys_arch`, `sys_version`; `sys_platform()` now also recognizes `"termux"`

*Local server:*
- **Route handlers now receive the request as a `ton.dict`** (`{method, path, query, params, body}`) instead of being called with no arguments — a breaking change from beta-1.0.0, closing the previous "can't read POST body or URL params" limitation. Routes can use `:name` path segments (`/user/:id`).

*Distribution & tooling:*
- **Precompiled binaries are now built and published by GitHub Actions** (`.github/workflows/release.yml`), cross-compiling Linux x64, Windows x64, and Termux/Android arm64 on every tagged release
- **`ton618 --update [version]`**: re-downloads the binary matching the current platform from GitHub Releases and replaces the running one
- **`ton618 --version` / `-v`**: prints the version, auto-detected platform, and architecture
- **`.github/workflows/pages.yml`** publishes `exemples/` (docs + playground) to GitHub Pages, with custom-domain support (`exemples/CNAME`)
- The browser playground (`ton618-lite.js`) now also implements `ton.mathutils`, `ton.strings`, `find`/`any`/`all`, `json_pretty`, `switch`/`case`, and string indexing — closing several gaps where it silently behaved differently from the real interpreter

**Earlier language additions** (pre-beta-1.0.1):

- **Compound assignment**: `+=`, `-=`, `*=`, `/=`, `%=`
- **Increment/decrement**: `++`, `--` (postfix, on variables and indexed targets)
- **Ternary operator**: `cond ? a : b`
- **`for ... in`** loop over arrays and dicts, alongside a classic C-style `for`
- **`try` / `catch` / `throw`** error handling
- **`ton.dict`** type, with `{key: value, ...}` literals, `[]` indexing, and `keys()`/`values()`/`has()`
- **Function expressions**: `ton.function(params) { ... }` as an inline value, for callbacks
- A large standard library: `type`, `assert`, `input`, math/string/array/dict helpers, `map`/`filter`/`reduce`
- A documented module-writing guide, and eight (now eleven) built-in system modules
- A browser playground (`exemples/playground.html`, backed by `exemples/ton618-lite.js`)
- `documentation.html` renders `DOCUMENTATION.md` directly (embedded at edit time via `make docs`)

---
---

## 🇫🇷 Français

### Table des matières
1. [Qu'est-ce que TON618](#quest-ce-que-ton618)
2. [Installer & compiler l'interpréteur](#installer--compiler-linterpréteur)
3. [Lancer un script](#lancer-un-script)
4. [Le playground dans le navigateur](#le-playground-dans-le-navigateur)
5. [Bases du langage](#bases-du-langage)
6. [Types](#types-1)
7. [Formatage de chaînes](#formatage-de-chaînes)
8. [Opérateurs](#opérateurs)
9. [Structures de contrôle](#structures-de-contrôle)
10. [Fonctions](#fonctions)
11. [Tableaux](#tableaux)
12. [Dictionnaires](#dictionnaires)
13. [Gestion des erreurs : try / catch / throw / finally](#gestion-des-erreurs--try--catch--throw--finally)
14. [Fonctions natives](#fonctions-natives)
15. [Type HTML](#type-html)
16. [Modules (IMPORT://)](#modules-import-1)
17. [Créer un module TON618 — guide complet](#créer-un-module-ton618--guide-complet)
18. [Modules système intégrés](#modules-système-intégrés)
19. [Serveur local & routes (API)](#serveur-local--routes-api)
20. [Le débogueur](#le-débogueur)
21. [Architecture — comment fonctionne l'interpréteur](#architecture--comment-fonctionne-linterpréteur)
22. [Étendre l'interpréteur lui-même](#étendre-linterpréteur-lui-même)
23. [Publier la doc sur GitHub Pages](#publier-la-doc-sur-github-pages)
24. [Limitations connues](#limitations-connues)
25. [Changelog](#changelog-1)

---

### Qu'est-ce que TON618

TON618 est un petit langage interprété, dans le même esprit que Python ou HolyC : tu écris un fichier `.ton`, tu le lances directement avec l'interpréteur `ton618`, et il s'exécute immédiatement — aucune étape de compilation séparée pour tes scripts.

L'interpréteur lui-même (`ton618`) est écrit en C++17 et se présente comme un unique binaire, sans dépendance. Il est compilé en croisé pour Linux, Termux (Android) et Windows (voir [Installer & compiler](#installer--compiler-linterpréteur)), et détecte automatiquement lequel de ces trois il exécute au démarrage.

La caractéristique principale du langage est le **préfixe `ton.`** : déclarer une variable ou une fonction l'utilise toujours (`ton.int x = 5`, `ton.function f() { ... }`). Une fois quelque chose déclaré, y faire référence à nouveau — le lire, le réassigner, ou nommer un paramètre de fonction — accepte aussi bien `ton.nom` que le simple `nom` ; les deux désignent la même chose. Cette documentation utilise `ton.` partout par souci de clarté, sauf pour des noms éphémères comme les compteurs de boucle et les paramètres de fonction, comme le fait déjà le style utilisé dans `exemples/`. Voir [Bases du langage](#bases-du-langage) pour savoir précisément où le préfixe est obligatoire ou optionnel.

TON618 prend en charge la **gestion d'erreurs** (`try`/`catch`/`throw`), un **type dictionnaire** (`ton.dict`), les boucles **`for ... in`** et `for` classique (style C), une instruction **`switch`/`case`**, une fonction **`format()`** pour construire des chaînes sans enchaîner les `+`, l'**assignation composée** (`+=`, `-=`, ...), l'**incrémentation/décrémentation** (`++`, `--`), un **opérateur ternaire** (`cond ? a : b`), des **expressions de fonction** (callbacks anonymes), et une bibliothèque standard assez large couvrant maths, chaînes, tableaux, dictionnaires, fichiers, JSON, expressions régulières, et encodage base64/hex/URL — voir le [Changelog](#changelog-1) pour l'historique complet.

---

### Installer & compiler l'interpréteur

Les **binaires précompilés** (aucun compilateur requis) sont construits par [GitHub Actions](.github/workflows/release.yml) — compilation croisée pour Linux x64, Windows x64 et Termux/Android arm64 — et publiés sur les [GitHub Releases](https://github.com/kurodaki-dev/ton618/releases). Les installeurs détectent automatiquement ta plateforme (Termux est distingué d'un Linux classique via sa variable d'environnement `$PREFIX`) et ajoutent `ton618` à ton `PATH` :

```bash
# Linux / Termux
curl -fsSL https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.sh | bash
```
```powershell
# Windows
powershell -c "irm https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.ps1 | iex"
```

Installe une release précise plutôt que la dernière en mettant `TON618_VERSION` (`install.sh`) ou `$env:TON618_VERSION` (`install.ps1`) à un tag de release (ex. `beta-1.0.1`) avant de lancer la commande ci-dessus. Désinstalle à tout moment avec `ton618 --uninstall`, ou les scripts `uninstall.sh`/`uninstall.ps1` correspondants.

Aucun compilateur requis pour tout ça — un binaire précompilé est tout ce que tu lances jamais. Si tu contribues à l'interpréteur lui-même et as besoin de le compiler depuis les sources, voir [Architecture](#architecture--comment-fonctionne-linterpréteur) et [Étendre l'interpréteur lui-même](#étendre-linterpréteur-lui-même) plus bas (le projet a juste besoin d'un compilateur C++17 et de `make` — `make` compile `./ton618`, `make docs` régénère `exemples/documentation.html` depuis ce fichier après une modification).

---

### Lancer un script

```bash
ton618 chemin/vers/script.ton
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

Tout ce qui suit le chemin du script et n'est pas une de ces options est transmis tel quel au script, lisible via `sys_args()` (voir [ton.sys](#modules-système-intégrés)).

Exemple :
```bash
ton618 exemples/test.ton --break=10 --break=20
ton618 --update             # met à jour vers la dernière release
ton618 --update beta-1.0.1  # épingle une release précise
```

---

### Le playground dans le navigateur

Pas envie d'installer quoi que ce soit juste pour essayer un extrait ? Ouvre `exemples/playground.html` dans n'importe quel navigateur (aucun serveur requis) pour un éditeur de code en direct avec un bouton "Run" et des extraits d'exemple à charger.

Le playground exécute **`ton618-lite.js`**, une réimplémentation JavaScript du langage, entièrement côté client — rien n'est envoyé nulle part, aucune installation requise. Il reproduit fidèlement l'interpréteur réel : la grammaire complète (y compris `switch`/`case`), et les modules intégrés `ton.random`, `ton.time`, `ton.json`, `ton.mathutils` et `ton.strings`, puisqu'aucun d'eux n'a besoin de quoi que ce soit qu'un navigateur ne puisse déjà faire.

Ce qu'il ne peut pas faire, parce qu'un bac à sable de navigateur n'a ni système de fichiers, ni sockets, ni arguments de ligne de commande pour les alimenter :
- `ton.sys`, `ton.os`, `ton.requests` — accès réel au processus/système de fichiers/réseau ; importer l'un d'eux donne une erreur claire expliquant pourquoi, plutôt que de se comporter silencieusement différemment de l'interpréteur réel
- `ton.encoding`, `ton.regex`, `ton.path` — pas encore réimplémentés en JavaScript (ils existent dans l'interpréteur réel)
- `IMPORT://<fichier>` pour tes propres fichiers de module (pas de système de fichiers pour les lire)
- `get`/`post`/`serve` (pas de serveur HTTP), `readfile`/`writefile` (pas d'accès disque)

Une différence cosmétique : afficher un flottant peut montrer plus de décimales dans le playground que dans l'interpréteur réel (les conversions nombre-vers-texte par défaut de JavaScript et de C++ arrondissent différemment) — la *valeur* est identique dans les deux cas, seule sa forme affichée diffère.

Pour tout ce qui dépasse de rapides expérimentations — et pour la bibliothèque standard complète — installe l'interpréteur réel (voir [Installer & compiler](#installer--compiler-linterpréteur) plus haut).

---

### Bases du langage

Chaque instruction se termine là où la suivante commence — les points-virgules sont optionnels et rarement utilisés en pratique. Les commentaires existent sous deux formes :

```
// un commentaire de ligne, va jusqu'à la fin de la ligne

/* un commentaire de bloc,
   peut s'étendre sur plusieurs lignes */
```

**Déclarer** quelque chose nécessite toujours le préfixe `ton.`, suivi d'un mot-clé de type et d'un nom :

```
ton.int age = 25
ton.function saluer(nom) {
    print("Bonjour, " + nom)
}
```

**Faire référence** à quelque chose déjà déclaré — lire une variable, la réassigner, appeler une fonction, ou nommer un paramètre de fonction — accepte la forme préfixée ou la forme nue ; le parseur traite `ton.x` et `x` de façon identique une fois que `x` existe dans le scope :

```
ton.int age = 25
print(ton.age)   // ok
print(age)        // ok aussi — même variable
age = age + 1      // ok aussi
```

Les paramètres de fonction s'écrivent conventionnellement sans le préfixe (`ton.function saluer(nom) { ... }`, pas `ton.function saluer(ton.nom)`), comme le fait chaque exemple de ce document et de `exemples/`.

Une assignation nue peut seulement *réassigner* une variable existante — `age = 5` échoue avec "was never declared" à moins que quelque chose ait déjà déclaré `age` plus tôt. Pour introduire une toute nouvelle variable, il faut toujours la forme complète `ton.<type> nom = valeur`.

---

### Types

| Type | Déclaration | Exemple |
|---|---|---|
| Entier/flottant | `ton.int` / `ton.float` | `ton.int x = 5` |
| Chaîne | `ton.string` | `ton.string s = "salut"` |
| Booléen | `ton.bool` | `ton.bool ok = true` |
| Tableau | `ton.array` | `ton.array a = [1, 2, 3]` |
| Dictionnaire | `ton.dict` | `ton.dict d = {a: 1}` |
| HTML | `ton.html` | `ton.html page = "<h1>Salut</h1>"` |
| Fonction | `ton.function` | `ton.function f() { ... }` |

`ton.int` et `ton.float` reposent tous les deux sur le même nombre interne en double précision — TON618 n'impose pas un stockage entier strict (voir [Limitations connues](#limitations-connues)). Assigner un littéral chaîne directement à une variable `ton.html` la convertit automatiquement en HTML (une commodité pour les gestionnaires de route qui renvoient du balisage).

`type(x)` renvoie le nom du type d'une valeur sous forme de chaîne : `"int/float"`, `"string"`, `"html"`, `"bool"`, `"array"`, `"dict"`, `"nil"`, ou `"function"`.

---

### Formatage de chaînes

Les littéraux chaîne sont simples — pas de syntaxe d'expression embarquée. Pour construire une chaîne à partir de variables sans un long enchaînement de `+`, utilise `format(modele, ...args)` : elle remplace chaque `{}` dans `modele`, de gauche à droite, par l'argument correspondant converti en texte de la même façon que `str()` le ferait.

```
ton.string nom = "kuro"
ton.int age = 5
print(format("Bonjour {}, tu auras {} l'année prochaine !", nom, age + 1))
// Bonjour kuro, tu auras 6 l'année prochaine !
```

Un argument peut être n'importe quelle valeur — un nombre, une chaîne, le résultat d'un appel de fonction, tout ce que `str()` peut convertir. Un `{}` en trop sans argument correspondant reste tel quel (`{}`) ; des arguments en trop au-delà du nombre de `{}` sont simplement ignorés.

---

### Opérateurs

| Catégorie | Opérateurs |
|---|---|
| Arithmétique | `+` `-` `*` `/` `%` |
| Comparaison | `==` `!=` `<` `<=` `>` `>=` |
| Logique | `&&`/`and`, `\|\|`/`or`, `!` |
| Coalescence nulle | `??` |
| Appartenance | `in` |
| Assignation | `=` `+=` `-=` `*=` `/=` `%=` |
| Incrémentation/décrémentation | `++` `--` (postfixe) |
| Ternaire | `cond ? a : b` |

Notes :
- `+` sur deux nombres les additionne ; si l'un des deux côtés est une chaîne ou du `ton.html`, il concatène (en convertissant l'autre côté en texte d'abord) — le résultat est `ton.html` si l'un des opérandes l'était.
- `<`, `<=`, `>`, `>=` comparent deux nombres numériquement, ou deux chaînes/valeurs `ton.html` lexicographiquement ; comparer toute autre combinaison (ou un tableau, un dict, un booléen) est une erreur d'exécution plutôt qu'un résultat silencieusement dénué de sens.
- `==`/`!=` utilisent l'égalité structurelle : nombres/booléens/nil comparent par valeur, une chaîne et une valeur html avec le même texte sont égales, et les tableaux/dicts comparent élément par élément (récursivement) plutôt que par référence — `[1, 2] == [1, 2]` vaut `true`.
- `/` et `%` lèvent tous les deux "Division by zero" pour un côté droit à zéro, plutôt que de produire silencieusement `inf`/`NaN`.
- `and`/`or` sont des synonymes exacts de `&&`/`||` — utilise celui qui se lit le mieux.
- `??` ("coalescence nulle") évalue son côté gauche et le retourne tel quel sauf s'il vaut `nil`, auquel cas il évalue et retourne le côté droit à la place — le côté droit n'est jamais évalué autrement, donc on peut y mettre un fallback avec effets de bord sans risque. Pratique avec les accès aux dicts : `dict["manquant"] ?? "defaut"`.
- `in` teste l'appartenance : `valeur in tableau` vérifie les éléments du tableau (avec les mêmes règles d'égalité que `==`), `valeur in dict` vérifie les **clés** du dict, et `valeur in chaine` cherche une sous-chaîne. Le côté droit doit être un tableau, un dict, ou une chaîne/`ton.html` — toute autre valeur est une erreur d'exécution.

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
while i < 5 {
    print(i)
    i++
}
```

**for** (style C)
```
for ton.int i = 0; i < 5; i++ {
    print(i)
}
```

**for ... in** (itère un tableau ou un dict)
```
ton.array noms = ["kuro", "rusher", "natlep"]
for ton.n in noms {
    print(n)
}

ton.dict scores = {kuro: 10, rusher: 20}
for ton.cle in scores {
    print(cle + " -> " + str(scores[cle]))
}
```
Itérer un `ton.dict` parcourt ses **clés**, dans l'ordre où elles ont été écrites ; utilise `scores[cle]` pour lire la valeur correspondante.

**switch / case** — pas de fallthrough : chaque case correspondante (ou `default`, si aucune ne correspond) exécute son propre bloc et le switch est terminé.
```
switch ton.statut {
    case 200, 201: {
        print("ok")
    }
    case 404: {
        print("introuvable")
    }
    default: {
        print("autre : " + str(ton.statut))
    }
}
```
Un case peut lister plusieurs valeurs séparées par des virgules. La correspondance utilise les mêmes règles d'égalité que `==`. Un `break;` égaré dans un case (habitude d'autres langages) est inoffensif — il quitte juste le switch plus tôt, puisqu'il n'y a de toute façon pas de fallthrough dont sortir.

`break` et `continue` sont pris en charge dans `while`, `for`, et `for ... in`.

---

### Fonctions

```
ton.function additionner(a, b) {
    return a + b
}
print(additionner(2, 3))   // 5
```

Les fonctions sont des valeurs : elles peuvent être stockées dans des variables, passées en argument, et renvoyées par d'autres fonctions. Les **expressions de fonction** en créent une en ligne, le plus souvent comme callback :

```
ton.array doubles = map([1, 2, 3], ton.function(n) { return n * 2 })
```

Une expression de fonction peut optionnellement porter un nom (`ton.function label(n) { ... }`) utilisé seulement pour un affichage débogueur/erreur plus clair — il n'est pas lié comme variable sous ce nom.

**Paramètres par défaut** — donne à un paramètre `= expression` pour le rendre optionnel ; le défaut est évalué au moment de l'appel (dans l'ordre, donc un défaut plus tardif peut référencer un paramètre précédent) et seulement quand l'argument est omis :

```
ton.function saluer(nom, salutation = "Bonjour") {
    return salutation + ", " + nom + " !"
}
print(saluer("kuro"))              // Bonjour, kuro !
print(saluer("kuro", "Salut"))     // Salut, kuro !

ton.function paire(a, b = a + 1) {
    return str(a) + "-" + str(b)
}
print(paire(5))   // 5-6
```

**Paramètres rest** — préfixe le dernier paramètre par `...` pour collecter les arguments en trop dans un tableau :

```
ton.function somme(...nombres) {
    ton.int total = 0
    for ton.n in nombres { total += n }
    return total
}
print(somme(1, 2, 3, 4))   // 10
print(somme())              // 0
```

Une fonction peut mélanger paramètres fixes et rest (`function label(prefixe, ...items) { ... }`), mais `...` ne peut apparaître que sur le dernier.

Appeler une fonction avec trop peu d'arguments requis (ou trop, sans paramètre rest) est une erreur d'exécution — le message s'adapte selon que la fonction a des défauts/un rest (`'ton.f' expects 2 argument(s) but got 1.`, `'ton.f' expects between 1 and 2 argument(s) but got 0.`, `'ton.f' expects at least 1 argument(s) but got 0.`).

Les fermetures (closures) fonctionnent comme attendu : une fonction se souvient des variables du scope où elle a été déclarée, même après que ce scope ait par ailleurs terminé.

---

### Tableaux

```
ton.array nums = [1, 2, 3]
push(nums, 4)          // [1, 2, 3, 4]
print(nums[0])          // 1
nums[0] = 10             // [10, 2, 3, 4]
print(len(nums))          // 4
```

Voir [Fonctions natives](#fonctions-natives) pour l'ensemble des opérations sur tableaux (`push`/`pop`/`shift`/`unshift`/`slice`/`join`/`sort`/`reverse`/`contains`/`indexOf`/`map`/`filter`/`reduce`/`find`/`any`/`all`). Indexer hors limites est une erreur d'exécution, pas `nil`.

Les chaînes prennent aussi en charge l'indexation en lecture seule — `s[0]` est le premier caractère, sous forme de chaîne d'un caractère. Les chaînes sont immuables : `s[0] = "x"` est une erreur d'exécution ; construis une nouvelle chaîne avec `replace()`/`substring()`/le module `ton.strings` à la place.

---

### Dictionnaires

```
ton.dict utilisateur = {nom: "kuro", age: 22}
print(utilisateur["nom"])     // kuro
utilisateur["age"] = 23
utilisateur["ville"] = "paris"   // ajoute une nouvelle clé
print(keys(utilisateur))          // [nom, age, ville]
print(has(utilisateur, "age"))     // true
```

Les clés de dict sont toujours des chaînes (voir [Limitations connues](#limitations-connues)) : une clé identifiant nue (`nom: ...`) est traitée comme son propre nom, et une clé non-identifiant nécessite un littéral chaîne (`"prénom nom": ...`). Lire une clé absente renvoie `nil` plutôt que de lever une erreur ; écrire sur une nouvelle clé l'ajoute. `keys()`/`values()` itèrent dans l'ordre d'insertion.

---

### Gestion des erreurs : try / catch / throw / finally

```
try {
    throw "quelque chose s'est mal passé"
} catch (ton.err) {
    print("attrapé : " + err)
}
```

Toute erreur d'exécution — une erreur native (division par zéro, mauvais nombre d'arguments, index hors limites, ...) ou un `throw` explicite — peut être attrapée. La valeur attrapée est toujours le message de l'erreur sous forme de chaîne, lié au nom donné dans `catch (nom)`. `break`, `continue`, et `return` traversent un `try` englobant sans y être affectés (ce ne sont pas des erreurs).

`finally` ajoute un bloc qui s'exécute toujours après le `try` (et son `catch`, s'il y en a un) — que le bloc se soit terminé normalement, ait levé une erreur, ou ait rencontré un `return` :

```
ton.function lireSansRisque() {
    try {
        return risque()
    } finally {
        print("nettoyage effectué")
    }
}
```

`try` a besoin d'au moins un `catch`/`finally` : `catch` seul (comme ci-dessus), `finally` seul (`try { ... } finally { ... }`, utile pour du nettoyage quand on veut que l'erreur continue de se propager), ou les deux ensemble sont tous valides.

---

### Fonctions natives

Toutes fonctionnent avec ou sans le préfixe `ton.` (voir [Bases du langage](#bases-du-langage)).

**Cœur**
| Fonction | Description |
|---|---|
| `print(x)` | Affiche une valeur sur stdout |
| `type(x)` | Renvoie le nom du type d'une valeur sous forme de chaîne |
| `str(x)` | Convertit une valeur en chaîne |
| `num(x)` | Convertit une valeur en nombre (`0` si non convertible) |
| `format(modele, ...args)` | Remplace chaque `{}` dans `modele` par l'argument correspondant (voir [Formatage de chaînes](#formatage-de-chaînes)) |
| `json(x)` | Convertit une valeur (tableau, dict, ou scalaire) en chaîne JSON |
| `assert(cond, [msg])` | Lève une erreur (attrapable) si `cond` est faux |
| `input([prompt])` | Affiche un prompt optionnel, lit et renvoie une ligne depuis stdin |
| `readfile(chemin)` | Lit le contenu d'un fichier sous forme de `ton.html` (résolu aussi relativement au dossier du script) |
| `writefile(chemin, contenu)` | Écrit (en écrasant) un fichier avec `contenu` ; renvoie si ça a réussi |

**Maths**
| Fonction | Description |
|---|---|
| `sqrt(x)`, `pow(x, y)`, `abs(x)` | Racine carrée, puissance, valeur absolue |
| `floor(x)`, `ceil(x)`, `round(x)` | Arrondis |
| `min(...)`, `max(...)` | Le plus petit/grand de n'importe quel nombre d'arguments |
| `random()` | Un flottant dans `[0, 1)` |
| `random(max)` | Un entier dans `[0, max)` |
| `random(min, max)` | Un entier dans `[min, max)` |

**Chaînes**
| Fonction | Description |
|---|---|
| `len(s)` | Longueur |
| `upper(s)`, `lower(s)` | Conversion de casse |
| `trim(s)` | Supprime les espaces au début/à la fin |
| `split(s, sep)` | Découpe en un `ton.array` de chaînes |
| `join(arr, sep)` | Assemble un tableau en une chaîne (voir aussi sous Tableaux) |
| `replace(s, recherche, remp)` | Remplace chaque occurrence de `recherche` par `remp` |
| `substring(s, debut, [longueur])` | Extrait une sous-chaîne |
| `contains(s, sous)` | Si `sous` apparaît dans `s` |
| `indexOf(s, sous)` | Index de la première occurrence, ou `-1` |
| `s[i]` | Le caractère à l'index `i`, sous forme de chaîne d'un caractère (lecture seule — voir [Tableaux](#tableaux)) |

**Tableaux**
| Fonction | Description |
|---|---|
| `len(arr)` | Nombre d'éléments |
| `push(arr, v)` | Ajoute une valeur (modifie en place) |
| `pop(arr)` | Retire et renvoie le dernier élément |
| `shift(arr)` | Retire et renvoie le premier élément |
| `unshift(arr, v)` | Insère une valeur au début |
| `slice(arr, debut, [fin])` | Renvoie un nouveau sous-tableau |
| `join(arr, sep)` | Assemble les éléments en une chaîne |
| `sort(arr)` | Trie en place (numérique ou alphabétique) et le retourne |
| `reverse(arr)` | Inverse en place et le retourne |
| `contains(arr, item)` | Si `item` est un élément (égalité structurelle) |
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
| `has(d, cle)` | Si `cle` existe |
| `json(d)` | Encode le dict en JSON |

**Serveur HTTP**
| Fonction | Description |
|---|---|
| `get(chemin, fn)` | Enregistre une route GET (voir [Serveur local](#serveur-local--routes-api)) |
| `post(chemin, fn)` | Enregistre une route POST |
| `serve(port)` / `serve(port, contenu)` | Démarre le serveur HTTP local |

---

### Type HTML

`ton.html` est un type proche des chaînes, pensé pour le balisage : il s'affiche et se concatène comme une chaîne, mais concaténer avec `+` avec autre chose garde le résultat en `ton.html`, et un gestionnaire de route (voir [Serveur local](#serveur-local--routes-api)) renvoyant du `ton.html` est servi avec `Content-Type: text/html` plutôt que `text/plain`.

```
ton.html page = "<h1>Salut</h1>"
ton.string nom = "kuro"
ton.html salutation = page + "<p>Salut " + nom + "</p>"
```

Un littéral chaîne assigné directement à une variable `ton.html` est converti automatiquement — tu as rarement besoin de penser à la distinction avant de renvoyer quelque chose depuis un gestionnaire de route.

---

### Modules (IMPORT://)

```
IMPORT://mathutils      // charge mathutils.ton (un module utilisateur — voir le guide plus bas)
IMPORT://ton.mathutils  // charge le module *intégré* ton.mathutils — note le préfixe "ton."
```

Le `ton.` en tête est ce qui permet au parseur de distinguer un module **intégré** d'un module **utilisateur** — ce n'est jamais une recherche sur le système de fichiers. Voir [Modules système intégrés](#modules-système-intégrés) pour la liste complète des modules intégrés, et le guide plus bas pour écrire les tiens.

---

### Créer un module TON618 — guide complet

#### 1. Où vivent les modules

`IMPORT://nom` cherche `nom.ton` à deux endroits, dans l'ordre :
1. Le même dossier que le script qui l'importe.
2. Un sous-dossier `modules/` à côté de ce script.

Un projet peut donc garder des aides ponctuelles à côté du script, et du code partagé sous `modules/`.

#### 2. Ce qu'un module peut déclarer

Un fichier module est un fichier `.ton` normal : il peut déclarer des variables et des fonctions (`ton.function`), et tout ce qu'il déclare devient disponible dans le scope global du script qui l'importe, une fois importé. Un module n'est pas un espace de noms séparé — il n'y a pas de syntaxe `mathutils.add(...)` ; tu appelles `add(...)` directement (voir la convention de nommage ci-dessous).

#### 3. Un exemple concret

`modules/greetings.ton` :
```
ton.function greetings_hello(nom) {
    return "Bonjour, " + nom + " !"
}

ton.function greetings_bye(nom) {
    return "Au revoir, " + nom + "."
}
```

Utilisation :
```
IMPORT://greetings

print(greetings_hello("kuro"))
print(greetings_bye("kuro"))
```

#### 4. Bonnes pratiques pour écrire un module

- **Préfixe chaque fonction avec le nom du module lui-même** (`greetings_hello`, pas `hello`) — puisque l'importation aplatit tout dans un seul scope global, c'est la seule chose empêchant deux modules (ou un module et le script principal) de silencieusement écraser les fonctions l'un de l'autre.
- Garde un module centré sur une seule responsabilité ; compose plusieurs petits modules plutôt qu'un seul gros.
- Un module n'est exécuté que la première fois qu'il est importé — importer le même nom deux fois (depuis n'importe où) ne fait rien la seconde fois, donc il est sûr que deux modules différents importent tous les deux un troisième dont ils dépendent tous les deux.

---

### Modules système intégrés

TON618 est livré avec onze **modules système intégrés**. Ce ne sont pas des fichiers sur le disque — ils vivent dans l'interpréteur — mais tu les importes quand même avec `IMPORT://ton.<nom>` avant de les utiliser, exactement comme un module utilisateur :

```
IMPORT://ton.sys
IMPORT://ton.os
IMPORT://ton.requests
IMPORT://ton.random
IMPORT://ton.time
IMPORT://ton.json
IMPORT://ton.mathutils
IMPORT://ton.strings
IMPORT://ton.encoding
IMPORT://ton.regex
IMPORT://ton.path
```

Appeler une des fonctions ci-dessous sans d'abord importer son module échoue avec une erreur "undefined function" — c'est voulu : un script ne paie (et n'expose) que les fonctions intégrées qu'il a effectivement demandées. Chaque module suit la convention de nommage recommandée pour les modules utilisateur ci-dessus : chaque fonction est préfixée par le nom du module lui-même.

#### ton.sys — informations sur l'exécution & le processus

| Fonction | Description |
|---|---|
| `sys_args()` | Tableau des arguments de ligne de commande passés après le chemin du script (`ton618 script.ton foo bar` → `["foo", "bar"]`) |
| `sys_platform()` | `"windows"`, `"termux"`, `"macos"`, ou `"linux"` — détecté automatiquement (Termux est distingué d'un Linux classique via la variable d'environnement `$PREFIX`) |
| `sys_arch()` | `"x64"`, `"arm64"`, `"arm"`, `"x86"`, ou `"unknown"` |
| `sys_version()` | La version de l'interpréteur lui-même (ex. `"beta-1.0.1"`), identique à `ton618 --version` |
| `sys_exit(code)` | Arrête immédiatement tout le programme avec le code de sortie donné |
| `sys_sleep(ms)` | Met en pause l'exécution pendant le nombre de millisecondes donné |

#### ton.os — variables d'environnement & système de fichiers

| Fonction | Description |
|---|---|
| `os_name()` | `"nt"` sur Windows, `"posix"` ailleurs |
| `os_getenv(nom)` | La valeur d'une variable d'environnement, ou `nil` si absente |
| `os_setenv(nom, valeur)` | Définit une variable d'environnement pour ce processus ; renvoie si ça a réussi |
| `os_cwd()` | Le dossier de travail actuel |
| `os_exists(chemin)` | Si un fichier ou dossier existe |
| `os_isfile(chemin)` / `os_isdir(chemin)` | Si `chemin` existe et est ce genre d'entrée |
| `os_mkdir(chemin)` | Crée un dossier (et les dossiers parents manquants) |
| `os_remove(chemin)` | Supprime un fichier ou un dossier vide |
| `os_rename(src, dst)` | Renomme/déplace un fichier ou dossier |
| `os_copy(src, dst)` | Copie un fichier, en écrasant `dst` s'il existe déjà |
| `os_listdir(chemin)` | Tableau des noms d'entrées directement dans un dossier (non récursif) |
| `os_readlines(chemin)` | Tableau des lignes du fichier (sans retours à la ligne finaux) |
| `os_appendfile(chemin, contenu)` | Ajoute du texte à un fichier, le créant si besoin |
| `os_tempdir()` | Le dossier temporaire du système |

#### ton.requests — un client HTTP minimal

Un petit client HTTP sans dépendance (sockets bruts, même esprit que le serveur intégré). **Limitation : `http://` seulement, pas de TLS/`https://`** — il n'y a pas de bibliothèque SSL embarquée, donc il peut parler à des services locaux, d'autres serveurs TON618, ou n'importe quel point d'accès HTTP simple, mais pas à une API exclusivement HTTPS.

| Fonction | Description |
|---|---|
| `requests_get(url)` | Effectue une requête GET |
| `requests_post(url, [corps])` | Effectue une requête POST avec un corps chaîne optionnel |
| `requests_request(methode, url, [corps])` | Effectue une requête avec n'importe quelle méthode HTTP |

Les trois renvoient un `ton.dict` de même forme :
```
{
    ok: true,      // si une réponse a été reçue du tout
    status: 200,     // le code de statut HTTP (0 si la requête a totalement échoué)
    body: "...",       // le corps brut de la réponse
    error: ""            // une raison lisible quand ok est faux
}
```

```
IMPORT://ton.requests

ton.dict res = requests_get("http://localhost:8081/api/users")
if res["ok"] {
    print("Status: " + str(res["status"]))
    print(res["body"])
} else {
    print("Requête échouée : " + res["error"])
}
```

#### ton.random — outils de hasard supplémentaires

Complète le `random()` toujours disponible avec quelques motifs courants :

| Fonction | Description |
|---|---|
| `random_int(min, max)` | Un entier dans `[min, max]`, inclusif des deux côtés |
| `random_float()` | Un flottant dans `[0, 1)` |
| `random_choice(arr)` | Un élément uniformément aléatoire d'un tableau non vide |
| `random_shuffle(arr)` | Mélange un tableau en place (Fisher-Yates) et le retourne |
| `random_seed(n)` | Réamorce le générateur aléatoire de façon déterministe — utile pour des tests reproductibles |

#### ton.time — horloges & timestamps

| Fonction | Description |
|---|---|
| `time_now()` | Secondes depuis l'epoch Unix |
| `time_millis()` | Millisecondes depuis l'epoch Unix — pratique pour mesurer un temps écoulé |
| `time_string([timestamp])` | Une chaîne d'heure locale lisible ; par défaut maintenant |
| `time_sleep(ms)` | Met en pause l'exécution pendant le nombre de millisecondes donné (identique à `sys_sleep`) |

#### ton.json — parsing JSON & affichage indenté

Le `json(x)` toujours disponible transforme déjà une valeur *en* JSON compact. `IMPORT://ton.json` ajoute l'autre sens, plus un format multi-lignes :

| Fonction | Description |
|---|---|
| `json_parse(texte)` | Parse une chaîne JSON en `ton.dict`/`ton.array`/scalaire |
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

Complète les `sqrt`/`pow`/`abs`/`floor`/`ceil`/`round`/`min`/`max` toujours disponibles avec trigonométrie, logarithmes, et petits outils de statistiques/théorie des nombres :

| Fonction | Description |
|---|---|
| `mathutils_pi()` / `mathutils_e()` | Les constantes π et e |
| `mathutils_sin/cos/tan(x)`, `mathutils_asin/acos/atan(x)`, `mathutils_atan2(y, x)` | Trigonométrie, en radians |
| `mathutils_log(x)` / `mathutils_log2(x)` / `mathutils_log10(x)` / `mathutils_exp(x)` | Logarithmes naturel, base-2, base-10, et e^x |
| `mathutils_hypot(x, y)` | `sqrt(x*x + y*y)`, calculé sans dépassement |
| `mathutils_degrees(rad)` / `mathutils_radians(deg)` | Conversion d'unité d'angle |
| `mathutils_clamp(x, lo, hi)` | Restreint `x` à `[lo, hi]` |
| `mathutils_lerp(a, b, t)` | Interpolation linéaire entre `a` et `b` à `t` (`0..1`) |
| `mathutils_sign(x)` | `-1`, `0`, ou `1` |
| `mathutils_gcd(a, b)` / `mathutils_lcm(a, b)` | PGCD / PPCM de deux entiers |
| `mathutils_factorial(n)` | `n!` pour un entier non-négatif `n` |
| `mathutils_is_prime(n)` | `true` si `n` est premier |
| `mathutils_sum(arr)` / `mathutils_mean(arr)` / `mathutils_median(arr)` / `mathutils_stddev(arr)` | Somme, moyenne, médiane, et écart-type d'un tableau numérique |
| `mathutils_min_of(arr)` / `mathutils_max_of(arr)` | Plus petit/grand nombre d'un tableau (contrairement à `min()`/`max()`, qui prennent des arguments séparés) |

```
IMPORT://ton.mathutils

print(mathutils_gcd(48, 18))          // 6
print(mathutils_mean([1, 2, 3, 4]))   // 2.5
print(mathutils_clamp(15, 0, 10))     // 10
```

#### ton.strings — aides supplémentaires sur les chaînes

Complète les `upper`/`lower`/`trim`/`split`/`replace`/`contains`/`substring` toujours disponibles :

| Fonction | Description |
|---|---|
| `strings_starts_with(s, prefixe)` / `strings_ends_with(s, suffixe)` | Tests de préfixe/suffixe |
| `strings_repeat(s, n)` | Répète `s` `n` fois |
| `strings_reverse(s)` | Inverse une chaîne (le `reverse()` toujours disponible ne marche que sur les tableaux) |
| `strings_capitalize(s)` | Met en majuscule la première lettre, en minuscule le reste |
| `strings_pad_left(s, largeur, [car])` / `strings_pad_right(s, largeur, [car])` | Complète `s` jusqu'à `largeur` avec `car` (`" "` par défaut) |
| `strings_center(s, largeur, [car])` | Centre `s` dans `largeur`, en complétant des deux côtés avec `car` |
| `strings_count(s, sous)` | Nombre d'occurrences non chevauchantes de `sous` dans `s` |
| `strings_trim_start(s)` / `strings_trim_end(s)` | Supprime les espaces seulement au début/à la fin de `s` (`trim()` supprime les deux) |
| `strings_words(s)` | Tableau des mots séparés par des espaces (contrairement à `split()`, qui a besoin d'un séparateur exact) |
| `strings_replace_first(s, recherche, remp)` | Remplace seulement la première occurrence de `recherche` |
| `strings_snake_case(s)` | `"Hello World"` → `"hello_world"` |
| `strings_camel_case(s)` | `"hello_world"` → `"helloWorld"` |

```
IMPORT://ton.strings

print(strings_capitalize("hello world"))  // "Hello world"
print(strings_pad_left("7", 3, "0"))      // "007"
```

#### ton.encoding — base64, hex, et encodage URL

Encodeurs/décodeurs sans dépendance pour les formats dont les scripts ont le plus souvent besoin en parlant à des API web :

| Fonction | Description |
|---|---|
| `base64_encode(s)` / `base64_decode(s)` | Base64 standard |
| `hex_encode(s)` / `hex_decode(s)` | Hexadécimal en minuscules |
| `url_encode(s)` / `url_decode(s)` | Encodage pourcentage (aussi utilisé en interne pour parser la query string d'une requête du serveur local) |

```
IMPORT://ton.encoding

print(base64_encode("hello world"))  // aGVsbG8gd29ybGQ=
print(url_encode("a b/c"))            // a%20b%2Fc
```

#### ton.regex — recherche de motifs

Reposant sur la bibliothèque standard `<regex>` de C++ (syntaxe ECMAScript) — pas de dépendance externe, et la même variante de regex que JavaScript.

| Fonction | Description |
|---|---|
| `regex_test(s, motif)` | Si `motif` correspond quelque part dans `s` |
| `regex_match(s, motif)` | `[correspondanceComplète, groupe1, groupe2, ...]` pour la première correspondance, ou `nil` |
| `regex_find_all(s, motif)` | Tableau de toutes les correspondances complètes trouvées dans `s` |
| `regex_replace(s, motif, remp)` | `s` avec chaque correspondance remplacée ; `remp` peut utiliser `$1`, `$2`... pour les groupes capturés |
| `regex_split(s, motif)` | Tableau de sous-chaînes, en découpant `s` à chaque correspondance |

```
IMPORT://ton.regex

print(regex_test("hello123", "[0-9]+"))                          // true
print(regex_replace("2024-01-15", "(\\d+)-(\\d+)-(\\d+)", "$3/$2/$1"))  // 15/01/2024
```

#### ton.path — manipulation de chemins de fichiers

Manipulation de chaînes/chemins pure — aucun accès disque (voir `ton.os` pour ça).

| Fonction | Description |
|---|---|
| `path_join(a, b, ...)` | Assemble n'importe quel nombre de segments de chemin avec le séparateur de la plateforme |
| `path_basename(p)` | Le nom de fichier avec son extension |
| `path_dirname(p)` | Le dossier parent |
| `path_extension(p)` | L'extension avec le point, ou `""` si aucune |
| `path_stem(p)` | Le nom de fichier sans son extension |
| `path_absolute(p)` | Un chemin absolu (résolu par rapport au dossier de travail actuel) |

```
IMPORT://ton.path

print(path_join("a", "b", "c.txt"))     // a/b/c.txt
print(path_extension("/foo/bar.txt"))    // .txt
```

---

### Serveur local & routes (API)

TON618 a un serveur HTTP intégré, sans dépendance externe (sockets bruts).

**Mode simple** — sert le même contenu à chaque requête :
```
ton.serve(8080, "<h1>Salut depuis TON618 !</h1>")
```

**Mode routage** — enregistre des gestionnaires par chemin et méthode, puis appelle `serve(port)` sans second argument. Chaque gestionnaire prend un seul argument, un `ton.dict` décrivant la requête :

```
{
    method: "GET",           // la méthode HTTP
    path: "/user/42",          // le chemin de la requête, sans la query string
    query: {a: "1"},             // les paramètres de query string parsés, en dict
    params: {id: "42"},            // les segments ":nom" capturés depuis le motif de route
    body: ""                         // le corps brut de la requête (payload POST/PUT/...)
}
```

Le chemin d'une route peut contenir des segments `:nom`, capturés dans `req["params"]` :

```
ton.function utilisateurParId(req) {
    ton.string id = req["params"]["id"]
    return json({id: id})
}

ton.function recherche(req) {
    ton.dict query = req["query"]
    ton.string q = has(query, "q") ? query["q"] : ""
    return "Tu as cherché : " + q
}

ton.function creerUtilisateur(req) {
    print("Corps reçu : " + req["body"])
    return json({ok: true})
}

ton.get("/user/:id", ton.utilisateurParId)
ton.get("/search", ton.recherche)          // GET /search?q=kuro -> req["query"]["q"]
ton.post("/users", ton.creerUtilisateur)

ton.serve(8081)
```

Un gestionnaire renvoyant du `ton.html` est servi avec `Content-Type: text/html` ; tout le reste est servi en `text/plain` (utilise `json(...)` pour construire un corps de réponse JSON, qui reste du texte brut sur le fil — configure ton client pour attendre `application/json` si tu as besoin de cet en-tête précis). Un chemin sans route ou méthode correspondante renvoie `404`. Le serveur est mono-thread et bloquant — il traite une requête à la fois (voir [Limitations connues](#limitations-connues)).

---

### Le débogueur

Lance un script avec `--debug` (ou `--break=<ligne>`) pour entrer dans un débogueur interactif pas-à-pas dès que l'exécution atteint un point d'arrêt (ou dès la première instruction, avec `--debug` seul) :

```bash
ton618 script.ton --debug
ton618 script.ton --break=12 --break=20
```

Commandes au prompt `(ton618-dbg)` :

| Commande | Effet |
|---|---|
| `s`, `step` | Exécute la ligne suivante (entre dans les appels de fonction) |
| `c`, `continue` | Continue jusqu'au prochain point d'arrêt |
| `b <ligne>` | Ajoute un point d'arrêt |
| `rb <ligne>` | Retire un point d'arrêt |
| `p <var>` | Affiche la valeur et le type d'une variable |
| `vars`, `locals` | Affiche toutes les variables locales du scope actuel |
| `bt`, `backtrace`, `stack` | Affiche la pile d'appels |
| `l`, `list` | Montre plus de contexte source autour de la ligne actuelle |
| `h`, `help`, `?` | Affiche cette liste |
| `q`, `quit`, `exit` | Arrête le programme |

---

### Architecture — comment fonctionne l'interpréteur

TON618 est un **interpréteur à parcours d'arbre** classique, composé de trois étapes formant un pipeline direct :

```
texte source (fichier .ton)
        │
        ▼
┌───────────────┐   transforme les caractères en une liste plate de Tokens
│     Lexer     │   (nombres, chaînes, identifiants, mots-clés, opérateurs)
└───────┬───────┘
        ▼  tokens
┌───────────────┐   parsing par descente récursive : groupe les Tokens en un
│     Parser    │   arbre syntaxique abstrait (un arbre de nœuds Stmt/Expr)
└───────┬───────┘   selon la grammaire du langage
        ▼  AST
┌───────────────┐   parcourt l'AST directement et l'exécute — pas de bytecode,
│  Interpreteur │   pas d'étape de compilation séparée ; chaque nœud est évalué
└───────────────┘   au moment où il est atteint
```

```
ton618/
├── Makefile
├── include/                 # en-têtes (.hpp) — déclarations et notes de conception
│   ├── Token.hpp               — le vocabulaire du langage (TokenType, Token)
│   ├── Lexer.hpp                 — texte -> tokens
│   ├── Ast.hpp                     — la forme des nœuds AST (Expr, Stmt)
│   ├── Parser.hpp                    — tokens -> AST
│   ├── Value.hpp                       — représentation des valeurs à l'exécution
│   ├── Environment.hpp                   — scopes de variables (nom -> Value, chaînés)
│   ├── Interpreter.hpp                     — parcourt l'AST, exécute le programme
│   ├── Debugger.hpp                          — points d'arrêt / mode pas-à-pas
│   ├── LocalServer.hpp                         — serveur HTTP en sockets bruts (ton.serve/get/post)
│   ├── HttpClient.hpp                            — client HTTP en sockets bruts (module ton.requests)
│   ├── JsonParser.hpp                              — texte JSON -> Value (json_parse de ton.json)
│   ├── Platform.hpp                                  — détection auto OS/Termux/architecture
│   └── Version.hpp                                     — la version de l'interpréteur lui-même
├── src/                     # implémentations (.cpp), une par en-tête ci-dessus,
│                              plus main.cpp (point d'entrée CLI : lit un fichier, lance
│                              le pipeline Lexer -> Parser -> Interpreter, et câble
│                              --debug / --break / --version / --update / --uninstall)
├── exemples/                # scripts .ton d'exemple, dont un qui exerce la majorité
│                              du langage (test_new_features.ton), plus documentation.html,
│                              playground.html, et ton618-lite.js (une réimplémentation JS
│                              utilisée seulement par le playground)
├── scripts/                 # install.sh/.ps1/.bat, uninstall.sh/.ps1/.bat, rebuild.sh,
│                              zig-strip.sh, et doc_templates/ (voir `make docs` plus haut)
└── modules/                 # modules .ton partagés (voir le guide de module plus haut)
```

Chaque en-tête commence par un court bloc de commentaire expliquant son rôle dans le pipeline et, si pertinent, ce qu'il faut toucher en premier pour étendre cette partie du langage — lis-les avant de plonger dans les fichiers `.cpp`.

Deux choix de conception qui valent la peine d'être mentionnés car ils façonnent presque tous les autres fichiers :

- **Les nœuds AST/Value sont des "fat structs", pas une hiérarchie de classes.** `Expr` et `Stmt` (Ast.hpp) et `Value` (Value.hpp) contiennent chacun tous les champs dont n'importe laquelle de leurs variantes pourrait avoir besoin, avec une étiquette `type` disant lesquels s'appliquent. Ça sacrifie un peu de mémoire pour un code où `evaluate()`/`execute()` sont de simples `switch` plutôt qu'un visiteur dispersé — bien plus facile à lire de bout en bout pour un langage de cette taille. Certains types de nœuds *réutilisent* les champs d'un autre type (documenté à côté de l'`enum`) plutôt que d'en ajouter de nouveaux, pour empêcher les structs de grossir sans limite — `SWITCH`, par exemple, réutilise les champs `condition`/`elseBranch` de `IF` pour son expression sujet et son bloc par défaut.
- **Les structures de contrôle utilisent les exceptions C++.** `break`, `continue`, et `return` sont implémentés comme de petits types d'exception (`BreakException`, `ContinueException`, `ReturnException`, voir Interpreter.hpp) levés depuis `execute()` et attrapés par la boucle, le `switch`, ou l'appel de fonction englobant le plus proche. Ça reproduit, presque un-à-un, la façon dont ces structures remontent à travers les blocs imbriqués de l'AST — aucun drapeau manuel "a-t-on break ?" n'a besoin d'être propagé à travers chaque appel récursif.

Une troisième chose à savoir si tu touches à la bibliothèque standard : `valuesEqual()` (une fonction libre près du haut de `src/Interpreter.cpp`) est la seule source de vérité pour ce que signifie "égal" — les opérateurs `==`/`!=`, la correspondance `switch`/`case`, et les natives `contains()`/`indexOf()` l'appellent tous, donc un correctif ou un nouveau type de valeur n'a besoin d'être géré qu'à un seul endroit.

---

### Étendre l'interpréteur lui-même

**Ajouter une nouvelle fonction native** (le changement le plus courant) : choisis le bon `registerBuiltin*()` dans `src/Interpreter.cpp` (ou `defineNatives()` pour quelque chose qui doit toujours être disponible, sans `IMPORT://`), et ajoute un appel `def("nom", [...](std::vector<Value>& args) -> Value { ... })`. Regarde une fonction voisine pour la convention de vérification d'arguments (lève `std::runtime_error` avec un message clair sur le mauvais type/nombre). Documente-la ensuite dans ce fichier, et — si c'est du calcul pur sans accès système de fichiers/réseau/processus — envisage de la porter aussi dans `exemples/ton618-lite.js`, pour que le playground du navigateur reste synchronisé (voir le commentaire d'en-tête de ce fichier pour ce qu'il peut et ne peut pas prendre en charge).

**Ajouter un nouvel opérateur ou mot-clé** : ajoute un `TokenType` (Token.hpp), apprends au Lexer à le produire (`src/Lexer.cpp` — la map `keywords` pour un mot, `scanToken()` pour de la ponctuation), puis apprends au Parser à le consommer au bon niveau de précédence (voir le commentaire de Parser.hpp sur la chaîne `expression -> assignment -> ternary -> ...`) et à l'Interpreter à l'évaluer.

**Ajouter un nouveau type d'instruction** (comme `switch` l'a été) : ajoute un `StmtType` dans Ast.hpp (en réutilisant les champs existants là où ça convient, plutôt que de faire grossir la struct), une méthode de parsing dans Parser.cpp appelée depuis `statement()`, et un `case` dans `Interpreter::execute()`.

**Ajouter un nouveau type de valeur** (par ex. un `ton.set`) :
1. Ajoute un `ValueType` et un champ de stockage dans `include/Value.hpp` ; étends `typeName()`, `toString()`, `toJson()`.
2. Ajoute un `VarKind` dans `include/Ast.hpp` et un `TokenType`/mot-clé (`Token.hpp`, `Lexer.cpp`) s'il a besoin de sa propre syntaxe de déclaration `ton.<type>`.
3. Étends `Interpreter::checkType` (`src/Interpreter.cpp`) pour que `ton.<type> x = ...` l'impose.
4. Ajoute la syntaxe littérale dans `Parser::primary()` si le type en a besoin (regarde comment les littéraux `ARRAY`/`DICT` y sont parsés), et le support d'évaluation dans `Interpreter::evaluate`.

Après tout changement, lance `make` et vérifie avec les scripts de `exemples/` (en particulier `exemples/test_new_features.ton`) avant de te fier au changement.

---

### Publier la doc sur GitHub Pages

`exemples/` est entièrement statique et autonome — `documentation.html` embarque ce fichier même (voir `make docs` plus haut), et `playground.html` tourne entièrement côté client via `ton618-lite.js`. `.github/workflows/pages.yml` publie tout ce dossier tel quel sur GitHub Pages (pas le reste du dépôt) : il copie `exemples/` verbatim, plus `documentation.html` en tant qu'`index.html` à la racine du site pour que la doc se charge à l'URL Pages toute seule, avec `playground.html` et `ton618-lite.js` accessibles juste à côté, exactement comme `documentation.html` les référence déjà.

Configuration à faire une seule fois :
1. **GitHub → ce dépôt → Settings → Pages.**
2. Sous **Build and deployment**, mets **Source** sur **GitHub Actions** (pas "Deploy from a branch").

C'est tout — `.github/workflows/pages.yml` se lance à chaque push sur `main` qui touche `exemples/` (ou manuellement depuis l'onglet **Actions**), et publie à :
```
https://<utilisateur>.github.io/ton618/                       — documentation.html (comme index.html)
https://<utilisateur>.github.io/ton618/playground.html         — le playground
https://<utilisateur>.github.io/ton618/ton618-lite.js          — le moteur du playground
```
(adapte l'org/l'utilisateur de ces URLs à celui qui possède ce dépôt.)

Chaque fois que `DOCUMENTATION.md` change, lance `make docs`, commite `exemples/documentation.html` régénéré, et push — le workflow republie automatiquement.

**Utiliser un domaine personnalisé** (ex. un sous-domaine [DuckDNS](https://www.duckdns.org) gratuit plutôt que `github.io`) : ça ne nécessite aucun serveur à toi — GitHub héberge le site, pas toi.

1. Sur DuckDNS, pointe l'IP de ton sous-domaine vers une des adresses fixes de GitHub Pages plutôt que vers un serveur que tu gères : `185.199.108.153`, `185.199.109.153`, `185.199.110.153`, ou `185.199.111.153` (DuckDNS n'accepte qu'une seule IP par sous-domaine, donc choisis-en une seule). Si ce sous-domaine est aussi maintenu à jour par un script de dynamic-DNS ailleurs (cron sur un VPS, un routeur, etc.), ce script écrasera ce réglage à la prochaine exécution — utilise un sous-domaine que rien d'autre ne met à jour automatiquement.
2. `exemples/CNAME` dans ce dépôt contient déjà le domaine auquel la doc est épinglée ; `.github/workflows/pages.yml` le publie tel quel, ce qui indique à GitHub Pages de servir ce domaine.
3. **GitHub → ce dépôt → Settings → Pages** devrait alors afficher le domaine personnalisé sous **Custom domain** (la propagation DNS peut prendre quelques minutes) — une fois vérifié, coche **Enforce HTTPS**.

---

### Limitations connues

- `serve()` est mono-thread et bloquant — une requête à la fois, pas de concurrence
- `ton.int` et `ton.float` sont actuellement le même type de nombre interne (pas d'imposition entier strict)
- Les clés de dict sont toujours des chaînes (pas de clés numériques ou structurelles imbriquées)
- `IMPORT://` ne résout qu'un nom de module plat, pas de sous-dossiers/espaces de noms
- Pas de types/classes définis par l'utilisateur — seulement les types scalaire/tableau/dict/html/fonction intégrés
- `ton.requests` ne parle que du `http://` simple — pas de TLS/`https://`, pas de redirections, pas de transfert chunké
- Le playground du navigateur (`ton618-lite.js`) n'implémente pas encore `ton.encoding`/`ton.regex`/`ton.path`, et peut afficher des flottants avec plus de décimales que l'interpréteur réel (une différence de formatage de nombre JavaScript-contre-C++, pas une différence de valeur)

---

### Changelog

**beta-1.0.3** — nouveaux opérateurs, paramètres de fonction flexibles, et `finally` :

- **Paramètres par défaut** : `function saluer(nom, salutation = "Bonjour") { ... }` — le défaut est évalué au moment de l'appel, dans l'ordre, donc un défaut plus tardif peut référencer un paramètre précédent.
- **Paramètres rest** : `function somme(...nombres) { ... }` collecte les arguments en trop dans un tableau ; peut être mélangé avec des paramètres fixes tant que `...` est en dernier. Les messages d'erreur d'arité s'adaptent maintenant (`expects between 1 and 2 argument(s)`, `expects at least 1 argument(s)`).
- **`??` (opérateur de coalescence nulle)** : `valeur ?? defaut` retourne `valeur` sauf si elle vaut `nil`, auquel cas il retourne `defaut` — le défaut n'est jamais évalué autrement (court-circuit). Voir [Opérateurs](#opérateurs).
- **`in` (opérateur d'appartenance)** : `valeur in tableau`, `valeur in dict` (teste les clés), `valeur in chaine` (test de sous-chaîne). Voir [Opérateurs](#opérateurs).
- **`finally`** : `try`/`catch` gagne un bloc `finally` qui s'exécute toujours — en cas de succès, d'erreur attrapée, ou à travers un `return` — et `catch` est maintenant optionnel tant que `finally` est présent (`try { ... } finally { ... }`). Voir [Gestion des erreurs](#gestion-des-erreurs--try--catch--throw--finally).

**beta-1.0.2** — formatage de chaînes simplifié, et un skill IA :

- **L'interpolation de chaînes (`"Bonjour ${nom}!"`) a été retirée** au profit du bien plus simple **`format(modele, ...args)`** : `format("Bonjour {}!", nom)`. L'interpolation demandait au lexer de relancer récursivement le lexing d'expressions arbitraires embarquées dans les littéraux chaîne (en suivant les guillemets et accolades imbriqués) — beaucoup de machinerie pour ce qu'une simple fonction de traitement de chaînes fait tout aussi bien, avec bien moins de choses à garder en tête. Voir [Formatage de chaînes](#formatage-de-chaînes).
- **Un skill IA pour TON618**, à la fois en `SKILL.md` façon Claude Code et en `AGENTS.md` générique, pour qu'un agent IA écrive du `.ton` correct sans avoir à redériver le langage depuis ce document — voir le lien **"Skills for AI"** à côté du playground, ou `exemples/skills/`.

**beta-1.0.1** — corrections de bugs, nouvelle syntaxe, et une bibliothèque standard bien plus grande :

*Corrections de justesse :*
- **`<`, `<=`, `>`, `>=` vérifient maintenant le type et prennent en charge les chaînes** : auparavant ils comparaient silencieusement le champ nombre interne quel que soit le type, donc par ex. `"apple" < "banana"` s'évaluait comme `0 < 0` (`false`) au lieu d'une comparaison lexicographique. Ils comparent maintenant les nombres numériquement, les chaînes/html lexicographiquement, et lèvent une erreur pour toute autre combinaison.
- **`==`/`!=` font maintenant une égalité structurelle pour les tableaux et dicts** (récursivement, élément par élément) au lieu d'être toujours "différents" l'un de l'autre quel que soit le contenu ; une chaîne et une valeur html avec le même texte sont maintenant égales aussi.
- **`%` lève maintenant une erreur pour une division par zéro** au lieu de produire silencieusement `NaN`, comme `/`.

*Nouvelle syntaxe :*
- ~~Interpolation de chaînes~~ — ajoutée ici, remplacée par `format()` en beta-1.0.2 (voir plus haut)
- **`switch` / `case` / `default`**, sans fallthrough et avec prise en charge de plusieurs valeurs par case
- **Indexation de chaînes** : `s[0]` lit un caractère (les chaînes restent immuables — assigner à un index est une erreur)

*Nouvelle bibliothèque standard :*
- Trois nouveaux modules intégrés : **`ton.encoding`** (base64/hex/URL), **`ton.regex`** (basé sur `<regex>`), **`ton.path`** (manipulation de chemins)
- Nouvelles natives toujours disponibles : `find`, `any`, `all`, `writefile`
- Nouveau dans `ton.os` : `os_appendfile`, `os_readlines`, `os_isfile`, `os_isdir`, `os_rename`, `os_tempdir`, `os_copy`
- Nouveau dans `ton.strings` : `strings_trim_start`, `strings_trim_end`, `strings_words`, `strings_center`, `strings_replace_first`, `strings_snake_case`, `strings_camel_case`
- Nouveau dans `ton.mathutils` : `mathutils_min_of`, `mathutils_max_of`
- Nouveau dans `ton.json` : `json_pretty`
- Nouveau dans `ton.sys` : `sys_arch`, `sys_version` ; `sys_platform()` reconnaît maintenant aussi `"termux"`

*Serveur local :*
- **Les gestionnaires de route reçoivent maintenant la requête sous forme de `ton.dict`** (`{method, path, query, params, body}`) au lieu d'être appelés sans arguments — un changement cassant depuis beta-1.0.0, qui résout l'ancienne limitation "impossible de lire le corps POST ou les paramètres d'URL". Les routes peuvent utiliser des segments de chemin `:nom` (`/user/:id`).

*Distribution & outillage :*
- **Les binaires précompilés sont maintenant construits et publiés par GitHub Actions** (`.github/workflows/release.yml`), compilant en croisé Linux x64, Windows x64, et Termux/Android arm64 à chaque release taguée
- **`ton618 --update [version]`** : retélécharge le binaire correspondant à la plateforme actuelle depuis les GitHub Releases et remplace celui en cours d'exécution
- **`ton618 --version` / `-v`** : affiche la version, la plateforme détectée, et l'architecture
- **`.github/workflows/pages.yml`** publie `exemples/` (doc + playground) sur GitHub Pages, avec prise en charge de domaine personnalisé (`exemples/CNAME`)
- Le playground du navigateur (`ton618-lite.js`) implémente maintenant aussi `ton.mathutils`, `ton.strings`, `find`/`any`/`all`, `json_pretty`, `switch`/`case`, et l'indexation de chaînes — comblant plusieurs écarts où il se comportait silencieusement différemment de l'interpréteur réel

**Ajouts antérieurs au langage** (avant beta-1.0.1) :

- **Assignation composée** : `+=`, `-=`, `*=`, `/=`, `%=`
- **Incrémentation/décrémentation** : `++`, `--` (postfixe, sur variables et cibles indexées)
- **Opérateur ternaire** : `cond ? a : b`
- **Boucle `for ... in`** sur tableaux et dictionnaires, aux côtés d'un `for` classique style C
- **Gestion d'erreurs `try` / `catch` / `throw`**
- **Type `ton.dict`**, avec littéraux `{cle: valeur, ...}`, indexation `[]`, et `keys()`/`values()`/`has()`
- **Expressions de fonction** : `ton.function(params) { ... }` comme valeur inline, pour les callbacks
- Une grande bibliothèque standard : `type`, `assert`, `input`, aides maths/chaînes/tableaux/dicts, `map`/`filter`/`reduce`
- Un guide documenté pour écrire des modules, et huit (maintenant onze) modules système intégrés
- Un playground dans le navigateur (`exemples/playground.html`, propulsé par `exemples/ton618-lite.js`)
- `documentation.html` affiche `DOCUMENTATION.md` directement (embarqué au moment de l'édition via `make docs`)
