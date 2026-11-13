#pragma once

// ============================================================================
// Version.hpp — the interpreter's own version string.
//
// Bump this (and tag the matching commit, e.g. `git tag beta-1.0.1`) whenever
// a release is cut. `--version` (main.cpp) and `ton.sys`'s sys_version()
// (src/Interpreter.cpp) both read it from here, and `--update` (main.cpp)
// compares it against GitHub release tags — see TON618_REPO below.
// ============================================================================

#define TON618_VERSION "1.0.2"

// "owner/repo" on GitHub that --update pulls precompiled binaries from and
// that install.sh / install.ps1 point at. Change this if the project ever
// moves to a different GitHub location.
#define TON618_REPO "kurodaki-dev/ton618"

// Default base URL for the module registry that `ton618 install <module>`
// queries (see main.cpp's runInstall()) — a small JSON API, not part of this
// repo, that maps a module name to {name, github, version}. Override without
// rebuilding by setting the TON618_REGISTRY_URL environment variable (no
// trailing slash) to point at your own deployment.
#define TON618_REGISTRY_URL "https://file330.duckdns.org/ton618-registry-api"

// Where `ton618 install/update/search/publish` talk to: the registry server
// (server/registry_server.py, SQLite) that serves index.json plus one .tonpkg
// file per published version, and holds the accounts. Override without
// rebuilding with the TON618_PACKAGES_URL environment variable (a file:// URL
// works too, for a local registry).
#define TON618_PACKAGE_REGISTRY_URL "https://api-ton.duckdns.org/registry"

// Read-only copy of the same packages on the documentation site, used only when the
// registry server above can't be reached (and no other registry was chosen).
#define TON618_PACKAGE_REGISTRY_FALLBACK_URL "https://c-ton618.duckdns.org/registry"
