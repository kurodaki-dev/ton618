# Changelog

## 2.2.0

- **A play button with the TON618 logo in Acode's top bar**, next to Acode's own Run button (shown for `.ton` files). It opens the full-window output.
- **The plugin's sidebar page is only the module library** now: no Run button, no output box.
- **`IMPORT://requests` works**: the JavaScript engine now has `ton.requests` (a synchronous request; the server must allow cross-origin requests, CORS) and `ton.encoding` (base64, hex).

## 2.1.2

- **Syntax highlighting for current Acode** (CodeMirror): TON618 is registered as a real language (comments, strings and f-strings, numbers, keywords, types, `IMPORT://`, `ton.`).
- **The `.ton` icon is drawn like Acode's own icons** (a 1em box), so it no longer comes out huge in the file list; it is also applied to files that were already open when the plugin loaded (tabs).
- The icon fix of 2.1.1 had silently not been applied; a test now checks it.

## 2.1.1

- Removed the `Ctrl+Enter` shortcut: on some keyboards the Enter key triggered Run instead of adding a new line.
- **Run opens a full window** over Acode with the program's output, and Stop / Again / Close buttons.
- The `.ton` file icon is now a fixed 24 px square (it was drawn too big and overlapped the file name), and the sidebar's own styles no longer apply to Acode's sidebar tab icon.
- Fixed `editorManager.editor.on is not a function`: newer Acode versions use CodeMirror instead of Ace. The plugin now detects the editor, and every step that touches it is guarded. The sidebar (Run, output, module library) works with any editor; colours and gutter marks need the Ace editor, and syntax errors are also shown in the sidebar.

## 2.1.0

- Fixed the install error `ace.define is not a function`: the highlighting no longer uses `ace.define`.
- The TON618 logo is the icon of `.ton` files.
- A TON618 sidebar: a Run button, the program's output, and the module library with Install / Update / Remove buttons (modules go in the `modules/` folder next to the open file).

## 2.0.0
- First release: highlighting, run with output panel, syntax errors while typing, IMPORT:// from the file's folder.
