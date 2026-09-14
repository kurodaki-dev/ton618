#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <filesystem>
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Interpreter.hpp"
#include "JsonParser.hpp"
#include "Platform.hpp"
#include "Version.hpp"

#ifdef _WIN32
  #include <windows.h>
  #define popen _popen
  #define pclose _pclose
#else
  #include <unistd.h>
  #include <climits>
  #include <sys/stat.h>
#endif

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Could not open file '" + path + "'.");
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static std::vector<std::string> splitLines(const std::string& src) {
    std::vector<std::string> lines;
    std::stringstream ss(src);
    std::string line;
    while (std::getline(ss, line)) lines.push_back(line);
    return lines;
}

static std::string dirOf(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

static void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " <file.ton> [options]\n"
              << "Options:\n"
              << "  --debug              run in debug mode (pause at start)\n"
              << "  --break=<line>        add a breakpoint at a given line (repeatable)\n"
              << "  --uninstall-ton       remove this ton618 install (binary + PATH entry)\n"
              << "  --update [version]    update ton618 from GitHub Releases (latest if no version given)\n"
              << "  --version, -v         print the interpreter's version, platform and architecture\n"
              << "  --help, -h            show this message\n"
              << "\nOther commands:\n"
              << "  " << prog << " install <module>      fetch a module from the TON618 module registry into ./modules/\n"
              << "  " << prog << " uninstall <module>    remove a module previously fetched with install, from ./modules/\n"
              << "\nExample:\n"
              << "  " << prog << " myscript.ton --debug --break=5\n";
}

static void printVersion() {
    std::cout << "ton618 " << TON618_VERSION
               << " (" << detectPlatform() << "/" << detectArch() << ")\n";
}

static std::string getExecutablePath() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    return (len > 0) ? std::string(buf, len) : "";
#else
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    return (len != -1) ? std::string(buf, len) : "";
#endif
}

// Removes the "# >>> ton618 installer PATH >>> ... # <<< ton618 installer
// PATH <<<" block that install.sh appended to a shell rc file, if present.
static void removePathBlock(const std::string& rcfile) {
    std::ifstream in(rcfile);
    if (!in) return;
    std::stringstream ss;
    ss << in.rdbuf();
    in.close();
    std::string content = ss.str();

    const std::string startMarker = "# >>> ton618 installer PATH >>>";
    const std::string endMarker = "# <<< ton618 installer PATH <<<";
    size_t start = content.find(startMarker);
    if (start == std::string::npos) return;
    size_t end = content.find(endMarker, start);
    if (end == std::string::npos) return;
    end += endMarker.size();
    if (end < content.size() && content[end] == '\n') end++;
    // also drop the blank line install.sh inserted just before the block
    if (start > 0 && content[start - 1] == '\n') start--;

    content.erase(start, end - start);

    std::ofstream out(rcfile, std::ios::trunc);
    out << content;
}

static int runUninstall() {
    std::string self = getExecutablePath();
    if (self.empty()) {
        std::cerr << "Could not determine the executable's path.\n";
        return 1;
    }

    const char* home = std::getenv("HOME");
    if (home) {
        removePathBlock(std::string(home) + "/.bashrc");
        removePathBlock(std::string(home) + "/.zshrc");
        removePathBlock(std::string(home) + "/.profile");
    }

#ifdef _WIN32
    // A running executable can't delete itself directly on Windows: spawn a
    // detached helper that waits for this process to exit, then removes the
    // file (and its install directory, if left empty).
    size_t slash = self.find_last_of("\\/");
    std::string dir = (slash == std::string::npos) ? "." : self.substr(0, slash);
    std::string cmd = "cmd /C \"timeout /T 1 /NOBREAK >NUL & del /F /Q \"" + self +
                       "\" & rmdir \"" + dir + "\" 2>NUL\"";

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> cmdline(cmd.begin(), cmd.end());
    cmdline.push_back('\0');
    CreateProcessA(NULL, cmdline.data(), NULL, NULL, FALSE,
                   CREATE_NEW_CONSOLE | DETACHED_PROCESS, NULL, NULL, &si, &pi);
    std::cout << "Uninstalling TON618...\n";
#else
    if (std::remove(self.c_str()) != 0) {
        std::cerr << "Failed to remove " << self << "\n";
        return 1;
    }
    std::cout << "TON618 has been uninstalled (" << self << " removed).\n";
#endif
    return 0;
}

// True if `cmd` resolves to something runnable on PATH.
static bool commandExists(const std::string& cmd) {
#ifdef _WIN32
    std::string check = "where " + cmd + " >NUL 2>NUL";
#else
    std::string check = "command -v " + cmd + " >/dev/null 2>&1";
#endif
    return std::system(check.c_str()) == 0;
}

// Downloads `url` to `outPath` by shelling out to curl (present on Linux,
// Termux with `pkg install curl`, and Windows 10 1803+ out of the box) or a
// fallback (wget on Linux, PowerShell's Invoke-WebRequest on Windows). No
// HTTPS client is implemented in-process — see HttpClient.hpp's own comment
// on why TON618 has no bundled TLS — so --update reuses whatever the OS
// already ships, exactly like install.sh/install.ps1 do.
static bool downloadFile(const std::string& url, const std::string& outPath) {
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
        std::cerr << "Neither curl nor wget is available to download the update.\n";
        return false;
    }
#endif
    return std::system(cmd.c_str()) == 0;
}

// Same tooling as downloadFile() above, but captures the response body as a
// string instead of writing it to a file — used to fetch the small JSON
// documents the module registry serves (see runInstall() below). Sets `ok`
// to false (and returns an empty string) on any failure.
static std::string fetchText(const std::string& url, bool& ok) {
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
        ok = false;
        return "";
    }
#endif
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) { ok = false; return ""; }
    std::string result;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), pipe)) > 0) result.append(buf, n);
    ok = (pclose(pipe) == 0);
    return result;
}

// True if every character of `s` is a letter, digit, '.', '-', or '_' (and
// `s` is non-empty) — the same restrictive charset runUpdate() validates its
// version argument against, reused here because these strings all end up
// interpolated into a shell command line (see downloadFile/fetchText): a
// module name typed by the user, or an owner/repo pulled out of a registry
// response, must never be able to smuggle in shell metacharacters.
static bool isSafeToken(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isalnum((unsigned char)c) && c != '.' && c != '-' && c != '_') return false;
    }
    return true;
}

// Pulls "owner/repo" out of a github field that may be a full URL
// (https://github.com/owner/repo, optionally with a trailing slash or
// ".git") or already just "owner/repo". Returns "" if it doesn't look like
// a github.com link at all, or if owner/repo contain anything outside
// isSafeToken()'s charset.
static std::string parseGithubOwnerRepo(std::string github) {
    const std::string marker = "github.com/";
    size_t pos = github.find(marker);
    std::string rest = (pos != std::string::npos) ? github.substr(pos + marker.size()) : github;
    while (!rest.empty() && rest.back() == '/') rest.pop_back();
    if (rest.size() > 4 && rest.compare(rest.size() - 4, 4, ".git") == 0) rest.erase(rest.size() - 4);

    size_t slash = rest.find('/');
    if (slash == std::string::npos) return "";
    std::string owner = rest.substr(0, slash);
    std::string repo = rest.substr(slash + 1);
    if (repo.find('/') != std::string::npos) return ""; // extra path segments: not a bare repo link
    if (!isSafeToken(owner) || !isSafeToken(repo)) return "";
    return owner + "/" + repo;
}

// `ton618 install <module>` — looks `module` up in the TON618 module
// registry (see TON618_REGISTRY_URL in Version.hpp, overridable via the
// TON618_REGISTRY_URL environment variable), which maps a module name to
// {name, github, version} — the registry stores that metadata only, never
// the module's actual code. The module's source is then downloaded straight
// from its GitHub repo (as "<name>.ton" on the default branch) into
// ./modules/, where IMPORT://<module> already knows to look for it.
static int runInstall(const std::string& moduleName) {
    if (!isSafeToken(moduleName)) {
        std::cerr << "Invalid module name '" << moduleName
                   << "': only letters, digits, '.', '-', '_' are allowed.\n";
        return 1;
    }

    const char* envRegistry = std::getenv("TON618_REGISTRY_URL");
    std::string registry = envRegistry ? envRegistry : TON618_REGISTRY_URL;
    std::string indexUrl = registry + "/api/modules/" + moduleName;

    std::cout << "[ton618] Looking up '" << moduleName << "' in the registry (" << registry << ") ...\n";
    bool ok = false;
    std::string body = fetchText(indexUrl, ok);
    if (!ok || body.empty()) {
        std::cerr << "[ton618] Could not reach the module registry, or '" << moduleName << "' isn't listed there.\n";
        return 1;
    }

    Value doc;
    try {
        doc = parseJson(body);
    } catch (std::exception& e) {
        std::cerr << "[ton618] The registry's response wasn't valid JSON: " << e.what() << "\n";
        return 1;
    }

    Value* errorField = doc.findDictEntry("error");
    if (errorField) {
        std::cerr << "[ton618] Registry error: " << errorField->toString() << "\n";
        return 1;
    }

    Value* githubField = doc.findDictEntry("github");
    Value* versionField = doc.findDictEntry("version");
    if (!githubField || githubField->type != ValueType::STRING) {
        std::cerr << "[ton618] The registry entry for '" << moduleName << "' has no usable 'github' link.\n";
        return 1;
    }

    std::string ownerRepo = parseGithubOwnerRepo(githubField->str);
    if (ownerRepo.empty()) {
        std::cerr << "[ton618] The registry's 'github' field ('" << githubField->str
                   << "') isn't a recognizable github.com/owner/repo link.\n";
        return 1;
    }

    std::string rawUrl = "https://raw.githubusercontent.com/" + ownerRepo + "/HEAD/" + moduleName + ".ton";

    std::error_code ec;
    std::filesystem::create_directories("modules", ec);
    std::string outPath = "modules/" + moduleName + ".ton";

    std::cout << "[ton618] Downloading " << moduleName
              << (versionField ? " (" + versionField->toString() + ")" : "")
              << " from " << rawUrl << " ...\n";

    if (!downloadFile(rawUrl, outPath)) {
        std::cerr << "[ton618] Download failed.\n";
        std::remove(outPath.c_str());
        return 1;
    }

    // A missing "<name>.ton" at the repo root 404s into a short plain-text
    // body rather than failing the download command's exit code outright —
    // reject anything implausibly small rather than install it as-is.
    {
        std::ifstream check(outPath, std::ios::binary | std::ios::ate);
        if (!check || check.tellg() < 1) {
            std::cerr << "[ton618] The downloaded file is empty (wrong module name, or no '"
                       << moduleName << ".ton' at the repo root?). Aborting.\n";
            std::remove(outPath.c_str());
            return 1;
        }
    }

    std::cout << "[ton618] Installed to " << outPath << " — use IMPORT://" << moduleName
              << " in your script to load it.\n";
    return 0;
}

// `ton618 --uninstall <module>` — removes "./modules/<module>.ton", the
// exact file `install` (above) places there. Only touches that one file —
// it never contacts the registry, and it doesn't error if the module was
// installed some other way as long as it's at that same path.
static int runUninstallModule(const std::string& moduleName) {
    if (!isSafeToken(moduleName)) {
        std::cerr << "Invalid module name '" << moduleName
                   << "': only letters, digits, '.', '-', '_' are allowed.\n";
        return 1;
    }

    std::string path = "modules/" + moduleName + ".ton";
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        std::cerr << "[ton618] '" << path << "' doesn't exist — nothing to uninstall.\n";
        return 1;
    }

    if (!std::filesystem::remove(path, ec) || ec) {
        std::cerr << "[ton618] Could not remove " << path
                   << (ec ? ": " + ec.message() : "") << "\n";
        return 1;
    }

    std::cout << "[ton618] Removed " << path << ".\n";
    return 0;
}

// --update [version] — re-downloads the precompiled binary matching this
// platform (auto-detected: Linux, Termux, or Windows — see Platform.hpp)
// from GitHub Releases and replaces the currently running executable.
// `version` is a release tag (e.g. "beta-1.0.0"); omit it to grab whatever
// release is currently "latest" on GitHub.
static int runUpdate(const std::string& version) {
    std::string platform = detectPlatform();
    std::string asset = releaseAssetName();
    if (asset.empty()) {
        std::cerr << "No precompiled ton618 binary is published for this platform (" << platform << ").\n"
                   << "See https://github.com/" << TON618_REPO << " to build from source instead.\n";
        return 1;
    }

    for (char c : version) {
        if (!std::isalnum((unsigned char)c) && c != '.' && c != '-' && c != '_') {
            std::cerr << "Invalid version '" << version << "': only letters, digits, '.', '-', '_' are allowed.\n";
            return 1;
        }
    }

    std::string base = std::string("https://github.com/") + TON618_REPO + "/releases/";
    std::string url = version.empty() ? (base + "latest/download/" + asset)
                                       : (base + "download/" + version + "/" + asset);

    std::string self = getExecutablePath();
    if (self.empty()) {
        std::cerr << "Could not determine the executable's own path.\n";
        return 1;
    }
    std::string tmpPath = self + ".update";

    std::cout << "[ton618] Current version: " << TON618_VERSION << " (" << platform << "/" << detectArch() << ")\n";
    std::cout << "[ton618] Downloading " << (version.empty() ? "latest" : version) << " from " << url << " ...\n";

    if (!downloadFile(url, tmpPath)) {
        std::cerr << "[ton618] Download failed. Does that release/tag exist? "
                   << "https://github.com/" << TON618_REPO << "/releases\n";
        std::remove(tmpPath.c_str());
        return 1;
    }

    // A bad tag can 404 into a small HTML error page instead of failing the
    // download command's exit code outright — reject anything implausibly
    // small rather than install it as if it were the real binary.
    {
        std::ifstream check(tmpPath, std::ios::binary | std::ios::ate);
        if (!check || check.tellg() < 10000) {
            std::cerr << "[ton618] The downloaded file looks invalid (wrong version/tag?). Aborting.\n";
            std::remove(tmpPath.c_str());
            return 1;
        }
    }

#ifdef _WIN32
    // A running .exe can't overwrite itself directly on Windows: spawn a
    // detached helper (same trick as --uninstall-ton) that waits for this
    // process to exit, then swaps the new binary into place.
    std::string cmd = "cmd /C \"timeout /T 1 /NOBREAK >NUL & move /Y \"" + tmpPath + "\" \"" + self + "\"\"";
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> cmdline(cmd.begin(), cmd.end());
    cmdline.push_back('\0');
    CreateProcessA(NULL, cmdline.data(), NULL, NULL, FALSE,
                   CREATE_NEW_CONSOLE | DETACHED_PROCESS, NULL, NULL, &si, &pi);
    std::cout << "[ton618] Update downloaded. Finishing in the background — "
                 "run 'ton618 --version' in a new terminal shortly to confirm.\n";
#else
    chmod(tmpPath.c_str(), 0755);
    if (std::rename(tmpPath.c_str(), self.c_str()) != 0) {
        std::cerr << "[ton618] Could not replace " << self << ": " << std::strerror(errno) << "\n";
        std::remove(tmpPath.c_str());
        return 1;
    }
    std::cout << "[ton618] Updated. Run 'ton618 --version' to confirm.\n";
#endif
    return 0;
}

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);

    // Flags that work standalone, without a script file, wherever they appear.
    for (size_t i = 0; i < args.size(); i++) {
        const std::string& arg = args[i];
        if (arg == "--uninstall-ton") return runUninstall();
        if (arg == "-h" || arg == "--help") { printUsage(argv[0]); return 0; }
        if (arg == "-v" || arg == "--version") { printVersion(); return 0; }
        if (arg == "--update") {
            std::string version = (i + 1 < args.size() && args[i + 1].rfind("--", 0) != 0)
                                       ? args[i + 1] : "";
            return runUpdate(version);
        }
    }

    if (args.empty()) { printUsage(argv[0]); return 1; }

    if (args[0] == "install") {
        if (args.size() < 2) {
            std::cerr << "Usage: " << argv[0] << " install <module>\n";
            return 1;
        }
        return runInstall(args[1]);
    }

    if (args[0] == "uninstall") {
        if (args.size() < 2) {
            std::cerr << "Usage: " << argv[0] << " uninstall <module>\n";
            return 1;
        }
        return runUninstallModule(args[1]);
    }

    std::string path = args[0];
    bool debugMode = false;
    std::vector<int> breakpoints;
    // Anything after the script path that isn't a recognized flag is a script
    // argument, exposed to the running .ton script as sys_args() (see
    // ton.sys in DOCUMENTATION.md) — this is how a .ton script reads CLI input.
    std::vector<std::string> scriptArgs;

    for (size_t i = 1; i < args.size(); i++) {
        const std::string& arg = args[i];
        if (arg == "--debug") debugMode = true;
        else if (arg.rfind("--break=", 0) == 0) { debugMode = true; breakpoints.push_back(std::stoi(arg.substr(8))); }
        else scriptArgs.push_back(arg);
    }

    std::string source;
    try {
        source = readFile(path);
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    try {
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.scanTokens();

        Parser parser(tokens);
        std::vector<StmtPtr> statements = parser.parse();

        Interpreter interpreter;
        interpreter.scriptDir = dirOf(path);
        interpreter.scriptArgs = scriptArgs;
        interpreter.debugger.enabled = debugMode;
        interpreter.debugger.sourceFilename = path;
        interpreter.debugger.sourceLines = splitLines(source);
        for (int bp : breakpoints) interpreter.debugger.addBreakpoint(bp);
        if (debugMode) {
            interpreter.debugger.mode = DebugMode::STEP;
            std::cout << "=== TON618 Debugger ===\nType 'h' for help.\n";
        }

        interpreter.interpret(statements);
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
