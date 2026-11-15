# Atome

Build an operating system in a few lines of TON. One program, two screens: a graphical system in Acode, the TON618 IDE, the playground
or a browser; a real system on a kernel written from scratch (graphical screen with a mouse, or text mode), as a bootable ISO. You write the apps; the Windows / Android looks are optional presets, `atome.style("custom")` imposes nothing.

| Folder | What |
|---|---|
| `module/atome.ton` | The `atome` module (apps, windows, widgets, a virtual disk, five apps). Published in the registry as the `atome` package (`registry/sources/atome`, kept in step by `scripts/sync_atome.py`): Acode installs it from the Modules panel, the web IDE and the playground download it. |
| `cli/atome.py` | The `atome` command: `new`, `check`, `run`, `build` (web, kernel, image, app), `doctor`. Python 3, standard library only. |
| `kernel/` | An x86 kernel (C + NASM, no libc) with its own TON interpreter (`tonvm.c`). `host_test.c` runs that interpreter on a normal computer for fast tests. |
| `examples/` | Small programs for the kernel. |

```bash
python3 atome/cli/atome.py new myos && cd myos
python3 ../atome/cli/atome.py run                        # the browser
python3 ../atome/cli/atome.py build --target image       # dist/myos.iso (needs gcc, nasm, grub-mkrescue, xorriso)
```

Tests: `tests/atome-apps.sh` (the apps on the kernel's interpreter), `tests/atome-kernel.sh` (the kernel under QEMU),
`tests/atome-cli.sh` (the command, and an ISO booted under QEMU), `tests/ui-lite.js` (`ton.ui`).

See the documentation chapter "Atome — an operating system in TON (plugin)". After editing `module/atome.ton`: run `python3 scripts/sync_atome.py`, raise `version` in `registry/sources/atome/tonproject.toml`, rebuild (`cd registry/sources/atome && ../../../ton618 build`) and move `dist/atome-<version>.tonpkg` to `registry/packages/`; CI runs `sync_atome.py --check`.
