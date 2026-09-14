---
name: ton618
description: Write, run, and debug TON618 (.ton) scripts — a small interpreted language (C++ runtime) with a distinctive "ton." prefix, built-in HTTP server, and a compact standard library. Use this whenever asked to write, edit, explain, or debug .ton files or TON618 code.
---

# TON618 language reference for AI coding agents

TON618 is a small interpreted language (`.ton` files), run with the `ton618` binary. No compilation step. Full docs: `DOCUMENTATION.md` in the project repo, or `https://github.com/kurodaki-dev/ton618`. This file is a dense reference — read it fully before writing TON618 code; the rest of this skill has more detail than you'll usually need in one script.

## Running a script

```bash
ton618 script.ton                # run it
ton618 script.ton --debug        # step debugger from line 1
ton618 script.ton --break=12     # step debugger, breakpoint at line 12
ton618 --version                 # version, platform, arch
```

## The one rule that shapes everything: the `ton.` prefix

- **Declaring** a variable or function always uses `ton.<type> name = value` or `ton.function name(...) { ... }`.
- **Referring** to something already declared (reading, reassigning, calling, naming a function parameter) accepts *either* `ton.name` or bare `name` — both resolve to the same thing. This reference uses `ton.` for declarations and top-level statements, and bare names for loop counters and function parameters, matching the style of every example below.
- A bare assignment (`x = 5`) only works if `x` was already declared — it never introduces a new variable. To declare, you always need the full `ton.<type> name = value` form.
- `ton.` is never a namespace or object access — `ton.foo` is just how the parser spells "the variable/function named foo".

## Types

| Type | Declare | Notes |
|---|---|---|
| number | `ton.int` / `ton.float` | Same internal double for both — no integer-only enforcement |
| string | `ton.string` | Immutable; `s[i]` reads a character (1-char string), can't be assigned |
| bool | `ton.bool` | `true` / `false` |
| array | `ton.array` | `[1, 2, 3]`; 0-indexed; out-of-bounds read/write is a runtime error |
| dict | `ton.dict` | `{key: value, ...}`; keys are always strings; missing-key read is `nil`, not an error |
| html | `ton.html` | String-like; `+`-concatenating with anything keeps the result html; a plain string literal assigned to a `ton.html` var auto-coerces; a route handler returning html is served as `text/html` |
| function | `ton.function` | First-class: can be stored, passed, returned |

`type(x)` returns `"int/float"`, `"string"`, `"html"`, `"bool"`, `"array"`, `"dict"`, `"nil"`, or `"function"`.

## Comments

```
// line comment
/* block
   comment */
```

## Building strings — no interpolation, use `format()`

There is **no** `${...}` or f-string syntax. Build strings with `+` concatenation or, for multiple substitutions, `format(template, ...args)`:

```
ton.string name = "kuro"
ton.int age = 5
print(format("Hello {}, you'll be {} next year!", name, age + 1))
// Hello kuro, you'll be 6 next year!
```

`{}` placeholders are filled left to right; an argument is converted with the same rules as `str()`. Extra `{}` with no argument are left as `{}` literally; extra arguments are ignored.

## Operators

- Arithmetic: `+ - * /  %` — `/` and `%` throw "Division by zero" on a zero right-hand side (never silently `inf`/`NaN`).
- `+` on any string/html operand concatenates (converting the other side to text); otherwise both sides must be numbers.
- Comparison `< <= > >=`: **both sides must be two numbers, or two strings/html** — comparing anything else (arrays, dicts, bools, mixed types) is a runtime error. No implicit coercion.
- Equality `== !=`: structural. Numbers/bools/nil by value; a string and an html with the same text are equal; arrays/dicts compare recursively element-by-element (`[1,2] == [1,2]` is `true`). Functions never compare equal to anything.
- Logical: `&& / and`, `|| / or`, `!` — `and`/`or` are exact keyword synonyms for `&&`/`||`.
- Nil-coalescing `??`: `left ?? right` — returns `left` unless it's `nil`, else evaluates and returns `right`. Right side is never evaluated when `left` isn't `nil` (short-circuits). Handy for dict lookups: `d["missing"] ?? "default"`.
- Membership `in`: `v in array` (element match, `==` rules), `v in dict` (tests **keys**), `v in string` (substring). Right side must be array/dict/string — anything else throws.
- Bitwise: `& | ^ ~ << >>` — both sides truncated to 64-bit integers, operated on, converted back to a plain number (no separate int type). Both operands must be numbers or it throws. `&`/`|`/`^` share one precedence level, looser than `==`, tighter than `&&`.
- Assignment: `= += -= *= /= %= &= |= ^= <<= >>=`; postfix `++ --`.
- Ternary: `cond ? a : b`.

## Control flow

```
if x < 10 {
    print("small")
} else if x < 100 {
    print("medium")
} else {
    print("big")
}

ton.int i = 0
while i < 5 { print(i); i++ }

for ton.int i = 0; i < 5; i++ { print(i) }        // C-style

ton.array names = ["a", "b"]
for ton.n in names { print(n) }                    // iterates array elements

ton.dict d = {a: 1, b: 2}
for ton.k in d { print(k + "=" + str(d[k])) }        // iterates dict KEYS

switch status {                                       // no fallthrough, ever
    case 200, 201: { print("ok") }
    case 404: { print("missing") }
    default: { print("other") }
}
```

`break`/`continue` work in `while`/`for`/`for...in`. Parentheses around conditions are optional. Every block needs `{ }` — there is no single-statement-without-braces form.

## Functions

```
ton.function add(a, b) { return a + b }
print(add(2, 3))   // 5

ton.function greet(name, greeting = "Hello") { return greeting + ", " + name + "!" }
print(greet("kuro"))              // Hello, kuro!
print(greet("kuro", "Salut"))     // Salut, kuro!

ton.function sum(...nums) {
    ton.int total = 0
    for ton.n in nums { total += n }
    return total
}
print(sum(1, 2, 3))   // 6
print(sum())            // 0
```

- **Default parameters**: `name = expr` makes a parameter optional; defaults are evaluated at call time, left to right, so a later default can reference an earlier parameter (`function pair(a, b = a + 1) {...}`).
- **Rest parameters**: `...name` as the *last* parameter collects extra args into an array. Can be mixed with fixed params before it.
- **Arity is enforced but flexible now**: with defaults/rest, the error message adapts — `expects 2 argument(s) but got 1.` / `expects between 1 and 2 argument(s) but got 0.` / `expects at least 1 argument(s) but got 0.`
- Function expressions (anonymous, for callbacks): `ton.function(n) { return n * 2 }` — optionally with a debug-only name: `ton.function label(n) { ... }`. Function expressions support defaults/rest too.
- Closures capture the enclosing scope normally.

## Arrays

```
ton.array a = [1, 2, 3]
push(a, 4)        // [1,2,3,4] — mutates in place
a[0] = 10
print(len(a))
```
Natives: `push pop shift unshift slice join sort reverse contains indexOf map filter reduce find any all`. `map`/`filter`/`reduce`/`find`/`any`/`all` take a `ton.function`/function-expression callback.

Spread: `[...a, x, ...b]` splices other arrays' elements in place. The spread expression must be an array or it throws.

## Dicts

```
ton.dict d = {name: "kuro", age: 22}
d["age"] = 23
d["city"] = "paris"    // adds a new key
print(keys(d))          // [name, age, city], insertion order
print(has(d, "age"))     // true
print(d["missing"])       // nil, not an error
```
Bare-identifier keys (`name: ...`) or string-literal keys (`"first name": ...`) for non-identifier names.

Spread: `{...d1, key: val, ...d2}` merges other dicts' entries in place — a later entry (spread or not) overrides an earlier same-key one, keeping its original position. The spread expression must be a dict or it throws.

## Error handling

```
try {
    throw "custom message"
} catch (ton.err) {
    print("caught: " + err)   // err is always a string
}
```
Any runtime error (div by zero, wrong arity, index out of bounds, bad comparison, explicit `throw`, ...) is catchable this way.

`finally` adds a block that always runs — success, caught error, or a `return` inside `try` all still run it:

```
ton.function withFinally() {
    try {
        return "value"
    } finally {
        print("cleanup")   // runs before the function actually returns
    }
}
```
`catch` is optional as long as `finally` is present (`try { ... } finally { ... }`, useful for cleanup while letting the error keep propagating) — but at least one of `catch`/`finally` is required.

## Full native-function list (always available, no import needed)

```
print type str num format json assert input readfile writefile
sqrt pow abs floor ceil round min max random
len upper lower trim split join replace substring contains indexOf
push pop shift unshift slice sort reverse map filter reduce find any all
keys values has
get post serve
```

## Built-in modules — must `IMPORT://ton.<name>` before use

```
IMPORT://ton.sys        sys_args() sys_platform() sys_arch() sys_version() sys_exit(code) sys_sleep(ms)
IMPORT://ton.os         os_name() os_getenv(n) os_setenv(n,v) os_cwd() os_exists(p) os_isfile(p) os_isdir(p)
                        os_mkdir(p) os_remove(p) os_rename(a,b) os_copy(a,b) os_listdir(p) os_readlines(p)
                        os_appendfile(p,c) os_tempdir()
IMPORT://ton.requests   requests_get(url) requests_post(url,[body]) requests_request(method,url,[body])
                        -> {ok, status, body, error} — http:// ONLY, no TLS/https, no redirects
IMPORT://ton.random     random_int(min,max) random_float() random_choice(arr) random_shuffle(arr) random_seed(n)
IMPORT://ton.time       time_now() time_millis() time_string([ts]) time_sleep(ms)
IMPORT://ton.json       json_parse(text) json_stringify(v) json_pretty(v)
IMPORT://ton.mathutils  mathutils_pi() mathutils_e() sin/cos/tan/asin/acos/atan(x) atan2(y,x)
                        log/log2/log10/exp(x) hypot(x,y) degrees/radians(x) clamp(x,lo,hi) lerp(a,b,t)
                        sign(x) gcd(a,b) lcm(a,b) factorial(n) is_prime(n)
                        sum/mean/median/stddev(arr) min_of/max_of(arr)     (all prefixed mathutils_)
IMPORT://ton.strings    strings_starts_with/ends_with(s,x) strings_repeat(s,n) strings_reverse(s)
                        strings_capitalize(s) strings_pad_left/pad_right(s,w,[ch]) strings_center(s,w,[ch])
                        strings_count(s,sub) strings_trim_start/trim_end(s) strings_words(s)
                        strings_replace_first(s,search,repl) strings_snake_case(s) strings_camel_case(s)
IMPORT://ton.encoding   base64_encode/decode(s) hex_encode/decode(s) url_encode/decode(s)
IMPORT://ton.regex      regex_test(s,pat) regex_match(s,pat) regex_find_all(s,pat)
                        regex_replace(s,pat,repl) regex_split(s,pat)   — std::regex ECMAScript syntax
IMPORT://ton.path       path_join(...) path_basename(p) path_dirname(p) path_extension(p)
                        path_stem(p) path_absolute(p)
IMPORT://ton.tensor     tensor_zeros/ones/full/random(shape) tensor_from_array(arr) tensor_to_array(t)
                        tensor_shape(t) tensor_size(t) tensor_clone(t) tensor_reshape(t,shape)
                        tensor_get(t,idx) tensor_set(t,idx,v) tensor_add/sub/mul/div(a,b)
                        tensor_add_bias(mat,bias) tensor_matmul(a,b) tensor_transpose(t)
                        tensor_sum/mean/max/min(t) tensor_relu/sigmoid/tanh/exp/log(t) tensor_softmax(t)
                        tensor_relu_grad(in)/sigmoid_grad(out)/tanh_grad(out) tensor_map(t,fn)
                        — 1D/2D only, CPU only, no autograd (see the community "atome" module for that)
```

A function from an unimported module fails with "undefined function" — always emit the matching `IMPORT://` line at the top of the script before using any of its functions.

`IMPORT://name` (no `ton.` prefix) loads a user module: `name.ton` next to the script, or in a `modules/` subfolder. A module is just a `.ton` file whose declarations get pulled into the caller's global scope — prefix every function in it with the module's own name (e.g. `greetings_hello`) to avoid clobbering.

## HTTP server

```
ton.function userById(req) {
    // req = {method, path, query: {...}, params: {...}, body: "..."}
    ton.string id = req["params"]["id"]
    return json({id: id})
}
ton.get("/user/:id", ton.userById)     // ":name" segments -> req["params"]
ton.post("/users", ton.createUser)      // handler still takes exactly one `req` arg
ton.serve(8081)                          // blocking, single-threaded, routing mode

ton.serve(8080, "<h1>static</h1>")        // simple mode: same content every request
```
Handler returning `ton.html` -> `Content-Type: text/html`; anything else -> `text/plain`. No route match -> `404`.

## Gotchas an AI should not trip on

1. **No f-strings/interpolation** — use `format("{} {}", a, b)` or `+`.
2. **Function arity is enforced** — a parameter is only optional if it has a `= default` or is the `...rest` parameter; never call with fewer required args or more args than a function without `...rest` accepts.
3. **Comparisons throw on type mismatch** — don't compare a number to a string or an array to anything with `<`/`>`.
4. **`/` and `%` throw on zero divisor** — wrap in `try`/`catch` if the divisor might be zero and you want to handle it gracefully.
5. **Strings are immutable** — `s[0] = "x"` is a runtime error; build a new string instead.
6. **`switch` never falls through** — don't rely on C-style fallthrough; list multiple values in one `case` instead (`case 1, 2:`).
7. **A bare `name = value` can't declare** — first declaration must be `ton.<type> name = value`.
8. **Route handlers take exactly one argument** (`req`), even in simple routes that ignore it — a zero-arg handler throws an arity error.
9. **Dict keys are always strings** — `d[1]` on a dict looks up the *string* `"1"`, not the number `1`.
10. **`ton.requests` is `http://` only** — it cannot reach an `https://`-only API.

## Minimal complete example

```
IMPORT://ton.json

ton.array users = ["kuro", "rusher", "natlep"]

ton.function greet(name) {
    return format("Hello, {}!", name)
}

for ton.u in users {
    print(greet(u))
}

print(json(users))
```
