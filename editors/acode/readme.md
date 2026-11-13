# TON618 for Acode

Write and run [TON618](https://c-ton618.duckdns.org/) code on your phone.

- **Syntax highlighting** for `.ton` files (TON618 2.0: classes, f-strings, arrow functions, ranges…).
- **A TON618 sidebar** (the logo in Acode's sidebar): the **▶ Run** button, the program's output (tap an error's line to jump to it), and the **module library**. `Ctrl+Enter` also runs the file, with a keyboard.
- **Module library**: every package of the TON618 registry with an **Install** button; after installing it shows ✓ installed, with Update / Remove. Modules are written to the `modules/` folder next to the open file and checked with SHA-256, then `IMPORT://name` works.
- The TON618 logo is the icon of `.ton` files.
- **Syntax errors** marked in the gutter while you type.
- **`IMPORT://name`** loads `name.ton` (or `modules/name.ton`) from the same folder as your file.

The code runs inside Acode with TON618's JavaScript engine (the same one as the online playground), so it needs no setup and works offline. Like the playground, it can't use `ton.sys`, `ton.os`, `ton.requests`, files or the HTTP server — for those, install the real interpreter in Termux:

```bash
curl -fsSL https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.sh | bash
ton618 my_script.ton
```

The TON618 logo is © 2026 kurodaki-dev, all rights reserved. The plugin's code is under the Apache 2.0 license.
