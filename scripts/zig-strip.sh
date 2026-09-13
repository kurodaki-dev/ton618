#!/usr/bin/env bash
# Strips an ELF binary in place using `zig objcopy` — a drop-in for the
# Makefile's single-argument $(MUSL_AARCH64_STRIP) invocation (e.g. `strip
# file`), since `zig objcopy --strip-all <in> <out>` refuses to write back to
# its own input file. Used by .github/workflows/release.yml's build-android
# job, which cross-compiles with `zig c++ -target aarch64-linux-musl` instead
# of a separately-downloaded musl cross toolchain (see that job's comments).
set -euo pipefail
f="$1"
tmp="$(mktemp)"
zig objcopy --strip-all "$f" "$tmp"
mv "$tmp" "$f"
