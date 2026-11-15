# Atome

An operating system whose visible parts are written in TON.

| Folder | What | Status |
|---|---|---|
| `kernel/` | An x86 kernel written from scratch (C + NASM, no libc). Boots through Multiboot, runs a TON program at startup. | Works, tested under QEMU (`tests/atome-kernel.sh`) |
| `module/` | The start of the `atome` TON module (windows and Android-style systems on top of `ton.ui`). | In progress |
| (to come) | The `atome` command: `new`, `build`, `run`, and a bootable image. | Not started |

```bash
cd kernel && make && qemu-system-i386 -kernel atome.elf -initrd ../examples/hello.ton -serial stdio
```

See the documentation chapter "Atome — an operating system in TON (plugin)".
