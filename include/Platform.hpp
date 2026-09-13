#pragma once
#include <cstdlib>
#include <string>

// ============================================================================
// Platform.hpp — a single place to answer "what am I running on?".
//
// Termux is a regular ARM64 Linux userland (no distinct compile target), so
// it can't be told apart from any other Linux at compile time — it's
// detected at runtime via the $PREFIX environment variable Termux always
// sets (see isTermux() below), exactly like scripts/install.sh already does.
//
// Used by ton.sys's sys_platform()/sys_arch() (src/Interpreter.cpp) and by
// --update (main.cpp) to pick the right precompiled asset from a GitHub
// release — see releaseAssetName() and include/Version.hpp.
// ============================================================================

inline bool isTermux() {
#ifdef _WIN32
    return false;
#else
    const char* prefix = std::getenv("PREFIX");
    return prefix && std::string(prefix).find("com.termux") != std::string::npos;
#endif
}

// One of "windows", "termux", "macos", "linux".
inline std::string detectPlatform() {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return isTermux() ? "termux" : "linux";
#endif
}

// One of "x64", "arm64", "arm", "x86", "unknown".
inline std::string detectArch() {
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x64";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
}

// The dist/ asset name a GitHub release publishes for the current platform
// (see .github/workflows/release.yml and the Makefile's linux/windows/android
// targets). Empty if this platform has no precompiled binary yet.
inline std::string releaseAssetName() {
    std::string platform = detectPlatform();
    if (platform == "windows") return "ton618-windows-x64.exe";
    if (platform == "linux") return "ton618-linux-x64";
    if (platform == "termux") return "ton618-android-arm64";
    return "";
}
