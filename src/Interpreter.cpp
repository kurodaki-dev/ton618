#include "Interpreter.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "LocalServer.hpp"
#include "HttpClient.hpp"
#include "JsonParser.hpp"
#include "Platform.hpp"
#include "Version.hpp"
#include <iostream>
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
            for (auto& e : *args[0].array) {
                if (e.type == args[1].type &&
                    ((e.type == ValueType::NUMBER && e.number == args[1].number) ||
                     ((e.type == ValueType::STRING || e.type == ValueType::HTML) && e.str == args[1].str) ||
                     (e.type == ValueType::BOOL && e.boolean == args[1].boolean)))
                    return Value::Bool(true);
            }
            return Value::Bool(false);
        }
        return Value::Bool(args[0].toString().find(args[1].toString()) != std::string::npos);
    });
    // ton.indexOf(x, item) — first matching index, or -1. Works on strings and arrays.
    def("indexOf", [](std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value::Number(-1);
        if (args[0].type == ValueType::ARRAY) {
            auto& arr = *args[0].array;
            for (size_t i = 0; i < arr.size(); i++) {
                auto& e = arr[i];
                if (e.type == args[1].type &&
                    ((e.type == ValueType::NUMBER && e.number == args[1].number) ||
                     ((e.type == ValueType::STRING || e.type == ValueType::HTML) && e.str == args[1].str) ||
                     (e.type == ValueType::BOOL && e.boolean == args[1].boolean)))
                    return Value::Number((double)i);
            }
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
    def("get", [this](std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING || args[1].type != ValueType::FUNCTION) {
            throw std::runtime_error("get(path, handlerFunction) expects a string path and a ton.function handler.");
        }
        routes.push_back({"GET", args[0].str, args[1]});
        return Value::Nil();
    });

    // ton.post(path, handlerFunction) — registers a POST route.
    def("post", [this](std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING || args[1].type != ValueType::FUNCTION) {
            throw std::runtime_error("post(path, handlerFunction) expects a string path and a ton.function handler.");
        }
        routes.push_back({"POST", args[0].str, args[1]});
        return Value::Nil();
    });

    // ton.serve(port) — starts the HTTP server and dispatches to registered routes.
    // ton.serve(port, html) — legacy simple mode: serves the same content on every request.
    def("serve", [this](std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::NUMBER) {
            throw std::runtime_error("serve(port) or serve(port, content) expects a port number.");
        }
        int port = (int)args[0].number;

        // Legacy mode: a fixed second argument is served for every request.
        if (args.size() >= 2) {
            std::string content = args[1].toString();
            HttpHandler handler = [content](const std::string&, const std::string&) -> HttpResponse {
                return HttpResponse{content, "text/html; charset=utf-8", 200};
            };
            std::string err = startLocalServer(port, handler);
            if (!err.empty()) throw std::runtime_error(err);
            return Value::Nil();
        }

        // Routing mode: dispatch based on registered ton.get / ton.post routes.
        HttpHandler handler = [this](const std::string& method, const std::string& path) -> HttpResponse {
            for (auto& route : routes) {
                if (route.method == method && route.path == path) {
                    std::vector<Value> noArgs;
                    Value result = callFunction(route.handler, noArgs, 0);
                    std::string contentType = (result.type == ValueType::HTML)
                        ? "text/html; charset=utf-8" : "text/plain; charset=utf-8";
                    return HttpResponse{result.toString(), contentType, 200};
                }
            }
            return HttpResponse{"404 Not Found: " + path, "text/plain; charset=utf-8", 404};
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
        runtimeError(line, "Unknown built-in module 'ton." + builtin +
                     "'. Available: ton.sys, ton.os, ton.requests, ton.random, ton.time, ton.json, "
                     "ton.mathutils, ton.strings.");
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
            try {
                execute(stmt->thenBranch, env);
            } catch (BreakException&) { throw; }
            catch (ContinueException&) { throw; }
            catch (ReturnException&) { throw; }
            catch (std::exception& e) {
                auto catchEnv = std::make_shared<Environment>(env);
                catchEnv->define(stmt->name, Value::String(e.what()));
                execute(stmt->elseBranch, catchEnv);
            }
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
            return Value::Nil();
        }

        case ExprType::LOGICAL: {
            Value left = evaluate(expr->left, env);
            if (expr->op == TokenType::OR) { if (left.isTruthy()) return left; }
            else { if (!left.isTruthy()) return left; }
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
                    return Value::Number(std::fmod(left.number, right.number));
                case TokenType::GREATER: return Value::Bool(left.number > right.number);
                case TokenType::GREATER_EQUAL: return Value::Bool(left.number >= right.number);
                case TokenType::LESS: return Value::Bool(left.number < right.number);
                case TokenType::LESS_EQUAL: return Value::Bool(left.number <= right.number);
                case TokenType::EQUAL_EQUAL: {
                    if (left.type != right.type) return Value::Bool(false);
                    if (left.type == ValueType::NUMBER) return Value::Bool(left.number == right.number);
                    if (left.type == ValueType::STRING || left.type == ValueType::HTML) return Value::Bool(left.str == right.str);
                    if (left.type == ValueType::BOOL) return Value::Bool(left.boolean == right.boolean);
                    if (left.type == ValueType::NIL) return Value::Bool(true);
                    return Value::Bool(false);
                }
                case TokenType::BANG_EQUAL: {
                    if (left.type != right.type) return Value::Bool(true);
                    if (left.type == ValueType::NUMBER) return Value::Bool(left.number != right.number);
                    if (left.type == ValueType::STRING || left.type == ValueType::HTML) return Value::Bool(left.str != right.str);
                    if (left.type == ValueType::BOOL) return Value::Bool(left.boolean != right.boolean);
                    if (left.type == ValueType::NIL) return Value::Bool(false);
                    return Value::Bool(true);
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
            for (auto& e : expr->elements) elems.push_back(evaluate(e, env));
            return Value::Array(elems);
        }

        case ExprType::DICT: {
            std::vector<std::pair<std::string, Value>> entries;
            for (auto& [keyExpr, valExpr] : expr->dictEntries) {
                entries.push_back({keyExpr->litString, evaluate(valExpr, env)});
            }
            return Value::Dict(entries);
        }

        case ExprType::FUNCTION_EXPR: {
            auto fn = std::make_shared<FunctionObj>();
            fn->name = expr->name.empty() ? "<anonymous>" : expr->name;
            fn->params = expr->fnParams;
            fn->body = expr->fnBody;
            fn->closure = env;
            Value v; v.type = ValueType::FUNCTION; v.function = fn;
            return v;
        }

        case ExprType::TERNARY:
            return evaluate(expr->condition, env).isTruthy() ? evaluate(expr->left, env) : evaluate(expr->right, env);

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

            if (target.type != ValueType::ARRAY)
                runtimeError(expr->line, "Cannot index a value that is not an array or a dict (got " + target.typeName() + ").");

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
    if (args.size() != fn->params.size()) {
        runtimeError(line, "'ton." + fn->name + "' expects " + std::to_string(fn->params.size()) +
                     " argument(s) but got " + std::to_string(args.size()) + ".");
    }

    auto callEnv = std::make_shared<Environment>(fn->closure);
    for (size_t i = 0; i < fn->params.size(); i++) callEnv->define(fn->params[i], args[i]);

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
}
