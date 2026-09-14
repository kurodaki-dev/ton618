#pragma once

// ============================================================================
// Version.hpp — the interpreter's own version string.
//
// Bump this (and tag the matching commit, e.g. `git tag beta-1.0.1`) whenever
// a release is cut. `--version` (main.cpp) and `ton.sys`'s sys_version()
// (src/Interpreter.cpp) both read it from here, and `--update` (main.cpp)
// compares it against GitHub release tags — see TON618_REPO below.
// ============================================================================

#define TON618_VERSION "beta-1.0.3"

// "owner/repo" on GitHub that --update pulls precompiled binaries from and
// that install.sh / install.ps1 point at. Change this if the project ever
// moves to a different GitHub location.
#define TON618_REPO "kurodaki-dev/ton618"
