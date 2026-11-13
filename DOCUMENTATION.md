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
13. [Classes & objects](#classes--objects)
14. [Method syntax & the dot operator](#method-syntax--the-dot-operator)
15. [Error handling: try / catch / throw / finally](#error-handling-try--catch--throw--finally)
16. [Built-in native functions](#built-in-native-functions)
17. [HTML type](#html-type)
18. [Modules (IMPORT://)](#modules-import)
19. [Creating a TON618 module — full guide](#creating-a-ton618-module--full-guide)
20. [Installing modules: `ton618 install`](#installing-modules-ton618-install)
21. [Built-in system modules](#built-in-system-modules)
22. [Local server & routes (API)](#local-server--routes-api)
23. [The debugger](#the-debugger)
24. [The IDE & editor extensions](#the-ide--editor-extensions)
25. [Architecture — how the interpreter works](#architecture--how-the-interpreter-works)
26. [Extending the interpreter itself](#extending-the-interpreter-itself)
27. [Publishing the docs to GitHub Pages](#publishing-the-docs-to-github-pages)
28. [Known limitations](#known-limitations)
29. [Changelog](#changelog)

---

### What is TON618

TON618 is a small interpreted language, similar in spirit to how Python or HolyC work: you write a `.ton` file, run it directly with the `ton618` interpreter, and it executes immediately — no separate compilation step for your scripts.

The interpreter itself (`ton618`) is written in C++17 and ships as a single, dependency-free binary. It's cross-compiled for Linux, Termux (Android), and Windows (see [Installing & building](#installing--building-the-interpreter)), and auto-detects which of those it's running on at startup.

The language's defining feature is the **`ton.` prefix**: declaring a variable or a function always uses it (`ton.int x = 5`, `ton.function f() { ... }`). Once something is declared, though, referring to it again — reading it, reassigning it, or naming a function parameter — can use either `ton.name` or the bare `name`; both resolve to the same thing. This documentation uses `ton.` everywhere for clarity, except for short-lived names like loop counters and function parameters, matching the style already used throughout `exemples/`. See [Language basics](#language-basics) for exactly where the prefix is required versus optional.

Since **beta-2.0.0** it is also an object-oriented, expression-friendly language: **classes** with inheritance, **method syntax on every value** (`"hi".upper()`, `nums.filter(...).map(...)`), **arrow functions**, **f-strings**, **ranges** and **slices**, **destructuring**, the **pipe operator** `|>`, an **interactive REPL**, helpful error messages with tracebacks — and an interpreter about 6x faster than 1.x. See [Classes & objects](#classes--objects) and the [Changelog](#changelog).

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

Install a specific release instead of the latest one by setting `TON618_VERSION` (`install.sh`) or `$env:TON618_VERSION` (`install.ps1`) to a release tag (e.g. `beta-1.0.1`) before running the command above. Uninstall any time with `ton618 --uninstall-ton`, or the matching `uninstall.sh`/`uninstall.ps1` script.

No compiler needed for any of this — a precompiled binary is all you ever run. If you're contributing to the interpreter itself and need to build it from source, see [Architecture](#architecture--how-the-interpreter-works) and [Extending the interpreter itself](#extending-the-interpreter-itself) below (the project just needs a C++17 compiler and `make` — `make` builds `./ton618`, `make docs` regenerates `exemples/documentation.html` from this file after an edit).

---

### Running a script

```bash
ton618 path/to/script.ton
```

Options:

| Flag | Effect |
|---|---|
| `--check` | `ton618 --check file.ton`: check the syntax without running (used by the editors) |
| `--debug` | Start in step-by-step debug mode |
| `--break=<line>` | Add a breakpoint at a given line (repeatable) |
| `-h`, `--help` | Show usage |
| `-v`, `--version` | Print the version, auto-detected platform (`linux`/`termux`/`windows`/`macos`), and architecture |
| `--update [version]` | Re-download the binary matching this platform from [GitHub Releases](https://github.com/kurodaki-dev/ton618/releases) and replace the running one — the latest release if `version` (a release tag) is omitted |
| `--uninstall-ton` | Remove this ton618 install: the binary, its PATH entry, and the current directory's `./modules/` folder (every module previously fetched with `install` there) |

Two more commands, used as `ton618 <command> <module>` rather than as a flag on a script:

| Command | Effect |
|---|---|
| `install <module>` | Fetch `<module>` from the [TON618 module registry](#installing-modules-ton618-install) and download its code into `./modules/` |
| `uninstall <module>` | Remove `./modules/<module>.ton` (undoes `install`) |

Anything after the script path that isn't one of these flags is passed through to the script itself, readable via `sys_args()` (see [ton.sys](#built-in-system-modules)).

Example:
```bash
ton618 exemples/test.ton --break=10 --break=20
ton618 --update             # update to the latest release
ton618 --update beta-1.0.1  # pin to a specific release tag
ton618 install stringutils   # fetch a module from the registry
ton618 uninstall stringutils # remove it again
```

**The interactive REPL** — run `ton618` with no arguments (or `ton618 repl`) to get a prompt where every line runs immediately and the value of an expression is printed back. Unclosed `(`, `[` or `{` continue on the next line, so you can type a whole function. `.vars` lists what you defined, `.exit` (or Ctrl-D) quits.

```
$ ton618
TON618 beta-2.0.0 (linux/x86_64)
ton> ton.let nums = [3, 1, 2]
ton> nums.sorted().map(n => n * 10)
[10, 20, 30]
ton> f"{len(nums)} items"
"3 items"
```

**One-liners** — `ton618 -e 'print(2 ** 10)'` runs a snippet without creating a file.

**Error reports** — an uncaught error prints the message, the offending source line, a "did you mean" suggestion for a misspelled name, and — when it happened inside function calls — a short traceback:

```
Error at line 2: Division by zero.
  --> calc.ton:2
   2 | return a / b
Traceback (most recent call last):
  line 9: called compute()
  line 5: called divide()
  line 2: error raised here
```

---

### The browser playground

Don't want to install anything just to try a snippet? Open `exemples/playground.html` in any browser (no server needed) for a live code editor with a "Run" button and example snippets to load.

The playground runs **`ton618-lite.js`**, a JavaScript re-implementation of the language, entirely client-side — nothing is sent anywhere, and no installation is required. It mirrors the real interpreter closely: the full grammar (including `switch`/`case`), and the `ton.random`, `ton.time`, `ton.json`, `ton.mathutils`, and `ton.strings` built-in modules, since none of those need anything a browser can't already do.

Since beta-2.0.0 it supports the whole 2.0 language (classes, method syntax, arrow functions, f-strings, ranges, slices, destructuring, `|>`, `?.`...) and the new standard functions — CI runs the `tests/` suite through both the real interpreter and `ton618-lite.js` and checks they print exactly the same thing. On the documentation website, the **Run** button on a code example opens it in the playground.

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

**`ton.let`, `ton.var` and `ton.const`** — declare a variable without a fixed type with `ton.let` (`ton.var` is a synonym). `ton.const` does the same but forbids reassigning it later: `PI = 3` after `ton.const PI = 3.14` is a runtime error. Typed declarations (`ton.int`, `ton.string`, ...) still check the type of the initial value.

```
ton.let anything = 42
anything = "now a string"     // fine: ton.let has no fixed type
ton.const MAX_USERS = 100
MAX_USERS = 5                 // Error: Cannot reassign 'ton.MAX_USERS': it was declared with ton.const.
```

**Number literals** — besides `42` and `3.14`: `1_000_000` (underscores are ignored), `1e9` / `2.5e-3` (exponent), `0xFF` (hexadecimal), `0b1010` (binary), `0o755` (octal).

**Semicolons** are optional; use them to put several statements on one line: `self.x = x; self.y = y`.

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

Since beta-2.0.0:

| Declaration | Meaning | Example |
|---|---|---|
| `ton.let` / `ton.var` | Any type, can change later | `ton.let x = 5` |
| `ton.const` | Any type, can't be reassigned | `ton.const PI = 3.14` |
| `ton.<ClassName>` | An instance of that class (or of a subclass), or `nil` | `ton.Point p = Point(1, 2)` |

`type(obj)` returns the class name for an object (`"Point"`), and `"class"` for a class itself. Numbers print in their shortest exact form: `0.1 + 0.2` prints `0.30000000000000004`, `3.14` prints `3.14` (versions before 2.0 printed `3.140000`). Use `fixed(x, 2)` for a fixed number of decimals.

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

**f-strings** (since beta-2.0.0) — prefix a string with `f` to embed any expression between braces. It's the shortest way to build text out of values; `format()` keeps working.

```
ton.string name = "kuro"
ton.int age = 21
print(f"Hello {name}, next year you'll be {age + 1}!")
print(f"{len(name)} letters, upper: {name.upper()}")
print(f"Braces: {{like this}}")     // {{ and }} are literal braces
```

**Raw strings** — backticks delimit a string with no escape sequences, which can span several lines. Handy for HTML, regular expressions and Windows paths:

```
ton.html page = `<div class="card">
  <p>C:\temp\new is not a newline here</p>
</div>`
```

**Escapes** in double-quoted strings: `\n` `\t` `\r` `\"` `\\` `\0`, and `\u{1F680}` / `\u00e9` for any Unicode character (written as UTF-8).

**Formatting numbers**: `fixed(3.14159, 2)` gives `"3.14"`, `"7".padStart(3, "0")` gives `"007"`.

---

### Operators

| Category | Operators |
|---|---|
| Arithmetic | `+` `-` `*` `/` `%` |
| Comparison | `==` `!=` `<` `<=` `>` `>=` |
| Logical | `&&`/`and`, `\|\|`/`or`, `!` |
| Nil-coalescing | `??` |
| Membership | `in` |
| Bitwise | `&` `\|` `^` `~` `<<` `>>` |
| Assignment | `=` `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` |
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
- `&`, `|`, `^`, `~`, `<<`, `>>` treat both sides as integers (truncating any fractional part), operate on a 64-bit two's-complement representation, then convert the result back to TON618's usual number type — there's no separate integer type, so `6 & 3` is just a number like any other (`2`). Both operands must be numbers; anything else is a runtime error. `&`/`|`/`^` all share one precedence level (looser than `==`, tighter than `&&`) — use parentheses when mixing them for clarity.

Added in beta-2.0.0:

| Operator | Meaning | Example |
|---|---|---|
| `**` / `**=` | Power (right-associative) | `2 ** 10` is `1024` |
| `..` / `..=` | Range, end excluded / included | `0..5`, `1..=10` |
| `\|>` | Pipe: the left value becomes the first argument | `x \|> f(y)` is `f(x, y)` |
| `?.` | Optional chaining: `nil` instead of an error | `user?.address?.city` |
| `??=` | Assign only if currently `nil` | `config.port ??= 8080` |
| `.` | Property / method access | `point.x`, `"hi".upper()` |
| `=>` | Arrow function | `n => n * 2` |

- `-2 ** 2` is `-4`: `**` binds tighter than the unary minus, as in maths and Python.
- `+` also concatenates two arrays (`[1, 2] + [3]` is `[1, 2, 3]`, a new array); `*` repeats a string (`"ab" * 3` is `"ababab"`).
- `a..b` evaluates to an array of the integers from `a` up to `b - 1` (`a..=b` includes `b`); in a `for` loop it counts lazily, so `for i in 0..1000000` never builds an array.
- `?.` works on property access, indexing and calls: `obj?.name`, `list?.[0]`, `callback?.()`. Use it at each step that may be `nil`.

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

**More `for ... in` forms** (since beta-2.0.0) — the `ton.` prefix on loop variables is optional, a second variable gives the index (arrays, strings) or the value (dicts), and ranges and strings can be iterated directly:

```
for i in 0..3 { print(i) }                    // 0 1 2
for i in 1..=3 { print(i) }                   // 1 2 3
for i, name in ["kuro", "rusher"] { print(i, name) }
for key, value in {a: 1, b: 2} { print(key, "=", value) }
for c in "abc" { print(c) }
```

Each iteration has its own scope, so a function created inside the loop remembers that iteration's value:

```
ton.array fns = []
for i in 0..3 { push(fns, () => i) }
print(fns.map(f => f()))    // [0, 1, 2]
```

`continue` inside a `switch` that is inside a loop now continues the loop. `catch` also accepts the error name without parentheses: `catch err { ... }`.

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

**Arrow functions** (since beta-2.0.0) — a short way to write a function expression, ideal for callbacks. A single parameter needs no parentheses; an expression body is returned automatically; a `{ block }` body works like a normal function.

```
ton.let double = n => n * 2
ton.let add = (a, b = 1) => a + b
ton.let greet = (name) => {
    ton.string msg = "Hello " + name
    return msg
}
print(double(21), add(1), greet("kuro"))   // 42 2 Hello kuro
```

**Spread in calls** — `f(...args)` passes the elements of an array as separate arguments: `max(...[4, 9, 2])`.

**The pipe operator `|>`** — `x |> f` calls `f(x)`, and `x |> f(a, b)` calls `f(x, a, b)`. It turns nested calls into a left-to-right pipeline:

```
ton.function inc(x, by = 1) { return x + by }
print(5 |> inc |> inc(10) |> str)              // 16
[1, 2, 3, 4] |> filter(n => n % 2 == 0) |> sum |> print   // 6
```

**`print` is also a function value**: `["a", "b"].forEach(print)`. `print(a, b, c)` prints its arguments separated by spaces.

Callbacks given to `map`, `filter`, `find`, `findIndex`, `forEach`, `any`, `all`, `count` and `flatMap` receive the index as a second argument when they declare two parameters: `names.map((name, i) => f"{i}. {name}")`.

Deep recursion is fine (the interpreter runs on a large stack), and runaway recursion stops with a catchable `Maximum recursion depth (10000) exceeded` error instead of crashing.

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

**Spread** (`...`) splices another array's elements into a literal in place:
```
ton.array a = [1, 2, 3]
ton.array b = [0, ...a, 4]   // [0, 1, 2, 3, 4]
ton.array both = [...a, ...a] // [1, 2, 3, 1, 2, 3]
```
The spread expression must itself be an array — spreading anything else is a runtime error.

**Negative indices and slices** (since beta-2.0.0) — `a[-1]` is the last element. `a[start:end]` returns a new array from `start` up to (not including) `end`; either bound can be left out, and both can be negative. Slices work on strings too.

```
ton.array a = [10, 20, 30, 40, 50]
print(a[-1], a[1:3], a[:2], a[3:], a[-2:])    // 50 [20, 30] [10, 20] [40, 50] [40, 50]
print("TON618"[0:3], "TON618"[-3:])          // TON 618
```

**Destructuring** — unpack an array into variables, with `...rest` collecting what's left:

```
ton.let [first, second, ...others] = a
print(first, second, others)                  // 10 20 [30, 40, 50]
```

**Method syntax** — every array function can be called on the array itself (see [Method syntax](#method-syntax--the-dot-operator)):

```
ton.array scores = [12, 7, 30, 18]
print(scores.filter(s => s > 10).map(s => s * 2).sorted())   // [24, 36, 60]
print(scores.sum(), scores.max(), scores.len(), scores.length)
```

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

**Spread** (`...`) merges another dict's entries into a literal in place — a later entry (spread or not) overrides an earlier one with the same key, keeping that key's original position rather than moving it to the end:
```
ton.dict base = {a: 1, b: 2}
ton.dict merged = {...base, b: 99, c: 3}   // {a: 1, b: 99, c: 3}
```
The spread expression must itself be a dict — spreading anything else is a runtime error.

**Dot access** (since beta-2.0.0) — `d.key` reads a key (or `nil`) and `d.key = value` sets it, exactly like `d["key"]`. Use brackets for keys that aren't valid names or are computed.

```
ton.dict cfg = {port: 8080}
cfg.host = "localhost"
cfg.port += 1
print(cfg.port, cfg.host)            // 8081 localhost
print(cfg.missing?.value)            // nil (optional chaining)
```

**Destructuring** — pick keys into variables, optionally renaming them with `key: name`, and collect the rest with `...rest`:

```
ton.let {name, age: years, ...rest} = {name: "rusher", age: 30, city: "paris"}
print(name, years, rest)             // rusher 30 {city: paris}
```

`for key, value in d { ... }` walks the pairs; `items(d)`, `dict(pairs)`, `merge(a, b)` and `remove(d, key)` complete the toolbox (see [Built-in native functions](#built-in-native-functions)).

---

### Classes & objects

Since beta-2.0.0 you can define your own types. A class groups **fields** (data) and **methods** (functions that work on that data). Inside a method, `self` is the object the method was called on.

```
ton.class Point {
    ton.function init(x, y) {
        self.x = x
        self.y = y
    }
    ton.function distanceTo(other) {
        return sqrt((self.x - other.x) ** 2 + (self.y - other.y) ** 2)
    }
    ton.function toString() {
        return f"({self.x}, {self.y})"
    }
}

ton.Point a = Point(0, 0)
ton.let b = Point(3, 4)
print(a.distanceTo(b))     // 5
print(b)                   // (3, 4) — print/str/f-strings use toString()
b.x = 10                   // fields can be read and written with the dot
```

- **Creating an object**: call the class like a function. Its `init` method (the constructor) receives the arguments; without `init`, the class takes no arguments.
- **Field declarations** in the class body (`ton.int count = 0`, `ton.let items = []`) give every new object its own initial value, before `init` runs. Fields can also be created at any time by assigning `self.name = ...`.
- **Typed variables**: `ton.Point p = ...` checks that the value is a `Point` (or a subclass of it), or `nil`.
- **`toString()`**, if defined, is used whenever the object is turned into text. Without it, an object prints as `Point{x: 3, y: 4}`; `json(obj)` gives `{"x":3,"y":4}`.
- **Methods are values**: `ton.let f = point.toString` keeps `self` bound, so `f()` works later.
- Objects are passed **by reference** (like arrays and dicts): `==` is true only for the very same object. `copy(obj)` / `deepcopy(obj)` duplicate one.

**Inheritance** — `extends` makes a class reuse (and override) everything from a parent class; `super.method(...)` calls the parent's version:

```
ton.class Animal {
    ton.string sound = "..."
    ton.function init(name) { self.name = name }
    ton.function speak() { return f"{self.name} says {self.sound}" }
}

ton.class Dog extends Animal {
    ton.int tricks = 0
    ton.function init(name) {
        super.init(name)
        self.sound = "woof"
    }
    ton.function learn() {
        self.tricks++
        return self              // returning self allows chaining
    }
}

ton.Dog rex = Dog("Rex")
print(rex.speak())                     // Rex says woof
print(rex.learn().learn().tricks)      // 2
print(isinstance(rex, Animal))         // true
print(type(rex))                       // Dog
```

Objects work with the rest of the language: `"x" in point` tests a field, `keys(obj)`/`values(obj)`/`items(obj)` list fields, `ton.let {x, y} = point` destructures one, `{...point}` copies its fields into a dict, and `throw MyError("...")` throws one (the `catch` receives the object itself).

---

### Method syntax & the dot operator

The dot works on every value, and does the obvious thing:

| Expression | What it does |
|---|---|
| `obj.method(args)` | Calls the method of an object, with `self = obj` |
| `obj.field` | Reads a field of an object |
| `dict.key` | Reads a key of a dict (`nil` if missing) |
| `module.fn(args)` | Calls a function of a module imported with `IMPORT://x as module` |
| `value.fn(args)` | Anything else: calls the function `fn(value, args)` |

The last line is called **uniform call syntax**: `x.f(a, b)` is the same as `f(x, a, b)` whenever `x` has no method or key called `f`. It means every function — built-in or your own — can be used as a method, and calls can be chained from left to right instead of being nested:

```
// The same computation, twice:
print(join(map(filter(split("5,12,8,30", ","), s => num(s) > 6), s => "#" + s), " "))
print("5,12,8,30".split(",").filter(s => num(s) > 6).map(s => "#" + s).join(" "))
// #12 #8 #30

ton.function shout(s) { return s.upper() + "!" }
print("hello".shout())                  // HELLO!
print([3, 1, 2].sort().reverse())       // [3, 2, 1]
print("  padded ".trim().padStart(10, "."))
```

`.length` is also available as a property on strings and arrays (`"abc".length` is `3`).

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

**Throwing any value** (since beta-2.0.0) — `throw` accepts any value, and `catch` receives exactly that value, so errors can carry structured data. Errors raised by the interpreter itself are still caught as a string (`"Error at line 3: Division by zero."`).

```
try {
    throw {code: 404, message: "not found"}
} catch (err) {
    print(err.code, err.message)      // 404 not found
}

ton.class ValidationError {
    ton.function init(field) { self.field = field }
    ton.function toString() { return "invalid field: " + self.field }
}
try { throw ValidationError("email") } catch e { print(e) }   // invalid field: email
```

Errors from native functions now include the line of the call (`Error at line 7: sum() expects an array of numbers (found a string).`). A `return`, `break` or `continue` inside `finally` takes precedence over the pending one.

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

**Added in beta-2.0.0** — every one of them can also be called with [method syntax](#method-syntax--the-dot-operator) (`arr.sum()`, `s.padStart(3, "0")`).

*Conversions & misc*

| Function | Description |
|---|---|
| `int(x, [base])` | Integer part of a number, or parse a string (`int("ff", 16)` is `255`) |
| `float(x)` | Parse a string as a number (errors on garbage, unlike `num`) |
| `bool(x)` | `true`/`false` according to truthiness |
| `fixed(x, [digits=2])` | Number to string with a fixed number of decimals |
| `round(x, [digits])` | Rounds, optionally to some decimals |
| `repr(x)` | Like `str`, but strings come back quoted |
| `isinstance(obj, Class)` | Whether `obj` is an instance of `Class` or of a subclass |
| `copy(x)` / `deepcopy(x)` | Shallow / recursive copy of an array, dict or object |
| `clamp(x, lo, hi)`, `sign(x)` | Number helpers |
| `exit([code])` | Stops the script immediately |
| `print(a, b, ...)` | `print` is also a function value, printing its arguments separated by spaces |

*Arrays*

| Function | Description |
|---|---|
| `range(end)`, `range(start, end, [step])` | Array of numbers, `end` excluded |
| `sum(arr)`, `avg(arr)` | Sum / mean of numbers |
| `min(arr)`, `max(arr)` | `min`/`max` also accept a single array |
| `sorted(arr, [key])` | Sorted copy; `key(x)` gives what to sort by: `sorted(words, w => len(w))` |
| `sort(arr, [compare])` | In place; `compare(a, b)` returns a negative number (or `true`) when `a` comes first |
| `forEach(arr, fn)` | Calls `fn(item)` (or `fn(item, index)`) for each element |
| `findIndex(arr, fn)` | Index of the first match, or `-1` |
| `count(arr, valueOrFn)` | How many elements equal a value / match a predicate (also counts substrings) |
| `flatMap(arr, fn)` | `map` then flatten one level |
| `unique(arr)` | Copy without duplicates (first occurrence kept) |
| `flatten(arr, [depth=1])` | Flattens nested arrays |
| `zip(a, b, ...)` | `[[a0, b0], [a1, b1], ...]` |
| `enumerate(arr)` | `[[0, a0], [1, a1], ...]` |
| `chunk(arr, size)` | Splits into arrays of `size` elements |
| `groupBy(arr, fn)` | Dict of arrays, grouped by `fn(item)` |
| `first(arr)`, `last(arr)` | First / last element, or `nil` |
| `insert(arr, index, value)`, `removeAt(arr, index)` | Insert / remove at a position |
| `remove(arr, value)` | Removes the first equal element; `remove(dict, key)` removes a key |
| `clear(x)`, `isEmpty(x)` | Empty an array/dict; test emptiness |
| `lastIndexOf(x, item)` | Last position in an array or string, or `-1` |
| `reduce(arr, fn)` | Without an initial value, starts from the first element |

*Dicts*

| Function | Description |
|---|---|
| `items(d)` | `[[key, value], ...]` |
| `dict(pairs)` | Builds a dict from `[[key, value], ...]` |
| `merge(a, b, ...)` | New dict, later keys win |

`keys`, `values`, `items`, `has` and `len` also work on objects (their fields).

*Strings*

| Function | Description |
|---|---|
| `startsWith(s, p)`, `endsWith(s, p)` | Prefix / suffix test |
| `padStart(s, width, [fill])`, `padEnd(...)` | Pad to a width |
| `repeat(s, n)` | Repeat a string (or an array) |
| `chars(s)`, `lines(s)` | Split into characters / lines |
| `capitalize(s)` | First letter in upper case |
| `reverse(s)` | `reverse` now also reverses a string |
| `ord(c)`, `chr(n)` | Character code and back |

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

**Namespaces** (since beta-2.0.0) — `IMPORT://name as alias` loads a module without adding its names to the global scope: they're reached through the alias instead. For a built-in module the `module_` prefix is dropped.

```
IMPORT://mathutils as m
print(m.add(2, 3))

IMPORT://ton.strings as s
print(s.snake_case("helloWorld"))     // strings_snake_case -> s.snake_case
```

**Subfolders** — `IMPORT://utils/text` loads `utils/text.ton` (next to the script, or in `modules/`).

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

### Installing modules: `ton618 install`

```
ton618 install greetings
```

This looks `greetings` up in the **TON618 module registry** — a small, separately-hosted index of community modules that stores only a module's **name, GitHub link, and version** (never its actual code) — then downloads `greetings.ton` straight from that GitHub repo's default branch into `./modules/greetings.ton`, right where `IMPORT://greetings` already looks for it (see [Creating a TON618 module](#creating-a-ton618-module--full-guide)).

The registry's URL defaults to a value baked into `include/Version.hpp` (`TON618_REGISTRY_URL`) and can be pointed at a different deployment without rebuilding, via the `TON618_REGISTRY_URL` environment variable:

```
TON618_REGISTRY_URL=https://your-registry.example ton618 install greetings
```

Like `--update`, `install` has no bundled TLS client of its own — it shells out to `curl` (or `wget`/PowerShell) for both the registry lookup and the GitHub download, the same as every other network operation in the CLI itself (as opposed to `ton.requests`, which is a separate, sandboxed HTTP client exposed to scripts — see [Built-in system modules](#built-in-system-modules)).

A module author doesn't need anything special to be installable this way: just a public GitHub repo with `<name>.ton` at its root, registered once in the registry with that repo's link and a version string.

To remove a module fetched this way, use `ton618 uninstall <module>` — it just deletes `./modules/<module>.ton` (the exact file `install` created), no registry lookup involved:

```
ton618 uninstall greetings
```

(Not to be confused with `ton618 --uninstall-ton`, which removes the `ton618` interpreter itself — see [Installing & building the interpreter](#installing--building-the-interpreter). That command also wipes out the current directory's whole `./modules/` folder, so a single `ton618 uninstall <module>` and a full `--uninstall-ton` remove different amounts: one module vs. every module fetched into this project.)

---

### Built-in system modules

TON618 ships with twelve **built-in system modules**. They aren't files on disk — they live inside the interpreter — but you still `IMPORT://ton.<name>` them before using them, exactly like a user module:

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
IMPORT://ton.tensor
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

#### ton.tensor — small 1D/2D numeric arrays for basic ML work

A **tensor** is a `ton.dict` with two fields — `{shape: [...], data: [...]}` (`data` is flat, row-major) — so it prints, `str()`s, and `json()`s like any other dict. The functions below implement element-wise/matmul/reduction ops as real C++ loops (bypassing the AST), a genuine speed-up over the same loop hand-written in TON618 — but see **honest limits** below before reaching for this to do anything ambitious. This same `{shape, data}` convention is what the community `atome` module (a plain `.ton` file, not part of the interpreter — see [`ton618 install atome`](#installing-modules-ton618-install)) builds its autograd/training layer on top of.

| Function | Description |
|---|---|
| `tensor_zeros(shape)` / `tensor_ones(shape)` / `tensor_full(shape, value)` | A new tensor of the given shape (1 or 2 dimensions), filled |
| `tensor_random(shape, [lo=0, hi=1])` | A new tensor filled with values uniformly random in `[lo, hi)` (uses the same RNG as `random()`/`ton.random` — seed it with `random_seed()` for reproducible runs) |
| `tensor_from_array(arr)` | Builds a tensor from a flat array (1D) or an array of same-length arrays (2D) |
| `tensor_to_array(t)` | The reverse: a plain `ton.array` (nested for 2D) |
| `tensor_shape(t)` / `tensor_size(t)` | The shape array / total element count |
| `tensor_clone(t)` | A deep copy — `t` and its dict/array fields are otherwise shared references like any dict, so mutating one alias mutates all of them |
| `tensor_reshape(t, newShape)` | A new tensor viewing the same data with a different shape (element count must match) |
| `tensor_get(t, indices)` / `tensor_set(t, indices, value)` | Read/write one element — `indices` is `[i]` for 1D, `[row, col]` for 2D |
| `tensor_add(a, b)` / `tensor_sub(a, b)` / `tensor_mul(a, b)` / `tensor_div(a, b)` | Element-wise; either side may be a plain number (scalar broadcast), but two tensors must have the *same* shape — there is no NumPy-style broadcasting between differently-shaped tensors |
| `tensor_add_bias(mat, bias)` | Adds a 1D bias to every row of a 2D matrix — the one deliberate exception to "no broadcasting," exactly what a dense/linear layer needs |
| `tensor_matmul(a, b)` | 2D matrix multiply (`a` is m×k, `b` is k×n, result is m×n) |
| `tensor_transpose(t)` | 2D transpose |
| `tensor_sum(t)` / `tensor_mean(t)` / `tensor_max(t)` / `tensor_min(t)` | Reduce every element to one number (no per-axis reduction) |
| `tensor_relu(t)` / `tensor_sigmoid(t)` / `tensor_tanh(t)` / `tensor_exp(t)` / `tensor_log(t)` | Element-wise activation/math functions |
| `tensor_softmax(t)` | Softmax over the whole vector (1D) or row-wise (2D) |
| `tensor_relu_grad(input)` / `tensor_sigmoid_grad(output)` / `tensor_tanh_grad(output)` | Element-wise derivatives for hand-rolled backprop — note `sigmoid_grad`/`tanh_grad` take that function's *output* (cheaper to express that way), while `relu_grad` takes its *input* |
| `tensor_map(t, fn)` | Applies a `ton.function`/function-expression to every element — slower than the natives above (one interpreter call per element) but works for any custom scalar function |
| `tensor_argmax(t)` | The index of the largest element (1D), or one index per row (2D) — the "which class did this predict" op |
| `tensor_conv1d(signal, kernel, [stride=1])` | 1D "valid" convolution (cross-correlation) on single-channel 1D tensors: `out[i] = sum_j signal[i*stride+j] * kernel[j]` — see the community `atome` module for how multi-channel Conv1D layers (and their gradient) are built from this primitive |
| `tensor_to_bytes(t)` | Packs the tensor's flat data as raw IEEE-754 float64 bytes (native endianness), returned as a `ton.string` — combined with `writefile` (which writes a string's *exact* bytes), this is what lets a module write real binary files instead of JSON text. Not portable between machines of different endianness |
| `tensor_from_bytes(bytes, shape)` | The inverse of `tensor_to_bytes` — unpacks a raw bytes string (e.g. from `readfile` + `substring`) back into a tensor of the given shape; errors if the byte length doesn't match `shape`'s element count × 8 |

```
IMPORT://ton.tensor

ton.dict a = tensor_from_array([[1, 2], [3, 4]])
ton.dict b = tensor_from_array([[5, 6], [7, 8]])
print(tensor_to_array(tensor_matmul(a, b)))   // [[19, 22], [43, 50]]
print(tensor_to_array(tensor_relu(tensor_from_array([-1, 0, 2]))))  // [0, 0, 2]
```

**Honest limits** — read this before building anything ambitious on top of `ton.tensor`:
- **CPU only.** There is no GPU compute backend (no CUDA/OpenCL bindings) anywhere in this interpreter, and none is planned — "GPU support" isn't something a `.ton` module can add on its own.
- **1D and 2D shapes only** — no 3D+ tensors (so no image batches, no attention-head-shaped tensors, etc. without flattening/reshaping tricks yourself).
- **Only scalar broadcasting** (plus the one explicit `tensor_add_bias` exception above) — `tensor + differently-shaped-tensor` (general NumPy-style broadcasting) does not work.
- **No autograd built in.** `ton.tensor` gives you the forward ops and the matching `_grad` helpers to build backpropagation *by hand* — there's no automatic computation-graph tracking. See the community `atome` module ([`ton618 install atome`](#installing-modules-ton618-install)) for a from-scratch autograd/training layer built on top of this.
- **Not remotely competitive with a real numeric library.** These are still C++ loops over a dynamically-typed `Value` array — real work for a small language, but nowhere near BLAS/cuDNN performance. Fine for learning, toy models, and small experiments; not for training anything at real scale.

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

### The IDE & editor extensions

**TON618 IDE** — a small editor made for TON618, for **Windows, Linux, macOS and Android**. It's built with [Tauri](https://tauri.app), which uses the system's own web view instead of shipping a browser: the whole app, interpreter included, is a ~2 MB download.

- An editor with TON618 highlighting, auto-indent, bracket pairs, `Ctrl+/` comments and syntax errors marked as you type.
- **▶ Run** (`Ctrl+Enter`): the output appears below; click an error's line to jump to it. **■ Stop** ends a runaway loop.
- Two ways to run code: the **built-in engine** (everywhere, Android included — the same engine as the playground) or the **ton618 interpreter** bundled with the desktop app (the full standard library, files, network, the HTTP server, and `input()` through the input box).
- Several files: `IMPORT://name` loads another file of the project. On desktop, **📂** opens a real folder; otherwise files are kept in the app's workspace.

**VS Code extension** — highlighting (including inside f-strings), snippets (`fn`, `class`, `for`, `try`...), **Run File** (▶ in the title bar, `Ctrl+Alt+R`), **Open the REPL**, and syntax errors underlined on save (using `ton618 --check`). Settings: `ton618.interpreterPath`, `ton618.checkOnSave`, `ton618.saveBeforeRun`.

**Acode plugin** (the code editor for Android) — highlighting, a **▶** button and a *TON618: Run* command with an output panel, syntax errors while typing, and `IMPORT://` from the file's folder. It runs code with the built-in engine, so it works offline with nothing else installed; for the full standard library, use the real interpreter in Termux.

**Where to get them** — every [release](https://github.com/kurodaki-dev/ton618/releases/latest) has:

| | File |
|---|---|
| IDE for Windows | `TON618-IDE-*-windows-x64-setup.exe` (or `.msi`) |
| IDE for Linux | `TON618-IDE-*-linux-x64.AppImage` (any distribution) or `.deb` |
| IDE for macOS | `TON618-IDE-*-macos-universal.dmg` (Intel and Apple Silicon) |
| IDE for Android | `TON618-IDE-*-android.apk` |
| VS Code | `ton618-vscode-*.vsix` — Extensions view → ⋯ → *Install from VSIX…* |
| Acode | `ton618-acode-plugin-*.zip` — Settings → Plugins → ⋮ → *Install from local* |

The apps aren't code-signed: on Windows click *More info → Run anyway* the first time, on macOS right-click the app → *Open*.

**How releases are made** — `.github/workflows/release.yml` builds everything on GitHub Actions (the interpreter for four platforms, the IDE for four platforms, both extensions) and publishes a release **automatically whenever `TON618_VERSION` in `include/Version.hpp` changes** on `main`. To release, bump the version and push; *Run workflow* rebuilds the current version. The sources are in `ide/`, `editors/vscode/` and `editors/acode/`.

`ton618 --check file.ton` (used by the editors) checks a file's syntax without running it.

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
│                              --debug / --break / --version / --update / --uninstall-ton / install / uninstall)
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

**Internals since beta-2.0.0**, for contributors:

- `break`/`continue`/`return` are no longer C++ exceptions: `Interpreter::execute()` returns a `Signal` (`NONE`, `BREAK`, `CONTINUE`, `RETURN`) that loops and calls check. Only real errors use exceptions (`LineError` for interpreter errors, `ThrownValue` for `throw`).
- Names are interned into integers (`Symbols` in `Environment.hpp`) and scopes are small flat vectors, with a hash index only for big scopes such as the globals. A block that declares nothing doesn't get its own scope at all.
- Classes are `ClassObj` and objects `InstanceObj` (`Value.hpp`); method calls go through `Interpreter::callMethod`, which also implements uniform call syntax.
- The standard library added in 2.0 lives in `src/Stdlib.cpp` (`defineNativesV2()`).
- `main.cpp` runs scripts on a thread with a 512 MB stack, so deep recursion reaches the interpreter's own depth limit (10,000 calls) instead of crashing.
- `tests/*.ton` with `tests/*.expected` is the regression suite: `make test` (also run by CI).

Together these make typical scripts about 6x faster than beta-1.x (a `fib(25)` plus a 1,000,000-iteration loop: 2.0 s → 0.3 s).

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

The documentation website (https://c-ton618.duckdns.org/) is built with **[Claide](https://github.com/kurodaki-dev/Claide)**, a Python module for building websites, straight from this file: `website/app.py` reads `DOCUMENTATION.md`, turns every `###` section of the English and the French half into its own page (with a sidebar, a search box, "on this page" links, a language switch and "Run" buttons that open a snippet in the playground), and exports the whole site as static HTML. The playground (`exemples/playground.html` + `ton618-lite.js`), the AI skills page, the logo and `exemples/CNAME` are published next to it.

Preview it locally:
```bash
pip install claide-web
python website/app.py          # http://127.0.0.1:8000, reloads on save
python website/app.py build    # static export into website/_site/ (or: make site)
```

`.github/workflows/pages.yml` installs Claide, builds the site and deploys it on every push to `main` that touches `DOCUMENTATION.md`, `website/`, `exemples/` or the version number — editing this file is all it takes to update the website. With a custom domain (`exemples/CNAME`) the site is served from the domain's root; without one, from `https://<user>.github.io/ton618/`.

One-time setup:
1. **GitHub → this repository → Settings → Pages.**
2. Under **Build and deployment**, set **Source** to **GitHub Actions** (not "Deploy from a branch").

`exemples/documentation.html` is the older single-page version of this document, still published at `/documentation.html` and regenerated with `make docs`.

**Using a custom domain** (e.g. a free [DuckDNS](https://www.duckdns.org) subdomain instead of `github.io`): this needs no server of your own — GitHub hosts the site, not you.

1. On DuckDNS, point your subdomain's IP at one of GitHub Pages' fixed addresses instead of any server you run: `185.199.108.153`, `185.199.109.153`, `185.199.110.153`, or `185.199.111.153` (DuckDNS only accepts one IP per subdomain, so pick just one). If that subdomain is also kept updated by a dynamic-DNS script running somewhere (cron on a VPS, a router, etc.), that script will overwrite this the next time it runs — use a subdomain that nothing else auto-updates.
2. `exemples/CNAME` in this repo already holds the domain the docs are pinned to; the workflow publishes it as-is, which is what tells GitHub Pages to serve that domain.
3. **GitHub → this repository → Settings → Pages** should then show the custom domain under **Custom domain** (DNS propagation can take a few minutes) — once it verifies, tick **Enforce HTTPS**.

---

### Known limitations

- `serve()` is single-threaded and blocking — one request at a time, no concurrency
- `ton.int` and `ton.float` are the same internal number type (no integer-only enforcement)
- Dict keys are always strings (no numeric or nested-structural keys)
- Classes have no static fields, no private members and no operator overloading (`==` compares objects by identity)
- `ton.requests` only speaks plain `http://` — no TLS/`https://`, no redirects, no chunked transfer-encoding
- The browser playground (`ton618-lite.js`) doesn't implement `ton.encoding`/`ton.regex`/`ton.path`/`ton.tensor`, and can print floats with more decimal digits than the real interpreter (a JavaScript-vs-C++ number-formatting difference, not a value difference)

---

### Changelog

**beta-2.0.2** — the Android icon, and a new documentation website:

- **Fixed the Android app icon**: Android crops the edges of app icons, so the new logo was shown cut to its centre on a white background. The icon now has the whole logo on black, inside the area Android always shows.
- **The documentation website is rewritten** with Claide: a full-height side menu with search and collapsible parts, a flat monochrome design in the logo's colours, and no light effect.

**beta-2.0.1** — the new logo:

- **New TON618 logo** everywhere: the website, the playground, the IDE's application icons, the VS Code and Acode icons. It carries its copyright notice (© 2026 kurodaki-dev, see `LOGO-LICENSE.md`).
- **The colours follow the logo**: the orange accent is replaced by the logo's white and grey (`#595959`) on black — on the website, in code examples, the playground, the IDE and the Acode plugin.

**beta-2.0.0** — a big modernization of the language, and a 6x faster interpreter:

*Objects:*
- **Classes**: `ton.class Name extends Parent { ... }` with fields, an `init` constructor, methods, `self`, `super`, `toString()`, and typed object variables (`ton.Point p = Point(1, 2)`) — see [Classes & objects](#classes--objects).
- **The dot operator everywhere**: `obj.field`, `obj.method()`, `dict.key`, and uniform call syntax (`"hi".upper()`, `arr.filter(...).map(...)`) — see [Method syntax](#method-syntax--the-dot-operator).

*New syntax:*
- **Arrow functions** `x => x * 2`, `(a, b) => { ... }`, and the **pipe** `x |> f(y)`.
- **f-strings** `f"Hello {name}!"`, **raw strings** in backticks, `\u{...}` escapes.
- **Ranges** `0..10` / `1..=10`, **slices** `a[1:3]`, **negative indices** `a[-1]`.
- **`ton.let` / `ton.var` / `ton.const`**, and **destructuring** `ton.let [a, ...rest] = arr`, `ton.let {x, y: alias} = obj`.
- `for k, v in dict`, `for i, x in array`, `for c in "text"`, loop variables without `ton.`, a fresh scope per iteration.
- **Optional chaining** `a?.b`, `a?.[i]`, `f?.()`; **`??=`**; **`**`** (power); **spread in calls** `f(...args)`.
- Number literals `1_000_000`, `1e9`, `0xFF`, `0b1010`, `0o755`.
- `throw` any value (dicts, objects); `catch err` without parentheses.
- `IMPORT://module as alias` namespaces and `IMPORT://folder/module`.
- `print(a, b, c)` with several arguments; `print`, `int`, `float`, `bool`, `dict` usable as function values.

*Standard library:* ~45 new functions — `sum`, `avg`, `sorted`, `sort` with a comparator, `range`, `zip`, `enumerate`, `unique`, `flatten`, `chunk`, `groupBy`, `flatMap`, `count`, `findIndex`, `forEach`, `items`, `dict`, `merge`, `remove`, `insert`, `removeAt`, `padStart`, `padEnd`, `startsWith`, `endsWith`, `repeat`, `chars`, `lines`, `capitalize`, `int`, `float`, `fixed`, `isinstance`, `copy`, `deepcopy`, `exit`... — see [Built-in native functions](#built-in-native-functions).

*Tooling:*
- **REPL**: `ton618` with no arguments; **`ton618 -e "code"`** for one-liners.
- **Better errors**: the source line, a traceback through function calls, "did you mean ...?" for misspelled names, and line numbers on errors from native functions.
- **Catchable recursion limit** (10,000 calls) instead of a crash.
- A regression test suite (`tests/`, `make test`, run by CI).
- **TON618 IDE** for Windows, Linux, macOS and Android, a **VS Code extension** and an **Acode plugin** — see [The IDE & editor extensions](#the-ide--editor-extensions). `ton618 --check` checks a file's syntax without running it.
- **Automatic releases**: everything is built by GitHub Actions and published when `TON618_VERSION` changes; the interpreter now also ships for **macOS** (universal binary), and `install.sh` supports it.
- The logo carries a copyright notice (see `LOGO-LICENSE.md`): it isn't covered by the Apache 2.0 license.

*Behavior changes:*
- Numbers print in their shortest exact form: `3.14` instead of `3.140000`, `0.5` instead of `0.500000` (also in `str()`, `json()` and string concatenation).
- `continue` inside a `switch` inside a loop now continues the loop (it used to be swallowed by the switch).
- Errors from native functions caught with `catch` now start with `Error at line N:` like every other runtime error.
- A closure created inside a `for ... in` loop captures that iteration's variable (it used to see the last value).

*Performance:* about 6x faster on typical code — names are resolved through interned symbols, scopes are flat vectors, control flow no longer uses C++ exceptions, and blocks that declare nothing don't allocate a scope.

**beta-1.0.10** — `tensor_to_bytes`/`tensor_from_bytes`, real binary files:

- **`tensor_to_bytes(t)` / `tensor_from_bytes(bytes, shape)`**: pack/unpack a tensor's data as raw IEEE-754 float64 bytes. Combined with `writefile`/`readfile` (which now open files in binary mode, so bytes round-trip exactly on Windows too, not just Linux/Mac), this is what lets a `.ton` module write real binary files — no more encoding numbers as JSON text and losing precision to `json_stringify`'s ~6-significant-digit rounding. The community `atome` module's `.atm` model files now use this instead of JSON.

**beta-1.0.9** — `--uninstall-ton` also removes `./modules/`:

- **Behavior change**: `ton618 --uninstall-ton` now also deletes the current directory's whole `./modules/` folder (every module previously fetched there with `ton618 install <module>`), in addition to removing the binary and its PATH entry. The standalone `scripts/uninstall.sh`/`uninstall.ps1` do the same. If you only want to remove one module, use `ton618 uninstall <module>` instead — see [Installing modules](#installing-modules-ton618-install).

**beta-1.0.8** — `ton618 install`/`uninstall` normalize module names to lowercase:

- **Bugfix**: `ton618 install <module>` now lowercases the module name before building the GitHub raw-content URL and the local `./modules/<name>.ton` path (the registry lookup itself still uses the name exactly as typed). Every existing module follows the lowercase-filename convention (`atome.ton`, etc.), and `raw.githubusercontent.com` URLs are case-sensitive, so a registry entry published with any other casing (e.g. `Atome`) used to 404 instead of finding the real, lowercase file at the repo root. `ton618 uninstall <module>` was updated the same way, so it keeps targeting the file `install` actually wrote.

**beta-1.0.7** — `tensor_argmax` and `tensor_conv1d`:

- **`tensor_argmax(t)`**: the index of the largest element (or one per row for a 2D tensor).
- **`tensor_conv1d(signal, kernel, [stride])`**: 1D convolution (cross-correlation) on single-channel tensors — the primitive the community `atome` module builds its (multi-channel, autograd-tracked) Conv1D layer on top of, verified against a numerical gradient check.

**beta-1.0.6** — `ton.tensor`, a native module for basic ML work:

- **New built-in module `ton.tensor`**: 1D/2D numeric arrays with element-wise ops, matrix multiply, transpose, reductions, common activations (relu/sigmoid/tanh/softmax) and their gradients, plus array/tensor conversion — implemented as real C++ loops for a genuine speed-up over the equivalent hand-written TON618 loop. CPU only, no autograd built in — see [ton.tensor](#built-in-system-modules) for the full function list and its honest limits. The community `atome` module (`ton618 install atome`, once published) is built on top of it and gained a 5-16x speed-up switching to it, with zero change to its own public API.

**beta-1.0.5** — bitwise operators and spread:

- **Bitwise operators**: `&`, `|`, `^`, `~`, `<<`, `>>`, plus their compound-assignment forms (`&=`, `|=`, `^=`, `<<=`, `>>=`). Operands are truncated to 64-bit integers, operated on, and converted back — see [Operators](#operators).
- **Spread (`...`) in array literals**: `[...a, x, ...b]` splices other arrays' elements in place — see [Arrays](#arrays).
- **Spread (`...`) in dict literals**: `{...d1, key: val, ...d2}` merges other dicts' entries in place, later entries overriding earlier ones with the same key (in their original position) — see [Dictionaries](#dictionaries).

**beta-1.0.4** — `ton618 uninstall <module>`, and a clearer name for the old `--uninstall`:

- **`ton618 --uninstall`** (which removed the `ton618` interpreter itself) is renamed to **`ton618 --uninstall-ton`**, to make room for the new command below and avoid the two being confused.
- **`ton618 uninstall <module>`**: removes `./modules/<module>.ton` — the counterpart to `ton618 install <module>` (see [Installing modules](#installing-modules-ton618-install)).

**beta-1.0.3** — new operators, flexible function parameters, and `finally`:

- **Default parameters**: `function greet(name, greeting = "Hello") { ... }` — the default is evaluated at call time, in order, so a later default can reference an earlier parameter.
- **Rest parameters**: `function sum(...nums) { ... }` collects any extra arguments into an array; may be mixed with fixed parameters as long as `...` is last. Arity-error messages now adapt (`expects between 1 and 2 argument(s)`, `expects at least 1 argument(s)`).
- **`??` (nil-coalescing operator)**: `value ?? fallback` returns `value` unless it's `nil`, in which case it returns `fallback` — the fallback is never evaluated otherwise (short-circuiting). See [Operators](#operators).
- **`in` (membership operator)**: `value in array`, `value in dict` (tests keys), `value in string` (substring test). See [Operators](#operators).
- **`finally`**: `try`/`catch` gained a `finally` block that always runs — on success, on a caught error, or through a `return` — and `catch` is now optional as long as `finally` is present (`try { ... } finally { ... }`). See [Error handling](#error-handling-try--catch--throw--finally).
- **`ton618 install <module>`**: fetches a module's metadata from the TON618 module registry and downloads its code straight from GitHub into `./modules/`. See [Installing modules](#installing-modules-ton618-install).
- **A new AI skill for authoring TON618 modules** (as opposed to writing scripts), alongside the existing general-purpose skill — see `exemples/skills/ton618-module-author/`.

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
13. [Classes & objets](#classes--objets)
14. [Syntaxe méthode & l'opérateur point](#syntaxe-méthode--lopérateur-point)
15. [Gestion des erreurs : try / catch / throw / finally](#gestion-des-erreurs--try--catch--throw--finally)
16. [Fonctions natives](#fonctions-natives)
17. [Type HTML](#type-html)
18. [Modules (IMPORT://)](#modules-import-1)
19. [Créer un module TON618 — guide complet](#créer-un-module-ton618--guide-complet)
20. [Installer des modules : `ton618 install`](#installer-des-modules--ton618-install)
21. [Modules système intégrés](#modules-système-intégrés)
22. [Serveur local & routes (API)](#serveur-local--routes-api)
23. [Le débogueur](#le-débogueur)
24. [L'IDE & les extensions d'éditeur](#lide--les-extensions-déditeur)
25. [Architecture — comment fonctionne l'interpréteur](#architecture--comment-fonctionne-linterpréteur)
26. [Étendre l'interpréteur lui-même](#étendre-linterpréteur-lui-même)
27. [Publier la doc sur GitHub Pages](#publier-la-doc-sur-github-pages)
28. [Limitations connues](#limitations-connues)
29. [Changelog](#changelog-1)

---

### Qu'est-ce que TON618

TON618 est un petit langage interprété, dans le même esprit que Python ou HolyC : tu écris un fichier `.ton`, tu le lances directement avec l'interpréteur `ton618`, et il s'exécute immédiatement — aucune étape de compilation séparée pour tes scripts.

L'interpréteur lui-même (`ton618`) est écrit en C++17 et se présente comme un unique binaire, sans dépendance. Il est compilé en croisé pour Linux, Termux (Android) et Windows (voir [Installer & compiler](#installer--compiler-linterpréteur)), et détecte automatiquement lequel de ces trois il exécute au démarrage.

La caractéristique principale du langage est le **préfixe `ton.`** : déclarer une variable ou une fonction l'utilise toujours (`ton.int x = 5`, `ton.function f() { ... }`). Une fois quelque chose déclaré, y faire référence à nouveau — le lire, le réassigner, ou nommer un paramètre de fonction — accepte aussi bien `ton.nom` que le simple `nom` ; les deux désignent la même chose. Cette documentation utilise `ton.` partout par souci de clarté, sauf pour des noms éphémères comme les compteurs de boucle et les paramètres de fonction, comme le fait déjà le style utilisé dans `exemples/`. Voir [Bases du langage](#bases-du-langage) pour savoir précisément où le préfixe est obligatoire ou optionnel.

Depuis **beta-2.0.0**, c'est aussi un langage orienté objet et expressif : des **classes** avec héritage, la **syntaxe méthode sur toutes les valeurs** (`"hi".upper()`, `nums.filter(...).map(...)`), des **fonctions fléchées**, des **f-strings**, des **intervalles** et **tranches**, la **déstructuration**, l'**opérateur pipe** `|>`, un **REPL interactif**, des messages d'erreur utiles avec trace d'appels — et un interpréteur environ 6 fois plus rapide que la 1.x. Voir [Classes & objets](#classes--objets) et le [Changelog](#changelog-1).

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

Installe une release précise plutôt que la dernière en mettant `TON618_VERSION` (`install.sh`) ou `$env:TON618_VERSION` (`install.ps1`) à un tag de release (ex. `beta-1.0.1`) avant de lancer la commande ci-dessus. Désinstalle à tout moment avec `ton618 --uninstall-ton`, ou les scripts `uninstall.sh`/`uninstall.ps1` correspondants.

Aucun compilateur requis pour tout ça — un binaire précompilé est tout ce que tu lances jamais. Si tu contribues à l'interpréteur lui-même et as besoin de le compiler depuis les sources, voir [Architecture](#architecture--comment-fonctionne-linterpréteur) et [Étendre l'interpréteur lui-même](#étendre-linterpréteur-lui-même) plus bas (le projet a juste besoin d'un compilateur C++17 et de `make` — `make` compile `./ton618`, `make docs` régénère `exemples/documentation.html` depuis ce fichier après une modification).

---

### Lancer un script

```bash
ton618 chemin/vers/script.ton
```

Options :

| Option | Effet |
|---|---|
| `--check` | `ton618 --check fichier.ton` : vérifie la syntaxe sans exécuter (utilisé par les éditeurs) |
| `--debug` | Démarre en mode débogage pas-à-pas |
| `--break=<ligne>` | Ajoute un point d'arrêt à une ligne donnée (répétable) |
| `-h`, `--help` | Affiche l'aide |
| `-v`, `--version` | Affiche la version, la plateforme détectée (`linux`/`termux`/`windows`/`macos`) et l'architecture |
| `--update [version]` | Retélécharge le binaire correspondant à cette plateforme depuis les [GitHub Releases](https://github.com/kurodaki-dev/ton618/releases) et remplace celui en cours d'exécution — la dernière release si `version` (un tag de release) est omis |
| `--uninstall-ton` | Désinstalle ton618 : le binaire, son entrée PATH, et le dossier `./modules/` du répertoire courant (tous les modules précédemment récupérés avec `install`) |

Deux autres commandes, utilisées comme `ton618 <commande> <module>` plutôt que comme une option sur un script :

| Commande | Effet |
|---|---|
| `install <module>` | Récupère `<module>` depuis le [registre de modules TON618](#installer-des-modules--ton618-install) et télécharge son code dans `./modules/` |
| `uninstall <module>` | Supprime `./modules/<module>.ton` (annule `install`) |

Tout ce qui suit le chemin du script et n'est pas une de ces options est transmis tel quel au script, lisible via `sys_args()` (voir [ton.sys](#modules-système-intégrés)).

Exemple :
```bash
ton618 exemples/test.ton --break=10 --break=20
ton618 --update             # met à jour vers la dernière release
ton618 --update beta-1.0.1  # épingle une release précise
ton618 install stringutils   # récupère un module depuis le registre
ton618 uninstall stringutils # le supprime à nouveau
```

**Le REPL interactif** — lance `ton618` sans argument (ou `ton618 repl`) pour obtenir une invite où chaque ligne s'exécute immédiatement et où la valeur d'une expression est affichée. Un `(`, `[` ou `{` non fermé continue sur la ligne suivante, on peut donc taper une fonction entière. `.vars` liste ce que tu as défini, `.exit` (ou Ctrl-D) quitte.

```
$ ton618
TON618 beta-2.0.0 (linux/x86_64)
ton> ton.let nums = [3, 1, 2]
ton> nums.sorted().map(n => n * 10)
[10, 20, 30]
ton> f"{len(nums)} éléments"
"3 éléments"
```

**Une ligne** — `ton618 -e 'print(2 ** 10)'` exécute un extrait sans créer de fichier.

**Rapports d'erreur** — une erreur non rattrapée affiche le message, la ligne de code fautive, une suggestion « did you mean » pour un nom mal orthographié, et — si elle s'est produite dans des appels de fonction — une courte trace d'appels :

```
Error at line 2: Division by zero.
  --> calc.ton:2
   2 | return a / b
Traceback (most recent call last):
  line 9: called compute()
  line 5: called divide()
  line 2: error raised here
```

---

### Le playground dans le navigateur

Pas envie d'installer quoi que ce soit juste pour essayer un extrait ? Ouvre `exemples/playground.html` dans n'importe quel navigateur (aucun serveur requis) pour un éditeur de code en direct avec un bouton "Run" et des extraits d'exemple à charger.

Le playground exécute **`ton618-lite.js`**, une réimplémentation JavaScript du langage, entièrement côté client — rien n'est envoyé nulle part, aucune installation requise. Il reproduit fidèlement l'interpréteur réel : la grammaire complète (y compris `switch`/`case`), et les modules intégrés `ton.random`, `ton.time`, `ton.json`, `ton.mathutils` et `ton.strings`, puisqu'aucun d'eux n'a besoin de quoi que ce soit qu'un navigateur ne puisse déjà faire.

Depuis beta-2.0.0 il prend en charge tout le langage 2.0 (classes, syntaxe méthode, fonctions fléchées, f-strings, intervalles, tranches, déstructuration, `|>`, `?.`...) et les nouvelles fonctions standard — la CI fait tourner la suite `tests/` à la fois dans le vrai interpréteur et dans `ton618-lite.js` et vérifie qu'ils affichent exactement la même chose. Sur le site de documentation, le bouton **Exécuter** d'un exemple de code l'ouvre dans le playground.

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

**`ton.let`, `ton.var` et `ton.const`** — `ton.let` déclare une variable sans type fixe (`ton.var` est un synonyme). `ton.const` fait de même mais interdit de la réassigner ensuite : `PI = 3` après `ton.const PI = 3.14` est une erreur d'exécution. Les déclarations typées (`ton.int`, `ton.string`, ...) vérifient toujours le type de la valeur initiale.

```
ton.let nimporte = 42
nimporte = "maintenant une chaîne"   // ok : ton.let n'a pas de type fixe
ton.const MAX_USERS = 100
MAX_USERS = 5                        // Erreur : Cannot reassign 'ton.MAX_USERS': it was declared with ton.const.
```

**Littéraux numériques** — en plus de `42` et `3.14` : `1_000_000` (les underscores sont ignorés), `1e9` / `2.5e-3` (exposant), `0xFF` (hexadécimal), `0b1010` (binaire), `0o755` (octal).

**Les points-virgules** sont optionnels ; ils servent à mettre plusieurs instructions sur une ligne : `self.x = x; self.y = y`.

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

Depuis beta-2.0.0 :

| Déclaration | Sens | Exemple |
|---|---|---|
| `ton.let` / `ton.var` | N'importe quel type, peut changer | `ton.let x = 5` |
| `ton.const` | N'importe quel type, non réassignable | `ton.const PI = 3.14` |
| `ton.<NomDeClasse>` | Une instance de cette classe (ou d'une sous-classe), ou `nil` | `ton.Point p = Point(1, 2)` |

`type(obj)` renvoie le nom de la classe pour un objet (`"Point"`), et `"class"` pour une classe. Les nombres s'affichent sous leur forme exacte la plus courte : `0.1 + 0.2` affiche `0.30000000000000004`, `3.14` affiche `3.14` (avant la 2.0 : `3.140000`). Utilise `fixed(x, 2)` pour un nombre fixe de décimales.

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

**f-strings** (depuis beta-2.0.0) — préfixe une chaîne par `f` pour y insérer n'importe quelle expression entre accolades. C'est la façon la plus courte de construire du texte ; `format()` fonctionne toujours.

```
ton.string name = "kuro"
ton.int age = 21
print(f"Salut {name}, l'an prochain tu auras {age + 1} ans !")
print(f"{len(name)} lettres, en majuscules : {name.upper()}")
print(f"Accolades : {{comme ça}}")     // {{ et }} sont des accolades littérales
```

**Chaînes brutes** — les backticks délimitent une chaîne sans séquences d'échappement, qui peut tenir sur plusieurs lignes. Pratique pour du HTML, des expressions régulières et des chemins Windows :

```
ton.html page = `<div class="card">
  <p>C:\temp\new n'est pas un retour à la ligne ici</p>
</div>`
```

**Échappements** dans les chaînes entre guillemets : `\n` `\t` `\r` `\"` `\\` `\0`, et `\u{1F680}` / `\u00e9` pour n'importe quel caractère Unicode (écrit en UTF-8).

**Formater des nombres** : `fixed(3.14159, 2)` donne `"3.14"`, `"7".padStart(3, "0")` donne `"007"`.

---

### Opérateurs

| Catégorie | Opérateurs |
|---|---|
| Arithmétique | `+` `-` `*` `/` `%` |
| Comparaison | `==` `!=` `<` `<=` `>` `>=` |
| Logique | `&&`/`and`, `\|\|`/`or`, `!` |
| Coalescence nulle | `??` |
| Appartenance | `in` |
| Binaire (bits) | `&` `\|` `^` `~` `<<` `>>` |
| Assignation | `=` `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` |
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
- `&`, `|`, `^`, `~`, `<<`, `>>` traitent les deux côtés comme des entiers (en tronquant la partie décimale), opèrent sur une représentation 64 bits en complément à deux, puis reconvertissent le résultat dans le type nombre habituel de TON618 — il n'y a pas de type entier séparé, donc `6 & 3` est juste un nombre comme un autre (`2`). Les deux opérandes doivent être des nombres, sinon c'est une erreur d'exécution. `&`/`|`/`^` partagent tous le même niveau de priorité (moins prioritaire que `==`, plus prioritaire que `&&`) — utilise des parenthèses en les mélangeant pour plus de clarté.

Ajoutés en beta-2.0.0 :

| Opérateur | Sens | Exemple |
|---|---|---|
| `**` / `**=` | Puissance (associatif à droite) | `2 ** 10` vaut `1024` |
| `..` / `..=` | Intervalle, fin exclue / incluse | `0..5`, `1..=10` |
| `\|>` | Pipe : la valeur de gauche devient le premier argument | `x \|> f(y)` vaut `f(x, y)` |
| `?.` | Chaînage optionnel : `nil` au lieu d'une erreur | `user?.address?.city` |
| `??=` | Assigne seulement si la valeur est `nil` | `config.port ??= 8080` |
| `.` | Accès à une propriété / méthode | `point.x`, `"hi".upper()` |
| `=>` | Fonction fléchée | `n => n * 2` |

- `-2 ** 2` vaut `-4` : `**` lie plus fort que le moins unaire, comme en maths et en Python.
- `+` concatène aussi deux tableaux (`[1, 2] + [3]` vaut `[1, 2, 3]`, un nouveau tableau) ; `*` répète une chaîne (`"ab" * 3` vaut `"ababab"`).
- `a..b` s'évalue en un tableau des entiers de `a` à `b - 1` (`a..=b` inclut `b`) ; dans une boucle `for` il compte de façon paresseuse, donc `for i in 0..1000000` ne construit jamais de tableau.
- `?.` marche sur l'accès à une propriété, l'indexation et l'appel : `obj?.name`, `list?.[0]`, `callback?.()`. Mets-le à chaque étape qui peut être `nil`.

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

**Nouvelles formes de `for ... in`** (depuis beta-2.0.0) — le préfixe `ton.` des variables de boucle est optionnel, une deuxième variable donne l'indice (tableaux, chaînes) ou la valeur (dicts), et on peut itérer directement sur un intervalle ou une chaîne :

```
for i in 0..3 { print(i) }                    // 0 1 2
for i in 1..=3 { print(i) }                   // 1 2 3
for i, name in ["kuro", "rusher"] { print(i, name) }
for key, value in {a: 1, b: 2} { print(key, "=", value) }
for c in "abc" { print(c) }
```

Chaque itération a son propre scope, donc une fonction créée dans la boucle se souvient de la valeur de son itération :

```
ton.array fns = []
for i in 0..3 { push(fns, () => i) }
print(fns.map(f => f()))    // [0, 1, 2]
```

Un `continue` dans un `switch` lui-même dans une boucle continue maintenant la boucle. `catch` accepte aussi le nom de l'erreur sans parenthèses : `catch err { ... }`.

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

**Fonctions fléchées** (depuis beta-2.0.0) — une façon courte d'écrire une fonction anonyme, idéale pour les callbacks. Un seul paramètre n'a pas besoin de parenthèses ; un corps qui est une expression est renvoyé automatiquement ; un corps `{ bloc }` marche comme une fonction normale.

```
ton.let double = n => n * 2
ton.let add = (a, b = 1) => a + b
ton.let greet = (name) => {
    ton.string msg = "Salut " + name
    return msg
}
print(double(21), add(1), greet("kuro"))   // 42 2 Salut kuro
```

**Spread dans les appels** — `f(...args)` passe les éléments d'un tableau comme arguments séparés : `max(...[4, 9, 2])`.

**L'opérateur pipe `|>`** — `x |> f` appelle `f(x)`, et `x |> f(a, b)` appelle `f(x, a, b)`. Il transforme des appels imbriqués en une chaîne qui se lit de gauche à droite :

```
ton.function inc(x, by = 1) { return x + by }
print(5 |> inc |> inc(10) |> str)              // 16
[1, 2, 3, 4] |> filter(n => n % 2 == 0) |> sum |> print   // 6
```

**`print` est aussi une valeur de fonction** : `["a", "b"].forEach(print)`. `print(a, b, c)` affiche ses arguments séparés par des espaces.

Les callbacks passés à `map`, `filter`, `find`, `findIndex`, `forEach`, `any`, `all`, `count` et `flatMap` reçoivent l'indice en second argument s'ils déclarent deux paramètres : `names.map((name, i) => f"{i}. {name}")`.

La récursion profonde fonctionne (l'interpréteur tourne sur une grande pile), et une récursion infinie s'arrête avec une erreur rattrapable `Maximum recursion depth (10000) exceeded` au lieu de planter.

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

**Spread** (`...`) éclate les éléments d'un autre tableau directement dans un littéral :
```
ton.array a = [1, 2, 3]
ton.array b = [0, ...a, 4]    // [0, 1, 2, 3, 4]
ton.array deux = [...a, ...a]  // [1, 2, 3, 1, 2, 3]
```
L'expression étalée doit elle-même être un tableau — étaler autre chose est une erreur d'exécution.

**Indices négatifs et tranches** (depuis beta-2.0.0) — `a[-1]` est le dernier élément. `a[début:fin]` renvoie un nouveau tableau de `début` jusqu'à `fin` (exclu) ; chaque borne peut être omise, et les deux peuvent être négatives. Les tranches marchent aussi sur les chaînes.

```
ton.array a = [10, 20, 30, 40, 50]
print(a[-1], a[1:3], a[:2], a[3:], a[-2:])    // 50 [20, 30] [10, 20] [40, 50] [40, 50]
print("TON618"[0:3], "TON618"[-3:])          // TON 618
```

**Déstructuration** — dépaquette un tableau dans des variables, `...reste` récupérant ce qui reste :

```
ton.let [premier, second, ...autres] = a
print(premier, second, autres)                // 10 20 [30, 40, 50]
```

**Syntaxe méthode** — toute fonction de tableau peut être appelée sur le tableau lui-même (voir [Syntaxe méthode](#syntaxe-méthode--lopérateur-point)) :

```
ton.array scores = [12, 7, 30, 18]
print(scores.filter(s => s > 10).map(s => s * 2).sorted())   // [24, 36, 60]
print(scores.sum(), scores.max(), scores.len(), scores.length)
```

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

**Spread** (`...`) fusionne les entrées d'un autre dict directement dans un littéral — une entrée plus tardive (étalée ou non) écrase une entrée précédente avec la même clé, en gardant la position d'origine de cette clé plutôt que de la déplacer à la fin :
```
ton.dict base = {a: 1, b: 2}
ton.dict fusion = {...base, b: 99, c: 3}   // {a: 1, b: 99, c: 3}
```
L'expression étalée doit elle-même être un dict — étaler autre chose est une erreur d'exécution.

**Accès par point** (depuis beta-2.0.0) — `d.cle` lit une clé (ou `nil`) et `d.cle = valeur` l'écrit, exactement comme `d["cle"]`. Garde les crochets pour les clés qui ne sont pas des noms valides ou qui sont calculées.

```
ton.dict cfg = {port: 8080}
cfg.host = "localhost"
cfg.port += 1
print(cfg.port, cfg.host)            // 8081 localhost
print(cfg.missing?.value)            // nil (chaînage optionnel)
```

**Déstructuration** — récupère des clés dans des variables, éventuellement renommées avec `cle: nom`, et le reste avec `...reste` :

```
ton.let {name, age: annees, ...reste} = {name: "rusher", age: 30, city: "paris"}
print(name, annees, reste)           // rusher 30 {city: paris}
```

`for cle, valeur in d { ... }` parcourt les paires ; `items(d)`, `dict(paires)`, `merge(a, b)` et `remove(d, cle)` complètent la boîte à outils (voir [Fonctions natives](#fonctions-natives)).

---

### Classes & objets

Depuis beta-2.0.0 tu peux définir tes propres types. Une classe regroupe des **champs** (les données) et des **méthodes** (des fonctions qui travaillent sur ces données). Dans une méthode, `self` est l'objet sur lequel la méthode a été appelée.

```
ton.class Point {
    ton.function init(x, y) {
        self.x = x
        self.y = y
    }
    ton.function distanceTo(other) {
        return sqrt((self.x - other.x) ** 2 + (self.y - other.y) ** 2)
    }
    ton.function toString() {
        return f"({self.x}, {self.y})"
    }
}

ton.Point a = Point(0, 0)
ton.let b = Point(3, 4)
print(a.distanceTo(b))     // 5
print(b)                   // (3, 4) — print/str/f-strings utilisent toString()
b.x = 10                   // les champs se lisent et s'écrivent avec le point
```

- **Créer un objet** : appelle la classe comme une fonction. Sa méthode `init` (le constructeur) reçoit les arguments ; sans `init`, la classe ne prend aucun argument.
- **Les déclarations de champs** dans le corps de la classe (`ton.int count = 0`, `ton.let items = []`) donnent à chaque nouvel objet sa propre valeur initiale, avant que `init` s'exécute. On peut aussi créer un champ à tout moment en assignant `self.nom = ...`.
- **Variables typées** : `ton.Point p = ...` vérifie que la valeur est un `Point` (ou une sous-classe), ou `nil`.
- **`toString()`**, si elle est définie, sert dès que l'objet est converti en texte. Sans elle, un objet s'affiche `Point{x: 3, y: 4}` ; `json(obj)` donne `{"x":3,"y":4}`.
- **Les méthodes sont des valeurs** : `ton.let f = point.toString` garde `self` lié, donc `f()` marche plus tard.
- Les objets sont passés **par référence** (comme les tableaux et les dicts) : `==` n'est vrai que pour le même objet. `copy(obj)` / `deepcopy(obj)` en font une copie.

**Héritage** — `extends` fait réutiliser (et redéfinir) tout ce que contient une classe parente ; `super.methode(...)` appelle la version du parent :

```
ton.class Animal {
    ton.string sound = "..."
    ton.function init(name) { self.name = name }
    ton.function speak() { return f"{self.name} fait {self.sound}" }
}

ton.class Dog extends Animal {
    ton.int tricks = 0
    ton.function init(name) {
        super.init(name)
        self.sound = "wouf"
    }
    ton.function learn() {
        self.tricks++
        return self              // renvoyer self permet d'enchaîner
    }
}

ton.Dog rex = Dog("Rex")
print(rex.speak())                     // Rex fait wouf
print(rex.learn().learn().tricks)      // 2
print(isinstance(rex, Animal))         // true
print(type(rex))                       // Dog
```

Les objets marchent avec le reste du langage : `"x" in point` teste un champ, `keys(obj)`/`values(obj)`/`items(obj)` listent les champs, `ton.let {x, y} = point` le déstructure, `{...point}` copie ses champs dans un dict, et `throw MonErreur("...")` le lance (le `catch` reçoit l'objet lui-même).

---

### Syntaxe méthode & l'opérateur point

Le point marche sur toutes les valeurs, et fait ce qu'on attend :

| Expression | Ce qu'elle fait |
|---|---|
| `obj.methode(args)` | Appelle la méthode d'un objet, avec `self = obj` |
| `obj.champ` | Lit un champ d'un objet |
| `dict.cle` | Lit une clé d'un dict (`nil` si absente) |
| `module.fn(args)` | Appelle une fonction d'un module importé avec `IMPORT://x as module` |
| `valeur.fn(args)` | Tout le reste : appelle la fonction `fn(valeur, args)` |

La dernière ligne s'appelle la **syntaxe d'appel uniforme** : `x.f(a, b)` est identique à `f(x, a, b)` dès que `x` n'a pas de méthode ou de clé `f`. Toute fonction — native ou la tienne — peut donc s'utiliser comme une méthode, et les appels s'enchaînent de gauche à droite au lieu de s'imbriquer :

```
// Le même calcul, deux fois :
print(join(map(filter(split("5,12,8,30", ","), s => num(s) > 6), s => "#" + s), " "))
print("5,12,8,30".split(",").filter(s => num(s) > 6).map(s => "#" + s).join(" "))
// #12 #8 #30

ton.function shout(s) { return s.upper() + "!" }
print("hello".shout())                  // HELLO!
print([3, 1, 2].sort().reverse())       // [3, 2, 1]
print("  padded ".trim().padStart(10, "."))
```

`.length` existe aussi comme propriété sur les chaînes et les tableaux (`"abc".length` vaut `3`).

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

**Lancer n'importe quelle valeur** (depuis beta-2.0.0) — `throw` accepte n'importe quelle valeur, et `catch` reçoit exactement cette valeur, donc une erreur peut transporter des données structurées. Les erreurs levées par l'interpréteur lui-même sont toujours reçues comme une chaîne (`"Error at line 3: Division by zero."`).

```
try {
    throw {code: 404, message: "introuvable"}
} catch (err) {
    print(err.code, err.message)      // 404 introuvable
}

ton.class ValidationError {
    ton.function init(field) { self.field = field }
    ton.function toString() { return "champ invalide : " + self.field }
}
try { throw ValidationError("email") } catch e { print(e) }   // champ invalide : email
```

Les erreurs des fonctions natives incluent maintenant la ligne de l'appel (`Error at line 7: sum() expects an array of numbers (found a string).`). Un `return`, `break` ou `continue` dans `finally` l'emporte sur celui en attente.

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

**Ajoutées en beta-2.0.0** — toutes peuvent aussi s'appeler avec la [syntaxe méthode](#syntaxe-méthode--lopérateur-point) (`arr.sum()`, `s.padStart(3, "0")`).

*Conversions & divers*

| Fonction | Description |
|---|---|
| `int(x, [base])` | Partie entière d'un nombre, ou analyse d'une chaîne (`int("ff", 16)` vaut `255`) |
| `float(x)` | Analyse une chaîne en nombre (erreur si invalide, contrairement à `num`) |
| `bool(x)` | `true`/`false` selon la « vérité » de la valeur |
| `fixed(x, [chiffres=2])` | Nombre vers chaîne avec un nombre fixe de décimales |
| `round(x, [chiffres])` | Arrondit, éventuellement à quelques décimales |
| `repr(x)` | Comme `str`, mais les chaînes sont entre guillemets |
| `isinstance(obj, Classe)` | Si `obj` est une instance de `Classe` ou d'une sous-classe |
| `copy(x)` / `deepcopy(x)` | Copie superficielle / récursive d'un tableau, dict ou objet |
| `clamp(x, min, max)`, `sign(x)` | Aides numériques |
| `exit([code])` | Arrête le script immédiatement |
| `print(a, b, ...)` | `print` est aussi une valeur de fonction, qui affiche ses arguments séparés par des espaces |

*Tableaux*

| Fonction | Description |
|---|---|
| `range(fin)`, `range(début, fin, [pas])` | Tableau de nombres, `fin` exclue |
| `sum(arr)`, `avg(arr)` | Somme / moyenne de nombres |
| `min(arr)`, `max(arr)` | `min`/`max` acceptent aussi un seul tableau |
| `sorted(arr, [cle])` | Copie triée ; `cle(x)` dit selon quoi trier : `sorted(mots, m => len(m))` |
| `sort(arr, [compare])` | Sur place ; `compare(a, b)` renvoie un nombre négatif (ou `true`) si `a` vient en premier |
| `forEach(arr, fn)` | Appelle `fn(item)` (ou `fn(item, indice)`) pour chaque élément |
| `findIndex(arr, fn)` | Indice du premier élément qui correspond, ou `-1` |
| `count(arr, valeurOuFn)` | Combien d'éléments égalent une valeur / vérifient un prédicat (compte aussi des sous-chaînes) |
| `flatMap(arr, fn)` | `map` puis aplatit d'un niveau |
| `unique(arr)` | Copie sans doublons (première occurrence gardée) |
| `flatten(arr, [profondeur=1])` | Aplatit des tableaux imbriqués |
| `zip(a, b, ...)` | `[[a0, b0], [a1, b1], ...]` |
| `enumerate(arr)` | `[[0, a0], [1, a1], ...]` |
| `chunk(arr, taille)` | Découpe en tableaux de `taille` éléments |
| `groupBy(arr, fn)` | Dict de tableaux, groupés selon `fn(item)` |
| `first(arr)`, `last(arr)` | Premier / dernier élément, ou `nil` |
| `insert(arr, indice, valeur)`, `removeAt(arr, indice)` | Insère / retire à une position |
| `remove(arr, valeur)` | Retire le premier élément égal ; `remove(dict, cle)` retire une clé |
| `clear(x)`, `isEmpty(x)` | Vide un tableau/dict ; teste s'il est vide |
| `lastIndexOf(x, item)` | Dernière position dans un tableau ou une chaîne, ou `-1` |
| `reduce(arr, fn)` | Sans valeur initiale, part du premier élément |

*Dictionnaires*

| Fonction | Description |
|---|---|
| `items(d)` | `[[cle, valeur], ...]` |
| `dict(paires)` | Construit un dict depuis `[[cle, valeur], ...]` |
| `merge(a, b, ...)` | Nouveau dict, les clés suivantes l'emportent |

`keys`, `values`, `items`, `has` et `len` marchent aussi sur les objets (leurs champs).

*Chaînes*

| Fonction | Description |
|---|---|
| `startsWith(s, p)`, `endsWith(s, p)` | Test de préfixe / suffixe |
| `padStart(s, largeur, [remplissage])`, `padEnd(...)` | Complète jusqu'à une largeur |
| `repeat(s, n)` | Répète une chaîne (ou un tableau) |
| `chars(s)`, `lines(s)` | Découpe en caractères / lignes |
| `capitalize(s)` | Première lettre en majuscule |
| `reverse(s)` | `reverse` inverse aussi une chaîne |
| `ord(c)`, `chr(n)` | Code d'un caractère et inverse |

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

**Espaces de noms** (depuis beta-2.0.0) — `IMPORT://nom as alias` charge un module sans ajouter ses noms au scope global : on les atteint via l'alias. Pour un module intégré, le préfixe `module_` est retiré.

```
IMPORT://mathutils as m
print(m.add(2, 3))

IMPORT://ton.strings as s
print(s.snake_case("helloWorld"))     // strings_snake_case -> s.snake_case
```

**Sous-dossiers** — `IMPORT://utils/text` charge `utils/text.ton` (à côté du script, ou dans `modules/`).

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

### Installer des modules : `ton618 install`

```
ton618 install greetings
```

Cela cherche `greetings` dans le **registre de modules TON618** — un petit index hébergé séparément qui recense les modules communautaires en stockant seulement le **nom, le lien GitHub, et la version** d'un module (jamais son code) — puis télécharge `greetings.ton` directement depuis la branche par défaut de ce dépôt GitHub vers `./modules/greetings.ton`, exactement là où `IMPORT://greetings` va déjà le chercher (voir [Créer un module TON618](#créer-un-module-ton618--guide-complet)).

L'URL du registre a une valeur par défaut intégrée dans `include/Version.hpp` (`TON618_REGISTRY_URL`) et peut être redirigée vers un autre déploiement sans recompiler, via la variable d'environnement `TON618_REGISTRY_URL` :

```
TON618_REGISTRY_URL=https://ton-registre.exemple ton618 install greetings
```

Comme `--update`, `install` n'a pas de client TLS intégré à lui — il délègue à `curl` (ou `wget`/PowerShell) à la fois pour l'interrogation du registre et le téléchargement GitHub, comme toute autre opération réseau du CLI lui-même (contrairement à `ton.requests`, qui est un client HTTP séparé et bac-à-sable exposé aux scripts — voir [Modules système intégrés](#modules-système-intégrés)).

Un auteur de module n'a besoin de rien de spécial pour être installable ainsi : juste un dépôt GitHub public avec `<nom>.ton` à sa racine, enregistré une fois dans le registre avec le lien de ce dépôt et une chaîne de version.

Pour retirer un module récupéré ainsi, utilise `ton618 uninstall <module>` — ça supprime juste `./modules/<module>.ton` (exactement le fichier créé par `install`), sans interroger le registre :

```
ton618 uninstall greetings
```

(À ne pas confondre avec `ton618 --uninstall-ton`, qui désinstalle l'interpréteur `ton618` lui-même — voir [Installer & compiler l'interpréteur](#installer--compiler-linterpréteur). Cette commande vide aussi tout le dossier `./modules/` du répertoire courant, donc `ton618 uninstall <module>` et `--uninstall-ton` ne suppriment pas la même quantité : un seul module contre tous les modules récupérés dans ce projet.)

---

### Modules système intégrés

TON618 est livré avec douze **modules système intégrés**. Ce ne sont pas des fichiers sur le disque — ils vivent dans l'interpréteur — mais tu les importes quand même avec `IMPORT://ton.<nom>` avant de les utiliser, exactement comme un module utilisateur :

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
IMPORT://ton.tensor
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

#### ton.tensor — petits tableaux numériques 1D/2D pour du ML basique

Un **tenseur** est un `ton.dict` avec deux champs — `{shape: [...], data: [...]}` (`data` est plat, ordre ligne par ligne) — donc il s'affiche, se `str()`-ise et se `json()`-ise comme n'importe quel dict. Les fonctions ci-dessous implémentent les opérations élément-par-élément/matmul/réduction en vraies boucles C++ (sans repasser par l'AST), un vrai gain de vitesse par rapport à la même boucle écrite à la main en TON618 — mais lis **les limites honnêtes** ci-dessous avant de construire quoi que ce soit d'ambitieux dessus. Cette même convention `{shape, data}` est celle sur laquelle le module communautaire `atome` (un simple fichier `.ton`, pas une partie de l'interpréteur — voir [`ton618 install atome`](#installer-des-modules--ton618-install)) construit sa couche d'autodiff/entraînement.

| Fonction | Description |
|---|---|
| `tensor_zeros(shape)` / `tensor_ones(shape)` / `tensor_full(shape, valeur)` | Un nouveau tenseur de la forme donnée (1 ou 2 dimensions), rempli |
| `tensor_random(shape, [lo=0, hi=1])` | Un nouveau tenseur rempli de valeurs aléatoires uniformes dans `[lo, hi)` (utilise le même RNG que `random()`/`ton.random` — seed avec `random_seed()` pour des runs reproductibles) |
| `tensor_from_array(arr)` | Construit un tenseur depuis un tableau plat (1D) ou un tableau de tableaux de même longueur (2D) |
| `tensor_to_array(t)` | L'inverse : un `ton.array` classique (imbriqué pour du 2D) |
| `tensor_shape(t)` / `tensor_size(t)` | Le tableau de forme / le nombre total d'éléments |
| `tensor_clone(t)` | Une copie profonde — `t` et ses champs dict/array sont sinon des références partagées comme n'importe quel dict, donc muter un alias mute tous les autres |
| `tensor_reshape(t, nouvelleForme)` | Un nouveau tenseur vue des mêmes données avec une forme différente (le nombre d'éléments doit correspondre) |
| `tensor_get(t, indices)` / `tensor_set(t, indices, valeur)` | Lit/écrit un élément — `indices` est `[i]` en 1D, `[ligne, colonne]` en 2D |
| `tensor_add(a, b)` / `tensor_sub(a, b)` / `tensor_mul(a, b)` / `tensor_div(a, b)` | Élément par élément ; un côté peut être un nombre simple (diffusion scalaire), mais deux tenseurs doivent avoir la *même* forme — il n'y a pas de diffusion façon NumPy entre tenseurs de formes différentes |
| `tensor_add_bias(mat, bias)` | Ajoute un biais 1D à chaque ligne d'une matrice 2D — la seule exception délibérée à "pas de diffusion", exactement ce dont une couche dense/linéaire a besoin |
| `tensor_matmul(a, b)` | Multiplication matricielle 2D (`a` en m×k, `b` en k×n, résultat en m×n) |
| `tensor_transpose(t)` | Transposition 2D |
| `tensor_sum(t)` / `tensor_mean(t)` / `tensor_max(t)` / `tensor_min(t)` | Réduit tous les éléments à un seul nombre (pas de réduction par axe) |
| `tensor_relu(t)` / `tensor_sigmoid(t)` / `tensor_tanh(t)` / `tensor_exp(t)` / `tensor_log(t)` | Fonctions d'activation/maths élément par élément |
| `tensor_softmax(t)` | Softmax sur tout le vecteur (1D) ou ligne par ligne (2D) |
| `tensor_relu_grad(entree)` / `tensor_sigmoid_grad(sortie)` / `tensor_tanh_grad(sortie)` | Dérivées élément par élément pour de la rétropropagation faite à la main — `sigmoid_grad`/`tanh_grad` prennent la *sortie* de cette fonction (moins cher à exprimer ainsi), tandis que `relu_grad` prend son *entrée* |
| `tensor_map(t, fn)` | Applique une `ton.function`/expression de fonction à chaque élément — plus lent que les natives ci-dessus (un appel interpréteur par élément) mais marche avec n'importe quelle fonction scalaire personnalisée |
| `tensor_argmax(t)` | L'index de l'élément le plus grand (1D), ou un index par ligne (2D) — l'opération "quelle classe ça a prédit" |
| `tensor_conv1d(signal, noyau, [pas=1])` | Convolution 1D "valid" (corrélation croisée) sur des tenseurs 1D à un seul canal : `out[i] = somme_j signal[i*pas+j] * noyau[j]` — voir le module communautaire `atome` pour comment des couches Conv1D multi-canaux (et leur gradient) sont construites à partir de cette primitive |
| `tensor_to_bytes(t)` | Empaquette les données du tenseur en octets bruts IEEE-754 float64 (endianness native), renvoyés sous forme de `ton.string` — combiné à `writefile` (qui écrit les octets *exacts* d'une chaîne), ça permet à un module d'écrire de vrais fichiers binaires plutôt que du JSON texte. Pas portable entre machines d'endianness différente |
| `tensor_from_bytes(octets, forme)` | L'inverse de `tensor_to_bytes` — décode une chaîne d'octets bruts (ex. depuis `readfile` + `substring`) en un tenseur de la forme donnée ; erreur si la longueur en octets ne correspond pas au nombre d'éléments de `forme` × 8 |

```
IMPORT://ton.tensor

ton.dict a = tensor_from_array([[1, 2], [3, 4]])
ton.dict b = tensor_from_array([[5, 6], [7, 8]])
print(tensor_to_array(tensor_matmul(a, b)))   // [[19, 22], [43, 50]]
print(tensor_to_array(tensor_relu(tensor_from_array([-1, 0, 2]))))  // [0, 0, 2]
```

**Limites honnêtes** — à lire avant de construire quoi que ce soit d'ambitieux sur `ton.tensor` :
- **CPU uniquement.** Il n'y a aucun backend de calcul GPU (pas de bindings CUDA/OpenCL) nulle part dans cet interpréteur, et ça ne changera pas — le "support GPU" n'est pas quelque chose qu'un module `.ton` peut ajouter tout seul.
- **Formes 1D et 2D seulement** — pas de tenseurs 3D+ (donc pas de batchs d'images, pas de tenseurs façon têtes d'attention, etc. sans les astuces d'aplatissement/reshape faites toi-même).
- **Seulement une diffusion scalaire** (plus l'exception explicite `tensor_add_bias` ci-dessus) — `tenseur + tenseur-de-forme-différente` (diffusion façon NumPy) ne marche pas.
- **Pas d'autodiff intégré.** `ton.tensor` te donne les opérations forward et les helpers `_grad` correspondants pour construire la rétropropagation *à la main* — il n'y a pas de suivi automatique du graphe de calcul. Voir le module communautaire `atome` ([`ton618 install atome`](#installer-des-modules--ton618-install)) pour une couche d'autodiff/entraînement construite à partir de zéro sur cette base.
- **Pas du tout compétitif avec une vraie bibliothèque numérique.** Ce sont toujours des boucles C++ sur un tableau de `Value` typé dynamiquement — un vrai travail pour un petit langage, mais loin des performances BLAS/cuDNN. Bien pour apprendre, des modèles jouets, et de petites expériences ; pas pour entraîner quoi que ce soit à échelle réelle.

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

### L'IDE & les extensions d'éditeur

**TON618 IDE** — un petit éditeur fait pour TON618, pour **Windows, Linux, macOS et Android**. Il est construit avec [Tauri](https://tauri.app), qui utilise le moteur web du système au lieu d'embarquer un navigateur : toute l'application, interpréteur compris, pèse environ 2 Mo.

- Un éditeur avec la coloration TON618, l'indentation automatique, les paires de crochets, les commentaires avec `Ctrl+/` et les erreurs de syntaxe signalées pendant la frappe.
- **▶ Run** (`Ctrl+Entrée`) : la sortie s'affiche dessous ; clique sur la ligne d'une erreur pour y aller. **■ Stop** arrête une boucle infinie.
- Deux façons d'exécuter : le **moteur intégré** (partout, Android compris — le même moteur que le playground) ou l'**interpréteur ton618** livré avec l'application desktop (toute la bibliothèque standard, les fichiers, le réseau, le serveur HTTP, et `input()` via la zone de saisie).
- Plusieurs fichiers : `IMPORT://nom` charge un autre fichier du projet. Sur desktop, **📂** ouvre un vrai dossier ; sinon les fichiers sont gardés dans l'espace de travail de l'application.

**Extension VS Code** — coloration (y compris dans les f-strings), snippets (`fn`, `class`, `for`, `try`...), **Run File** (▶ dans la barre de titre, `Ctrl+Alt+R`), **Open the REPL**, et erreurs de syntaxe soulignées à la sauvegarde (via `ton618 --check`). Réglages : `ton618.interpreterPath`, `ton618.checkOnSave`, `ton618.saveBeforeRun`.

**Plugin Acode** (l'éditeur de code pour Android) — coloration, un bouton **▶** et une commande *TON618: Run* avec un panneau de sortie, erreurs de syntaxe pendant la frappe, et `IMPORT://` depuis le dossier du fichier. Il exécute le code avec le moteur intégré, donc il marche hors ligne sans rien installer d'autre ; pour toute la bibliothèque standard, utilise le vrai interpréteur dans Termux.

**Où les trouver** — chaque [release](https://github.com/kurodaki-dev/ton618/releases/latest) contient :

| | Fichier |
|---|---|
| IDE pour Windows | `TON618-IDE-*-windows-x64-setup.exe` (ou `.msi`) |
| IDE pour Linux | `TON618-IDE-*-linux-x64.AppImage` (toutes distributions) ou `.deb` |
| IDE pour macOS | `TON618-IDE-*-macos-universal.dmg` (Intel et Apple Silicon) |
| IDE pour Android | `TON618-IDE-*-android.apk` |
| VS Code | `ton618-vscode-*.vsix` — vue Extensions → ⋯ → *Install from VSIX…* |
| Acode | `ton618-acode-plugin-*.zip` — Réglages → Plugins → ⋮ → *Install from local* |

Les applications ne sont pas signées : sur Windows clique *Informations complémentaires → Exécuter quand même* la première fois, sur macOS clic droit sur l'application → *Ouvrir*.

**Comment les releases sont faites** — `.github/workflows/release.yml` compile tout sur GitHub Actions (l'interpréteur pour quatre plateformes, l'IDE pour quatre plateformes, les deux extensions) et publie une release **automatiquement dès que `TON618_VERSION` dans `include/Version.hpp` change** sur `main`. Pour publier, augmente la version et pousse ; *Run workflow* recompile la version actuelle. Les sources sont dans `ide/`, `editors/vscode/` et `editors/acode/`.

`ton618 --check fichier.ton` (utilisé par les éditeurs) vérifie la syntaxe d'un fichier sans l'exécuter.

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
│                              --debug / --break / --version / --update / --uninstall-ton / install / uninstall)
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

**Fonctionnement interne depuis beta-2.0.0**, pour les contributeurs :

- `break`/`continue`/`return` ne sont plus des exceptions C++ : `Interpreter::execute()` renvoie un `Signal` (`NONE`, `BREAK`, `CONTINUE`, `RETURN`) que les boucles et les appels vérifient. Seules les vraies erreurs utilisent des exceptions (`LineError` pour les erreurs de l'interpréteur, `ThrownValue` pour `throw`).
- Les noms sont internés en entiers (`Symbols` dans `Environment.hpp`) et les scopes sont de petits vecteurs plats, avec un index haché seulement pour les gros scopes comme les globales. Un bloc qui ne déclare rien n'a pas de scope du tout.
- Les classes sont des `ClassObj` et les objets des `InstanceObj` (`Value.hpp`) ; les appels de méthode passent par `Interpreter::callMethod`, qui implémente aussi la syntaxe d'appel uniforme.
- La bibliothèque standard ajoutée en 2.0 est dans `src/Stdlib.cpp` (`defineNativesV2()`).
- `main.cpp` exécute les scripts sur un thread avec une pile de 512 Mo, donc une récursion profonde atteint la limite de l'interpréteur (10 000 appels) au lieu de planter.
- `tests/*.ton` avec `tests/*.expected` forment la suite de non-régression : `make test` (aussi lancé par la CI).

Ensemble, ces changements rendent les scripts typiques environ 6 fois plus rapides qu'en beta-1.x (un `fib(25)` plus une boucle de 1 000 000 d'itérations : 2,0 s → 0,3 s).

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

Le site de documentation (https://c-ton618.duckdns.org/) est construit avec **[Claide](https://github.com/kurodaki-dev/Claide)**, un module Python pour faire des sites web, directement à partir de ce fichier : `website/app.py` lit `DOCUMENTATION.md`, transforme chaque section `###` de la moitié anglaise et de la moitié française en une page (avec une barre latérale, une recherche, des liens « sur cette page », un bouton de langue et des boutons « Exécuter » qui ouvrent un extrait dans le playground), et exporte tout le site en HTML statique. Le playground (`exemples/playground.html` + `ton618-lite.js`), la page des skills IA, le logo et `exemples/CNAME` sont publiés à côté.

Pour le prévisualiser en local :
```bash
pip install claide-web
python website/app.py          # http://127.0.0.1:8000, recharge à chaque sauvegarde
python website/app.py build    # export statique dans website/_site/ (ou : make site)
```

`.github/workflows/pages.yml` installe Claide, construit le site et le déploie à chaque push sur `main` qui touche `DOCUMENTATION.md`, `website/`, `exemples/` ou le numéro de version — modifier ce fichier suffit à mettre le site à jour. Avec un domaine personnalisé (`exemples/CNAME`) le site est servi à la racine du domaine ; sans, depuis `https://<utilisateur>.github.io/ton618/`.

Configuration unique :
1. **GitHub → ce dépôt → Settings → Pages.**
2. Sous **Build and deployment**, mets **Source** sur **GitHub Actions** (pas "Deploy from a branch").

`exemples/documentation.html` est l'ancienne version de ce document sur une seule page, toujours publiée à `/documentation.html` et régénérée avec `make docs`.

**Utiliser un domaine personnalisé** (par ex. un sous-domaine [DuckDNS](https://www.duckdns.org) gratuit au lieu de `github.io`) : pas besoin de serveur à toi — c'est GitHub qui héberge le site.

1. Sur DuckDNS, fais pointer l'IP de ton sous-domaine vers une des adresses fixes de GitHub Pages au lieu d'un serveur à toi : `185.199.108.153`, `185.199.109.153`, `185.199.110.153`, ou `185.199.111.153` (DuckDNS n'accepte qu'une IP par sous-domaine, choisis-en une seule). Si ce sous-domaine est aussi mis à jour par un script de DNS dynamique quelque part (cron sur un VPS, un routeur...), ce script l'écrasera à sa prochaine exécution — utilise un sous-domaine que rien d'autre ne met à jour.
2. `exemples/CNAME` dans ce dépôt contient déjà le domaine de la doc ; le workflow le publie tel quel, ce qui indique à GitHub Pages de servir ce domaine.
3. **GitHub → ce dépôt → Settings → Pages** devrait alors afficher le domaine sous **Custom domain** (la propagation DNS peut prendre quelques minutes) — une fois vérifié, coche **Enforce HTTPS**.

---

### Limitations connues

- `serve()` est mono-thread et bloquant — une requête à la fois, pas de concurrence
- `ton.int` et `ton.float` sont le même type de nombre interne (pas de stockage entier forcé)
- Les clés de dict sont toujours des chaînes (pas de clés numériques ou structurées)
- Les classes n'ont ni champs statiques, ni membres privés, ni surcharge d'opérateurs (`==` compare les objets par identité)
- `ton.requests` ne parle que `http://` en clair — pas de TLS/`https://`, pas de redirections, pas de transfert chunked
- Le playground du navigateur (`ton618-lite.js`) n'implémente pas `ton.encoding`/`ton.regex`/`ton.path`/`ton.tensor`, et peut afficher des flottants avec plus de décimales que le vrai interpréteur (une différence de formatage JavaScript vs C++, pas de valeur)

---

### Changelog

**beta-2.0.2** — l'icône Android, et un nouveau site de documentation :

- **Icône de l'application Android corrigée** : Android rogne les bords des icônes, donc le nouveau logo apparaissait coupé à son centre sur fond blanc. L'icône montre maintenant tout le logo sur fond noir, dans la zone qu'Android affiche toujours.
- **Le site de documentation est réécrit** avec Claide : un menu latéral pleine hauteur avec recherche et parties repliables, un design plat et monochrome aux couleurs du logo, sans effet de lumière.

**beta-2.0.1** — le nouveau logo :

- **Nouveau logo TON618** partout : le site, le playground, les icônes de l'IDE, les icônes VS Code et Acode. Il porte sa mention de copyright (© 2026 kurodaki-dev, voir `LOGO-LICENSE.md`).
- **Les couleurs suivent le logo** : l'accent orange est remplacé par le blanc et le gris (`#595959`) du logo sur fond noir — sur le site, dans les exemples de code, le playground, l'IDE et le plugin Acode.

**beta-2.0.0** — une grande modernisation du langage, et un interpréteur 6 fois plus rapide :

*Objets :*
- **Classes** : `ton.class Nom extends Parent { ... }` avec champs, constructeur `init`, méthodes, `self`, `super`, `toString()`, et variables d'objet typées (`ton.Point p = Point(1, 2)`) — voir [Classes & objets](#classes--objets).
- **L'opérateur point partout** : `obj.champ`, `obj.methode()`, `dict.cle`, et la syntaxe d'appel uniforme (`"hi".upper()`, `arr.filter(...).map(...)`) — voir [Syntaxe méthode](#syntaxe-méthode--lopérateur-point).

*Nouvelle syntaxe :*
- **Fonctions fléchées** `x => x * 2`, `(a, b) => { ... }`, et le **pipe** `x |> f(y)`.
- **f-strings** `f"Salut {name} !"`, **chaînes brutes** entre backticks, échappements `\u{...}`.
- **Intervalles** `0..10` / `1..=10`, **tranches** `a[1:3]`, **indices négatifs** `a[-1]`.
- **`ton.let` / `ton.var` / `ton.const`**, et **déstructuration** `ton.let [a, ...reste] = arr`, `ton.let {x, y: alias} = obj`.
- `for k, v in dict`, `for i, x in tableau`, `for c in "texte"`, variables de boucle sans `ton.`, un nouveau scope à chaque itération.
- **Chaînage optionnel** `a?.b`, `a?.[i]`, `f?.()` ; **`??=`** ; **`**`** (puissance) ; **spread dans les appels** `f(...args)`.
- Littéraux numériques `1_000_000`, `1e9`, `0xFF`, `0b1010`, `0o755`.
- `throw` de n'importe quelle valeur (dicts, objets) ; `catch err` sans parenthèses.
- Espaces de noms `IMPORT://module as alias` et `IMPORT://dossier/module`.
- `print(a, b, c)` avec plusieurs arguments ; `print`, `int`, `float`, `bool`, `dict` utilisables comme valeurs de fonction.

*Bibliothèque standard :* ~45 nouvelles fonctions — `sum`, `avg`, `sorted`, `sort` avec comparateur, `range`, `zip`, `enumerate`, `unique`, `flatten`, `chunk`, `groupBy`, `flatMap`, `count`, `findIndex`, `forEach`, `items`, `dict`, `merge`, `remove`, `insert`, `removeAt`, `padStart`, `padEnd`, `startsWith`, `endsWith`, `repeat`, `chars`, `lines`, `capitalize`, `int`, `float`, `fixed`, `isinstance`, `copy`, `deepcopy`, `exit`... — voir [Fonctions natives](#fonctions-natives).

*Outils :*
- **REPL** : `ton618` sans argument ; **`ton618 -e "code"`** pour une ligne.
- **Meilleures erreurs** : la ligne de code, une trace à travers les appels de fonction, « did you mean ...? » pour les noms mal orthographiés, et les numéros de ligne sur les erreurs des fonctions natives.
- **Limite de récursion rattrapable** (10 000 appels) au lieu d'un plantage.
- Une suite de tests de non-régression (`tests/`, `make test`, lancée par la CI).
- **TON618 IDE** pour Windows, Linux, macOS et Android, une **extension VS Code** et un **plugin Acode** — voir [L'IDE & les extensions d'éditeur](#lide--les-extensions-déditeur). `ton618 --check` vérifie la syntaxe d'un fichier sans l'exécuter.
- **Releases automatiques** : tout est compilé par GitHub Actions et publié quand `TON618_VERSION` change ; l'interpréteur existe maintenant aussi pour **macOS** (binaire universel), pris en charge par `install.sh`.
- Le logo porte une mention de copyright (voir `LOGO-LICENSE.md`) : il n'est pas couvert par la licence Apache 2.0.

*Changements de comportement :*
- Les nombres s'affichent sous leur forme exacte la plus courte : `3.14` au lieu de `3.140000`, `0.5` au lieu de `0.500000` (aussi dans `str()`, `json()` et la concaténation).
- Un `continue` dans un `switch` dans une boucle continue maintenant la boucle (il était avalé par le switch).
- Les erreurs des fonctions natives rattrapées par `catch` commencent maintenant par `Error at line N:` comme toutes les autres erreurs d'exécution.
- Une closure créée dans une boucle `for ... in` capture la variable de son itération (elle voyait la dernière valeur).

*Performances :* environ 6 fois plus rapide sur du code typique — les noms sont résolus via des symboles internés, les scopes sont des vecteurs plats, le contrôle de flux n'utilise plus d'exceptions C++, et les blocs qui ne déclarent rien n'allouent pas de scope.

**beta-1.0.10** — `tensor_to_bytes`/`tensor_from_bytes`, de vrais fichiers binaires :

- **`tensor_to_bytes(t)` / `tensor_from_bytes(octets, forme)`** : empaquette/décode les données d'un tenseur en octets bruts IEEE-754 float64. Combiné à `writefile`/`readfile` (désormais ouverts en mode binaire, pour que les octets fassent l'aller-retour exactement même sous Windows, pas seulement Linux/Mac), ça permet à un module `.ton` d'écrire de vrais fichiers binaires — plus besoin d'encoder les nombres en JSON texte et de perdre en précision à cause de l'arrondi à ~6 chiffres significatifs de `json_stringify`. Les fichiers modèles `.atm` du module communautaire `atome` utilisent désormais ça plutôt que du JSON.

**beta-1.0.9** — `--uninstall-ton` supprime aussi `./modules/` :

- **Changement de comportement** : `ton618 --uninstall-ton` supprime désormais aussi tout le dossier `./modules/` du répertoire courant (tous les modules récupérés là avec `ton618 install <module>`), en plus de supprimer le binaire et son entrée PATH. Les scripts autonomes `scripts/uninstall.sh`/`uninstall.ps1` font pareil. Pour ne supprimer qu'un seul module, utilise plutôt `ton618 uninstall <module>` — voir [Installer des modules](#installer-des-modules--ton618-install).

**beta-1.0.8** — `ton618 install`/`uninstall` normalisent le nom du module en minuscules :

- **Correction de bug** : `ton618 install <module>` met désormais le nom du module en minuscules avant de construire l'URL brute GitHub et le chemin local `./modules/<nom>.ton` (la recherche dans le registre, elle, utilise toujours le nom exactement tel que tapé). Tous les modules existants suivent la convention du nom de fichier en minuscules (`atome.ton`, etc.), et les URLs `raw.githubusercontent.com` sont sensibles à la casse — une entrée de registre publiée avec une autre casse (par exemple `Atome`) provoquait donc un 404 au lieu de trouver le vrai fichier, en minuscules, à la racine du dépôt. `ton618 uninstall <module>` a été corrigé de la même façon, pour continuer à cibler le fichier réellement écrit par `install`.

**beta-1.0.7** — `tensor_argmax` et `tensor_conv1d` :

- **`tensor_argmax(t)`** : l'index de l'élément le plus grand (ou un par ligne pour un tenseur 2D).
- **`tensor_conv1d(signal, noyau, [pas])`** : convolution 1D (corrélation croisée) sur des tenseurs à un seul canal — la primitive sur laquelle le module communautaire `atome` construit sa couche Conv1D (multi-canaux, suivie par l'autodiff), vérifiée par un test de gradient numérique.

**beta-1.0.6** — `ton.tensor`, un module natif pour du ML basique :

- **Nouveau module intégré `ton.tensor`** : tableaux numériques 1D/2D avec opérations élément par élément, multiplication matricielle, transposition, réductions, activations courantes (relu/sigmoid/tanh/softmax) et leurs gradients, plus conversion tableau/tenseur — implémenté en vraies boucles C++ pour un vrai gain de vitesse par rapport à la même boucle écrite à la main en TON618. CPU uniquement, pas d'autodiff intégré — voir [ton.tensor](#modules-système-intégrés) pour la liste complète des fonctions et ses limites honnêtes. Le module communautaire `atome` (`ton618 install atome`, une fois publié) est construit dessus et a gagné 5 à 16x en vitesse en migrant vers lui, sans aucun changement à sa propre API publique.

**beta-1.0.5** — opérateurs binaires (bits) et spread :

- **Opérateurs binaires** : `&`, `|`, `^`, `~`, `<<`, `>>`, plus leurs formes d'assignation composée (`&=`, `|=`, `^=`, `<<=`, `>>=`). Les opérandes sont tronqués en entiers 64 bits, l'opération appliquée, puis reconvertis — voir [Opérateurs](#opérateurs).
- **Spread (`...`) dans les littéraux de tableau** : `[...a, x, ...b]` éclate les éléments d'autres tableaux directement dedans — voir [Tableaux](#tableaux).
- **Spread (`...`) dans les littéraux de dict** : `{...d1, cle: val, ...d2}` fusionne les entrées d'autres dicts, une entrée plus tardive écrasant une entrée précédente avec la même clé (à sa position d'origine) — voir [Dictionnaires](#dictionnaires).

**beta-1.0.4** — `ton618 uninstall <module>`, et un nom plus clair pour l'ancien `--uninstall` :

- **`ton618 --uninstall`** (qui désinstallait l'interpréteur `ton618` lui-même) est renommé en **`ton618 --uninstall-ton`**, pour laisser la place à la commande ci-dessous et éviter de confondre les deux.
- **`ton618 uninstall <module>`** : supprime `./modules/<module>.ton` — le pendant de `ton618 install <module>` (voir [Installer des modules](#installer-des-modules--ton618-install)).

**beta-1.0.3** — nouveaux opérateurs, paramètres de fonction flexibles, et `finally` :

- **Paramètres par défaut** : `function saluer(nom, salutation = "Bonjour") { ... }` — le défaut est évalué au moment de l'appel, dans l'ordre, donc un défaut plus tardif peut référencer un paramètre précédent.
- **Paramètres rest** : `function somme(...nombres) { ... }` collecte les arguments en trop dans un tableau ; peut être mélangé avec des paramètres fixes tant que `...` est en dernier. Les messages d'erreur d'arité s'adaptent maintenant (`expects between 1 and 2 argument(s)`, `expects at least 1 argument(s)`).
- **`??` (opérateur de coalescence nulle)** : `valeur ?? defaut` retourne `valeur` sauf si elle vaut `nil`, auquel cas il retourne `defaut` — le défaut n'est jamais évalué autrement (court-circuit). Voir [Opérateurs](#opérateurs).
- **`in` (opérateur d'appartenance)** : `valeur in tableau`, `valeur in dict` (teste les clés), `valeur in chaine` (test de sous-chaîne). Voir [Opérateurs](#opérateurs).
- **`finally`** : `try`/`catch` gagne un bloc `finally` qui s'exécute toujours — en cas de succès, d'erreur attrapée, ou à travers un `return` — et `catch` est maintenant optionnel tant que `finally` est présent (`try { ... } finally { ... }`). Voir [Gestion des erreurs](#gestion-des-erreurs--try--catch--throw--finally).
- **`ton618 install <module>`** : récupère les métadonnées d'un module depuis le registre de modules TON618 et télécharge son code directement depuis GitHub dans `./modules/`. Voir [Installer des modules](#installer-des-modules--ton618-install).
- **Un nouveau skill IA pour créer des modules TON618** (par opposition à écrire des scripts), en plus du skill généraliste existant — voir `exemples/skills/ton618-module-author/`.

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
