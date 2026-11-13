# Changelog

## 2.1.1

- The `.ton` file icon is now a fixed 24 px square (it was drawn too big and overlapped the file name), and the sidebar's own styles no longer apply to Acode's sidebar tab icon.
- Fixed `editorManager.editor.on is not a function`: newer Acode versions use CodeMirror instead of Ace. The plugin now detects the editor, and every step that touches it is guarded. The sidebar (Run, output, module library) works with any editor; colours and gutter marks need the Ace editor, and syntax errors are also shown in the sidebar.

## 2.1.0

- Fixed the install error `ace.define is not a function`: the highlighting no longer uses `ace.define`.
- The TON618 logo is the icon of `.ton` files.
- A TON618 sidebar: a Run button, the program's output, and the module library with Install / Update / Remove buttons (modules go in the `modules/` folder next to the open file).

## 2.0.0
- First release: highlighting, run with output panel, syntax errors while typing, IMPORT:// from the file's folder.
