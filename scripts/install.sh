#!/usr/bin/env bash
# TON618 installer — Linux & Termux
# Usage: curl -fsSL https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/install.sh | bash
#
# Always installs a precompiled binary from a GitHub Release (built by
# .github/workflows/release.yml). Nothing is ever compiled on this device:
# no clang, no g++, no make needed.
#
# Set TON618_VERSION to install a specific release tag instead of the latest
# one, e.g.: TON618_VERSION=beta-1.0.0 curl -fsSL .../install.sh | bash
set -euo pipefail

REPO="kurodaki-dev/ton618"
VERSION="${TON618_VERSION:-latest}"
BIN_NAME="ton618"

info()  { printf '\033[1;34m[ton618]\033[0m %s\n' "$1"; }
ok()    { printf '\033[1;32m[ton618]\033[0m %s\n' "$1"; }
err()   { printf '\033[1;31m[ton618]\033[0m %s\n' "$1" >&2; }

# --- Detect environment -----------------------------------------------------

IS_TERMUX=false
if [ -n "${PREFIX:-}" ] && [[ "$PREFIX" == *"com.termux"* ]]; then
    IS_TERMUX=true
fi

OS="$(uname -s)"
ARCH="$(uname -m)"

if $IS_TERMUX; then
    INSTALL_DIR="$PREFIX/bin"
elif [ "$OS" = "Darwin" ]; then
    INSTALL_DIR="$HOME/.local/bin"
elif [ "$OS" = "Linux" ]; then
    INSTALL_DIR="$HOME/.local/bin"
else
    err "Unsupported platform for this installer ($OS)."
    exit 1
fi

mkdir -p "$INSTALL_DIR"
TARGET="$INSTALL_DIR/$BIN_NAME"

# --- Pick the right precompiled binary --------------------------------------

ASSET=""
if $IS_TERMUX; then
    case "$ARCH" in
        aarch64|arm64) ASSET="ton618-android-arm64" ;;
        *)
            err "No precompiled Termux binary for architecture '$ARCH' yet (only aarch64/arm64 is supported)."
            exit 1
            ;;
    esac
elif [ "$OS" = "Linux" ] && [ "$ARCH" = "x86_64" ]; then
    ASSET="ton618-linux-x64"
else
    err "No precompiled binary for $OS/$ARCH yet. Build from source: https://github.com/$REPO"
    exit 1
fi

if [ "$VERSION" = "latest" ]; then
    DOWNLOAD_URL="https://github.com/$REPO/releases/latest/download/$ASSET"
else
    DOWNLOAD_URL="https://github.com/$REPO/releases/download/$VERSION/$ASSET"
fi

info "Downloading $ASSET ($VERSION) from GitHub Releases..."
curl -fsSL "$DOWNLOAD_URL" -o "$TARGET"
chmod +x "$TARGET"
ok "Binary installed: $TARGET"

# --- Make sure the install dir is on PATH -----------------------------------

add_path_line() {
    local rcfile="$1"
    local marker="# >>> ton618 installer PATH >>>"
    [ -f "$rcfile" ] || return 0
    grep -qF "$marker" "$rcfile" 2>/dev/null && return 0
    {
        echo ""
        echo "$marker"
        echo "export PATH=\"$INSTALL_DIR:\$PATH\""
        echo "# <<< ton618 installer PATH <<<"
    } >> "$rcfile"
    info "PATH updated in $rcfile"
}

case ":$PATH:" in
    *":$INSTALL_DIR:"*) ;; # already on PATH
    *)
        if $IS_TERMUX; then
            : # $PREFIX/bin is already on PATH in Termux
        else
            add_path_line "$HOME/.bashrc"
            add_path_line "$HOME/.zshrc"
            add_path_line "$HOME/.profile"
            info "Open a new terminal (or run 'source ~/.bashrc') so the 'ton618' command is picked up."
        fi
        ;;
esac

ok "Install complete. Try: ton618 --help"
