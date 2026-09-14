#!/usr/bin/env bash
# TON618 uninstaller — Linux & Termux
# Usage: curl -fsSL https://raw.githubusercontent.com/kurodaki-dev/ton618/main/scripts/uninstall.sh | bash
# (or simply: ton618 --uninstall-ton)
set -uo pipefail

BIN_NAME="ton618"
FOUND=false

info() { printf '\033[1;34m[ton618]\033[0m %s\n' "$1"; }
ok()   { printf '\033[1;32m[ton618]\033[0m %s\n' "$1"; }

CANDIDATES=(
    "$HOME/.local/bin/$BIN_NAME"
    "${PREFIX:-}/bin/$BIN_NAME"
    "/usr/local/bin/$BIN_NAME"
)

for path in "${CANDIDATES[@]}"; do
    [ -z "$path" ] && continue
    if [ -f "$path" ]; then
        rm -f "$path"
        ok "Removed: $path"
        FOUND=true
    fi
done

# Remove the PATH block this installer added to shell rc files.
for rcfile in "$HOME/.bashrc" "$HOME/.zshrc" "$HOME/.profile"; do
    [ -f "$rcfile" ] || continue
    if grep -qF "# >>> ton618 installer PATH >>>" "$rcfile" 2>/dev/null; then
        sed -i.bak '/# >>> ton618 installer PATH >>>/,/# <<< ton618 installer PATH <<</d' "$rcfile"
        rm -f "$rcfile.bak"
        info "PATH entry removed from $rcfile"
    fi
done

if $FOUND; then
    ok "TON618 has been uninstalled."
else
    info "No TON618 install found."
fi
