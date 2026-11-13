#include "Packages.hpp"
#include "JsonParser.hpp"
#include "Lexer.hpp"
#include "Net.hpp"
#include "Parser.hpp"
#include "Toml.hpp"
#include "Version.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace {

const char* const MANIFEST = "tonproject.toml";
const size_t MAX_FILE_BYTES = 256 * 1024;
const size_t MAX_TOTAL_BYTES = 1024 * 1024;
const size_t MAX_FILES = 200;

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

void say(const std::string& m) { std::cout << "[ton618] " << m << "\n"; }
int fail(const std::string& m) { std::cerr << "[ton618] " << m << "\n"; return 1; }

std::string registryBase() {
    const char* env = std::getenv("TON618_PACKAGES_URL");
    std::string url = env && *env ? env : TON618_PACKAGE_REGISTRY_URL;
    while (!url.empty() && url.back() == '/') url.pop_back();
    return url;
}

std::string registryRepo() {
    const char* env = std::getenv("TON618_REGISTRY_REPO");
    return env && *env ? env : "kurodaki-dev/ton618";
}

bool readFileBytes(const fs::path& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool writeFileBytes(const fs::path& path, const std::string& content) {
    std::error_code ec;
    if (path.has_parent_path()) fs::create_directories(path.parent_path(), ec);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << content;
    return (bool)f;
}

bool validUtf8(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = (unsigned char)s[i];
        int n = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : -1;
        if (n < 0) return false;
        for (int k = 1; k <= n; k++) {
            if (i + (size_t)k >= s.size() || ((unsigned char)s[i + (size_t)k] & 0xC0) != 0x80) return false;
        }
        i += (size_t)n + 1;
    }
    return true;
}

// "mathx", "string-utils": lowercase, starts with a letter, 2-40 characters.
bool validPackageName(const std::string& n) {
    if (n.size() < 2 || n.size() > 40 || !std::islower((unsigned char)n[0])) return false;
    for (char c : n) {
        if (!(std::islower((unsigned char)c) || std::isdigit((unsigned char)c) || c == '_' || c == '-')) return false;
    }
    return n != "ton" && n != "modules" && n != "ton618";
}

std::string str(const Value& d, const char* key, const std::string& fallback = "") {
    Value* v = d.findDictEntry(key);
    return v && (v->type == ValueType::STRING || v->type == ValueType::HTML) ? v->str : fallback;
}

void setKey(Value& d, const std::string& key, const Value& v) {
    if (Value* e = d.findDictEntry(key)) *e = v;
    else d.dict->push_back({key, v});
}

bool delKey(Value& d, const std::string& key) {
    if (!d.dict) return false;
    for (size_t i = 0; i < d.dict->size(); i++) {
        if ((*d.dict)[i].first == key) { d.dict->erase(d.dict->begin() + (long)i); return true; }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Versions: 1.2.3 and 1.2.3-beta.1, and ranges (^1.2, ~1.2.3, >=1.0 <2, 1.x)
// ---------------------------------------------------------------------------

struct SemVer {
    long n[3] = {0, 0, 0};
    int parts = 3;      // how many numbers were written (a range may say "1.2")
    std::string pre;    // "beta.1"
};

bool parseSemVer(const std::string& text, SemVer& out, bool partial = false) {
    out = SemVer();
    size_t i = 0;
    int k = 0;
    while (k < 3) {
        size_t start = i;
        while (i < text.size() && std::isdigit((unsigned char)text[i])) i++;
        if (i == start || i - start > 9) return false;
        out.n[k++] = std::stol(text.substr(start, i - start));
        if (i < text.size() && text[i] == '.' && k < 3) { i++; continue; }
        break;
    }
    out.parts = k;
    if (k < 3 && !partial) return false;
    if (i < text.size() && text[i] == '-') {
        out.pre = text.substr(i + 1);
        if (out.pre.empty()) return false;
        for (char c : out.pre) if (!std::isalnum((unsigned char)c) && c != '.' && c != '-') return false;
        i = text.size();
    }
    return i == text.size();
}

int comparePre(const std::string& a, const std::string& b) {
    if (a.empty() && b.empty()) return 0;
    if (a.empty()) return 1;   // 1.0.0 is newer than 1.0.0-beta
    if (b.empty()) return -1;
    auto split = [](const std::string& s) {
        std::vector<std::string> out; std::stringstream ss(s); std::string part;
        while (std::getline(ss, part, '.')) out.push_back(part);
        return out;
    };
    auto pa = split(a), pb = split(b);
    for (size_t i = 0; i < std::min(pa.size(), pb.size()); i++) {
        bool na = std::all_of(pa[i].begin(), pa[i].end(), ::isdigit), nb = std::all_of(pb[i].begin(), pb[i].end(), ::isdigit);
        if (na && nb) { long x = std::stol(pa[i]), y = std::stol(pb[i]); if (x != y) return x < y ? -1 : 1; }
        else if (na != nb) return na ? -1 : 1;
        else if (pa[i] != pb[i]) return pa[i] < pb[i] ? -1 : 1;
    }
    return pa.size() == pb.size() ? 0 : (pa.size() < pb.size() ? -1 : 1);
}

int compareVersions(const SemVer& a, const SemVer& b) {
    for (int i = 0; i < 3; i++) if (a.n[i] != b.n[i]) return a.n[i] < b.n[i] ? -1 : 1;
    return comparePre(a.pre, b.pre);
}

struct Bound { std::string op; SemVer v; };      // op: >= > <= < =
struct Range { std::vector<Bound> bounds; bool allowPre = false; };

SemVer bumped(const SemVer& v, int index) {
    SemVer r;
    for (int i = 0; i < index; i++) r.n[i] = v.n[i];
    r.n[index] = v.n[index] + 1;
    return r;
}

// Parses "^1.2.3", "~1.2", ">=1.0 <2.0", "1.x", "1.2.3", "*" ... Returns false if malformed.
bool parseRange(const std::string& text, Range& range) {
    range = Range();
    std::string spaced = text;
    for (char& c : spaced) if (c == ',') c = ' ';
    std::stringstream ss(spaced);
    std::string tok;
    while (ss >> tok) {
        if (tok == "*" || tok == "x" || tok == "X" || tok == "latest") continue;
        std::string op;
        std::string rest = tok;
        for (const char* o : {">=", "<=", ">", "<", "=", "^", "~"}) {
            if (tok.rfind(o, 0) == 0) { op = o; rest = tok.substr(std::string(o).size()); break; }
        }
        SemVer v;
        // 1.x / 1.2.x / 1.*
        bool wildcard = false;
        if (op.empty() && !rest.empty() && (rest.back() == 'x' || rest.back() == 'X' || rest.back() == '*')) {
            rest.pop_back();
            if (!rest.empty() && rest.back() == '.') rest.pop_back();
            wildcard = true;
        }
        if (!parseSemVer(rest, v, true)) return false;
        if (!v.pre.empty()) range.allowPre = true;
        SemVer low = v;
        if (op == "^") {
            int bump;
            int p = std::min(v.parts, 3), firstNonZero = -1;
            for (int i = 0; i < p; i++) if (v.n[i] != 0) { firstNonZero = i; break; }
            bump = firstNonZero >= 0 ? firstNonZero : p - 1;
            range.bounds.push_back({">=", low});
            range.bounds.push_back({"<", bumped(v, bump)});
        } else if (op == "~") {
            range.bounds.push_back({">=", low});
            range.bounds.push_back({"<", bumped(v, v.parts >= 2 ? 1 : 0)});
        } else if (wildcard) {
            if (v.parts == 0) continue;
            range.bounds.push_back({">=", low});
            range.bounds.push_back({"<", bumped(v, v.parts - 1)});
        } else if (op.empty() || op == "=") {
            if (v.parts == 3) range.bounds.push_back({"=", v});
            else { range.bounds.push_back({">=", low}); range.bounds.push_back({"<", bumped(v, v.parts - 1)}); }
            if (v.parts == 3) range.allowPre = range.allowPre || !v.pre.empty();
        } else {
            range.bounds.push_back({op, v});
        }
    }
    return true;
}

bool satisfies(const Range& range, const SemVer& v) {
    if (!v.pre.empty() && !range.allowPre) return false;
    for (const Bound& b : range.bounds) {
        int c = compareVersions(v, b.v);
        bool ok = b.op == ">=" ? c >= 0 : b.op == ">" ? c > 0 : b.op == "<=" ? c <= 0 : b.op == "<" ? c < 0 : c == 0;
        if (!ok) return false;
    }
    return true;
}

bool validRange(const std::string& text) { Range r; return parseRange(text, r); }

SemVer runningVersion() {
    std::string v = TON618_VERSION;
    if (v.rfind("beta-", 0) == 0) v = v.substr(5);
    else if (!v.empty() && v[0] == 'v') v = v.substr(1);
    SemVer out;
    parseSemVer(v, out, true);
    return out;
}

// ---------------------------------------------------------------------------
// The files of a package
// ---------------------------------------------------------------------------

bool allowedExtension(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    return ext == ".ton" || ext == ".md" || ext == ".txt" || ext == ".json" || ext == ".csv";
}

// A path inside a package: relative, no "..", no drive or backslash, plain characters.
bool safePackagePath(const std::string& p) {
    if (p.empty() || p.size() > 200 || p[0] == '/' || p.back() == '/') return false;
    std::stringstream ss(p);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (part.empty() || part == "." || part == ".." || part[0] == '.') return false;
    }
    for (char c : p) {
        if (!(std::isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-' || c == '/')) return false;
    }
    return allowedExtension(p);
}

// '*' matches within one path segment, '**' across segments.
bool globMatch(const std::string& pat, size_t pi, const std::string& s, size_t si) {
    while (pi < pat.size()) {
        if (pat[pi] == '*') {
            bool deep = pi + 1 < pat.size() && pat[pi + 1] == '*';
            size_t next = pi + (deep ? 2 : 1);
            for (size_t k = si; k <= s.size(); k++) {
                if (globMatch(pat, next, s, k)) return true;
                if (k < s.size() && s[k] == '/' && !deep) break;
            }
            return false;
        }
        if (si >= s.size() || pat[pi] != s[si]) return false;
        pi++; si++;
    }
    return si == s.size();
}

bool matchesPattern(const std::string& pattern, const std::string& rel) {
    if (pattern.empty()) return false;
    if (pattern.back() == '/') return rel.rfind(pattern, 0) == 0;
    if (pattern.find('*') == std::string::npos) return rel == pattern || rel.rfind(pattern + "/", 0) == 0;
    // like .gitignore: a pattern without '/' matches the file name at any depth
    if (pattern.find('/') == std::string::npos) {
        return globMatch(pattern, 0, fs::path(rel).filename().string(), 0);
    }
    return globMatch(pattern, 0, rel, 0);
}

std::vector<std::string> stringList(const Value* v) {
    std::vector<std::string> out;
    if (v && v->type == ValueType::ARRAY) for (auto& x : *v->array) if (x.type == ValueType::STRING) out.push_back(x.str);
    return out;
}

// ---------------------------------------------------------------------------
// tonproject.toml -> .tonpkg
// ---------------------------------------------------------------------------

struct Built {
    std::string name, version, path, sha256;
    size_t size = 0, fileCount = 0;
    Value package;
};

bool readManifest(const fs::path& dir, Value& root, std::string& error) {
    std::string text;
    if (!readFileBytes(dir / MANIFEST, text)) {
        error = std::string("No ") + MANIFEST + " here. Create one with: ton618 init";
        return false;
    }
    try { root = parseToml(text, MANIFEST); }
    catch (std::exception& e) { error = e.what(); return false; }
    return true;
}

bool buildPackage(const fs::path& dir, Built& out, std::string& error, bool quiet) {
    Value root;
    if (!readManifest(dir, root, error)) return false;
    Value* project = root.findDictEntry("project");
    if (!project || project->type != ValueType::DICT) { error = std::string(MANIFEST) + ": missing the [project] table."; return false; }

    std::string name = str(*project, "name"), version = str(*project, "version");
    if (!validPackageName(name)) {
        error = std::string(MANIFEST) + ": project.name must be 2-40 characters: lowercase letters, digits, '-' or '_', starting with a letter (and not 'ton', 'modules' or 'ton618').";
        return false;
    }
    SemVer sv;
    if (!parseSemVer(version, sv)) { error = std::string(MANIFEST) + ": project.version must look like 1.2.3 (or 1.2.3-beta.1)."; return false; }
    std::string requires_ = str(*project, "requires");
    if (!requires_.empty() && !validRange(requires_)) { error = std::string(MANIFEST) + ": project.requires isn't a valid version range."; return false; }

    // dependencies
    Value deps = Value::Dict({});
    if (Value* d = root.findDictEntry("dependencies")) {
        if (d->type != ValueType::DICT) { error = std::string(MANIFEST) + ": [dependencies] must be a table."; return false; }
        for (auto& kv : *d->dict) {
            std::string range = kv.second.type == ValueType::STRING ? kv.second.str : str(kv.second, "version", "*");
            if (!validPackageName(kv.first)) { error = std::string(MANIFEST) + ": '" + kv.first + "' isn't a valid package name."; return false; }
            if (kv.first == name) { error = std::string(MANIFEST) + ": a package can't depend on itself."; return false; }
            if (!validRange(range)) { error = std::string(MANIFEST) + ": the range \"" + range + "\" for " + kv.first + " isn't valid (try \"^1.0\")."; return false; }
            setKey(deps, kv.first, Value::String(range));
        }
    }

    // which files go in
    std::string main = str(*project, "main", name + ".ton");
    std::string readme = str(*project, "readme");
    if (readme.empty() && fs::exists(dir / "README.md")) readme = "README.md";
    Value* build = root.findDictEntry("build");
    std::vector<std::string> include = build ? stringList(build->findDictEntry("include")) : std::vector<std::string>();
    std::vector<std::string> exclude = build ? stringList(build->findDictEntry("exclude")) : std::vector<std::string>();
    bool customInclude = !include.empty();
    if (!customInclude) include = {"*.ton", "README*", "LICENSE*", "CHANGELOG*"};

    std::set<std::string> skipDirs = {".git", "dist", "modules", "node_modules"};
    if (!customInclude) for (const char* d : {"tests", "test", "examples", "exemples"}) skipDirs.insert(d);

    std::vector<std::string> paths;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        std::string rel = fs::relative(it->path(), dir).generic_string();
        std::string leaf = it->path().filename().string();
        if (it->is_directory()) {
            if (skipDirs.count(leaf) || leaf[0] == '.') it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file() || leaf[0] == '.') continue;
        bool wanted = rel == main || (!readme.empty() && rel == readme);
        for (auto& p : include) if (matchesPattern(p, rel)) wanted = true;
        for (auto& p : exclude) if (matchesPattern(p, rel) && rel != main) wanted = false;
        if (wanted) paths.push_back(rel);
    }
    std::sort(paths.begin(), paths.end());
    if (paths.size() > MAX_FILES) { error = "Too many files (" + std::to_string(paths.size()) + "; the limit is " + std::to_string(MAX_FILES) + ")."; return false; }

    Value files = Value::Dict({});
    size_t total = 0;
    bool hasMain = false, syntaxOk = true;
    for (const std::string& rel : paths) {
        if (!safePackagePath(rel)) {
            error = "'" + rel + "' can't go in a package: use plain names (letters, digits, '-', '_', '.') and one of .ton .md .txt .json .csv.";
            return false;
        }
        std::string content;
        if (!readFileBytes(dir / rel, content)) { error = "Could not read " + rel; return false; }
        if (content.size() > MAX_FILE_BYTES) { error = rel + " is " + std::to_string(content.size() / 1024) + " KB; the limit is " + std::to_string(MAX_FILE_BYTES / 1024) + " KB per file."; return false; }
        if (!validUtf8(content)) { error = rel + " isn't valid UTF-8 text."; return false; }
        total += content.size();
        if (rel == main) hasMain = true;
        if (fs::path(rel).extension() == ".ton") {
            try {
                Lexer lexer(content);
                Parser parser(lexer.scanTokens());
                parser.parse();
            } catch (std::exception& e) {
                std::cerr << "[ton618] " << rel << ": " << e.what() << "\n";
                syntaxOk = false;
            }
        }
        setKey(files, rel, Value::String(content));
    }
    if (!hasMain) { error = "The main file '" + main + "' doesn't exist (set project.main in " + MANIFEST + ")."; return false; }
    if (!syntaxOk) { error = "Fix the syntax errors above, then build again."; return false; }
    if (total > MAX_TOTAL_BYTES) { error = "The package is " + std::to_string(total / 1024) + " KB; the limit is " + std::to_string(MAX_TOTAL_BYTES / 1024) + " KB."; return false; }

    Value pkg = Value::Dict({});
    setKey(pkg, "format", Value::Number(1));
    setKey(pkg, "name", Value::String(name));
    setKey(pkg, "version", Value::String(version));
    setKey(pkg, "description", Value::String(str(*project, "description")));
    std::vector<Value> authors;
    for (auto& a : stringList(project->findDictEntry("authors"))) authors.push_back(Value::String(a));
    if (authors.empty() && !str(*project, "author").empty()) authors.push_back(Value::String(str(*project, "author")));
    setKey(pkg, "authors", Value::Array(authors));
    setKey(pkg, "license", Value::String(str(*project, "license")));
    std::string homepage = str(*project, "repository", str(*project, "homepage"));
    setKey(pkg, "homepage", Value::String(homepage));
    std::vector<Value> keywords;
    for (auto& k : stringList(project->findDictEntry("keywords"))) keywords.push_back(Value::String(k));
    setKey(pkg, "keywords", Value::Array(keywords));
    setKey(pkg, "requires", Value::String(requires_));
    setKey(pkg, "main", Value::String(main));
    setKey(pkg, "dependencies", deps);
    setKey(pkg, "files", files);

    std::string json = pkg.toJsonPretty() + "\n";
    fs::path distDir = dir / "dist";
    fs::path outPath = distDir / (name + "-" + version + ".tonpkg");
    if (!writeFileBytes(outPath, json)) { error = "Could not write " + outPath.string(); return false; }

    out.name = name; out.version = version; out.path = outPath.generic_string();
    out.sha256 = sha256Hex(json); out.size = json.size(); out.fileCount = paths.size(); out.package = pkg;
    if (!quiet) {
        say("Built " + out.path + " (" + std::to_string(out.size) + " bytes, " + std::to_string(paths.size()) + " files)");
        for (const std::string& rel : paths) std::cout << "         " << rel << (rel == main ? "   (main)" : "") << "\n";
        say("sha256 " + out.sha256);
        if (str(*project, "description").empty()) say("Tip: add a description to [project] so people can find it with 'ton618 search'.");
        if (str(*project, "license").empty()) say("Tip: add a license to [project] (for example license = \"MIT\").");
    }
    return true;
}

// ---------------------------------------------------------------------------
// The registry index and the installed-modules record
// ---------------------------------------------------------------------------

bool loadIndex(Value& index, std::string& error) {
    std::string base = registryBase();
    bool ok = false;
    std::string body = fetchText(base + "/index.json", ok);
    if (!ok || body.empty()) { error = "Could not reach the package registry (" + base + ")."; return false; }
    try { index = parseJson(body); }
    catch (std::exception& e) { error = std::string("The registry's index isn't valid JSON: ") + e.what(); return false; }
    Value* packages = index.findDictEntry("packages");
    if (!packages || packages->type != ValueType::DICT) { error = "The registry's index has no 'packages'."; return false; }
    return true;
}

struct Candidate { std::string version; SemVer sv; const Value* info; };

// Versions of `entry` that satisfy `range`, newest first (yanked ones skipped).
std::vector<Candidate> candidatesOf(const Value& entry, const Range& range) {
    std::vector<Candidate> out;
    Value* versions = entry.findDictEntry("versions");
    if (!versions || versions->type != ValueType::DICT) return out;
    for (auto& kv : *versions->dict) {
        SemVer sv;
        if (!parseSemVer(kv.first, sv)) continue;
        Value* yanked = kv.second.findDictEntry("yanked");
        if (yanked && yanked->isTruthy()) continue;
        if (satisfies(range, sv)) out.push_back({kv.first, sv, &kv.second});
    }
    std::sort(out.begin(), out.end(), [](const Candidate& a, const Candidate& b) { return compareVersions(a.sv, b.sv) > 0; });
    return out;
}

const char* const DB_PATH = "modules/.installed.json";

Value loadDb() {
    std::string text;
    if (readFileBytes(DB_PATH, text)) {
        try {
            Value v = parseJson(text);
            if (v.findDictEntry("packages")) return v;
        } catch (std::exception&) {}
    }
    Value db = Value::Dict({});
    setKey(db, "format", Value::Number(1));
    setKey(db, "packages", Value::Dict({}));
    return db;
}

bool saveDb(const Value& db) { return writeFileBytes(DB_PATH, db.toJsonPretty() + "\n"); }

Value* installedPackages(Value& db) { return db.findDictEntry("packages"); }

// Packages (other than `name`) that list `name` among their dependencies.
std::vector<std::pair<std::string, std::string>> dependentsOf(Value& db, const std::string& name) {
    std::vector<std::pair<std::string, std::string>> out;
    for (auto& kv : *installedPackages(db)->dict) {
        if (kv.first == name) continue;
        Value* deps = kv.second.findDictEntry("dependencies");
        if (!deps || deps->type != ValueType::DICT) continue;
        if (Value* r = deps->findDictEntry(name)) out.push_back({kv.first, r->toString()});
    }
    return out;
}

void removePackageFiles(const Value& entry) {
    std::set<fs::path> dirs;
    Value* files = entry.findDictEntry("files");
    if (!files || files->type != ValueType::ARRAY) return;
    for (auto& f : *files->array) {
        std::string rel = f.toString();
        if (rel.rfind("modules/", 0) != 0 || rel.find("..") != std::string::npos) continue; // only ever inside modules/
        std::error_code ec;
        fs::remove(rel, ec);
        for (fs::path p = fs::path(rel).parent_path(); p.generic_string() != "modules" && !p.empty(); p = p.parent_path()) dirs.insert(p);
    }
    for (auto it = dirs.rbegin(); it != dirs.rend(); ++it) { std::error_code ec; fs::remove(*it, ec); } // only removes empty ones
}

// ---------------------------------------------------------------------------
// install / update / uninstall
// ---------------------------------------------------------------------------

struct Installer {
    Value index;
    bool haveIndex = false;
    Value db = loadDb();
    std::function<int(const std::string&)> legacy;
    bool force = false;
    std::set<std::string> stack;
    std::string error;

    bool needIndex() {
        if (haveIndex) return true;
        if (!loadIndex(index, error)) return false;
        haveIndex = true;
        return true;
    }

    const Value* entryFor(const std::string& name) {
        Value* packages = index.findDictEntry("packages");
        return packages ? packages->findDictEntry(name) : nullptr;
    }

    // `explicitRequest`: the user typed this name (so it may replace another version).
    bool installOne(const std::string& name, const std::string& rangeText, bool explicitRequest) {
        if (!validPackageName(name)) { error = "'" + name + "' isn't a valid package name."; return false; }
        if (stack.count(name)) return true; // a cycle: the package is already being installed
        Range range;
        if (!parseRange(rangeText, range)) { error = "'" + rangeText + "' isn't a valid version range (try 1.2.0, ^1.2 or >=1.0)."; return false; }

        Value* installed = installedPackages(db)->findDictEntry(name);
        SemVer have;
        if (installed) parseSemVer(str(*installed, "version"), have);
        if (installed && satisfies(range, have)) {
            if (explicitRequest) say(name + " " + str(*installed, "version") + " is already installed. (ton618 update " + name + " to get a newer one.)");
            return true;
        }
        if (installed && !explicitRequest) {
            error = "Conflict: a package needs " + name + " " + rangeText + ", but " + str(*installed, "version") + " is installed. Run 'ton618 install " + name + "@" + rangeText + "' to replace it.";
            return false;
        }

        if (!needIndex()) return false;
        const Value* entry = entryFor(name);
        if (!entry) {
            if (explicitRequest && legacy && (rangeText.empty() || rangeText == "*")) {
                say("'" + name + "' isn't in the package registry; trying the original module registry ...");
                if (legacy(name) == 0) return true;
                error = "Not found. Try: ton618 search " + name;
                return false;
            }
            error = "No package called '" + name + "' in the registry. Try: ton618 search " + name;
            return false;
        }
        std::vector<Candidate> found = candidatesOf(*entry, range);
        if (found.empty()) {
            std::string available;
            for (auto& c : candidatesOf(*entry, Range())) available += (available.empty() ? "" : ", ") + c.version;
            error = "No version of " + name + " matches '" + rangeText + "'." + (available.empty() ? "" : " Available: " + available + ".");
            return false;
        }
        const Candidate& pick = found.front();
        if (installed && !force) { // don't break a package that needs the version now installed
            for (auto& dep : dependentsOf(db, name)) {
                Range needed;
                if (parseRange(dep.second, needed) && !satisfies(needed, pick.sv)) {
                    error = dep.first + " needs " + name + " " + dep.second + ", which " + pick.version + " doesn't satisfy. Use --force to install it anyway.";
                    return false;
                }
            }
        }
        return installVersion(name, pick, installed != nullptr);
    }

    bool installVersion(const std::string& name, const Candidate& pick, bool replacing) {
        std::string expectedSha = str(*pick.info, "sha256");
        std::string file = str(*pick.info, "file");
        if (expectedSha.size() != 64 || file.empty() || file.find("..") != std::string::npos || file[0] == '/') {
            error = "The registry entry for " + name + " " + pick.version + " is incomplete.";
            return false;
        }
        if (!replacing && !force && fs::exists("modules/" + name + ".ton")) {
            error = "modules/" + name + ".ton already exists and isn't managed by ton618. Use --force to replace it.";
            return false;
        }

        std::string url = registryBase() + "/" + file;
        fs::path tmp = fs::temp_directory_path() / ("ton618-" + name + "-" + pick.version + ".tonpkg");
        say("Downloading " + name + " " + pick.version + " ...");
        if (!downloadFile(url, tmp.string())) { error = "Download failed (" + url + ")."; return false; }
        std::string bytes;
        bool read = readFileBytes(tmp, bytes);
        std::error_code ec;
        fs::remove(tmp, ec);
        if (!read) { error = "Could not read the downloaded file."; return false; }
        if (sha256Hex(bytes) != expectedSha) { error = "Checksum mismatch for " + name + " " + pick.version + ": the download isn't what the registry lists. Nothing was installed."; return false; }

        Value pkg;
        try { pkg = parseJson(bytes); }
        catch (std::exception& e) { error = std::string("The package isn't valid: ") + e.what(); return false; }
        Value* files = pkg.findDictEntry("files");
        if (pkg.findDictEntry("format") == nullptr || pkg.findDictEntry("format")->number != 1 ||
            str(pkg, "name") != name || str(pkg, "version") != pick.version || !files || files->type != ValueType::DICT) {
            error = "The package's contents don't match its registry entry."; return false;
        }
        std::string main = str(pkg, "main");
        if (!files->findDictEntry(main)) { error = "The package's main file is missing."; return false; }
        size_t total = 0;
        for (auto& kv : *files->dict) {
            if (!safePackagePath(kv.first) || kv.second.type != ValueType::STRING) { error = "The package holds an unsafe file name ('" + kv.first + "'). Nothing was installed."; return false; }
            total += kv.second.str.size();
        }
        if (files->dict->size() > MAX_FILES || total > MAX_TOTAL_BYTES) { error = "The package is larger than allowed."; return false; }

        std::string requires_ = str(pkg, "requires");
        Range needs;
        if (!requires_.empty() && !force && parseRange(requires_, needs)) {
            SemVer me = runningVersion();
            bool ok = true;
            for (const Bound& b : needs.bounds) {
                int c = compareVersions(me, b.v);
                ok = ok && (b.op == ">=" ? c >= 0 : b.op == ">" ? c > 0 : b.op == "<=" ? c <= 0 : b.op == "<" ? c < 0 : c == 0);
            }
            if (!ok) { error = name + " " + pick.version + " needs ton618 " + requires_ + " (you have " + TON618_VERSION + "). Update with: ton618 --update"; return false; }
        }

        // dependencies first, so a failure leaves the old version in place
        stack.insert(name);
        Value depsRecord = Value::Dict({});
        if (Value* deps = pkg.findDictEntry("dependencies")) {
            if (deps->type == ValueType::DICT) {
                for (auto& kv : *deps->dict) {
                    std::string r = kv.second.type == ValueType::STRING ? kv.second.str : "*";
                    say(name + " needs " + kv.first + " " + r);
                    if (!installOne(kv.first, r, false)) { stack.erase(name); return false; }
                    setKey(depsRecord, kv.first, Value::String(r));
                }
            }
        }
        stack.erase(name);

        if (Value* old = installedPackages(db)->findDictEntry(name)) removePackageFiles(*old);

        std::vector<Value> written;
        for (auto& kv : *files->dict) {
            std::string target = kv.first == main ? "modules/" + name + ".ton" : "modules/" + name + "/" + kv.first;
            if (!writeFileBytes(target, kv.second.str)) { error = "Could not write " + target; return false; }
            written.push_back(Value::String(target));
        }
        Value record = Value::Dict({});
        setKey(record, "version", Value::String(pick.version));
        setKey(record, "sha256", Value::String(expectedSha));
        setKey(record, "dependencies", depsRecord);
        setKey(record, "files", Value::Array(written));
        setKey(*installedPackages(db), name, record);
        saveDb(db);
        say("Installed " + name + " " + pick.version + " -> use IMPORT://" + name + " in your scripts.");
        for (auto& kv : *files->dict) {
            if (kv.first != main && kv.first.size() > 4 && kv.first.compare(kv.first.size() - 4, 4, ".ton") == 0) {
                say("(its other .ton files are in modules/" + name + "/, imported as IMPORT://" + name + "/<file>)");
                break;
            }
        }
        return true;
    }
};

// Adds `name = "^version"` to [dependencies] of tonproject.toml (text edit, comments kept).
void saveDependency(const std::string& name, const std::string& version) {
    std::string text;
    if (!readFileBytes(MANIFEST, text)) { say(std::string("No ") + MANIFEST + " to save into (ton618 init creates one)."); return; }
    std::string line = name + " = \"^" + version + "\"";
    // an existing entry is replaced
    std::stringstream in(text);
    std::string out, l;
    bool inDeps = false, done = false;
    while (std::getline(in, l)) {
        std::string t = l;
        t.erase(0, t.find_first_not_of(" \t"));
        if (!t.empty() && t[0] == '[') {
            if (inDeps && !done) { out += line + "\n"; done = true; }
            inDeps = t.rfind("[dependencies]", 0) == 0;
        } else if (inDeps && t.rfind(name, 0) == 0 && t.find('=') != std::string::npos && !done) {
            l = line; done = true;
        }
        out += l + "\n";
    }
    if (!done) {
        if (inDeps) out += line + "\n";
        else out += "\n[dependencies]\n" + line + "\n";
    }
    writeFileBytes(MANIFEST, out);
    say("Saved " + line + " in " + MANIFEST + ".");
}

int cmdInstall(const std::vector<std::string>& args, const std::function<int(const std::string&)>& legacy) {
    Installer inst;
    inst.legacy = legacy;
    bool save = false;
    std::vector<std::string> wanted;
    for (size_t i = 1; i < args.size(); i++) {
        if (args[i] == "--force") inst.force = true;
        else if (args[i] == "--save") save = true;
        else wanted.push_back(args[i]);
    }
    if (wanted.empty()) { // install what tonproject.toml lists
        Value root;
        std::string err;
        if (!readManifest(".", root, err)) return fail("Nothing to install: give a package name (ton618 install mathx) or run this in a project with a " + std::string(MANIFEST) + ".");
        Value* deps = root.findDictEntry("dependencies");
        if (!deps || deps->type != ValueType::DICT || deps->dict->empty()) { say("No [dependencies] in tonproject.toml: nothing to install."); return 0; }
        for (auto& kv : *deps->dict) {
            std::string r = kv.second.type == ValueType::STRING ? kv.second.str : str(kv.second, "version", "*");
            if (!inst.installOne(kv.first, r, false) && !inst.error.empty()) return fail(inst.error);
        }
        say("Dependencies are installed.");
        return 0;
    }
    for (const std::string& spec : wanted) {
        std::string name = spec, range = "*";
        size_t at = spec.find('@');
        if (at != std::string::npos) { name = spec.substr(0, at); range = spec.substr(at + 1); }
        for (char& c : name) c = (char)std::tolower((unsigned char)c);
        if (!isSafeToken(name)) return fail("Invalid package name '" + name + "'.");
        if (!inst.installOne(name, range, true)) return fail(inst.error);
        if (save) {
            if (Value* rec = installedPackages(inst.db)->findDictEntry(name)) saveDependency(name, str(*rec, "version"));
        }
    }
    return 0;
}

int cmdUpdate(const std::vector<std::string>& args, const std::function<int(const std::string&)>& legacy) {
    Installer inst;
    inst.legacy = legacy;
    std::vector<std::string> targets;
    for (size_t i = 1; i < args.size(); i++) {
        if (args[i] == "--force") inst.force = true;
        else targets.push_back(args[i]);
    }
    Value* packages = installedPackages(inst.db);
    if (targets.empty()) for (auto& kv : *packages->dict) targets.push_back(kv.first);
    if (targets.empty()) { say("No packages installed with ton618 in this folder."); return 0; }
    if (!inst.needIndex()) return fail(inst.error);

    int updated = 0;
    for (const std::string& name : targets) {
        Value* rec = packages->findDictEntry(name);
        if (!rec) { std::cerr << "[ton618] " << name << " isn't installed (or wasn't installed by ton618 install).\n"; continue; }
        const Value* entry = inst.entryFor(name);
        if (!entry) { say(name + ": no longer in the registry, left as it is."); continue; }
        // the newest version that every package depending on it still accepts
        std::string constraint;
        for (auto& dep : dependentsOf(inst.db, name)) constraint += (constraint.empty() ? "" : " ") + dep.second;
        Range range;
        parseRange(constraint, range);
        std::vector<Candidate> found = candidatesOf(*entry, range);
        SemVer have;
        parseSemVer(str(*rec, "version"), have);
        std::string oldVersion = str(*rec, "version");
        if (found.empty() || compareVersions(found.front().sv, have) <= 0) {
            std::vector<Candidate> newest = candidatesOf(*entry, Range());
            if (!newest.empty() && compareVersions(newest.front().sv, have) > 0)
                say(name + " " + oldVersion + ": " + newest.front().version + " exists but a package that depends on it needs " + constraint + ".");
            else say(name + " " + oldVersion + " is up to date.");
            continue;
        }
        if (!inst.installVersion(name, found.front(), true)) return fail(inst.error);
        say("Updated " + name + ": " + oldVersion + " -> " + found.front().version);
        updated++;
    }
    if (updated == 0) say("Everything is up to date.");
    return 0;
}

int cmdUninstall(const std::vector<std::string>& args, const std::function<int(const std::string&)>& legacyUninstall) {
    bool force = false;
    std::vector<std::string> names;
    for (size_t i = 1; i < args.size(); i++) { if (args[i] == "--force") force = true; else names.push_back(args[i]); }
    if (names.empty()) return fail("Usage: ton618 uninstall <package>");
    Value db = loadDb();
    int code = 0;
    for (std::string name : names) {
        for (char& c : name) c = (char)std::tolower((unsigned char)c);
        Value* rec = installedPackages(db)->findDictEntry(name);
        if (!rec) { if (legacyUninstall && legacyUninstall(name) != 0) code = 1; continue; }
        auto users = dependentsOf(db, name);
        if (!users.empty() && !force) {
            std::string who;
            for (auto& u : users) who += (who.empty() ? "" : ", ") + u.first;
            code = fail(name + " is needed by " + who + ". Uninstall those first, or use --force.");
            continue;
        }
        removePackageFiles(*rec);
        delKey(*installedPackages(db), name);
        saveDb(db);
        say("Removed " + name + ".");
    }
    return code;
}

int cmdList(const std::vector<std::string>& args) {
    bool outdated = std::find(args.begin(), args.end(), "--outdated") != args.end();
    Value db = loadDb();
    Value index;
    std::string err;
    bool haveIndex = outdated && loadIndex(index, err);
    if (outdated && !haveIndex) std::cerr << "[ton618] " << err << "\n";
    Value* packages = installedPackages(db);
    size_t shown = 0;
    for (auto& kv : *packages->dict) {
        std::string line = kv.first + " " + str(kv.second, "version");
        if (haveIndex) {
            Value* ps = index.findDictEntry("packages");
            Value* entry = ps ? ps->findDictEntry(kv.first) : nullptr;
            std::vector<Candidate> all = entry ? candidatesOf(*entry, Range()) : std::vector<Candidate>();
            SemVer have; parseSemVer(str(kv.second, "version"), have);
            if (!all.empty() && compareVersions(all.front().sv, have) > 0) line += "  ->  " + all.front().version + " available";
            else if (outdated) continue;
        }
        std::cout << "  " << line << "\n";
        shown++;
    }
    // modules in ./modules/ that ton618 didn't install
    std::error_code ec;
    std::vector<std::string> loose;
    if (!outdated && fs::is_directory("modules", ec)) {
        for (auto& e : fs::directory_iterator("modules", ec)) {
            if (e.path().extension() != ".ton") continue;
            std::string n = e.path().stem().string();
            if (!packages->findDictEntry(n)) loose.push_back(n);
        }
    }
    std::sort(loose.begin(), loose.end());
    for (auto& n : loose) { std::cout << "  " << n << "  (not managed by ton618 install)\n"; shown++; }
    if (shown == 0) say(outdated ? "Everything is up to date." : "No modules installed here.");
    return 0;
}

int cmdSearch(const std::vector<std::string>& args) {
    Value index;
    std::string err;
    if (!loadIndex(index, err)) return fail(err);
    std::string needle;
    for (size_t i = 1; i < args.size(); i++) needle += (needle.empty() ? "" : " ") + args[i];
    for (char& c : needle) c = (char)std::tolower((unsigned char)c);
    size_t hits = 0;
    for (auto& kv : *index.findDictEntry("packages")->dict) {
        std::string hay = kv.first + " " + str(kv.second, "description");
        if (Value* kw = kv.second.findDictEntry("keywords")) for (auto& k : stringList(kw)) hay += " " + k;
        for (char& c : hay) c = (char)std::tolower((unsigned char)c);
        if (!needle.empty() && hay.find(needle) == std::string::npos) continue;
        std::cout << "  " << kv.first << " " << str(kv.second, "latest") << "  " << str(kv.second, "description") << "\n";
        hits++;
    }
    if (hits == 0) say("No package matches '" + needle + "'.");
    return 0;
}

int cmdInfo(const std::vector<std::string>& args) {
    if (args.size() < 2) return fail("Usage: ton618 info <package>");
    std::string name = args[1];
    for (char& c : name) c = (char)std::tolower((unsigned char)c);
    Value index;
    std::string err;
    if (!loadIndex(index, err)) return fail(err);
    Value* entry = index.findDictEntry("packages")->findDictEntry(name);
    if (!entry) return fail("No package called '" + name + "' in the registry.");
    std::cout << name << " " << str(*entry, "latest") << "\n  " << str(*entry, "description") << "\n";
    if (!str(*entry, "author").empty()) std::cout << "  author:   " << str(*entry, "author") << "\n";
    if (!str(*entry, "license").empty()) std::cout << "  license:  " << str(*entry, "license") << "\n";
    if (!str(*entry, "homepage").empty()) std::cout << "  homepage: " << str(*entry, "homepage") << "\n";
    std::string versions;
    for (auto& c : candidatesOf(*entry, Range())) versions += (versions.empty() ? "" : ", ") + c.version;
    std::cout << "  versions: " << versions << "\n";
    std::vector<Candidate> newest = candidatesOf(*entry, Range());
    if (!newest.empty()) {
        if (Value* deps = newest.front().info->findDictEntry("dependencies")) {
            if (deps->type == ValueType::DICT && !deps->dict->empty()) {
                std::string list;
                for (auto& kv : *deps->dict) list += (list.empty() ? "" : ", ") + kv.first + " " + kv.second.toString();
                std::cout << "  needs:    " << list << "\n";
            }
        }
    }
    Value db = loadDb();
    if (Value* rec = installedPackages(db)->findDictEntry(name)) std::cout << "  installed here: " << str(*rec, "version") << "\n";
    else std::cout << "  install:  ton618 install " << name << "\n";
    return 0;
}

// ---------------------------------------------------------------------------
// init / build / publish
// ---------------------------------------------------------------------------

std::string sanitizeName(std::string s) {
    for (char& c : s) {
        c = (char)std::tolower((unsigned char)c);
        if (!(std::islower((unsigned char)c) || std::isdigit((unsigned char)c) || c == '-' || c == '_')) c = '-';
    }
    while (!s.empty() && !std::islower((unsigned char)s[0])) s.erase(0, 1);
    if (s.size() < 2) s = "my-module";
    return s.substr(0, 40);
}

int cmdInit(const std::vector<std::string>& args) {
    if (fs::exists(MANIFEST)) return fail(std::string(MANIFEST) + " already exists here.");
    std::string name = args.size() > 1 ? args[1] : sanitizeName(fs::current_path().filename().string());
    if (!validPackageName(name)) return fail("'" + name + "' isn't a valid package name: 2-40 characters, lowercase letters, digits, '-' or '_', starting with a letter.");
    std::string prefix = name;
    for (char& c : prefix) if (c == '-') c = '_';

    std::string toml =
        "# tonproject.toml — describes this TON618 module (like pyproject.toml for Python).\n"
        "# Build it with `ton618 build`, share it with `ton618 publish`.\n"
        "# Docs: https://c-ton618.duckdns.org/docs/\n\n"
        "[project]\n"
        "name = \"" + name + "\"\n"
        "version = \"0.1.0\"\n"
        "description = \"What " + name + " does, in one sentence.\"\n"
        "authors = [\"Your Name\"]\n"
        "license = \"MIT\"\n"
        "readme = \"README.md\"\n"
        "main = \"" + name + ".ton\"\n"
        "# repository = \"https://github.com/you/" + name + "\"\n"
        "# keywords = [\"text\", \"utils\"]\n"
        "# requires = \">=2.1.0\"      # the ton618 versions this module works with\n\n"
        "[dependencies]\n"
        "# stringutils = \"^1.0\"       # other packages, installed with this one\n\n"
        "# [build]\n"
        "# include = [\"*.ton\", \"lib/\"]   # default: every .ton file, README and LICENSE\n"
        "# exclude = [\"scratch.ton\"]\n";
    std::string code =
        "// " + name + ".ton — the main file of the module (set by `main` in tonproject.toml).\n"
        "// Convention: start every public name with \"" + prefix + "_\", so it can't clash with the\n"
        "// scripts that import this module.\n\n"
        "ton.function " + prefix + "_hello(who = \"world\") {\n"
        "    return f\"Hello, {who}!\"\n"
        "}\n";
    std::string readme =
        "# " + name + "\n\nWhat " + name + " does, in a few lines.\n\n"
        "## Install\n\n```\nton618 install " + name + "\n```\n\n"
        "## Use\n\n```\nIMPORT://" + name + "\nprint(" + prefix + "_hello(\"TON618\"))\n```\n";
    bool ok = writeFileBytes(MANIFEST, toml);
    if (!fs::exists(name + ".ton")) ok = writeFileBytes(name + ".ton", code) && ok;
    if (!fs::exists("README.md")) ok = writeFileBytes("README.md", readme) && ok;
    if (!fs::exists(".gitignore")) writeFileBytes(".gitignore", "dist/\nmodules/\n");
    if (!ok) return fail("Could not write the project files here.");
    say("Created " + std::string(MANIFEST) + ", " + name + ".ton and README.md.");
    say("Next: edit them, then `ton618 build`, and `ton618 publish` to share it.");
    return 0;
}

int cmdBuild(const std::vector<std::string>&) {
    Built built;
    std::string error;
    if (!buildPackage(".", built, error, false)) return fail(error);
    return 0;
}

int cmdPublish(const std::vector<std::string>& args) {
    bool dryRun = false, noOpen = false;
    for (size_t i = 1; i < args.size(); i++) {
        if (args[i] == "--dry-run") dryRun = true;
        else if (args[i] == "--no-open") noOpen = true;
    }
    Built built;
    std::string error;
    if (!buildPackage(".", built, error, false)) return fail(error);

    std::string description = str(built.package, "description");
    if (description.empty() || description.rfind("What " + built.name + " does", 0) == 0)
        return fail("Write a real description in tonproject.toml before publishing: it's what people see when they search.");
    if (str(built.package, "license").empty()) return fail("Add a license to tonproject.toml before publishing (for example license = \"MIT\").");

    // already published?
    Value index;
    std::string err;
    if (loadIndex(index, err)) {
        Value* entry = index.findDictEntry("packages")->findDictEntry(built.name);
        Value* versions = entry ? entry->findDictEntry("versions") : nullptr;
        if (versions && versions->findDictEntry(built.version))
            return fail(built.name + " " + built.version + " is already in the registry, and published versions never change. Raise the version in tonproject.toml, then build again.");
        if (entry) say(built.name + " already exists in the registry (latest " + str(*entry, "latest") + "): you're adding " + built.version + ".");
    } else {
        say("(Couldn't check the registry: " + err + ")");
    }

    std::string repo = registryRepo();
    std::string page = "https://github.com/" + repo + "/upload/main/registry/packages";
    std::string file = fs::absolute(built.path).string();
    if (dryRun) {
        say("Dry run: " + built.name + " " + built.version + " is ready to be published. Nothing was sent.");
        return 0;
    }
    std::cout << "\n  Publishing " << built.name << " " << built.version << " takes one pull request:\n\n"
              << "    1. Sign in to GitHub in the page that opens:\n       " << page << "\n"
              << "    2. Drop this file on it:\n       " << file << "\n"
              << "    3. Choose \"Create a new branch ... and start a pull request\", then \"Propose changes\".\n\n"
              << "  A maintainer reviews it (every module runs with the full power of ton618, so each one is\n"
              << "  read first). Once it's merged, the site updates in about a minute and anyone can run:\n\n"
              << "    ton618 install " << built.name << "\n\n";
    if (!noOpen && !openUrl(page)) say("(Couldn't open a browser: open the link above yourself.)");
    return 0;
}

} // namespace

bool isPackageCommand(const std::string& c) {
    return c == "init" || c == "build" || c == "publish" || c == "install" || c == "update" ||
           c == "uninstall" || c == "list" || c == "search" || c == "info";
}

int runPackageCommand(const std::vector<std::string>& args,
                      const std::function<int(const std::string&)>& legacyInstall,
                      const std::function<int(const std::string&)>& legacyUninstall) {
    const std::string& c = args[0];
    if (c == "init") return cmdInit(args);
    if (c == "build") return cmdBuild(args);
    if (c == "publish") return cmdPublish(args);
    if (c == "install") return cmdInstall(args, legacyInstall);
    if (c == "update") return cmdUpdate(args, legacyInstall);
    if (c == "uninstall") return cmdUninstall(args, legacyUninstall);
    if (c == "list") return cmdList(args);
    if (c == "search") return cmdSearch(args);
    if (c == "info") return cmdInfo(args);
    return 1;
}
