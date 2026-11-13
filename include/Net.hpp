#pragma once
#include <string>

// ============================================================================
// Net.hpp — the small system helpers shared by the command line tools
// (--update, install, the package manager): running curl/wget/PowerShell to
// download, a SHA-256 to check what was downloaded, and opening a web page.
//
// TON618 has no TLS client of its own (see HttpClient.hpp), so downloads reuse
// whatever the OS already ships. Because every URL ends up inside a shell
// command, downloadFile()/fetchText() refuse any URL or path holding a
// character that a shell could interpret.
// ============================================================================

// True if `cmd` resolves to something runnable on PATH.
bool commandExists(const std::string& cmd);

// True for a non-empty string of letters, digits, '.', '-' and '_': safe to
// put in a URL or a command line (module names, versions, owner/repo parts).
bool isSafeToken(const std::string& s);

// True for a URL made only of characters that can't change the meaning of a
// shell command (no quotes, spaces, $, `, ;, |, &, <, >, \ ...).
bool isSafeUrl(const std::string& url);

// Downloads `url` to the file `outPath`. Returns false on any failure.
bool downloadFile(const std::string& url, const std::string& outPath);

// Downloads `url` and returns its body (`ok` tells whether it worked).
std::string fetchText(const std::string& url, bool& ok);

// Lowercase hexadecimal SHA-256 of `data`.
std::string sha256Hex(const std::string& data);

// Opens `url` in the default browser. Returns false if there is no way to.
bool openUrl(const std::string& url);
