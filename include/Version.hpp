#pragma once

// ============================================================================
// Version.hpp — the interpreter's own version string.
//
// Bump this (and tag the matching commit, e.g. `git tag beta-1.0.1`) whenever
// a release is cut. `--version` (main.cpp) and `ton.sys`'s sys_version()
// (src/Interpreter.cpp) both read it from here, and `--update` (main.cpp)
// compares it against GitHub release tags — see TON618_REPO below.
// ============================================================================

#define TON618_VERSION "beta-1.0.8"

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
