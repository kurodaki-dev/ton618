#include "Net.hpp"
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
  #include <windows.h>
  #define popen _popen
  #define pclose _pclose
#else
  #include <sys/wait.h>
#endif

bool commandExists(const std::string& cmd) {
#ifdef _WIN32
    std::string check = "where " + cmd + " >NUL 2>NUL";
#else
    std::string check = "command -v " + cmd + " >/dev/null 2>&1";
#endif
    return std::system(check.c_str()) == 0;
}

bool isSafeToken(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isalnum((unsigned char)c) && c != '.' && c != '-' && c != '_') return false;
    }
    return true;
}

bool isSafeUrl(const std::string& url) {
    if (url.empty() || url.size() > 2000) return false;
    for (char c : url) {
        if (std::isalnum((unsigned char)c)) continue;
        if (std::string(":/._~%?=&#@+,-").find(c) != std::string::npos) continue;
        return false;
    }
    return true;
}

static bool isSafePath(const std::string& p) {
    if (p.empty()) return false;
    for (char c : p) {
        if (c == '"' || c == '\'' || c == '$' || c == '`' || c == '\n' || c == '\r' || c == '%' || c == '!') return false;
    }
    return true;
}

bool downloadFile(const std::string& url, const std::string& outPath) {
    if (!isSafeUrl(url)) { std::cerr << "[ton618] Refusing to download from a URL with unsafe characters.\n"; return false; }
    if (!isSafePath(outPath)) { std::cerr << "[ton618] Refusing to write to a path with unsafe characters.\n"; return false; }
    std::string cmd;
#ifdef _WIN32
    if (commandExists("curl")) {
        cmd = "curl -fsSL -o \"" + outPath + "\" \"" + url + "\"";
    } else {
        cmd = "powershell -NoProfile -ExecutionPolicy Bypass -Command "
              "\"$ProgressPreference='SilentlyContinue'; Invoke-WebRequest -Uri '" + url +
              "' -OutFile '" + outPath + "'\"";
    }
#else
    if (commandExists("curl")) {
        cmd = "curl -fsSL -o '" + outPath + "' '" + url + "'";
    } else if (commandExists("wget")) {
        cmd = "wget -q -O '" + outPath + "' '" + url + "'";
    } else {
        std::cerr << "Neither curl nor wget is available to download files.\n";
        return false;
    }
#endif
    return std::system(cmd.c_str()) == 0;
}

std::string fetchText(const std::string& url, bool& ok) {
    ok = false;
    if (!isSafeUrl(url)) return "";
    std::string cmd;
#ifdef _WIN32
    if (commandExists("curl")) {
        cmd = "curl -fsSL \"" + url + "\"";
    } else {
        cmd = "powershell -NoProfile -ExecutionPolicy Bypass -Command "
              "\"$ProgressPreference='SilentlyContinue'; (Invoke-WebRequest -Uri '" + url + "').Content\"";
    }
#else
    if (commandExists("curl")) {
        cmd = "curl -fsSL '" + url + "'";
    } else if (commandExists("wget")) {
        cmd = "wget -q -O- '" + url + "'";
    } else {
        return "";
    }
#endif
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    std::string result;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), pipe)) > 0) result.append(buf, n);
    ok = (pclose(pipe) == 0);
    return result;
}

// ---- SHA-256 (FIPS 180-4) ---------------------------------------------------

std::string sha256Hex(const std::string& data) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    auto rotr = [](uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };

    std::string msg = data;
    uint64_t bits = (uint64_t)data.size() * 8;
    msg += (char)0x80;
    while (msg.size() % 64 != 56) msg += (char)0;
    for (int i = 7; i >= 0; i--) msg += (char)((bits >> (i * 8)) & 0xFF);

    for (size_t off = 0; off < msg.size(); off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)(unsigned char)msg[off + i * 4] << 24) | ((uint32_t)(unsigned char)msg[off + i * 4 + 1] << 16) |
                   ((uint32_t)(unsigned char)msg[off + i * 4 + 2] << 8) | (uint32_t)(unsigned char)msg[off + i * 4 + 3];
        }
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (uint32_t v : h) for (int i = 28; i >= 0; i -= 4) out += hex[(v >> i) & 0xF];
    return out;
}

bool openUrl(const std::string& url) {
    if (!isSafeUrl(url)) return false;
#ifdef _WIN32
    return std::system(("start \"\" \"" + url + "\"").c_str()) == 0;
#elif defined(__APPLE__)
    return std::system(("open '" + url + "' >/dev/null 2>&1").c_str()) == 0;
#else
    for (const char* opener : {"termux-open-url", "xdg-open"}) {
        if (commandExists(opener)) return std::system((std::string(opener) + " '" + url + "' >/dev/null 2>&1").c_str()) == 0;
    }
    return false;
#endif
}

// ---- base64 -----------------------------------------------------------------

std::string base64Encode(const std::string& data) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    while (i + 2 < data.size()) {
        uint32_t v = ((uint32_t)(unsigned char)data[i] << 16) | ((uint32_t)(unsigned char)data[i + 1] << 8) | (unsigned char)data[i + 2];
        out += t[(v >> 18) & 63]; out += t[(v >> 12) & 63]; out += t[(v >> 6) & 63]; out += t[v & 63];
        i += 3;
    }
    if (i + 1 == data.size()) {
        uint32_t v = (uint32_t)(unsigned char)data[i] << 16;
        out += t[(v >> 18) & 63]; out += t[(v >> 12) & 63]; out += "==";
    } else if (i + 2 == data.size()) {
        uint32_t v = ((uint32_t)(unsigned char)data[i] << 16) | ((uint32_t)(unsigned char)data[i + 1] << 8);
        out += t[(v >> 18) & 63]; out += t[(v >> 12) & 63]; out += t[(v >> 6) & 63]; out += '=';
    }
    return out;
}

bool base64Decode(const std::string& text, std::string& out) {
    out.clear();
    uint32_t acc = 0;
    int bits = 0;
    for (char c : text) {
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+' || c == '-') v = 62;
        else if (c == '/' || c == '_') v = 63;
        else if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        else return false;
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) { bits -= 8; out += (char)((acc >> bits) & 0xFF); }
    }
    return true;
}

// ---- configuration folder ---------------------------------------------------

std::string configDir() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    return std::string(appdata ? appdata : ".") + "\\ton618";
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) return std::string(xdg) + "/ton618";
    const char* home = std::getenv("HOME");
    return std::string(home ? home : ".") + "/.config/ton618";
#endif
}

// ---- HTTP(S) through curl ---------------------------------------------------

namespace {

std::string quoteForCurlConfig(const std::string& v) {
    std::string out = "\"";
    for (char c : v) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out + "\"";
}

bool slurp(const std::filesystem::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

std::string lowerCase(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

} // namespace

HttpResult curlRequest(const HttpOptions& o) {
    namespace fs = std::filesystem;
    HttpResult r;
    auto bad = [&](const std::string& why) { r.error = why; return r; };

    std::string lowerUrl = lowerCase(o.url.substr(0, 8));
    if (lowerUrl.rfind("http://", 0) != 0 && lowerUrl.rfind("https://", 0) != 0) return bad("Only http:// and https:// URLs are supported.");
    for (char c : o.url) if ((unsigned char)c < 0x20 || c == ' ') return bad("The URL contains spaces or control characters.");
    for (auto& h : o.headers) {
        if (h.first.empty() || h.first.find_first_of(":\r\n") != std::string::npos || h.second.find_first_of("\r\n") != std::string::npos)
            return bad("Invalid HTTP header '" + h.first + "'.");
    }
    for (char c : o.method) if (!std::isalpha((unsigned char)c)) return bad("Invalid HTTP method.");
    if (!commandExists("curl")) return bad("curl is needed for this (Termux: pkg install curl).");

    static unsigned counter = 0;
    std::error_code ec;
    fs::path dir = fs::temp_directory_path(ec);
    std::string stamp = std::to_string((unsigned long long)std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(counter++);
    fs::path cfg = dir / ("ton618-curl-" + stamp + ".cfg"), bodyOut = dir / ("ton618-curl-" + stamp + ".out"),
             headOut = dir / ("ton618-curl-" + stamp + ".hdr"), errOut = dir / ("ton618-curl-" + stamp + ".err"),
             bodyIn = dir / ("ton618-curl-" + stamp + ".in");
    auto cleanup = [&]() { for (auto& p : {cfg, bodyOut, headOut, errOut, bodyIn}) fs::remove(p, ec); };
    for (auto& p : {cfg, bodyOut, headOut, errOut, bodyIn}) if (!isSafePath(p.string())) return bad("The temporary folder has an unusable name.");

    std::string c = "silent\nshow-error\nproto = \"=http,https\"\nproto-redir = \"=http,https\"\n";
    c += "url = " + quoteForCurlConfig(o.url) + "\n";
    std::string method = o.method;
    for (char& ch : method) ch = (char)std::toupper((unsigned char)ch);
    if (method == "HEAD") c += "head\n"; else if (method != "GET") c += "request = " + quoteForCurlConfig(method) + "\n";
    for (auto& h : o.headers) c += "header = " + quoteForCurlConfig(h.first + ": " + h.second) + "\n";
    if (o.hasBody) {
        std::ofstream f(bodyIn, std::ios::binary);
        f << o.body;
        f.close();
        c += "data-binary = " + quoteForCurlConfig("@" + bodyIn.string()) + "\n";
    }
    int timeout = o.timeoutSeconds < 1 ? 1 : (o.timeoutSeconds > 600 ? 600 : o.timeoutSeconds);
    c += "max-time = " + std::to_string(timeout) + "\nconnect-timeout = " + std::to_string(timeout < 15 ? timeout : 15) + "\n";
    if (o.followRedirects) c += "location\nmax-redirs = 10\n";
    if (!o.verifyTls) c += "insecure\n";
    c += "user-agent = \"ton618\"\n";
    c += "output = " + quoteForCurlConfig(bodyOut.string()) + "\n";
    c += "dump-header = " + quoteForCurlConfig(headOut.string()) + "\n";
    c += "stderr = " + quoteForCurlConfig(errOut.string()) + "\n";
    c += "write-out = \"%{http_code}\\n%{url_effective}\\n\"\n";
    {
        std::ofstream f(cfg, std::ios::binary);
        f << c;
    }
#ifndef _WIN32
    fs::permissions(cfg, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, ec); // it holds tokens
#endif

    std::string cmd = "curl -q -K \"" + cfg.string() + "\"";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) { cleanup(); return bad("Could not start curl."); }
    std::string out;
    char buf[512];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), pipe)) > 0) out.append(buf, n);
    int rc = pclose(pipe);
#ifndef _WIN32
    if (WIFEXITED(rc)) rc = WEXITSTATUS(rc);
#endif

    std::stringstream lines(out);
    std::string code, finalUrl;
    std::getline(lines, code);
    std::getline(lines, finalUrl);
    if (!code.empty() && code.back() == '\r') code.pop_back();
    if (!finalUrl.empty() && finalUrl.back() == '\r') finalUrl.pop_back();
    r.status = std::atoi(code.c_str());
    r.url = finalUrl.empty() ? o.url : finalUrl;

    if (rc != 0 || r.status == 0) {
        std::string why;
        slurp(errOut, why);
        while (!why.empty() && (why.back() == '\n' || why.back() == '\r')) why.pop_back();
        r.error = why.empty() ? "The request failed (curl exit code " + std::to_string(rc) + ")." : why;
        cleanup();
        return r;
    }
    r.ok = true;
    slurp(bodyOut, r.body);
    std::string headText;
    slurp(headOut, headText);
    // the dump holds one block per redirect: keep the last one
    size_t lastBlock = headText.rfind("HTTP/");
    if (lastBlock != std::string::npos) headText = headText.substr(lastBlock);
    std::stringstream hs(headText);
    std::string line;
    std::getline(hs, line); // status line
    while (std::getline(hs, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t colon = line.find(':');
        if (colon == std::string::npos || colon == 0) continue;
        std::string name = lowerCase(line.substr(0, colon)), value = line.substr(colon + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        bool merged = false;
        for (auto& h : r.headers) if (h.first == name) { h.second += ", " + value; merged = true; break; }
        if (!merged) r.headers.push_back({name, value});
    }
    cleanup();
    return r;
}
