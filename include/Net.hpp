#pragma once
#include <string>
#include <utility>
#include <vector>

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

// ---- A full HTTP(S) client on top of curl ----------------------------------

struct HttpOptions {
    std::string method = "GET";
    std::string url;                                           // http:// or https:// only
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    bool hasBody = false;
    int timeoutSeconds = 30;
    bool followRedirects = true;
    bool verifyTls = true;
};

struct HttpResult {
    bool ok = false;           // a response arrived (any status); false: network/TLS/timeout error
    int status = 0;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;   // names lowercased
    std::string url;           // the final URL, after redirects
    std::string error;
};

// Sends one request with curl. The request is described in a private config
// file (never on a command line, so tokens don't show up in `ps`) and only
// http/https URLs are allowed.
HttpResult curlRequest(const HttpOptions& options);

std::string base64Encode(const std::string& data);
bool base64Decode(const std::string& text, std::string& out);

// Where ton618 keeps per-user settings (the saved GitHub token): ~/.config/ton618,
// or %APPDATA%\ton618 on Windows.
std::string configDir();
