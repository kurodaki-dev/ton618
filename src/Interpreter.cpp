#include "Interpreter.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "LocalServer.hpp"
#include "HttpClient.hpp"
#include "JsonParser.hpp"
#include "Platform.hpp"
#include "Version.hpp"
#include <iostream>
#include <regex>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>
#include <filesystem>

// Structural equality: numbers/strings/bools/nil compare by value, and arrays/
// dicts compare element-by-element (recursively) rather than always being
// "not equal" to each other — used by the '==' / '!=' operators, switch/case
// matching, and the always-available contains()/indexOf() natives, so all
// four agree on what "equal" means. A ton.string and a ton.html holding the
// same text compare equal (they already interoperate everywhere else:
// concatenation, printing, any string function) — every other type mismatch
// is never equal. Functions never compare equal to anything.
static bool valuesEqual(const Value& a, const Value& b) {
    bool aIsText = a.type == ValueType::STRING || a.type == ValueType::HTML;
    bool bIsText = b.type == ValueType::STRING || b.type == ValueType::HTML;
    if (aIsText && bIsText) return a.str == b.str;
    if (a.type != b.type) return false;
    switch (a.type) {
        case ValueType::NIL: return true;
        case ValueType::BOOL: return a.boolean == b.boolean;
        case ValueType::NUMBER: return a.number == b.number;
        case ValueType::ARRAY: {
            if (!a.array || !b.array) return a.array == b.array;
            if (a.array->size() != b.array->size()) return false;
            for (size_t i = 0; i < a.array->size(); i++)
                if (!valuesEqual((*a.array)[i], (*b.array)[i])) return false;
            return true;
        }
        case ValueType::DICT: {
            if (!a.dict || !b.dict) return a.dict == b.dict;
            if (a.dict->size() != b.dict->size()) return false;
            for (auto& [k, v] : *a.dict) {
                const Value* other = b.findDictEntry(k);
                if (!other || !valuesEqual(v, *other)) return false;
            }
            return true;
        }
        default: return false; // functions (user or native) never compare equal
    }
}

// ---- Percent/URL-encoding helpers ------------------------------------------
// Shared by ton.encoding's url_encode/url_decode (src/Interpreter.cpp below)
// and by the local server's own query-string parsing (registerBuiltinSys's
// sibling "serve" native), so both agree on the same escaping rules.

static std::string urlEncode(const std::string& in) {
    static const char* hexDigits = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : in) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
        else { out += '%'; out += hexDigits[c >> 4]; out += hexDigits[c & 0xF]; }
    }
    return out;
}

static int hexDigitValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static std::string urlDecode(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); i++) {
        if (in[i] == '%' && i + 2 < in.size()) {
            int hi = hexDigitValue(in[i + 1]), lo = hexDigitValue(in[i + 2]);
            if (hi >= 0 && lo >= 0) { out += (char)((hi << 4) | lo); i += 2; continue; }
        }
        out += (in[i] == '+') ? ' ' : in[i];
    }
    return out;
}

// Parses a raw "a=1&b=2" query string into a ton.dict, url-decoding both
// keys and values. Used by the local server to populate request["query"].
static Value parseQueryDict(const std::string& query) {
    std::vector<std::pair<std::string, Value>> entries;
    std::stringstream ss(query);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        if (pair.empty()) continue;
        size_t eq = pair.find('=');
        std::string key = (eq == std::string::npos) ? pair : pair.substr(0, eq);
        std::string val = (eq == std::string::npos) ? "" : pair.substr(eq + 1);
        entries.push_back({urlDecode(key), Value::String(urlDecode(val))});
    }
    return Value::Dict(entries);
}

// Matches a route pattern like "/user/:id/edit" against an actual request
// path like "/user/42/edit": segments starting with ':' bind whatever's in
// that position into `params` (key = name without the ':', value = the
// actual path segment); every other segment must match literally. Segment
// counts must match exactly (no wildcards). Used by the local server's route
// dispatcher (the "serve" native) to populate request["params"].
static bool matchRoute(const std::string& pattern, const std::string& path,
                        std::vector<std::pair<std::string, std::string>>& params) {
    auto splitSegments = [](const std::string& s) {
        std::vector<std::string> segs;
        std::stringstream ss(s);
        std::string seg;
        while (std::getline(ss, seg, '/')) if (!seg.empty()) segs.push_back(seg);
        return segs;
    };
    auto patternSegs = splitSegments(pattern);
    auto pathSegs = splitSegments(path);
    if (patternSegs.size() != pathSegs.size()) return false;
    for (size_t i = 0; i < patternSegs.size(); i++) {
        if (!patternSegs[i].empty() && patternSegs[i][0] == ':') {
            params.push_back({patternSegs[i].substr(1), pathSegs[i]});
        } else if (patternSegs[i] != pathSegs[i]) {
            return false;
        }
    }
    return true;
}

Interpreter::Interpreter() {
    globals = std::make_shared<Environment>();
    std::srand((unsigned)std::time(nullptr));
    defineNatives();
}

void Interpreter::runtimeError(int line, const std::string& msg) {
    throw std::runtime_error("Error at line " + std::to_string(line) + ": " + msg);
}

void Interpreter::checkType(VarKind kind, const Value& v, const std::string& varName, int line) {
    if (kind == VarKind::NONE) return;
    bool ok = true;
    std::string expected;
    switch (kind) {
        case VarKind::INT:
        case VarKind::FLOAT:
            expected = "int/float";
            ok = (v.type == ValueType::NUMBER);
            break;
        case VarKind::STRING:
            expected = "string";
            ok = (v.type == ValueType::STRING);
            break;
        case VarKind::BOOL:
            expected = "bool";
            ok = (v.type == ValueType::BOOL);
            break;
        case VarKind::ARRAY:
            expected = "array";
            ok = (v.type == ValueType::ARRAY);
            break;
        case VarKind::HTML:
            expected = "html";
            // Une string litterale peut etre assignee directement a du html (confort d'ecriture)
            ok = (v.type == ValueType::HTML || v.type == ValueType::STRING);
            break;
        case VarKind::DICT:
            expected = "dict";
            ok = (v.type == ValueType::DICT);
            break;
        default: break;
    }
    if (!ok) {
        runtimeError(line, "Invalid type for 'ton." + varName + "': expected " + expected +
                     " but got " + v.typeName() + ".");
    }
}

void Interpreter::defineNatives() {
    auto def = [this](const std::string& name, NativeFn fn) {
        Value v; v.type = ValueType::NATIVE_FUNCTION;
        v.nativeFn = std::make_shared<NativeFn>(std::move(fn));
        globals->define(name, v);
    };

    def("len", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Number(0);
        if (args[0].type == ValueType::STRING || args[0].type == ValueType::HTML)
            return Value::Number((double)args[0].str.size());
        if (args[0].type == ValueType::ARRAY) return Value::Number((double)args[0].array->size());
        if (args[0].type == ValueType::DICT) return Value::Number((double)args[0].dict->size());
        return Value::Number(0);
    });

    def("str", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::String("");
        return Value::String(args[0].toString());
    });

    def("num", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Number(0);
        if (args[0].type == ValueType::NUMBER) return args[0];
        if (args[0].type == ValueType::STRING) {
            try { return Value::Number(std::stod(args[0].str)); } catch (...) { return Value::Number(0); }
        }
        return Value::Number(0);
    });

    // format(template, ...args) — replaces each "{}" in template, left to
    // right, with str(args[i]). Simpler alternative to building a string out
    // of a chain of '+' concatenations: format("Hello {}, you are {}!", name, age).
    def("format", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::String("");
        std::string tmpl = args[0].toString(), out;
        size_t argIndex = 1;
        for (size_t i = 0; i < tmpl.size(); i++) {
            if (tmpl[i] == '{' && i + 1 < tmpl.size() && tmpl[i + 1] == '}') {
                out += (argIndex < args.size()) ? args[argIndex].toString() : "{}";
                argIndex++;
                i++;
                continue;
            }
            out += tmpl[i];
        }
        return Value::String(out);
    });

    def("push", [](std::vector<Value>& args) -> Value {
        if (args.size() >= 2 && args[0].type == ValueType::ARRAY) args[0].array->push_back(args[1]);
        return Value::Nil();
    });

    // ton.readfile(path) — reads a file's content as a string (useful to serve static HTML).
    def("readfile", [this](std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) {
            throw std::runtime_error("readfile(path) expects a string path.");
        }
        std::string path = args[0].str;
        // Resolve relative to the script's directory, like IMPORT:// does.
        std::vector<std::string> candidates = { path, scriptDir + "/" + path };
        for (auto& c : candidates) {
            std::ifstream file(c);
            if (file) {
                std::stringstream ss;
                ss << file.rdbuf();
                return Value::Html(ss.str());
            }
        }
        throw std::runtime_error("readfile: file not found '" + path + "'.");
    });

    // writefile(path, content) — writes (overwriting) a file with the given
    // text; content is converted with str() first if it isn't already a
    // string/html. Returns true on success, false if the file couldn't be
    // opened for writing (e.g. a missing parent directory — see os_mkdir).
    def("writefile", [](std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING) {
            throw std::runtime_error("writefile(path, content) expects a string path and content.");
        }
        std::ofstream file(args[0].str, std::ios::trunc);
        if (!file) return Value::Bool(false);
        file << args[1].toString();
        return Value::Bool(true);
    });

    // ton.json(value) — converts an array/value into a JSON string, for building APIs.
    def("json", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::String("null");
        return Value::String(args[0].toJson());
    });

    // ---- type() / assert() / input() ----------------------------------------------

    def("type", [](std::vector<Value>& args) -> Value {
        return Value::String(args.empty() ? "nil" : args[0].typeName());
    });

    // ton.assert(condition, [message]) — throws (catchable by try/catch) if the
    // condition is falsy. Handy for writing quick sanity checks in scripts.
    def("assert", [](std::vector<Value>& args) -> Value {
        if (args.empty() || !args[0].isTruthy()) {
            std::string msg = (args.size() >= 2) ? args[1].toString() : "assertion failed";
            throw std::runtime_error(msg);
        }
        return Value::Nil();
    });

    // ton.input([prompt]) — prints an optional prompt, reads one line from stdin.
    def("input", [](std::vector<Value>& args) -> Value {
        if (!args.empty()) std::cout << args[0].toString();
        std::string line;
        if (!std::getline(std::cin, line)) return Value::Nil();
        return Value::String(line);
    });

    // ---- math ------------------------------------------------------------------

    def("sqrt", [](std::vector<Value>& args) -> Value { return Value::Number(std::sqrt(args.empty() ? 0 : args[0].number)); });
    def("pow", [](std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::Number(0);
        return Value::Number(std::pow(args[0].number, args[1].number));
    });
    def("abs", [](std::vector<Value>& args) -> Value { return Value::Number(std::fabs(args.empty() ? 0 : args[0].number)); });
    def("floor", [](std::vector<Value>& args) -> Value { return Value::Number(std::floor(args.empty() ? 0 : args[0].number)); });
    def("ceil", [](std::vector<Value>& args) -> Value { return Value::Number(std::ceil(args.empty() ? 0 : args[0].number)); });
    def("round", [](std::vector<Value>& args) -> Value { return Value::Number(std::round(args.empty() ? 0 : args[0].number)); });
    def("min", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Nil();
        double m = args[0].number;
        for (auto& a : args) m = std::min(m, a.number);
        return Value::Number(m);
    });
    def("max", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Nil();
        double m = args[0].number;
        for (auto& a : args) m = std::max(m, a.number);
        return Value::Number(m);
    });
    // ton.random() -> a float in [0, 1). ton.random(max) -> an int in [0, max).
    // ton.random(min, max) -> an int in [min, max).
    def("random", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Number((double)std::rand() / ((double)RAND_MAX + 1.0));
        if (args.size() == 1) return Value::Number(std::rand() % std::max(1, (int)args[0].number));
        int lo = (int)args[0].number, hi = (int)args[1].number;
        if (hi <= lo) return Value::Number(lo);
        return Value::Number(lo + std::rand() % (hi - lo));
    });

    // ---- strings -----------------------------------------------------------------

    def("upper", [](std::vector<Value>& args) -> Value {
        std::string s = args.empty() ? "" : args[0].toString();
        for (auto& c : s) c = (char)std::toupper((unsigned char)c);
        return Value::String(s);
    });
    def("lower", [](std::vector<Value>& args) -> Value {
        std::string s = args.empty() ? "" : args[0].toString();
        for (auto& c : s) c = (char)std::tolower((unsigned char)c);
        return Value::String(s);
    });
    def("trim", [](std::vector<Value>& args) -> Value {
        std::string s = args.empty() ? "" : args[0].toString();
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return Value::String("");
        size_t b = s.find_last_not_of(" \t\r\n");
        return Value::String(s.substr(a, b - a + 1));
    });
    // ton.split(str, separator) -> array of strings
    def("split", [](std::vector<Value>& args) -> Value {
        std::vector<Value> parts;
        if (args.size() < 2 || args[1].toString().empty()) {
            if (!args.empty()) parts.push_back(Value::String(args[0].toString()));
            return Value::Array(parts);
        }
        std::string s = args[0].toString(), sep = args[1].toString();
        size_t pos = 0, next;
        while ((next = s.find(sep, pos)) != std::string::npos) {
            parts.push_back(Value::String(s.substr(pos, next - pos)));
            pos = next + sep.size();
        }
        parts.push_back(Value::String(s.substr(pos)));
        return Value::Array(parts);
    });
    // ton.replace(str, search, replacement) -> str with every occurrence of "search" swapped
    def("replace", [](std::vector<Value>& args) -> Value {
        if (args.size() < 3) return args.empty() ? Value::String("") : Value::String(args[0].toString());
        std::string s = args[0].toString(), search = args[1].toString(), repl = args[2].toString();
        if (search.empty()) return Value::String(s);
        std::string out;
        size_t pos = 0, next;
        while ((next = s.find(search, pos)) != std::string::npos) {
            out += s.substr(pos, next - pos) + repl;
            pos = next + search.size();
        }
        out += s.substr(pos);
        return Value::String(out);
    });
    // ton.substring(str, start, [length])
    def("substring", [](std::vector<Value>& args) -> Value {
        if (args.size() < 2) return args.empty() ? Value::String("") : Value::String(args[0].toString());
        std::string s = args[0].toString();
        int start = std::max(0, (int)args[1].number);
        if (start >= (int)s.size()) return Value::String("");
        size_t len = (args.size() >= 3) ? (size_t)std::max(0, (int)args[2].number) : std::string::npos;
        return Value::String(s.substr(start, len));
    });
    // ton.contains(x, item) — substring search for strings, element search for arrays.
    def("contains", [](std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::Bool(false);
        if (args[0].type == ValueType::ARRAY) {
            for (auto& e : *args[0].array) if (valuesEqual(e, args[1])) return Value::Bool(true);
            return Value::Bool(false);
        }
        return Value::Bool(args[0].toString().find(args[1].toString()) != std::string::npos);
    });
    // ton.indexOf(x, item) — first matching index, or -1. Works on strings and arrays.
    def("indexOf", [](std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::Number(-1);
        if (args[0].type == ValueType::ARRAY) {
            auto& arr = *args[0].array;
            for (size_t i = 0; i < arr.size(); i++) if (valuesEqual(arr[i], args[1])) return Value::Number((double)i);
            return Value::Number(-1);
        }
        auto pos = args[0].toString().find(args[1].toString());
        return Value::Number(pos == std::string::npos ? -1 : (double)pos);
    });

    // ---- arrays --------------------------------------------------------------------

    def("pop", [](std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::ARRAY || args[0].array->empty()) return Value::Nil();
        Value last = args[0].array->back();
        args[0].array->pop_back();
        return last;
    });
    def("shift", [](std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::ARRAY || args[0].array->empty()) return Value::Nil();
        Value first = args[0].array->front();
        args[0].array->erase(args[0].array->begin());
        return first;
    });
    def("unshift", [](std::vector<Value>& args) -> Value {
        if (args.size() >= 2 && args[0].type == ValueType::ARRAY) args[0].array->insert(args[0].array->begin(), args[1]);
        return Value::Nil();
    });
    // ton.slice(arr, start, [end]) — like array/string slicing in most languages.
    def("slice", [](std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::ARRAY) return Value::Array({});
        auto& arr = *args[0].array;
        int start = args.size() >= 2 ? std::max(0, (int)args[1].number) : 0;
        int end = args.size() >= 3 ? std::min((int)arr.size(), (int)args[2].number) : (int)arr.size();
        std::vector<Value> out;
        for (int i = start; i < end; i++) out.push_back(arr[i]);
        return Value::Array(out);
    });
    // ton.join(arr, separator) -> string
    def("join", [](std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::ARRAY) return Value::String("");
        std::string sep = args.size() >= 2 ? args[1].toString() : ",";
        std::string out;
        auto& arr = *args[0].array;
        for (size_t i = 0; i < arr.size(); i++) {
            if (i > 0) out += sep;
            out += arr[i].toString();
        }
        return Value::String(out);
    });
    def("reverse", [](std::vector<Value>& args) -> Value {
        if (!args.empty() && args[0].type == ValueType::ARRAY) std::reverse(args[0].array->begin(), args[0].array->end());
        return args.empty() ? Value::Nil() : args[0];
    });
    // ton.sort(arr) — sorts numbers ascending or strings alphabetically, in place.
    def("sort", [](std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::ARRAY) return args.empty() ? Value::Nil() : args[0];
        std::sort(args[0].array->begin(), args[0].array->end(), [](const Value& a, const Value& b) {
            if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER) return a.number < b.number;
            return a.toString() < b.toString();
        });
        return args[0];
    });
    // ton.map(arr, fn) — applies fn to every element, returns a new array of the results.
    def("map", [this](std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::ARRAY) return Value::Array({});
        std::vector<Value> out;
        for (auto& item : *args[0].array) {
            std::vector<Value> callArgs = {item};
            out.push_back(callFunction(args[1], callArgs, 0));
        }
        return Value::Array(out);
    });
    // ton.filter(arr, fn) — keeps only the elements for which fn(element) is truthy.
    def("filter", [this](std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::ARRAY) return Value::Array({});
        std::vector<Value> out;
        for (auto& item : *args[0].array) {
            std::vector<Value> callArgs = {item};
            if (callFunction(args[1], callArgs, 0).isTruthy()) out.push_back(item);
        }
        return Value::Array(out);
    });
    // ton.reduce(arr, fn, initial) — folds the array into a single value via fn(acc, element).
    def("reduce", [this](std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::ARRAY) return Value::Nil();
        Value acc = args.size() >= 3 ? args[2] : Value::Nil();
        for (auto& item : *args[0].array) {
            std::vector<Value> callArgs = {acc, item};
            acc = callFunction(args[1], callArgs, 0);
        }
        return acc;
    });
    // ton.find(arr, fn) — the first element for which fn(element) is truthy, or nil.
    def("find", [this](std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::ARRAY) return Value::Nil();
        for (auto& item : *args[0].array) {
            std::vector<Value> callArgs = {item};
            if (callFunction(args[1], callArgs, 0).isTruthy()) return item;
        }
        return Value::Nil();
    });
    // ton.any(arr, fn) — true if fn(element) is truthy for at least one element.
    def("any", [this](std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::ARRAY) return Value::Bool(false);
        for (auto& item : *args[0].array) {
            std::vector<Value> callArgs = {item};
            if (callFunction(args[1], callArgs, 0).isTruthy()) return Value::Bool(true);
        }
        return Value::Bool(false);
    });
    // ton.all(arr, fn) — true if fn(element) is truthy for every element.
    def("all", [this](std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::ARRAY) return Value::Bool(true);
        for (auto& item : *args[0].array) {
            std::vector<Value> callArgs = {item};
            if (!callFunction(args[1], callArgs, 0).isTruthy()) return Value::Bool(false);
        }
        return Value::Bool(true);
    });

    // ---- dicts -----------------------------------------------------------------

    def("keys", [](std::vector<Value>& args) -> Value {
        std::vector<Value> out;
        if (!args.empty() && args[0].type == ValueType::DICT)
            for (auto& kv : *args[0].dict) out.push_back(Value::String(kv.first));
        return Value::Array(out);
    });
    def("values", [](std::vector<Value>& args) -> Value {
        std::vector<Value> out;
        if (!args.empty() && args[0].type == ValueType::DICT)
            for (auto& kv : *args[0].dict) out.push_back(kv.second);
        return Value::Array(out);
    });
    def("has", [](std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::DICT) return Value::Bool(false);
        std::string key = (args[1].type == ValueType::STRING) ? args[1].str : args[1].toString();
        return Value::Bool(args[0].findDictEntry(key) != nullptr);
    });

    // ton.get(path, handlerFunction) — registers a GET route.
    // ton.get(path, handlerFunction) — registers a GET route. `path` may contain
    // ":name" segments (e.g. "/user/:id") captured into request["params"].
    def("get", [this](std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING || args[1].type != ValueType::FUNCTION) {
            throw std::runtime_error("get(path, handlerFunction) expects a string path and a ton.function handler.");
        }
        routes.push_back({"GET", args[0].str, args[1]});
        return Value::Nil();
    });

    // ton.post(path, handlerFunction) — registers a POST route (same ":name" support as get()).
    def("post", [this](std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING || args[1].type != ValueType::FUNCTION) {
            throw std::runtime_error("post(path, handlerFunction) expects a string path and a ton.function handler.");
        }
        routes.push_back({"POST", args[0].str, args[1]});
        return Value::Nil();
    });

    // ton.serve(port) — starts the HTTP server and dispatches to registered routes.
    // Each handler is called with a single ton.dict argument:
    //   { method, path, query: {..}, params: {..}, body }
    // ton.serve(port, html) — legacy simple mode: serves the same content on every request.
    def("serve", [this](std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::NUMBER) {
            throw std::runtime_error("serve(port) or serve(port, content) expects a port number.");
        }
        int port = (int)args[0].number;

        // Legacy mode: a fixed second argument is served for every request.
        if (args.size() >= 2) {
            std::string content = args[1].toString();
            HttpHandler handler = [content](const HttpRequest&) -> HttpResponse {
                return HttpResponse{content, "text/html; charset=utf-8", 200};
            };
            std::string err = startLocalServer(port, handler);
            if (!err.empty()) throw std::runtime_error(err);
            return Value::Nil();
        }

        // Routing mode: dispatch based on registered ton.get / ton.post routes.
        HttpHandler handler = [this](const HttpRequest& req) -> HttpResponse {
            for (auto& route : routes) {
                if (route.method != req.method) continue;
                std::vector<std::pair<std::string, std::string>> matchedParams;
                if (!matchRoute(route.path, req.path, matchedParams)) continue;

                std::vector<std::pair<std::string, Value>> paramsDict;
                for (auto& [k, v] : matchedParams) paramsDict.push_back({k, Value::String(v)});

                std::vector<std::pair<std::string, Value>> reqEntries = {
                    {"method", Value::String(req.method)},
                    {"path", Value::String(req.path)},
                    {"query", parseQueryDict(req.query)},
                    {"params", Value::Dict(paramsDict)},
                    {"body", Value::String(req.body)},
                };
                std::vector<Value> callArgs = {Value::Dict(reqEntries)};
                Value result = callFunction(route.handler, callArgs, 0);
                std::string contentType = (result.type == ValueType::HTML)
                    ? "text/html; charset=utf-8" : "text/plain; charset=utf-8";
                return HttpResponse{result.toString(), contentType, 200};
            }
            return HttpResponse{"404 Not Found: " + req.path, "text/plain; charset=utf-8", 404};
        };
        std::string err = startLocalServer(port, handler);
        if (!err.empty()) throw std::runtime_error(err);
        return Value::Nil();
    });
}

void Interpreter::runImport(const std::string& moduleName, int line) {
    if (importedModules.count(moduleName)) return;
    importedModules.insert(moduleName);

    // "ton.<name>" never refers to a file — it's one of the interpreter's own
    // built-in system modules. Each is only wired into `globals` right here, the
    // first time it's actually imported, which is why calling e.g. requests_get()
    // without "IMPORT://ton.requests" first fails with "undefined function".
    if (moduleName.rfind("ton.", 0) == 0) {
        std::string builtin = moduleName.substr(4);
        if (builtin == "sys") { registerBuiltinSys(); return; }
        if (builtin == "os") { registerBuiltinOs(); return; }
        if (builtin == "requests") { registerBuiltinRequests(); return; }
        if (builtin == "random") { registerBuiltinRandom(); return; }
        if (builtin == "time") { registerBuiltinTime(); return; }
        if (builtin == "json") { registerBuiltinJson(); return; }
        if (builtin == "mathutils") { registerBuiltinMathUtils(); return; }
        if (builtin == "strings") { registerBuiltinStrings(); return; }
        if (builtin == "encoding") { registerBuiltinEncoding(); return; }
        if (builtin == "regex") { registerBuiltinRegex(); return; }
        if (builtin == "path") { registerBuiltinPath(); return; }
        if (builtin == "tensor") { registerBuiltinTensor(); return; }
        runtimeError(line, "Unknown built-in module 'ton." + builtin +
                     "'. Available: ton.sys, ton.os, ton.requests, ton.random, ton.time, ton.json, "
                     "ton.mathutils, ton.strings, ton.encoding, ton.regex, ton.path, ton.tensor.");
    }

    std::vector<std::string> candidates = {
        scriptDir + "/" + moduleName + ".ton",
        scriptDir + "/modules/" + moduleName + ".ton"
    };

    std::string path, source;
    bool found = false;
    for (auto& c : candidates) {
        std::ifstream file(c);
        if (file) {
            std::stringstream ss;
            ss << file.rdbuf();
            source = ss.str();
            path = c;
            found = true;
            break;
        }
    }

    if (!found) {
        runtimeError(line, "Module '" + moduleName + "' not found (looked for: " +
                     candidates[0] + " or " + candidates[1] + ").");
    }

    try {
        Lexer lexer(source);
        auto tokens = lexer.scanTokens();
        Parser parser(tokens);
        auto statements = parser.parse();
        for (auto& stmt : statements) execute(stmt, globals);
    } catch (std::exception& e) {
        runtimeError(line, "Error in module '" + moduleName + "': " + e.what());
    }
}

void Interpreter::interpret(const std::vector<StmtPtr>& statements) {
    auto env = globals;
    for (auto& stmt : statements) execute(stmt, env);
}

void Interpreter::executeBlock(const std::vector<StmtPtr>& stmts, std::shared_ptr<Environment> env) {
    for (auto& s : stmts) execute(s, env);
}

void Interpreter::execute(const StmtPtr& stmt, std::shared_ptr<Environment> env) {
    if (!stmt) return;
    if (debugger.shouldPause(stmt->line)) debugger.pauseAndPrompt(stmt->line, env);

    switch (stmt->type) {
        case StmtType::EXPR_STMT:
            evaluate(stmt->expr, env);
            break;

        case StmtType::VAR_DECL: {
            Value val = Value::Nil();
            if (stmt->initializer) val = evaluate(stmt->initializer, env);
            checkType(stmt->declaredType, val, stmt->name, stmt->line);
            // Coercion: une string litterale assignee a ton.html devient bien du HTML.
            if (stmt->declaredType == VarKind::HTML && val.type == ValueType::STRING) {
                val = Value::Html(val.str);
            }
            env->define(stmt->name, val);
            break;
        }

        case StmtType::BLOCK: {
            auto blockEnv = std::make_shared<Environment>(env);
            executeBlock(stmt->statements, blockEnv);
            break;
        }

        case StmtType::IF: {
            Value cond = evaluate(stmt->condition, env);
            if (cond.isTruthy()) execute(stmt->thenBranch, env);
            else if (stmt->elseBranch) execute(stmt->elseBranch, env);
            break;
        }

        case StmtType::WHILE: {
            while (evaluate(stmt->condition, env).isTruthy()) {
                try { execute(stmt->thenBranch, env); }
                catch (BreakException&) { break; }
                catch (ContinueException&) { continue; }
            }
            break;
        }

        case StmtType::FOR: {
            auto forEnv = std::make_shared<Environment>(env);
            if (stmt->forInit) execute(stmt->forInit, forEnv);
            while (!stmt->forCondition || evaluate(stmt->forCondition, forEnv).isTruthy()) {
                try { execute(stmt->forBody, forEnv); }
                catch (BreakException&) { break; }
                catch (ContinueException&) {}
                if (stmt->forIncrement) evaluate(stmt->forIncrement, forEnv);
            }
            break;
        }

        case StmtType::FOR_IN: {
            Value collection = evaluate(stmt->condition, env);
            auto forEnv = std::make_shared<Environment>(env);
            if (collection.type == ValueType::ARRAY) {
                for (auto& item : *collection.array) {
                    forEnv->define(stmt->name, item);
                    try { execute(stmt->thenBranch, forEnv); }
                    catch (BreakException&) { break; }
                    catch (ContinueException&) { continue; }
                }
            } else if (collection.type == ValueType::DICT) {
                // Iterating a dict walks its keys, as a string, one at a time.
                for (auto& kv : *collection.dict) {
                    forEnv->define(stmt->name, Value::String(kv.first));
                    try { execute(stmt->thenBranch, forEnv); }
                    catch (BreakException&) { break; }
                    catch (ContinueException&) { continue; }
                }
            } else {
                runtimeError(stmt->line, "'for ... in' expects an array or a dict, got " + collection.typeName() + ".");
            }
            break;
        }

        case StmtType::TRY_CATCH: {
            // The optional "finally" block runs exactly once no matter what:
            // the try succeeds, the catch handles an error, the catch block
            // itself raises, or a break/continue/return unwinds through
            // either — so it's run both on the normal path below and from the
            // catch-all rethrow here before letting anything propagate further.
            try {
                try {
                    execute(stmt->thenBranch, env);
                } catch (BreakException&) { throw; }
                catch (ContinueException&) { throw; }
                catch (ReturnException&) { throw; }
                catch (std::exception& e) {
                    if (!stmt->elseBranch) throw; // no catch clause: let it propagate (finally still runs, below)
                    auto catchEnv = std::make_shared<Environment>(env);
                    catchEnv->define(stmt->name, Value::String(e.what()));
                    execute(stmt->elseBranch, catchEnv);
                }
            } catch (...) {
                if (stmt->finallyBranch) execute(stmt->finallyBranch, env);
                throw;
            }
            if (stmt->finallyBranch) execute(stmt->finallyBranch, env);
            break;
        }

        case StmtType::THROW: {
            Value val = stmt->expr ? evaluate(stmt->expr, env) : Value::Nil();
            throw std::runtime_error(val.toString());
        }

        case StmtType::FN_DECL: {
            auto fn = std::make_shared<FunctionObj>();
            fn->name = stmt->fnName;
            fn->params = stmt->params;
            fn->paramDefaults = stmt->paramDefaults;
            fn->hasRestParam = stmt->hasRestParam;
            fn->body = stmt->body;
            fn->closure = env;
            Value v; v.type = ValueType::FUNCTION; v.function = fn;
            env->define(stmt->fnName, v);
            break;
        }

        case StmtType::RETURN: {
            Value val = Value::Nil();
            if (stmt->expr) val = evaluate(stmt->expr, env);
            throw ReturnException{val};
        }

        case StmtType::PRINT: {
            Value val = evaluate(stmt->expr, env);
            std::cout << val.toString() << std::endl;
            break;
        }

        case StmtType::IMPORT:
            runImport(stmt->moduleName, stmt->line);
            break;

        case StmtType::BREAK: throw BreakException{};
        case StmtType::CONTINUE: throw ContinueException{};

        case StmtType::SWITCH: {
            Value subject = evaluate(stmt->condition, env);
            bool matched = false;
            try {
                for (auto& [values, body] : stmt->switchCases) {
                    bool hit = false;
                    for (auto& valExpr : values) {
                        if (valuesEqual(subject, evaluate(valExpr, env))) { hit = true; break; }
                    }
                    if (hit) {
                        auto caseEnv = std::make_shared<Environment>(env);
                        execute(body, caseEnv);
                        matched = true;
                        break;
                    }
                }
                if (!matched && stmt->elseBranch) {
                    auto defEnv = std::make_shared<Environment>(env);
                    execute(stmt->elseBranch, defEnv);
                }
            } catch (BreakException&) {
                // A stray "break;" (habit from other languages) just exits the
                // switch early — there's no fallthrough to break out of anyway.
            }
            break;
        }
    }
}

Value Interpreter::evaluate(const ExprPtr& expr, std::shared_ptr<Environment> env) {
    if (!expr) return Value::Nil();

    switch (expr->type) {
        case ExprType::LITERAL: {
            if (expr->isNil) return Value::Nil();
            if (expr->isBoolLit) return Value::Bool(expr->litBool);
            if (expr->isNumber) return Value::Number(expr->litNumber);
            if (expr->isString) return Value::String(expr->litString);
            return Value::Nil();
        }

        case ExprType::VARIABLE: {
            Value v;
            if (!env->get(expr->name, v)) {
                runtimeError(expr->line, "Undefined variable or function 'ton." + expr->name + "'.");
            }
            return v;
        }

        case ExprType::ASSIGN: {
            Value val = evaluate(expr->value, env);
            if (!env->assign(expr->name, val)) {
                runtimeError(expr->line, "Cannot assign: 'ton." + expr->name +
                             "' was never declared. Use ton.int/ton.string/ton.bool/ton.float/ton.array/ton.html to declare it first.");
            }
            return val;
        }

        case ExprType::GROUPING:
            return evaluate(expr->inner, env);

        case ExprType::UNARY: {
            Value operand = evaluate(expr->operand, env);
            if (expr->op == TokenType::MINUS) {
                if (operand.type != ValueType::NUMBER) runtimeError(expr->line, "Operand of '-' must be a number.");
                return Value::Number(-operand.number);
            }
            if (expr->op == TokenType::BANG) return Value::Bool(!operand.isTruthy());
            if (expr->op == TokenType::TILDE) {
                if (operand.type != ValueType::NUMBER) runtimeError(expr->line, "Operand of '~' must be a number.");
                return Value::Number((double)(~(long long)operand.number));
            }
            return Value::Nil();
        }

        case ExprType::LOGICAL: {
            Value left = evaluate(expr->left, env);
            if (expr->op == TokenType::OR) { if (left.isTruthy()) return left; }
            else if (expr->op == TokenType::AND) { if (!left.isTruthy()) return left; }
            else { if (left.type != ValueType::NIL) return left; } // "??"
            return evaluate(expr->right, env);
        }

        case ExprType::BINARY: {
            Value left = evaluate(expr->left, env);
            Value right = evaluate(expr->right, env);

            switch (expr->op) {
                case TokenType::PLUS:
                    if (left.type == ValueType::STRING || right.type == ValueType::STRING ||
                        left.type == ValueType::HTML || right.type == ValueType::HTML) {
                        // Si l'un des deux est du html, le resultat reste du html.
                        if (left.type == ValueType::HTML || right.type == ValueType::HTML)
                            return Value::Html(left.toString() + right.toString());
                        return Value::String(left.toString() + right.toString());
                    }
                    if (left.type == ValueType::NUMBER && right.type == ValueType::NUMBER)
                        return Value::Number(left.number + right.number);
                    runtimeError(expr->line, "Invalid operands for '+'.");
                case TokenType::MINUS:
                    if (left.type != ValueType::NUMBER || right.type != ValueType::NUMBER)
                        runtimeError(expr->line, "Invalid operands for '-'.");
                    return Value::Number(left.number - right.number);
                case TokenType::STAR:
                    if (left.type != ValueType::NUMBER || right.type != ValueType::NUMBER)
                        runtimeError(expr->line, "Invalid operands for '*'.");
                    return Value::Number(left.number * right.number);
                case TokenType::SLASH:
                    if (left.type != ValueType::NUMBER || right.type != ValueType::NUMBER)
                        runtimeError(expr->line, "Invalid operands for '/'.");
                    if (right.number == 0) runtimeError(expr->line, "Division by zero.");
                    return Value::Number(left.number / right.number);
                case TokenType::PERCENT:
                    if (left.type != ValueType::NUMBER || right.type != ValueType::NUMBER)
                        runtimeError(expr->line, "Invalid operands for '%'.");
                    if (right.number == 0) runtimeError(expr->line, "Division by zero (in '%').");
                    return Value::Number(std::fmod(left.number, right.number));
                // Relational operators: numbers compare numerically, strings/html
                // compare lexicographically (like sort() already does) — mixing
                // types, or comparing anything else (arrays, dicts, bools...), is
                // a clear error rather than a silently-wrong "0 < 0" comparison.
                case TokenType::GREATER:
                case TokenType::GREATER_EQUAL:
                case TokenType::LESS:
                case TokenType::LESS_EQUAL: {
                    bool bothNumbers = left.type == ValueType::NUMBER && right.type == ValueType::NUMBER;
                    bool bothTextual = (left.type == ValueType::STRING || left.type == ValueType::HTML) &&
                                       (right.type == ValueType::STRING || right.type == ValueType::HTML);
                    if (!bothNumbers && !bothTextual) {
                        runtimeError(expr->line, "Cannot compare " + left.typeName() + " and " + right.typeName() +
                                     " with a relational operator (<, <=, >, >=): both sides must be numbers, or both strings.");
                    }
                    int cmp = bothNumbers ? (left.number < right.number ? -1 : (left.number > right.number ? 1 : 0))
                                          : left.str.compare(right.str);
                    switch (expr->op) {
                        case TokenType::GREATER: return Value::Bool(cmp > 0);
                        case TokenType::GREATER_EQUAL: return Value::Bool(cmp >= 0);
                        case TokenType::LESS: return Value::Bool(cmp < 0);
                        default: return Value::Bool(cmp <= 0);
                    }
                }
                case TokenType::EQUAL_EQUAL: return Value::Bool(valuesEqual(left, right));
                case TokenType::BANG_EQUAL: return Value::Bool(!valuesEqual(left, right));
                // "value in collection" — an element of an array (structural
                // equality, like contains()), a key of a dict (like has()), or
                // a substring of a string/html (like contains() on a string).
                case TokenType::IN: {
                    if (right.type == ValueType::ARRAY) {
                        for (auto& item : *right.array) if (valuesEqual(item, left)) return Value::Bool(true);
                        return Value::Bool(false);
                    }
                    if (right.type == ValueType::DICT) {
                        std::string key = (left.type == ValueType::STRING) ? left.str : left.toString();
                        return Value::Bool(right.findDictEntry(key) != nullptr);
                    }
                    if (right.type == ValueType::STRING || right.type == ValueType::HTML) {
                        return Value::Bool(right.str.find(left.toString()) != std::string::npos);
                    }
                    runtimeError(expr->line, "'in' expects an array, dict, or string on the right-hand side (got " +
                                 right.typeName() + ").");
                }
                // Bitwise: operate on both sides truncated to a 64-bit integer,
                // then convert the result back to a double like every other
                // number in TON618 (see Value.hpp — there's no separate int type).
                case TokenType::AMPERSAND:
                case TokenType::PIPE:
                case TokenType::CARET:
                case TokenType::LESS_LESS:
                case TokenType::GREATER_GREATER: {
                    if (left.type != ValueType::NUMBER || right.type != ValueType::NUMBER) {
                        runtimeError(expr->line, "Bitwise operators need two numbers (got " +
                                     left.typeName() + " and " + right.typeName() + ").");
                    }
                    long long a = (long long)left.number, b = (long long)right.number;
                    switch (expr->op) {
                        case TokenType::AMPERSAND: return Value::Number((double)(a & b));
                        case TokenType::PIPE: return Value::Number((double)(a | b));
                        case TokenType::CARET: return Value::Number((double)(a ^ b));
                        case TokenType::LESS_LESS: return Value::Number((double)(a << b));
                        default: return Value::Number((double)(a >> b));
                    }
                }
                default: break;
            }
            return Value::Nil();
        }

        case ExprType::CALL: {
            Value callee = evaluate(expr->callee, env);
            std::vector<Value> args;
            for (auto& a : expr->args) args.push_back(evaluate(a, env));
            return callFunction(callee, args, expr->line);
        }

        case ExprType::ARRAY: {
            std::vector<Value> elems;
            for (auto& e : expr->elements) {
                if (e->type == ExprType::SPREAD) {
                    Value spread = evaluate(e->operand, env);
                    if (spread.type != ValueType::ARRAY) {
                        runtimeError(e->line, "'...' inside an array literal expects an array (got " + spread.typeName() + ").");
                    }
                    for (auto& item : *spread.array) elems.push_back(item);
                } else {
                    elems.push_back(evaluate(e, env));
                }
            }
            return Value::Array(elems);
        }

        case ExprType::DICT: {
            std::vector<std::pair<std::string, Value>> entries;
            auto setOrAppend = [&](const std::string& key, const Value& value) {
                for (auto& kv : entries) {
                    if (kv.first == key) { kv.second = value; return; }
                }
                entries.push_back({key, value});
            };
            for (auto& [keyExpr, valExpr] : expr->dictEntries) {
                if (!keyExpr) { // spread entry: {nullptr, spreadNode}
                    Value spread = evaluate(valExpr->operand, env);
                    if (spread.type != ValueType::DICT) {
                        runtimeError(valExpr->line, "'...' inside a dict literal expects a dict (got " + spread.typeName() + ").");
                    }
                    for (auto& kv : *spread.dict) setOrAppend(kv.first, kv.second);
                } else {
                    setOrAppend(keyExpr->litString, evaluate(valExpr, env));
                }
            }
            return Value::Dict(entries);
        }

        case ExprType::FUNCTION_EXPR: {
            auto fn = std::make_shared<FunctionObj>();
            fn->name = expr->name.empty() ? "<anonymous>" : expr->name;
            fn->params = expr->fnParams;
            fn->paramDefaults = expr->fnParamDefaults;
            fn->hasRestParam = expr->fnHasRestParam;
            fn->body = expr->fnBody;
            fn->closure = env;
            Value v; v.type = ValueType::FUNCTION; v.function = fn;
            return v;
        }

        case ExprType::TERNARY:
            return evaluate(expr->condition, env).isTruthy() ? evaluate(expr->left, env) : evaluate(expr->right, env);

        // SPREAD only ever appears as an element inside an ARRAY/DICT literal
        // (see those cases above), which unwrap and evaluate expr->operand
        // themselves — a SPREAD node is never evaluate()'d directly.
        case ExprType::SPREAD:
            runtimeError(expr->line, "'...' is only valid inside an array or dict literal.");

        case ExprType::INDEX: {
            Value target = evaluate(expr->indexTarget, env);

            if (target.type == ValueType::DICT) {
                Value keyVal = evaluate(expr->indexValue, env);
                std::string key = (keyVal.type == ValueType::STRING) ? keyVal.str : keyVal.toString();
                if (expr->value) {
                    Value v = evaluate(expr->value, env);
                    Value* existing = target.findDictEntry(key);
                    if (existing) *existing = v;
                    else target.dict->push_back({key, v});
                    return v;
                }
                Value* found = target.findDictEntry(key);
                return found ? *found : Value::Nil();
            }

            // Read-only character indexing: s[0] is a 1-character string.
            // Strings are immutable in TON618 — build a new one with
            // replace()/substring()/strings_* instead of assigning into an index.
            if (target.type == ValueType::STRING || target.type == ValueType::HTML) {
                if (expr->value) runtimeError(expr->line, "Strings are immutable: cannot assign to a string index. "
                                                            "Use replace()/substring() to build a new string instead.");
                Value idxVal = evaluate(expr->indexValue, env);
                int i = (int)idxVal.number;
                if (i < 0 || i >= (int)target.str.size()) runtimeError(expr->line, "String index out of bounds.");
                return Value::String(std::string(1, target.str[i]));
            }

            if (target.type != ValueType::ARRAY)
                runtimeError(expr->line, "Cannot index a value that is not an array, a dict, or a string (got " + target.typeName() + ").");

            Value idx = evaluate(expr->indexValue, env);
            int i = (int)idx.number;
            if (expr->value) {
                Value v = evaluate(expr->value, env);
                if (i < 0 || i >= (int)target.array->size()) runtimeError(expr->line, "Index out of bounds.");
                (*target.array)[i] = v;
                return v;
            }
            if (i < 0 || i >= (int)target.array->size()) runtimeError(expr->line, "Index out of bounds.");
            return (*target.array)[i];
        }
    }
    return Value::Nil();
}

Value Interpreter::callFunction(const Value& callee, std::vector<Value>& args, int line) {
    if (callee.type == ValueType::NATIVE_FUNCTION) return (*callee.nativeFn)(args);
    if (callee.type != ValueType::FUNCTION) runtimeError(line, "Only functions can be called.");

    auto fn = callee.function;
    // The rest parameter (if any) is always last and isn't counted below; a
    // fixed parameter is "required" only if it has no default value.
    size_t fixedCount = fn->hasRestParam ? fn->params.size() - 1 : fn->params.size();
    size_t requiredCount = 0;
    for (size_t i = 0; i < fixedCount; i++) if (!fn->paramDefaults[i]) requiredCount++;

    if (args.size() < requiredCount || (!fn->hasRestParam && args.size() > fixedCount)) {
        std::string expectation;
        if (fn->hasRestParam) expectation = "at least " + std::to_string(requiredCount);
        else if (requiredCount == fixedCount) expectation = std::to_string(fixedCount);
        else expectation = "between " + std::to_string(requiredCount) + " and " + std::to_string(fixedCount);
        runtimeError(line, "'ton." + fn->name + "' expects " + expectation +
                     " argument(s) but got " + std::to_string(args.size()) + ".");
    }

    auto callEnv = std::make_shared<Environment>(fn->closure);
    for (size_t i = 0; i < fixedCount; i++) {
        Value v = (i < args.size()) ? args[i] : evaluate(fn->paramDefaults[i], callEnv);
        callEnv->define(fn->params[i], v);
    }
    if (fn->hasRestParam) {
        std::vector<Value> rest;
        for (size_t i = fixedCount; i < args.size(); i++) rest.push_back(args[i]);
        callEnv->define(fn->params.back(), Value::Array(rest));
    }

    debugger.callStack.push_back({fn->name, line, callEnv});
    Value result = Value::Nil();
    try {
        executeBlock(fn->body->statements, callEnv);
    } catch (ReturnException& r) {
        result = r.value;
    }
    debugger.callStack.pop_back();
    return result;
}

// ============================================================================
// Built-in system modules — each function below is only wired into `globals`
// once the matching "IMPORT://ton.<name>" statement runs (see runImport
// above). They follow the same module_functionName() prefixing convention
// documented for user-written modules in DOCUMENTATION.md.
// ============================================================================

// ---- ton.sys — runtime/process information -------------------------------
void Interpreter::registerBuiltinSys() {
    auto def = [this](const std::string& name, NativeFn fn) {
        Value v; v.type = ValueType::NATIVE_FUNCTION;
        v.nativeFn = std::make_shared<NativeFn>(std::move(fn));
        globals->define(name, v);
    };

    // sys_args() — extra command-line arguments passed after the script path.
    def("sys_args", [this](std::vector<Value>&) -> Value {
        std::vector<Value> out;
        for (auto& a : scriptArgs) out.push_back(Value::String(a));
        return Value::Array(out);
    });

    // sys_platform() — "windows", "termux", "macos", or "linux" (auto-detected;
    // see Platform.hpp for how Termux is told apart from plain Linux).
    def("sys_platform", [](std::vector<Value>&) -> Value {
        return Value::String(detectPlatform());
    });

    // sys_arch() — "x64", "arm64", "arm", "x86", or "unknown".
    def("sys_arch", [](std::vector<Value>&) -> Value {
        return Value::String(detectArch());
    });

    // sys_version() — the interpreter's own version string (see Version.hpp),
    // the same one printed by `ton618 --version` and used by `--update`.
    def("sys_version", [](std::vector<Value>&) -> Value {
        return Value::String(TON618_VERSION);
    });

    // sys_exit(code) — stops the whole program immediately with the given exit code.
    def("sys_exit", [](std::vector<Value>& args) -> Value {
        std::exit(args.empty() ? 0 : (int)args[0].number);
    });

    // sys_sleep(ms) — pauses execution for the given number of milliseconds.
    def("sys_sleep", [](std::vector<Value>& args) -> Value {
        if (!args.empty() && args[0].number > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds((long long)args[0].number));
        return Value::Nil();
    });
}

// ---- ton.os — environment variables and filesystem access -----------------
void Interpreter::registerBuiltinOs() {
    auto def = [this](const std::string& name, NativeFn fn) {
        Value v; v.type = ValueType::NATIVE_FUNCTION;
        v.nativeFn = std::make_shared<NativeFn>(std::move(fn));
        globals->define(name, v);
    };

    def("os_name", [](std::vector<Value>&) -> Value {
#ifdef _WIN32
        return Value::String("nt");
#else
        return Value::String("posix");
#endif
    });

    def("os_getenv", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Nil();
        const char* v = std::getenv(args[0].toString().c_str());
        return v ? Value::String(v) : Value::Nil();
    });

    def("os_setenv", [](std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::Bool(false);
#ifdef _WIN32
        return Value::Bool(_putenv_s(args[0].toString().c_str(), args[1].toString().c_str()) == 0);
#else
        return Value::Bool(setenv(args[0].toString().c_str(), args[1].toString().c_str(), 1) == 0);
#endif
    });

    def("os_cwd", [](std::vector<Value>&) -> Value {
        std::error_code ec;
        auto p = std::filesystem::current_path(ec);
        return ec ? Value::String("") : Value::String(p.string());
    });

    def("os_exists", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Bool(false);
        std::error_code ec;
        return Value::Bool(std::filesystem::exists(args[0].toString(), ec));
    });

    def("os_mkdir", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Bool(false);
        std::error_code ec;
        return Value::Bool(std::filesystem::create_directories(args[0].toString(), ec));
    });

    def("os_remove", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Bool(false);
        std::error_code ec;
        return Value::Bool(std::filesystem::remove(args[0].toString(), ec));
    });

    // os_listdir(path) -> array of entry names (files and subdirectories), not recursive.
    def("os_listdir", [](std::vector<Value>& args) -> Value {
        std::vector<Value> out;
        if (args.empty()) return Value::Array(out);
        std::error_code ec;
        for (auto& entry : std::filesystem::directory_iterator(args[0].toString(), ec)) {
            out.push_back(Value::String(entry.path().filename().string()));
        }
        return Value::Array(out);
    });

    // os_tempdir() -> the system's temporary-files directory.
    def("os_tempdir", [](std::vector<Value>&) -> Value {
        std::error_code ec;
        auto p = std::filesystem::temp_directory_path(ec);
        return ec ? Value::String("") : Value::String(p.string());
    });

    // os_copy(src, dst) -> copies a file, overwriting dst if it already exists.
    def("os_copy", [](std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::Bool(false);
        std::error_code ec;
        return Value::Bool(std::filesystem::copy_file(
            args[0].toString(), args[1].toString(),
            std::filesystem::copy_options::overwrite_existing, ec));
    });

    // os_rename(src, dst) -> renames/moves a file or directory.
    def("os_rename", [](std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::Bool(false);
        std::error_code ec;
        std::filesystem::rename(args[0].toString(), args[1].toString(), ec);
        return Value::Bool(!ec);
    });

    // os_isfile(path) / os_isdir(path) -> whether path exists and is that kind of entry.
    def("os_isfile", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Bool(false);
        std::error_code ec;
        return Value::Bool(std::filesystem::is_regular_file(args[0].toString(), ec));
    });
    def("os_isdir", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Bool(false);
        std::error_code ec;
        return Value::Bool(std::filesystem::is_directory(args[0].toString(), ec));
    });

    // os_appendfile(path, content) -> appends text to a file, creating it if needed.
    def("os_appendfile", [](std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::Bool(false);
        std::ofstream file(args[0].toString(), std::ios::app);
        if (!file) return Value::Bool(false);
        file << args[1].toString();
        return Value::Bool(true);
    });

    // os_readlines(path) -> array of the file's lines (no trailing newlines).
    def("os_readlines", [](std::vector<Value>& args) -> Value {
        std::vector<Value> out;
        if (args.empty()) return Value::Array(out);
        std::ifstream file(args[0].toString());
        if (!file) return Value::Array(out);
        std::string line;
        while (std::getline(file, line)) out.push_back(Value::String(line));
        return Value::Array(out);
    });
}

// ---- ton.requests — a minimal HTTP client (http:// only, see HttpClient.hpp) ----
void Interpreter::registerBuiltinRequests() {
    auto def = [this](const std::string& name, NativeFn fn) {
        Value v; v.type = ValueType::NATIVE_FUNCTION;
        v.nativeFn = std::make_shared<NativeFn>(std::move(fn));
        globals->define(name, v);
    };

    auto toResultDict = [](const HttpClientResponse& r) -> Value {
        std::vector<std::pair<std::string, Value>> entries = {
            {"ok", Value::Bool(r.ok)},
            {"status", Value::Number(r.status)},
            {"body", Value::String(r.body)},
            {"error", Value::String(r.error)},
        };
        return Value::Dict(entries);
    };

    // requests_get(url) -> {ok, status, body, error}
    def("requests_get", [toResultDict](std::vector<Value>& args) -> Value {
        if (args.empty()) throw std::runtime_error("requests_get(url) expects a URL string.");
        return toResultDict(httpRequest("GET", args[0].toString(), ""));
    });

    // requests_post(url, [body]) -> {ok, status, body, error}
    def("requests_post", [toResultDict](std::vector<Value>& args) -> Value {
        if (args.empty()) throw std::runtime_error("requests_post(url, [body]) expects a URL string.");
        std::string body = args.size() >= 2 ? args[1].toString() : "";
        return toResultDict(httpRequest("POST", args[0].toString(), body));
    });

    // requests_request(method, url, [body]) -> {ok, status, body, error} — for any HTTP method.
    def("requests_request", [toResultDict](std::vector<Value>& args) -> Value {
        if (args.size() < 2) throw std::runtime_error("requests_request(method, url, [body]) expects a method and a URL.");
        std::string body = args.size() >= 3 ? args[2].toString() : "";
        return toResultDict(httpRequest(args[0].toString(), args[1].toString(), body));
    });
}

// ---- ton.random — extra randomness helpers on top of the always-available random() ----
void Interpreter::registerBuiltinRandom() {
    auto def = [this](const std::string& name, NativeFn fn) {
        Value v; v.type = ValueType::NATIVE_FUNCTION;
        v.nativeFn = std::make_shared<NativeFn>(std::move(fn));
        globals->define(name, v);
    };

    // random_int(min, max) -> an integer in [min, max], inclusive on both ends.
    def("random_int", [](std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::Number(0);
        int lo = (int)args[0].number, hi = (int)args[1].number;
        if (hi <= lo) return Value::Number(lo);
        return Value::Number(lo + std::rand() % (hi - lo + 1));
    });

    def("random_float", [](std::vector<Value>&) -> Value {
        return Value::Number((double)std::rand() / ((double)RAND_MAX + 1.0));
    });

    // random_choice(arr) -> a uniformly random element from a non-empty array.
    def("random_choice", [](std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::ARRAY || args[0].array->empty()) return Value::Nil();
        auto& arr = *args[0].array;
        return arr[std::rand() % arr.size()];
    });

    // random_shuffle(arr) -> shuffles the array in place (Fisher-Yates) and returns it.
    def("random_shuffle", [](std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::ARRAY) return args.empty() ? Value::Nil() : args[0];
        auto& arr = *args[0].array;
        for (size_t i = arr.size(); i > 1; i--) {
            size_t j = std::rand() % i;
            std::swap(arr[i - 1], arr[j]);
        }
        return args[0];
    });

    // random_seed(n) -> reseeds the RNG deterministically (useful for reproducible tests).
    def("random_seed", [](std::vector<Value>& args) -> Value {
        std::srand(args.empty() ? 0 : (unsigned)args[0].number);
        return Value::Nil();
    });
}

// ---- ton.time — clocks and human-readable timestamps -----------------------
void Interpreter::registerBuiltinTime() {
    auto def = [this](const std::string& name, NativeFn fn) {
        Value v; v.type = ValueType::NATIVE_FUNCTION;
        v.nativeFn = std::make_shared<NativeFn>(std::move(fn));
        globals->define(name, v);
    };

    // time_now() -> seconds since the Unix epoch.
    def("time_now", [](std::vector<Value>&) -> Value {
        return Value::Number((double)std::time(nullptr));
    });

    // time_millis() -> milliseconds since the Unix epoch — handy for benchmarking.
    def("time_millis", [](std::vector<Value>&) -> Value {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        return Value::Number((double)std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    });

    // time_string([timestamp]) -> a human-readable local time string; defaults to now.
    def("time_string", [](std::vector<Value>& args) -> Value {
        std::time_t t = args.empty() ? std::time(nullptr) : (std::time_t)args[0].number;
        std::string s = std::ctime(&t);
        if (!s.empty() && s.back() == '\n') s.pop_back();
        return Value::String(s);
    });

    // time_sleep(ms) -> pauses execution for the given number of milliseconds.
    def("time_sleep", [](std::vector<Value>& args) -> Value {
        if (!args.empty() && args[0].number > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds((long long)args[0].number));
        return Value::Nil();
    });
}

// ---- ton.json — JSON parsing, complementing the always-available json() encoder ----
void Interpreter::registerBuiltinJson() {
    auto def = [this](const std::string& name, NativeFn fn) {
        Value v; v.type = ValueType::NATIVE_FUNCTION;
        v.nativeFn = std::make_shared<NativeFn>(std::move(fn));
        globals->define(name, v);
    };

    // json_parse(text) -> a ton.dict/ton.array/scalar built from the given JSON text.
    def("json_parse", [](std::vector<Value>& args) -> Value {
        if (args.empty()) throw std::runtime_error("json_parse(text) expects a JSON string.");
        return parseJson(args[0].toString());
    });

    // json_stringify(value) -> same as the always-available json(value); provided here
    // too so a script that imports ton.json can use one consistent naming style.
    def("json_stringify", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::String("null");
        return Value::String(args[0].toJson());
    });

    // json_pretty(value) -> same as json_stringify, indented across multiple lines.
    def("json_pretty", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::String("null");
        return Value::String(args[0].toJsonPretty());
    });
}

// ---- ton.mathutils — trig/log/stats helpers, complementing the always-available
// sqrt/pow/abs/floor/ceil/round/min/max from defineNatives() -----------------
void Interpreter::registerBuiltinMathUtils() {
    auto def = [this](const std::string& name, NativeFn fn) {
        Value v; v.type = ValueType::NATIVE_FUNCTION;
        v.nativeFn = std::make_shared<NativeFn>(std::move(fn));
        globals->define(name, v);
    };

    def("mathutils_pi", [](std::vector<Value>&) -> Value { return Value::Number(M_PI); });
    def("mathutils_e", [](std::vector<Value>&) -> Value { return Value::Number(M_E); });

    def("mathutils_sin", [](std::vector<Value>& a) -> Value { return Value::Number(std::sin(a.empty() ? 0 : a[0].number)); });
    def("mathutils_cos", [](std::vector<Value>& a) -> Value { return Value::Number(std::cos(a.empty() ? 0 : a[0].number)); });
    def("mathutils_tan", [](std::vector<Value>& a) -> Value { return Value::Number(std::tan(a.empty() ? 0 : a[0].number)); });
    def("mathutils_asin", [](std::vector<Value>& a) -> Value { return Value::Number(std::asin(a.empty() ? 0 : a[0].number)); });
    def("mathutils_acos", [](std::vector<Value>& a) -> Value { return Value::Number(std::acos(a.empty() ? 0 : a[0].number)); });
    def("mathutils_atan", [](std::vector<Value>& a) -> Value { return Value::Number(std::atan(a.empty() ? 0 : a[0].number)); });
    def("mathutils_atan2", [](std::vector<Value>& a) -> Value {
        if (a.size() < 2) throw std::runtime_error("mathutils_atan2(y, x) expects two numbers.");
        return Value::Number(std::atan2(a[0].number, a[1].number));
    });

    def("mathutils_log", [](std::vector<Value>& a) -> Value { return Value::Number(std::log(a.empty() ? 0 : a[0].number)); });
    def("mathutils_log2", [](std::vector<Value>& a) -> Value { return Value::Number(std::log2(a.empty() ? 0 : a[0].number)); });
    def("mathutils_log10", [](std::vector<Value>& a) -> Value { return Value::Number(std::log10(a.empty() ? 0 : a[0].number)); });
    def("mathutils_exp", [](std::vector<Value>& a) -> Value { return Value::Number(std::exp(a.empty() ? 0 : a[0].number)); });
    def("mathutils_hypot", [](std::vector<Value>& a) -> Value {
        if (a.size() < 2) throw std::runtime_error("mathutils_hypot(x, y) expects two numbers.");
        return Value::Number(std::hypot(a[0].number, a[1].number));
    });

    def("mathutils_degrees", [](std::vector<Value>& a) -> Value { return Value::Number((a.empty() ? 0 : a[0].number) * 180.0 / M_PI); });
    def("mathutils_radians", [](std::vector<Value>& a) -> Value { return Value::Number((a.empty() ? 0 : a[0].number) * M_PI / 180.0); });

    // mathutils_clamp(x, lo, hi) -> x restricted to [lo, hi].
    def("mathutils_clamp", [](std::vector<Value>& a) -> Value {
        if (a.size() < 3) throw std::runtime_error("mathutils_clamp(x, lo, hi) expects three numbers.");
        double x = a[0].number, lo = a[1].number, hi = a[2].number;
        return Value::Number(std::min(std::max(x, lo), hi));
    });

    // mathutils_lerp(a, b, t) -> linear interpolation between a and b at t (0..1).
    def("mathutils_lerp", [](std::vector<Value>& a) -> Value {
        if (a.size() < 3) throw std::runtime_error("mathutils_lerp(a, b, t) expects three numbers.");
        return Value::Number(a[0].number + (a[1].number - a[0].number) * a[2].number);
    });

    def("mathutils_sign", [](std::vector<Value>& a) -> Value {
        double x = a.empty() ? 0 : a[0].number;
        return Value::Number(x > 0 ? 1 : (x < 0 ? -1 : 0));
    });

    // mathutils_gcd(a, b) -> greatest common divisor of two integers.
    def("mathutils_gcd", [](std::vector<Value>& a) -> Value {
        if (a.size() < 2) throw std::runtime_error("mathutils_gcd(a, b) expects two integers.");
        long long x = std::llabs((long long)a[0].number), y = std::llabs((long long)a[1].number);
        while (y != 0) { long long t = y; y = x % y; x = t; }
        return Value::Number((double)x);
    });

    // mathutils_lcm(a, b) -> least common multiple of two integers.
    def("mathutils_lcm", [](std::vector<Value>& a) -> Value {
        if (a.size() < 2) throw std::runtime_error("mathutils_lcm(a, b) expects two integers.");
        long long x = std::llabs((long long)a[0].number), y = std::llabs((long long)a[1].number);
        if (x == 0 || y == 0) return Value::Number(0);
        long long g = x, h = y;
        while (h != 0) { long long t = h; h = g % h; g = t; }
        return Value::Number((double)(x / g * y));
    });

    // mathutils_factorial(n) -> n! for a non-negative integer n.
    def("mathutils_factorial", [](std::vector<Value>& a) -> Value {
        long long n = a.empty() ? 0 : (long long)a[0].number;
        if (n < 0) throw std::runtime_error("mathutils_factorial(n) expects a non-negative integer.");
        double result = 1;
        for (long long i = 2; i <= n; i++) result *= (double)i;
        return Value::Number(result);
    });

    // mathutils_is_prime(n) -> true if n is a prime integer.
    def("mathutils_is_prime", [](std::vector<Value>& a) -> Value {
        long long n = a.empty() ? 0 : (long long)a[0].number;
        if (n < 2) return Value::Bool(false);
        for (long long i = 2; i * i <= n; i++) if (n % i == 0) return Value::Bool(false);
        return Value::Bool(true);
    });

    // mathutils_sum(arr) -> sum of a numeric array.
    def("mathutils_sum", [](std::vector<Value>& a) -> Value {
        if (a.empty() || a[0].type != ValueType::ARRAY) return Value::Number(0);
        double total = 0;
        for (auto& v : *a[0].array) total += v.number;
        return Value::Number(total);
    });

    // mathutils_mean(arr) -> arithmetic mean of a numeric array.
    def("mathutils_mean", [](std::vector<Value>& a) -> Value {
        if (a.empty() || a[0].type != ValueType::ARRAY || a[0].array->empty()) return Value::Number(0);
        double total = 0;
        for (auto& v : *a[0].array) total += v.number;
        return Value::Number(total / (double)a[0].array->size());
    });

    // mathutils_median(arr) -> median of a numeric array.
    def("mathutils_median", [](std::vector<Value>& a) -> Value {
        if (a.empty() || a[0].type != ValueType::ARRAY || a[0].array->empty()) return Value::Number(0);
        std::vector<double> nums;
        for (auto& v : *a[0].array) nums.push_back(v.number);
        std::sort(nums.begin(), nums.end());
        size_t n = nums.size();
        return Value::Number(n % 2 == 1 ? nums[n / 2] : (nums[n / 2 - 1] + nums[n / 2]) / 2.0);
    });

    // mathutils_min_of(arr) / mathutils_max_of(arr) -> smallest/largest number in an
    // array — unlike the always-available min()/max(), which take separate arguments
    // (min(1, 2, 3)) rather than one array value.
    def("mathutils_min_of", [](std::vector<Value>& a) -> Value {
        if (a.empty() || a[0].type != ValueType::ARRAY || a[0].array->empty()) return Value::Nil();
        double m = (*a[0].array)[0].number;
        for (auto& v : *a[0].array) m = std::min(m, v.number);
        return Value::Number(m);
    });
    def("mathutils_max_of", [](std::vector<Value>& a) -> Value {
        if (a.empty() || a[0].type != ValueType::ARRAY || a[0].array->empty()) return Value::Nil();
        double m = (*a[0].array)[0].number;
        for (auto& v : *a[0].array) m = std::max(m, v.number);
        return Value::Number(m);
    });

    // mathutils_stddev(arr) -> population standard deviation of a numeric array.
    def("mathutils_stddev", [](std::vector<Value>& a) -> Value {
        if (a.empty() || a[0].type != ValueType::ARRAY || a[0].array->empty()) return Value::Number(0);
        auto& arr = *a[0].array;
        double mean = 0;
        for (auto& v : arr) mean += v.number;
        mean /= (double)arr.size();
        double variance = 0;
        for (auto& v : arr) variance += (v.number - mean) * (v.number - mean);
        variance /= (double)arr.size();
        return Value::Number(std::sqrt(variance));
    });
}

// ---- ton.strings — extra string helpers, complementing the always-available
// upper/lower/trim/split/replace/contains/substring from defineNatives() ------
void Interpreter::registerBuiltinStrings() {
    auto def = [this](const std::string& name, NativeFn fn) {
        Value v; v.type = ValueType::NATIVE_FUNCTION;
        v.nativeFn = std::make_shared<NativeFn>(std::move(fn));
        globals->define(name, v);
    };

    def("strings_starts_with", [](std::vector<Value>& a) -> Value {
        if (a.size() < 2) return Value::Bool(false);
        std::string s = a[0].toString(), p = a[1].toString();
        return Value::Bool(s.size() >= p.size() && s.compare(0, p.size(), p) == 0);
    });

    def("strings_ends_with", [](std::vector<Value>& a) -> Value {
        if (a.size() < 2) return Value::Bool(false);
        std::string s = a[0].toString(), p = a[1].toString();
        return Value::Bool(s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0);
    });

    def("strings_repeat", [](std::vector<Value>& a) -> Value {
        if (a.size() < 2) return Value::String("");
        std::string s = a[0].toString();
        int n = std::max(0, (int)a[1].number);
        std::string out;
        out.reserve(s.size() * n);
        for (int i = 0; i < n; i++) out += s;
        return Value::String(out);
    });

    def("strings_reverse", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        std::string s = a[0].toString();
        std::reverse(s.begin(), s.end());
        return Value::String(s);
    });

    def("strings_capitalize", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        std::string s = a[0].toString();
        if (!s.empty()) s[0] = (char)std::toupper((unsigned char)s[0]);
        for (size_t i = 1; i < s.size(); i++) s[i] = (char)std::tolower((unsigned char)s[i]);
        return Value::String(s);
    });

    // strings_pad_left(s, width, [ch]) -> left-pads s with ch (default " ") up to width.
    def("strings_pad_left", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        std::string s = a[0].toString();
        int width = a.size() >= 2 ? (int)a[1].number : 0;
        char ch = (a.size() >= 3 && !a[2].toString().empty()) ? a[2].toString()[0] : ' ';
        if ((int)s.size() >= width) return Value::String(s);
        return Value::String(std::string(width - s.size(), ch) + s);
    });

    // strings_pad_right(s, width, [ch]) -> right-pads s with ch (default " ") up to width.
    def("strings_pad_right", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        std::string s = a[0].toString();
        int width = a.size() >= 2 ? (int)a[1].number : 0;
        char ch = (a.size() >= 3 && !a[2].toString().empty()) ? a[2].toString()[0] : ' ';
        if ((int)s.size() >= width) return Value::String(s);
        return Value::String(s + std::string(width - s.size(), ch));
    });

    // strings_count(s, sub) -> number of non-overlapping occurrences of sub in s.
    def("strings_count", [](std::vector<Value>& a) -> Value {
        if (a.size() < 2 || a[1].toString().empty()) return Value::Number(0);
        std::string s = a[0].toString(), sub = a[1].toString();
        int count = 0;
        size_t pos = 0;
        while ((pos = s.find(sub, pos)) != std::string::npos) { count++; pos += sub.size(); }
        return Value::Number(count);
    });

    // strings_trim_start(s) / strings_trim_end(s) -> trim() (see defineNatives)
    // only strips both ends at once; these strip just one side.
    def("strings_trim_start", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        std::string s = a[0].toString();
        size_t start = s.find_first_not_of(" \t\n\r");
        return Value::String(start == std::string::npos ? "" : s.substr(start));
    });

    def("strings_trim_end", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        std::string s = a[0].toString();
        size_t end = s.find_last_not_of(" \t\n\r");
        return Value::String(end == std::string::npos ? "" : s.substr(0, end + 1));
    });

    // strings_words(s) -> array of whitespace-separated words (unlike split(),
    // which needs an exact separator and produces empty entries on runs of it).
    def("strings_words", [](std::vector<Value>& a) -> Value {
        std::vector<Value> out;
        if (a.empty()) return Value::Array(out);
        std::istringstream iss(a[0].toString());
        std::string w;
        while (iss >> w) out.push_back(Value::String(w));
        return Value::Array(out);
    });

    // strings_center(s, width, [ch]) -> centers s within width, padding both sides with ch.
    def("strings_center", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        std::string s = a[0].toString();
        int width = a.size() >= 2 ? (int)a[1].number : 0;
        char ch = (a.size() >= 3 && !a[2].toString().empty()) ? a[2].toString()[0] : ' ';
        int total = width - (int)s.size();
        if (total <= 0) return Value::String(s);
        int left = total / 2, right = total - left;
        return Value::String(std::string(left, ch) + s + std::string(right, ch));
    });

    // strings_replace_first(s, search, repl) -> replaces only the first occurrence of search.
    def("strings_replace_first", [](std::vector<Value>& a) -> Value {
        if (a.size() < 3) return a.empty() ? Value::String("") : a[0];
        std::string s = a[0].toString(), search = a[1].toString(), repl = a[2].toString();
        if (search.empty()) return Value::String(s);
        size_t pos = s.find(search);
        if (pos == std::string::npos) return Value::String(s);
        return Value::String(s.substr(0, pos) + repl + s.substr(pos + search.size()));
    });

    // strings_snake_case(s) -> "helloWorld"/"Hello World" -> "hello_world".
    def("strings_snake_case", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        std::string s = a[0].toString(), out;
        for (size_t i = 0; i < s.size(); i++) {
            char c = s[i];
            if (c == ' ' || c == '-') { out += '_'; continue; }
            if (std::isupper((unsigned char)c) && i > 0 && s[i - 1] != '_' && s[i - 1] != ' ' && s[i - 1] != '-') out += '_';
            out += (char)std::tolower((unsigned char)c);
        }
        return Value::String(out);
    });

    // strings_camel_case(s) -> "hello_world"/"hello world" -> "helloWorld".
    def("strings_camel_case", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        std::string s = a[0].toString(), out;
        bool upperNext = false;
        for (char c : s) {
            if (c == '_' || c == ' ' || c == '-') { upperNext = true; continue; }
            out += upperNext ? (char)std::toupper((unsigned char)c) : c;
            upperNext = false;
        }
        return Value::String(out);
    });
}

// ---- ton.encoding — base64, hex, and URL percent-encoding, dependency-free ----
void Interpreter::registerBuiltinEncoding() {
    auto def = [this](const std::string& name, NativeFn fn) {
        Value v; v.type = ValueType::NATIVE_FUNCTION;
        v.nativeFn = std::make_shared<NativeFn>(std::move(fn));
        globals->define(name, v);
    };

    static const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    def("base64_encode", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        std::string in = a[0].toString(), out;
        int val = 0, bits = -6;
        for (unsigned char c : in) {
            val = (val << 8) + c;
            bits += 8;
            while (bits >= 0) { out += base64Chars[(val >> bits) & 0x3F]; bits -= 6; }
        }
        if (bits > -6) out += base64Chars[((val << 8) >> (bits + 8)) & 0x3F];
        while (out.size() % 4) out += '=';
        return Value::String(out);
    });

    def("base64_decode", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        static int table[256];
        static bool initialized = false;
        if (!initialized) {
            for (int i = 0; i < 256; i++) table[i] = -1;
            for (int i = 0; i < 64; i++) table[(unsigned char)base64Chars[i]] = i;
            initialized = true;
        }
        std::string in = a[0].toString(), out;
        int val = 0, bits = -8;
        for (unsigned char c : in) {
            if (table[c] == -1) continue;
            val = (val << 6) + table[c];
            bits += 6;
            if (bits >= 0) { out += (char)((val >> bits) & 0xFF); bits -= 8; }
        }
        return Value::String(out);
    });

    def("hex_encode", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        static const char* hexDigits = "0123456789abcdef";
        std::string in = a[0].toString(), out;
        out.reserve(in.size() * 2);
        for (unsigned char c : in) { out += hexDigits[c >> 4]; out += hexDigits[c & 0xF]; }
        return Value::String(out);
    });

    def("hex_decode", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        std::string in = a[0].toString(), out;
        for (size_t i = 0; i + 1 < in.size(); i += 2) {
            int hi = hexDigitValue(in[i]), lo = hexDigitValue(in[i + 1]);
            if (hi < 0 || lo < 0) break;
            out += (char)((hi << 4) | lo);
        }
        return Value::String(out);
    });

    def("url_encode", [](std::vector<Value>& a) -> Value {
        return Value::String(a.empty() ? "" : urlEncode(a[0].toString()));
    });
    def("url_decode", [](std::vector<Value>& a) -> Value {
        return Value::String(a.empty() ? "" : urlDecode(a[0].toString()));
    });
}

// ---- ton.regex — pattern matching backed by C++'s standard <regex> (ECMAScript
// syntax), no external dependency ---------------------------------------------
void Interpreter::registerBuiltinRegex() {
    auto def = [this](const std::string& name, NativeFn fn) {
        Value v; v.type = ValueType::NATIVE_FUNCTION;
        v.nativeFn = std::make_shared<NativeFn>(std::move(fn));
        globals->define(name, v);
    };

    auto compile = [](const std::string& pattern) -> std::regex {
        try {
            return std::regex(pattern, std::regex::ECMAScript);
        } catch (std::regex_error& e) {
            throw std::runtime_error("Invalid regex pattern '" + pattern + "': " + e.what());
        }
    };

    def("regex_test", [compile](std::vector<Value>& a) -> Value {
        if (a.size() < 2) throw std::runtime_error("regex_test(s, pattern) expects two strings.");
        return Value::Bool(std::regex_search(a[0].toString(), compile(a[1].toString())));
    });

    // regex_match(s, pattern) -> [fullMatch, group1, group2, ...] for the first
    // match, or nil if the pattern doesn't match anywhere in s.
    def("regex_match", [compile](std::vector<Value>& a) -> Value {
        if (a.size() < 2) throw std::runtime_error("regex_match(s, pattern) expects two strings.");
        std::string s = a[0].toString();
        std::smatch m;
        if (!std::regex_search(s, m, compile(a[1].toString()))) return Value::Nil();
        std::vector<Value> out;
        for (auto& g : m) out.push_back(Value::String(g.str()));
        return Value::Array(out);
    });

    // regex_find_all(s, pattern) -> array of every full match found in s.
    def("regex_find_all", [compile](std::vector<Value>& a) -> Value {
        if (a.size() < 2) throw std::runtime_error("regex_find_all(s, pattern) expects two strings.");
        std::string s = a[0].toString();
        std::regex re = compile(a[1].toString());
        std::vector<Value> out;
        for (auto it = std::sregex_iterator(s.begin(), s.end(), re); it != std::sregex_iterator(); ++it)
            out.push_back(Value::String(it->str()));
        return Value::Array(out);
    });

    // regex_replace(s, pattern, repl) -> s with every match replaced; repl can
    // use $1, $2... for capture groups (ECMAScript replacement syntax).
    def("regex_replace", [compile](std::vector<Value>& a) -> Value {
        if (a.size() < 3) throw std::runtime_error("regex_replace(s, pattern, repl) expects three strings.");
        return Value::String(std::regex_replace(a[0].toString(), compile(a[1].toString()), a[2].toString()));
    });

    // regex_split(s, pattern) -> array of substrings, splitting s on every match.
    def("regex_split", [compile](std::vector<Value>& a) -> Value {
        if (a.size() < 2) throw std::runtime_error("regex_split(s, pattern) expects two strings.");
        std::string s = a[0].toString();
        std::regex re = compile(a[1].toString());
        std::vector<Value> out;
        std::sregex_token_iterator it(s.begin(), s.end(), re, -1), end;
        for (; it != end; ++it) out.push_back(Value::String(*it));
        return Value::Array(out);
    });
}

// ---- ton.path — filesystem path manipulation (no I/O — see ton.os for that) --
void Interpreter::registerBuiltinPath() {
    auto def = [this](const std::string& name, NativeFn fn) {
        Value v; v.type = ValueType::NATIVE_FUNCTION;
        v.nativeFn = std::make_shared<NativeFn>(std::move(fn));
        globals->define(name, v);
    };

    // path_join(a, b, ...) -> joins any number of path segments with the platform separator.
    def("path_join", [](std::vector<Value>& a) -> Value {
        std::filesystem::path p;
        for (auto& v : a) p /= v.toString();
        return Value::String(p.generic_string());
    });
    def("path_basename", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        return Value::String(std::filesystem::path(a[0].toString()).filename().generic_string());
    });
    def("path_dirname", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        return Value::String(std::filesystem::path(a[0].toString()).parent_path().generic_string());
    });
    def("path_extension", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        return Value::String(std::filesystem::path(a[0].toString()).extension().generic_string());
    });
    def("path_stem", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        return Value::String(std::filesystem::path(a[0].toString()).stem().generic_string());
    });
    def("path_absolute", [](std::vector<Value>& a) -> Value {
        if (a.empty()) return Value::String("");
        std::error_code ec;
        auto p = std::filesystem::absolute(a[0].toString(), ec);
        return ec ? a[0] : Value::String(p.generic_string());
    });
}

// ---- ton.tensor — small 1D/2D numeric arrays for basic ML work -------------
//
// A tensor is a plain ton.dict with two fields:
//   "shape": ton.array of dimension sizes (exactly 1 or 2 entries)
//   "data":  flat ton.array of numbers, row-major
// so a tensor prints/serializes (str()/json()) like any other dict, and only
// the functions below need to know the convention. This exact representation
// is also what the "atome" community module (a plain .ton file — not part of
// this interpreter) builds its autograd engine on top of, so a script mixing
// atome_tensor_* and tensor_* calls can pass tensors between them freely.
//
// Honesty about scope: this is CPU-only (there is no GPU compute backend —
// no CUDA/OpenCL — anywhere in this interpreter), limited to 1D/2D shapes,
// and only broadcasts a plain scalar against a tensor (the one exception is
// tensor_add_bias, which explicitly broadcasts a 1D bias across a 2D
// matrix's rows — exactly what a dense/linear layer needs). Element-wise/
// matmul/reduction ops are implemented as real C++ loops over the underlying
// std::vector<Value> (bypassing the AST entirely), a genuine, meaningful
// speed-up over the same loop hand-written in TON618 — just not remotely
// competitive with a real BLAS/cuDNN-backed library.

static std::vector<long long> tensorShapeVec(const Value& t) {
    std::vector<long long> shape;
    Value* s = t.findDictEntry("shape");
    if (s && s->type == ValueType::ARRAY) for (auto& v : *s->array) shape.push_back((long long)v.number);
    return shape;
}

static long long tensorSizeOf(const std::vector<long long>& shape) {
    if (shape.empty()) return 0;
    long long n = 1;
    for (auto d : shape) n *= d;
    return n;
}

static bool isTensorValue(const Value& v) {
    return v.type == ValueType::DICT && v.findDictEntry("shape") && v.findDictEntry("data");
}

static Value makeTensor(const std::vector<long long>& shape, std::vector<Value> data) {
    std::vector<Value> shapeArr;
    for (auto d : shape) shapeArr.push_back(Value::Number((double)d));
    std::vector<std::pair<std::string, Value>> entries;
    entries.push_back({"shape", Value::Array(shapeArr)});
    entries.push_back({"data", Value::Array(std::move(data))});
    return Value::Dict(entries);
}

void Interpreter::registerBuiltinTensor() {
    auto def = [this](const std::string& name, NativeFn fn) {
        Value v; v.type = ValueType::NATIVE_FUNCTION;
        v.nativeFn = std::make_shared<NativeFn>(std::move(fn));
        globals->define(name, v);
    };

    auto requireTensor = [](std::vector<Value>& a, size_t i, const char* fnName) -> Value& {
        if (i >= a.size() || !isTensorValue(a[i]))
            throw std::runtime_error(std::string(fnName) + "() expects a tensor (from tensor_zeros/tensor_from_array/...) at argument " + std::to_string(i + 1) + ".");
        return a[i];
    };

    auto shapeFromArg = [](std::vector<Value>& a, size_t i, const char* fnName) -> std::vector<long long> {
        if (i >= a.size() || a[i].type != ValueType::ARRAY)
            throw std::runtime_error(std::string(fnName) + "() expects a shape array, e.g. [3, 4].");
        std::vector<long long> shape;
        for (auto& v : *a[i].array) shape.push_back((long long)v.number);
        if (shape.empty() || shape.size() > 2)
            throw std::runtime_error(std::string(fnName) + "(): only 1D and 2D shapes are supported (got " + std::to_string(shape.size()) + " dimensions).");
        return shape;
    };

    def("tensor_zeros", [shapeFromArg](std::vector<Value>& a) -> Value {
        auto shape = shapeFromArg(a, 0, "tensor_zeros");
        return makeTensor(shape, std::vector<Value>(tensorSizeOf(shape), Value::Number(0)));
    });
    def("tensor_ones", [shapeFromArg](std::vector<Value>& a) -> Value {
        auto shape = shapeFromArg(a, 0, "tensor_ones");
        return makeTensor(shape, std::vector<Value>(tensorSizeOf(shape), Value::Number(1)));
    });
    def("tensor_full", [shapeFromArg](std::vector<Value>& a) -> Value {
        auto shape = shapeFromArg(a, 0, "tensor_full");
        if (a.size() < 2) throw std::runtime_error("tensor_full(shape, value) expects a fill value.");
        return makeTensor(shape, std::vector<Value>(tensorSizeOf(shape), Value::Number(a[1].number)));
    });
    // tensor_random(shape, [lo=0, hi=1)) -> uniformly random values, using the
    // same std::rand() the always-available random() and ton.random use —
    // seed it with random_seed() (see ton.random) for reproducible runs.
    def("tensor_random", [shapeFromArg](std::vector<Value>& a) -> Value {
        auto shape = shapeFromArg(a, 0, "tensor_random");
        double lo = a.size() > 1 ? a[1].number : 0.0, hi = a.size() > 2 ? a[2].number : 1.0;
        long long n = tensorSizeOf(shape);
        std::vector<Value> data;
        data.reserve(n);
        for (long long i = 0; i < n; i++) {
            double r = (double)std::rand() / ((double)RAND_MAX + 1.0);
            data.push_back(Value::Number(lo + r * (hi - lo)));
        }
        return makeTensor(shape, data);
    });

    def("tensor_shape", [requireTensor](std::vector<Value>& a) -> Value {
        Value& t = requireTensor(a, 0, "tensor_shape");
        return *t.findDictEntry("shape");
    });
    def("tensor_size", [requireTensor](std::vector<Value>& a) -> Value {
        Value& t = requireTensor(a, 0, "tensor_size");
        return Value::Number((double)t.findDictEntry("data")->array->size());
    });
    def("tensor_clone", [requireTensor](std::vector<Value>& a) -> Value {
        Value& t = requireTensor(a, 0, "tensor_clone");
        auto& d = *t.findDictEntry("data")->array;
        return makeTensor(tensorShapeVec(t), std::vector<Value>(d.begin(), d.end()));
    });
    def("tensor_reshape", [requireTensor, shapeFromArg](std::vector<Value>& a) -> Value {
        Value& t = requireTensor(a, 0, "tensor_reshape");
        auto newShape = shapeFromArg(a, 1, "tensor_reshape");
        auto& d = *t.findDictEntry("data")->array;
        if (tensorSizeOf(newShape) != (long long)d.size())
            throw std::runtime_error("tensor_reshape(): new shape doesn't match the tensor's element count (" + std::to_string(d.size()) + ").");
        return makeTensor(newShape, std::vector<Value>(d.begin(), d.end()));
    });

    // tensor_get(t, indices) / tensor_set(t, indices, value) — indices is
    // [i] for a 1D tensor, or [row, col] for a 2D tensor.
    auto flatIndex = [](const std::vector<long long>& shape, std::vector<Value>& idx, const char* fnName) -> long long {
        if (idx.size() != shape.size())
            throw std::runtime_error(std::string(fnName) + "(): expected " + std::to_string(shape.size()) + " index/indices for this tensor's shape.");
        if (shape.size() == 1) {
            long long i = (long long)idx[0].number;
            if (i < 0 || i >= shape[0]) throw std::runtime_error(std::string(fnName) + "(): index out of bounds.");
            return i;
        }
        long long row = (long long)idx[0].number, col = (long long)idx[1].number;
        if (row < 0 || row >= shape[0] || col < 0 || col >= shape[1])
            throw std::runtime_error(std::string(fnName) + "(): index out of bounds.");
        return row * shape[1] + col;
    };
    def("tensor_get", [requireTensor, flatIndex](std::vector<Value>& a) -> Value {
        Value& t = requireTensor(a, 0, "tensor_get");
        if (a.size() < 2 || a[1].type != ValueType::ARRAY) throw std::runtime_error("tensor_get(t, indices) expects an index array.");
        return (*t.findDictEntry("data")->array)[flatIndex(tensorShapeVec(t), *a[1].array, "tensor_get")];
    });
    def("tensor_set", [requireTensor, flatIndex](std::vector<Value>& a) -> Value {
        Value& t = requireTensor(a, 0, "tensor_set");
        if (a.size() < 3 || a[1].type != ValueType::ARRAY) throw std::runtime_error("tensor_set(t, indices, value) expects an index array and a value.");
        (*t.findDictEntry("data")->array)[flatIndex(tensorShapeVec(t), *a[1].array, "tensor_set")] = Value::Number(a[2].number);
        return t;
    });

    // Element-wise binary ops: both tensors of the same shape, or one side a
    // plain number (scalar broadcast) — never two differently-shaped tensors.
    auto elementwise = [](std::vector<Value>& a, const char* fnName, double (*op)(double, double)) -> Value {
        bool leftIsTensor = !a.empty() && isTensorValue(a[0]);
        bool rightIsTensor = a.size() > 1 && isTensorValue(a[1]);
        if (a.size() < 2 || (!leftIsTensor && !rightIsTensor))
            throw std::runtime_error(std::string(fnName) + "(a, b): at least one side must be a tensor.");
        if (leftIsTensor && rightIsTensor) {
            auto shapeA = tensorShapeVec(a[0]), shapeB = tensorShapeVec(a[1]);
            if (shapeA != shapeB)
                throw std::runtime_error(std::string(fnName) + "(): tensors must have the same shape (no broadcasting between two differently-shaped tensors).");
            auto& da = *a[0].findDictEntry("data")->array;
            auto& db = *a[1].findDictEntry("data")->array;
            std::vector<Value> out(da.size());
            for (size_t i = 0; i < da.size(); i++) out[i] = Value::Number(op(da[i].number, db[i].number));
            return makeTensor(shapeA, out);
        }
        Value& tensor = leftIsTensor ? a[0] : a[1];
        double scalar = leftIsTensor ? a[1].number : a[0].number;
        auto shape = tensorShapeVec(tensor);
        auto& d = *tensor.findDictEntry("data")->array;
        std::vector<Value> out(d.size());
        for (size_t i = 0; i < d.size(); i++) out[i] = Value::Number(leftIsTensor ? op(d[i].number, scalar) : op(scalar, d[i].number));
        return makeTensor(shape, out);
    };
    def("tensor_add", [elementwise](std::vector<Value>& a) -> Value { return elementwise(a, "tensor_add", [](double x, double y) { return x + y; }); });
    def("tensor_sub", [elementwise](std::vector<Value>& a) -> Value { return elementwise(a, "tensor_sub", [](double x, double y) { return x - y; }); });
    def("tensor_mul", [elementwise](std::vector<Value>& a) -> Value { return elementwise(a, "tensor_mul", [](double x, double y) { return x * y; }); });
    def("tensor_div", [elementwise](std::vector<Value>& a) -> Value { return elementwise(a, "tensor_div", [](double x, double y) { return x / y; }); });

    // tensor_add_bias(mat, bias) — adds a 1D bias to every row of a 2D
    // matrix. The one deliberate, explicit exception to "no broadcasting" —
    // exactly what a dense/linear layer needs, and nothing more general.
    def("tensor_add_bias", [requireTensor](std::vector<Value>& a) -> Value {
        Value& mat = requireTensor(a, 0, "tensor_add_bias");
        Value& bias = requireTensor(a, 1, "tensor_add_bias");
        auto shape = tensorShapeVec(mat);
        if (shape.size() != 2) throw std::runtime_error("tensor_add_bias(): the first argument must be a 2D tensor.");
        long long rows = shape[0], cols = shape[1];
        auto& biasData = *bias.findDictEntry("data")->array;
        if ((long long)biasData.size() != cols)
            throw std::runtime_error("tensor_add_bias(): bias length must match the column count (" + std::to_string(cols) + ").");
        auto& d = *mat.findDictEntry("data")->array;
        std::vector<Value> out(d.size());
        for (long long r = 0; r < rows; r++)
            for (long long c = 0; c < cols; c++)
                out[r * cols + c] = Value::Number(d[r * cols + c].number + biasData[c].number);
        return makeTensor(shape, out);
    });

    // Element-wise unary ops.
    auto unaryOp = [requireTensor](std::vector<Value>& a, const char* fnName, double (*op)(double)) -> Value {
        Value& t = requireTensor(a, 0, fnName);
        auto shape = tensorShapeVec(t);
        auto& d = *t.findDictEntry("data")->array;
        std::vector<Value> out(d.size());
        for (size_t i = 0; i < d.size(); i++) out[i] = Value::Number(op(d[i].number));
        return makeTensor(shape, out);
    };
    def("tensor_relu", [unaryOp](std::vector<Value>& a) -> Value { return unaryOp(a, "tensor_relu", [](double x) { return x > 0 ? x : 0.0; }); });
    def("tensor_sigmoid", [unaryOp](std::vector<Value>& a) -> Value { return unaryOp(a, "tensor_sigmoid", [](double x) { return 1.0 / (1.0 + std::exp(-x)); }); });
    def("tensor_tanh", [unaryOp](std::vector<Value>& a) -> Value { return unaryOp(a, "tensor_tanh", [](double x) { return std::tanh(x); }); });
    def("tensor_exp", [unaryOp](std::vector<Value>& a) -> Value { return unaryOp(a, "tensor_exp", [](double x) { return std::exp(x); }); });
    def("tensor_log", [unaryOp](std::vector<Value>& a) -> Value { return unaryOp(a, "tensor_log", [](double x) { return std::log(x); }); });

    // Gradient helpers for backprop — each takes the *output* of the
    // matching forward op (not its input), which is the convenient form for
    // an autograd implementation built on top of this module (see the
    // "atome" module): relu needs the input's sign, so tensor_relu_grad
    // takes the input; sigmoid'/tanh' are cheapest expressed in terms of
    // their own output, so those two take the output.
    def("tensor_relu_grad", [unaryOp](std::vector<Value>& a) -> Value { return unaryOp(a, "tensor_relu_grad", [](double x) { return x > 0 ? 1.0 : 0.0; }); });
    def("tensor_sigmoid_grad", [unaryOp](std::vector<Value>& a) -> Value { return unaryOp(a, "tensor_sigmoid_grad", [](double s) { return s * (1.0 - s); }); });
    def("tensor_tanh_grad", [unaryOp](std::vector<Value>& a) -> Value { return unaryOp(a, "tensor_tanh_grad", [](double t) { return 1.0 - t * t; }); });

    // tensor_map(t, fn) — apply a ton.function/native function element-wise.
    // Slower than the natives above (it calls back into the interpreter once
    // per element) but works for any custom scalar function.
    def("tensor_map", [this, requireTensor](std::vector<Value>& a) -> Value {
        Value& t = requireTensor(a, 0, "tensor_map");
        if (a.size() < 2 || (a[1].type != ValueType::FUNCTION && a[1].type != ValueType::NATIVE_FUNCTION))
            throw std::runtime_error("tensor_map(t, fn) expects a function as the second argument.");
        auto shape = tensorShapeVec(t);
        auto& d = *t.findDictEntry("data")->array;
        std::vector<Value> out(d.size());
        for (size_t i = 0; i < d.size(); i++) {
            std::vector<Value> callArgs = {d[i]};
            out[i] = callFunction(a[1], callArgs, 0);
        }
        return makeTensor(shape, out);
    });

    // tensor_matmul(a, b) — 2D matrix multiply only (a: m x k, b: k x n -> m x n).
    def("tensor_matmul", [requireTensor](std::vector<Value>& a) -> Value {
        Value& x = requireTensor(a, 0, "tensor_matmul");
        Value& y = requireTensor(a, 1, "tensor_matmul");
        auto shapeX = tensorShapeVec(x), shapeY = tensorShapeVec(y);
        if (shapeX.size() != 2 || shapeY.size() != 2) throw std::runtime_error("tensor_matmul(): both tensors must be 2D.");
        if (shapeX[1] != shapeY[0])
            throw std::runtime_error("tensor_matmul(): inner dimensions must match (" + std::to_string(shapeX[1]) + " vs " + std::to_string(shapeY[0]) + ").");
        long long m = shapeX[0], k = shapeX[1], n = shapeY[1];
        auto& dx = *x.findDictEntry("data")->array;
        auto& dy = *y.findDictEntry("data")->array;
        std::vector<double> raw(m * n, 0.0);
        for (long long i = 0; i < m; i++) {
            for (long long p = 0; p < k; p++) {
                double xv = dx[i * k + p].number;
                if (xv == 0.0) continue;
                for (long long j = 0; j < n; j++) raw[i * n + j] += xv * dy[p * n + j].number;
            }
        }
        std::vector<Value> out(raw.size());
        for (size_t i = 0; i < raw.size(); i++) out[i] = Value::Number(raw[i]);
        return makeTensor({m, n}, out);
    });

    def("tensor_transpose", [requireTensor](std::vector<Value>& a) -> Value {
        Value& t = requireTensor(a, 0, "tensor_transpose");
        auto shape = tensorShapeVec(t);
        if (shape.size() != 2) throw std::runtime_error("tensor_transpose(): only 2D tensors are supported.");
        long long rows = shape[0], cols = shape[1];
        auto& d = *t.findDictEntry("data")->array;
        std::vector<Value> out(d.size());
        for (long long i = 0; i < rows; i++)
            for (long long j = 0; j < cols; j++)
                out[j * rows + i] = d[i * cols + j];
        return makeTensor({cols, rows}, out);
    });

    def("tensor_sum", [requireTensor](std::vector<Value>& a) -> Value {
        Value& t = requireTensor(a, 0, "tensor_sum");
        double s = 0;
        for (auto& v : *t.findDictEntry("data")->array) s += v.number;
        return Value::Number(s);
    });
    def("tensor_mean", [requireTensor](std::vector<Value>& a) -> Value {
        Value& t = requireTensor(a, 0, "tensor_mean");
        auto& d = *t.findDictEntry("data")->array;
        if (d.empty()) return Value::Number(0);
        double s = 0;
        for (auto& v : d) s += v.number;
        return Value::Number(s / (double)d.size());
    });
    def("tensor_max", [requireTensor](std::vector<Value>& a) -> Value {
        Value& t = requireTensor(a, 0, "tensor_max");
        auto& d = *t.findDictEntry("data")->array;
        if (d.empty()) throw std::runtime_error("tensor_max(): tensor is empty.");
        double m = d[0].number;
        for (auto& v : d) m = std::max(m, v.number);
        return Value::Number(m);
    });
    def("tensor_min", [requireTensor](std::vector<Value>& a) -> Value {
        Value& t = requireTensor(a, 0, "tensor_min");
        auto& d = *t.findDictEntry("data")->array;
        if (d.empty()) throw std::runtime_error("tensor_min(): tensor is empty.");
        double m = d[0].number;
        for (auto& v : d) m = std::min(m, v.number);
        return Value::Number(m);
    });

    // tensor_softmax(t) — over the whole vector for a 1D tensor, or row-wise for a 2D tensor.
    def("tensor_softmax", [requireTensor](std::vector<Value>& a) -> Value {
        Value& t = requireTensor(a, 0, "tensor_softmax");
        auto shape = tensorShapeVec(t);
        auto& d = *t.findDictEntry("data")->array;
        std::vector<Value> out(d.size());
        if (shape.size() == 1) {
            double mx = d.empty() ? 0.0 : d[0].number;
            for (auto& v : d) mx = std::max(mx, v.number);
            double sum = 0;
            std::vector<double> exps(d.size());
            for (size_t i = 0; i < d.size(); i++) { exps[i] = std::exp(d[i].number - mx); sum += exps[i]; }
            for (size_t i = 0; i < d.size(); i++) out[i] = Value::Number(exps[i] / sum);
        } else {
            long long rows = shape[0], cols = shape[1];
            for (long long r = 0; r < rows; r++) {
                double mx = d[r * cols].number;
                for (long long c = 0; c < cols; c++) mx = std::max(mx, d[r * cols + c].number);
                double sum = 0;
                std::vector<double> exps(cols);
                for (long long c = 0; c < cols; c++) { exps[c] = std::exp(d[r * cols + c].number - mx); sum += exps[c]; }
                for (long long c = 0; c < cols; c++) out[r * cols + c] = Value::Number(exps[c] / sum);
            }
        }
        return makeTensor(shape, out);
    });

    // tensor_from_array(arr) — arr is a flat array (-> 1D tensor) or an
    // array of same-length arrays (-> 2D tensor).
    def("tensor_from_array", [](std::vector<Value>& a) -> Value {
        if (a.empty() || a[0].type != ValueType::ARRAY) throw std::runtime_error("tensor_from_array() expects an array (1D) or array-of-arrays (2D).");
        auto& arr = *a[0].array;
        if (arr.empty()) throw std::runtime_error("tensor_from_array(): array is empty.");
        if (arr[0].type == ValueType::ARRAY) {
            long long rows = (long long)arr.size(), cols = (long long)arr[0].array->size();
            std::vector<Value> data;
            data.reserve(rows * cols);
            for (auto& row : arr) {
                if (row.type != ValueType::ARRAY || (long long)row.array->size() != cols)
                    throw std::runtime_error("tensor_from_array(): all rows must be arrays of the same length.");
                for (auto& v : *row.array) data.push_back(Value::Number(v.number));
            }
            return makeTensor({rows, cols}, data);
        }
        std::vector<Value> data;
        data.reserve(arr.size());
        for (auto& v : arr) data.push_back(Value::Number(v.number));
        return makeTensor({(long long)arr.size()}, data);
    });
    def("tensor_to_array", [requireTensor](std::vector<Value>& a) -> Value {
        Value& t = requireTensor(a, 0, "tensor_to_array");
        auto shape = tensorShapeVec(t);
        auto& d = *t.findDictEntry("data")->array;
        if (shape.size() == 1) return Value::Array(std::vector<Value>(d.begin(), d.end()));
        long long rows = shape[0], cols = shape[1];
        std::vector<Value> rowsOut;
        for (long long r = 0; r < rows; r++) rowsOut.push_back(Value::Array(std::vector<Value>(d.begin() + r * cols, d.begin() + (r + 1) * cols)));
        return Value::Array(rowsOut);
    });

    // tensor_argmax(t) -> the index of the largest element (1D), or one index
    // per row (2D) — the "which class did this predict" op classification
    // code always needs, done as a native loop instead of by hand each time.
    def("tensor_argmax", [requireTensor](std::vector<Value>& a) -> Value {
        Value& t = requireTensor(a, 0, "tensor_argmax");
        auto shape = tensorShapeVec(t);
        auto& d = *t.findDictEntry("data")->array;
        if (d.empty()) throw std::runtime_error("tensor_argmax(): tensor is empty.");
        if (shape.size() == 1) {
            size_t best = 0;
            for (size_t i = 1; i < d.size(); i++) if (d[i].number > d[best].number) best = i;
            return Value::Number((double)best);
        }
        long long rows = shape[0], cols = shape[1];
        std::vector<Value> out(rows);
        for (long long r = 0; r < rows; r++) {
            long long best = 0;
            for (long long c = 1; c < cols; c++) if (d[r * cols + c].number > d[r * cols + best].number) best = c;
            out[r] = Value::Number((double)best);
        }
        return Value::Array(out);
    });

    // tensor_conv1d(signal, kernel, [stride=1]) -> 1D "valid" convolution
    // (technically cross-correlation, same convention every ML framework
    // uses): out[i] = sum_j signal[i*stride + j] * kernel[j]. Single-channel
    // only — see the "atome" module for how multi-channel Conv1D layers are
    // composed out of this primitive (and where its gradient is computed).
    def("tensor_conv1d", [requireTensor](std::vector<Value>& a) -> Value {
        Value& sig = requireTensor(a, 0, "tensor_conv1d");
        Value& ker = requireTensor(a, 1, "tensor_conv1d");
        long long stride = a.size() > 2 ? (long long)a[2].number : 1;
        if (stride < 1) throw std::runtime_error("tensor_conv1d(): stride must be at least 1.");
        auto sigShape = tensorShapeVec(sig), kerShape = tensorShapeVec(ker);
        if (sigShape.size() != 1 || kerShape.size() != 1)
            throw std::runtime_error("tensor_conv1d(): both the signal and the kernel must be 1D tensors.");
        long long L = sigShape[0], K = kerShape[0];
        if (K > L) throw std::runtime_error("tensor_conv1d(): the kernel is longer than the signal.");
        long long outLen = (L - K) / stride + 1;
        auto& sd = *sig.findDictEntry("data")->array;
        auto& kd = *ker.findDictEntry("data")->array;
        std::vector<Value> out(outLen);
        for (long long i = 0; i < outLen; i++) {
            double s = 0;
            long long base = i * stride;
            for (long long j = 0; j < K; j++) s += sd[base + j].number * kd[j].number;
            out[i] = Value::Number(s);
        }
        return makeTensor({outLen}, out);
    });
}
