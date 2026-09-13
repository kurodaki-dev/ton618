#!/usr/bin/env bash
# Dev-only helper: rebuild every precompiled binary in dist/ from current
# source, entirely on this machine. Useful for testing all three targets
# locally before pushing a tag — .github/workflows/release.yml does the same
# three builds on GitHub's runners and publishes them as release assets, so
# this script never needs to run in CI itself.
#
# All three targets are cross-compiled here via `make` — end users never
# need a compiler on their own device:
#   - Linux x86_64:      native g++
#   - Windows x86_64:    mingw-w64 (x86_64-w64-mingw32-g++)
#   - Termux/Android:    Zig (bundles musl libc, cross-compiles straight to
#                         aarch64-linux-musl), or a separately installed musl
#                         cross toolchain — either way NOT the Android NDK.
#                         Statically linked, so it runs on Termux with
#                         nothing installed on-device (no clang, no make, no
#                         NDK/bionic). See .github/workflows/release.yml's
#                         build-android job for the same thing done in CI.
set -euo pipefail

cd "$(dirname "$0")/.."

echo "[rebuild] make distclean"
make distclean

echo "[rebuild] Building Linux binary (make linux)..."
make linux

if command -v "${MINGW_CXX:-x86_64-w64-mingw32-g++}" >/dev/null 2>&1; then
    echo "[rebuild] Building Windows binary (make windows)..."
    make windows
else
    echo "[rebuild] mingw-w64 (x86_64-w64-mingw32-g++) not found, skipping the Windows binary."
    echo "[rebuild] Install it with: sudo apt-get install -y g++-mingw-w64-x86-64"
fi

if command -v zig >/dev/null 2>&1; then
    echo "[rebuild] Building Termux/Android arm64 binary with zig (make android)..."
    make android MUSL_AARCH64_CXX="zig c++ -target aarch64-linux-musl" MUSL_AARCH64_STRIP="scripts/zig-strip.sh"
elif [ -n "${MUSL_AARCH64_CXX:-}" ] && command -v "$MUSL_AARCH64_CXX" >/dev/null 2>&1; then
    echo "[rebuild] Building Termux/Android arm64 binary (make android)..."
    make android MUSL_AARCH64_CXX="$MUSL_AARCH64_CXX"
else
    echo "[rebuild] Neither zig nor a musl cross toolchain was found, skipping the Android binary."
    echo "[rebuild] Easiest: install zig (https://ziglang.org/download/) and re-run this script."
    echo "[rebuild] Or fetch a musl-cross toolchain and pass it as MUSL_AARCH64_CXX=/path/to/aarch64-linux-musl-g++."
fi

echo "[rebuild] Done. dist/ now contains:"
ls -la dist
