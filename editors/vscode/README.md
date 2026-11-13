# TON618 for Visual Studio Code

Language support for [TON618](https://c-ton618.duckdns.org/), a small and friendly programming language.

- **Syntax highlighting** for `.ton` files — including the expressions inside f-strings (`f"Hello {name}"`).
- **Run the current file** with the ▶ button in the editor title bar, the context menu, or `Ctrl+Alt+R` (`Cmd+Alt+R` on macOS).
- **Syntax errors underlined** when a file is opened or saved (`ton618 --check`).
- **The REPL** in a terminal: *TON618: Open the REPL* in the command palette.
- **Snippets**: `fn`, `class`, `classext`, `for`, `forr`, `if`, `ife`, `while`, `try`, `switch`, `let`, `const`, `pf`, `imp`, `route`…

## Requirements

The `ton618` interpreter, installed with:

```bash
curl -fsSL https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.sh | bash          # Linux, macOS, Termux
powershell -c "irm https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.ps1 | iex"  # Windows
```

If it isn't on your `PATH`, set **TON618: Interpreter Path** (`ton618.interpreterPath`).

## Settings

| Setting | Default | |
|---|---|---|
| `ton618.interpreterPath` | `ton618` | Path to the interpreter |
| `ton618.checkOnSave` | `true` | Underline syntax errors on open/save |
| `ton618.saveBeforeRun` | `true` | Save the file before running it |

The TON618 logo is © 2026 kurodaki-dev, all rights reserved. The extension's code is under the MIT license.
