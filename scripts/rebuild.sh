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
#   - Termux/Android:    a musl cross toolchain targeting aarch64, NOT the
#                         Android NDK. Statically linked, so it runs on
#                         Termux with nothing installed on-device (no clang,
#                         no make, no NDK/bionic).
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

MUSL_AARCH64_CXX="${MUSL_AARCH64_CXX:-/home/ubuntu/toolchains/aarch64-linux-musl-cross/bin/aarch64-linux-musl-g++}"
if command -v "$MUSL_AARCH64_CXX" >/dev/null 2>&1; then
    echo "[rebuild] Building Termux/Android arm64 binary (make android)..."
    make android MUSL_AARCH64_CXX="$MUSL_AARCH64_CXX"
else
    echo "[rebuild] aarch64 musl cross toolchain not found at $MUSL_AARCH64_CXX, skipping the Android binary."
    echo "[rebuild] Fetch it with:"
    echo "[rebuild]   curl -fsSL -o /tmp/aarch64-linux-musl-cross.tgz https://musl.cc/aarch64-linux-musl-cross.tgz"
    echo "[rebuild]   mkdir -p /home/ubuntu/toolchains && tar xzf /tmp/aarch64-linux-musl-cross.tgz -C /home/ubuntu/toolchains"
fi

echo "[rebuild] Done. dist/ now contains:"
ls -la dist
