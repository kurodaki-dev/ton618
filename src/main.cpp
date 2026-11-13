#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <filesystem>
#include <functional>
#include <regex>
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Interpreter.hpp"
#include "JsonParser.hpp"
#include "Platform.hpp"
#include "Version.hpp"
#include "Net.hpp"
#include "Packages.hpp"

#ifdef _WIN32
  #include <windows.h>
  #define popen _popen
  #define pclose _pclose
#else
  #include <unistd.h>
  #include <pthread.h>
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

// Runs `fn` on a thread with a large stack (512 MB reserved, committed only
// as it's used), so deep — but legitimate — recursion in a .ton script hits
// the interpreter's own catchable "Maximum recursion depth" error instead
// of overflowing the native stack and crashing.
static int runOnBigStack(std::function<int()> fn) {
    static std::function<int()>* job;
    static int result;
    job = &fn;
    result = 1;
    const size_t stackSize = 512u * 1024u * 1024u;
#ifdef _WIN32
    HANDLE th = CreateThread(nullptr, stackSize,
        [](LPVOID) -> DWORD { result = (*job)(); return 0; },
        nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (!th) return fn();
    WaitForSingleObject(th, INFINITE);
    CloseHandle(th);
    return result;
#else
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_t th;
    bool ok = pthread_attr_setstacksize(&attr, stackSize) == 0 &&
              pthread_create(&th, &attr, [](void*) -> void* { result = (*job)(); return nullptr; }, nullptr) == 0;
    pthread_attr_destroy(&attr);
    if (!ok) return fn();
    pthread_join(th, nullptr);
    return result;
#endif
}

static int lineFromMessage(const std::string& msg) {
    std::smatch m;
    static const std::regex re("[Ll]ine (\\d+)");
    if (std::regex_search(msg, m, re)) return std::stoi(m[1]);
    return 0;
}

static bool useColor() {
#ifdef _WIN32
    return false;
#else
    return isatty(2);
#endif
}

// Prints an uncaught error: the message, the offending source line, and —
// when the error happened inside function calls — a short traceback.
static void reportError(const std::exception& e, Interpreter& in, const std::vector<std::string>& lines,
                        const std::string& file) {
    bool color = useColor();
    const char* red = color ? "\033[1;31m" : "";
    const char* dim = color ? "\033[2m" : "";
    const char* reset = color ? "\033[0m" : "";
    std::string msg = e.what();
    int line = 0;
    if (auto* tv = dynamic_cast<const ThrownValue*>(&e)) {
        if (!in.errorTrace.empty()) line = in.errorTrace.back().line;
        msg = "Uncaught error" + (line ? " at line " + std::to_string(line) : std::string()) + ": " + tv->value.toString();
    } else {
        line = lineFromMessage(msg);
    }
    std::cerr << red << msg << reset << "\n";
    bool inModule = msg.find("in module '") != std::string::npos;
    if (line > 0 && line <= (int)lines.size() && !inModule) {
        std::string src = lines[line - 1];
        size_t start = src.find_first_not_of(" \t");
        if (start != std::string::npos) src = src.substr(start);
        std::cerr << dim << "  --> " << file << ":" << line << reset << "\n";
        std::cerr << "   " << line << " | " << src << "\n";
    }
    if (in.errorTrace.size() > 1) {
        std::cerr << dim << "Traceback (most recent call last):" << reset << "\n";
        size_t n = in.errorTrace.size();
        size_t from = n > 12 ? n - 12 : 0;
        if (from > 0) std::cerr << "  ... " << from << " earlier call(s) hidden\n";
        for (size_t i = from; i + 1 < n; i++)
            std::cerr << "  line " << in.errorTrace[i].line << ": called " << in.errorTrace[i].function << "()\n";
        std::cerr << "  line " << in.errorTrace.back().line << ": "
                  << (in.errorTrace.back().function == "throw" ? "thrown here" : "error raised here") << "\n";
    }
}

// Rough "is this input finished?" check for the REPL: are all ( [ { closed,
// ignoring anything inside string literals and comments?
static int openBrackets(const std::string& src) {
    int depth = 0;
    char quote = 0;
    for (size_t i = 0; i < src.size(); i++) {
        char c = src[i];
        if (quote) {
            if (c == '\\' && quote == '"') i++;
            else if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '`') quote = c;
        else if (c == '/' && i + 1 < src.size() && src[i + 1] == '/') { while (i < src.size() && src[i] != '\n') i++; }
        else if (c == '(' || c == '[' || c == '{') depth++;
        else if (c == ')' || c == ']' || c == '}') depth--;
    }
    return depth;
}

// The interactive prompt: "ton618" with no arguments.
static int runRepl() {
    Interpreter interpreter;
    interpreter.scriptDir = ".";
    bool tty =
#ifdef _WIN32
        true;
#else
        isatty(0);
#endif
    if (tty) {
        std::cout << "TON618 " << TON618_VERSION << " (" << detectPlatform() << "/" << detectArch() << ")\n"
                  << "Type an expression or a statement. .help for help, .exit (or Ctrl-D) to quit.\n";
    }
    std::string buffer, line;
    while (true) {
        if (tty) std::cout << (buffer.empty() ? "ton> " : "...> ") << std::flush;
        if (!std::getline(std::cin, line)) { if (tty) std::cout << "\n"; break; }
        if (buffer.empty()) {
            std::string cmd = line;
            cmd.erase(0, cmd.find_first_not_of(" \t"));
            while (!cmd.empty() && std::isspace((unsigned char)cmd.back())) cmd.pop_back();
            if (cmd == ".exit" || cmd == "exit" || cmd == "quit") break;
            if (cmd == ".help") {
                std::cout << "  Any TON618 code works here; the value of an expression is printed.\n"
                          << "  Unclosed ( [ { continue on the next line.\n"
                          << "  .vars   list the variables you defined\n"
                          << "  .exit   quit\n";
                continue;
            }
            if (cmd == ".vars") {
                for (auto& slot : interpreter.globals->slots) {
                    if (slot.value.type == ValueType::NATIVE_FUNCTION) continue;
                    std::cout << "  " << Symbols::name(slot.id) << " = " << slot.value.repr() << "\n";
                }
                continue;
            }
            if (cmd.empty()) continue;
        }
        buffer += line + "\n";
        if (openBrackets(buffer) > 0) continue;
        std::vector<std::string> lines = splitLines(buffer);
        try {
            Lexer lexer(buffer);
            Parser parser(lexer.scanTokens());
            auto stmts = parser.parse();
            Value v = interpreter.interpretRepl(stmts);
            if (v.type != ValueType::NIL) std::cout << v.repr() << "\n";
        } catch (std::exception& e) {
            std::cout.flush();
            reportError(e, interpreter, lines, "<repl>");
        }
        interpreter.errorTrace.clear();
        buffer.clear();
    }
    return 0;
}

static void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " <file.ton> [options]   run a script\n"
              << "       " << prog << "                          start the interactive REPL\n"
              << "       " << prog << " -e \"code\"                run a one-liner\n"
              << "Options:\n"
              << "  --check <file.ton>    check the syntax without running the script\n"
              << "  --debug              run in debug mode (pause at start)\n"
              << "  --break=<line>        add a breakpoint at a given line (repeatable)\n"
              << "  --uninstall-ton       remove this ton618 install (binary + PATH entry + ./modules/)\n"
              << "  --update [version]    update ton618 from GitHub Releases (latest if no version given)\n"
              << "  --version, -v         print the interpreter's version, platform and architecture\n"
              << "  --help, -h            show this message\n"
              << "\nOther commands:\n"
              << "  " << prog << " init [name]           start a module project (creates tonproject.toml)\n"
              << "  " << prog << " build                 pack the project into dist/<name>-<version>.tonpkg\n"
              << "  " << prog << " publish               share the built package through the registry\n"
              << "  " << prog << " install [pkg[@range]]  install packages into ./modules/ (no name: those in tonproject.toml)\n"
              << "  " << prog << " update [pkg]           update installed packages\n"
              << "  " << prog << " uninstall <pkg>       remove an installed package\n"
              << "  " << prog << " list [--outdated]     show installed packages\n"
              << "  " << prog << " search [text]          look for packages;  info <pkg>  to see one\n"
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

    // Also wipe out any modules `install` fetched into the current
    // directory's ./modules/ — the same folder `install`/`uninstall
    // <module>` operate on. It's per-project, not tied to where the binary
    // itself lives, so this only ever touches the directory you actually
    // run `--uninstall-ton` from.
    {
        std::error_code ec;
        if (std::filesystem::exists("modules", ec)) {
            std::uintmax_t removed = std::filesystem::remove_all("modules", ec);
            if (ec) {
                std::cerr << "[ton618] Could not remove ./modules/: " << ec.message() << "\n";
            } else {
                std::cout << "[ton618] Removed ./modules/ (" << removed << " item(s)).\n";
            }
        }
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

    // The registry entry's name can carry whatever case whoever published it
    // typed (and lookups above used moduleName exactly as the user typed it,
    // since that's what the registry indexes on) — but GitHub repo files and
    // TON618's own module-naming convention are lowercase (every existing
    // module is, e.g. "atome.ton"), and raw.githubusercontent.com URLs are
    // case-sensitive. Downloading and installing under a lowercased name
    // means "ton618 install Atome" still finds "atome.ton" at the repo root
    // instead of 404ing on a case mismatch, and the installed file's name
    // matches what IMPORT://<module> (itself case-sensitive on the
    // filesystem) will actually look for.
    std::string fileName = moduleName;
    for (char& c : fileName) c = (char)std::tolower((unsigned char)c);

    std::string rawUrl = "https://raw.githubusercontent.com/" + ownerRepo + "/HEAD/" + fileName + ".ton";

    std::error_code ec;
    std::filesystem::create_directories("modules", ec);
    std::string outPath = "modules/" + fileName + ".ton";

    std::cout << "[ton618] Downloading " << fileName
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
                       << fileName << ".ton' at the repo root?). Aborting.\n";
            std::remove(outPath.c_str());
            return 1;
        }
    }

    recordLegacyInstall(fileName, versionField ? versionField->toString() : "", ownerRepo);
    std::cout << "[ton618] Installed to " << outPath << " — use IMPORT://" << fileName
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

    std::string fileName = moduleName;
    for (char& c : fileName) c = (char)std::tolower((unsigned char)c);

    std::string path = "modules/" + fileName + ".ton";
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
// The tag of the newest GitHub release ("" when it can't be found): GitHub
// answers /releases/latest with a redirect to /releases/tag/<tag>.
static std::string latestReleaseTag() {
    HttpOptions o;
    o.url = std::string("https://github.com/") + TON618_REPO + "/releases/latest?t=" + std::to_string((long long)std::time(nullptr));
    o.followRedirects = false;
    o.timeoutSeconds = 15;
    HttpResult r = curlRequest(o);
    if (!r.ok) return "";
    for (auto& h : r.headers) {
        if (h.first != "location") continue;
        size_t at = h.second.rfind("/tag/");
        if (at == std::string::npos) return "";
        std::string tag = h.second.substr(at + 5);
        while (!tag.empty() && (tag.back() == '\r' || tag.back() == '\n' || tag.back() == ' ')) tag.pop_back();
        for (char c : tag) if (!std::isalnum((unsigned char)c) && c != '.' && c != '-' && c != '_') return "";
        return tag;
    }
    return "";
}

// Is release tag `a` newer than `b`? Stable versions (1.0.0) are newer than the
// beta-* series that came before; inside a series the numbers decide.
static bool releaseIsNewer(const std::string& a, const std::string& b) {
    auto parse = [](std::string v, int& stable, int n[3]) {
        stable = 1;
        if (v.rfind("beta-", 0) == 0) { stable = 0; v = v.substr(5); }
        else if (!v.empty() && v[0] == 'v') v = v.substr(1);
        n[0] = n[1] = n[2] = 0;
        std::sscanf(v.c_str(), "%d.%d.%d", &n[0], &n[1], &n[2]);
    };
    int sa, sb, na[3], nb[3];
    parse(a, sa, na);
    parse(b, sb, nb);
    if (sa != sb) return sa > sb;
    for (int i = 0; i < 3; i++) if (na[i] != nb[i]) return na[i] > nb[i];
    return false;
}

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

    // Without an explicit version: look up the newest release first, and stop
    // if this binary already is it (set TON618_FORCE_UPDATE=1 to reinstall).
    if (version.empty() && !std::getenv("TON618_FORCE_UPDATE")) {
        std::string latest = latestReleaseTag();
        if (!latest.empty()) {
            std::cout << "[ton618] Current version: " << TON618_VERSION << "  -  latest release: " << latest << "\n";
            if (!releaseIsNewer(latest, TON618_VERSION)) {
                std::cout << "[ton618] You already have the newest version.\n";
                return 0;
            }
        }
    }

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

    // TON618_UNBUFFERED=1: flush after every write, so a program's output
    // shows up live when ton618 runs behind a pipe (the IDE, editor
    // extensions) instead of all at once when it exits.
    if (const char* ub = std::getenv("TON618_UNBUFFERED")) {
        if (*ub && std::string(ub) != "0") { std::cout << std::unitbuf; std::cerr << std::unitbuf; }
    }

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

    // No arguments at all: the interactive REPL.
    if (args.empty() || args[0] == "repl") return runOnBigStack([]() { return runRepl(); });

    // ton618 --check file.ton — parse only: report syntax errors (as
    // "file:line: message") without running anything. Used by the editor
    // extensions (VS Code, Acode) and the IDE to underline mistakes.
    if (args[0] == "--check") {
        if (args.size() < 2) { std::cerr << "Usage: " << argv[0] << " --check <file.ton>\n"; return 1; }
        std::string src;
        try { src = readFile(args[1]); } catch (std::exception& e) { std::cerr << "Error: " << e.what() << "\n"; return 1; }
        try {
            Lexer lexer(src);
            Parser parser(lexer.scanTokens());
            parser.parse();
        } catch (std::exception& e) {
            std::string msg = e.what();
            std::cerr << args[1] << ":" << lineFromMessage(msg) << ": " << msg << "\n";
            return 1;
        }
        std::cout << args[1] << ": OK\n";
        return 0;
    }

    // ton618 -e "print(1 + 2)" — run a one-liner.
    if (args[0] == "-e" || args[0] == "--eval") {
        if (args.size() < 2) { std::cerr << "Usage: " << argv[0] << " -e \"code\"\n"; return 1; }
        std::string code = args[1];
        return runOnBigStack([&]() -> int {
            Interpreter interpreter;
            interpreter.scriptDir = ".";
            std::vector<std::string> lines = splitLines(code);
            try {
                Lexer lexer(code);
                Parser parser(lexer.scanTokens());
                interpreter.interpret(parser.parse());
            } catch (std::exception& e) {
                std::cout.flush();
                reportError(e, interpreter, lines, "<eval>");
                return 1;
            }
            std::cout.flush();
            return 0;
        });
    }

    // The package manager: init, build, publish, install, update, uninstall, list, search, info.
    // (A file with exactly that name in the current folder is still run as a script.)
    if (isPackageCommand(args[0]) && !(args[0] != "install" && args[0] != "uninstall" && std::filesystem::is_regular_file(args[0]))) {
        return runPackageCommand(args, runInstall, runUninstallModule);
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

    return runOnBigStack([&]() -> int {
        Interpreter interpreter;
        std::vector<std::string> lines = splitLines(source);
        try {
            Lexer lexer(source);
            std::vector<Token> tokens = lexer.scanTokens();

            Parser parser(tokens);
            std::vector<StmtPtr> statements = parser.parse();

            interpreter.scriptDir = dirOf(path);
            interpreter.scriptArgs = scriptArgs;
            interpreter.debugger.enabled = debugMode;
            interpreter.debugger.sourceFilename = path;
            interpreter.debugger.sourceLines = lines;
            for (int bp : breakpoints) interpreter.debugger.addBreakpoint(bp);
            if (debugMode) {
                interpreter.debugger.mode = DebugMode::STEP;
                std::cout << "=== TON618 Debugger ===\nType 'h' for help.\n";
            }

            interpreter.interpret(statements);
        } catch (std::exception& e) {
            std::cout.flush();
            reportError(e, interpreter, lines, path);
            return 1;
        }
        std::cout.flush();
        return 0;
    });

}
