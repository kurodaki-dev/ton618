#pragma once
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <cmath>

// ============================================================================
// Value.hpp — the runtime representation of every piece of data a TON618
// program can hold: numbers, strings, bools, arrays, dicts, html, and both
// user-defined (ton.function) and native (C++) functions.
//
// This is a single tagged-union-like struct (`type` says which fields are
// meaningful) rather than a class hierarchy, again for simplicity: the whole
// interpreter can pass Value by value cheaply (heavy payloads like arrays and
// dicts are shared_ptr-backed, so copying a Value never deep-copies them).
//
// To add a brand-new value kind (say, a "set" type): add it to ValueType, add
// whatever storage field it needs, extend typeName()/toString()/toJson()
// here, then teach the Interpreter and Parser about its syntax.
// ============================================================================

struct Value;
struct Environment;
struct Stmt;
using StmtPtr = std::shared_ptr<Stmt>;
struct Expr;
using ExprPtr = std::shared_ptr<Expr>;

enum class ValueType { NUMBER, STRING, BOOL, NIL, FUNCTION, NATIVE_FUNCTION, ARRAY, HTML, DICT };

struct FunctionObj {
    std::string name;
    std::vector<std::string> params;
    // Parallel to params (nullptr = no default value); if hasRestParam is
    // true, the LAST entry of params collects every extra argument into a
    // ton.array instead of binding a single value — see Interpreter::callFunction.
    std::vector<ExprPtr> paramDefaults;
    bool hasRestParam = false;
    StmtPtr body;
    std::shared_ptr<Environment> closure;
};

using NativeFn = std::function<Value(std::vector<Value>&)>;

struct Value {
    ValueType type = ValueType::NIL;
    double number = 0;
    std::string str;
    bool boolean = false;
    std::shared_ptr<FunctionObj> function;
    std::shared_ptr<NativeFn> nativeFn;
    std::shared_ptr<std::vector<Value>> array;
    // Insertion-ordered key/value pairs — used instead of a hash map so that
    // keys()/values()/toString()/toJson() iterate in the order the script wrote them.
    std::shared_ptr<std::vector<std::pair<std::string, Value>>> dict;

    Value() : type(ValueType::NIL) {}
    static Value Number(double n) { Value v; v.type = ValueType::NUMBER; v.number = n; return v; }
    static Value String(std::string s) { Value v; v.type = ValueType::STRING; v.str = std::move(s); return v; }
    static Value Html(std::string s) { Value v; v.type = ValueType::HTML; v.str = std::move(s); return v; }
    static Value Bool(bool b) { Value v; v.type = ValueType::BOOL; v.boolean = b; return v; }
    static Value Nil() { return Value(); }
    static Value Array(std::vector<Value> elems) {
        Value v; v.type = ValueType::ARRAY;
        v.array = std::make_shared<std::vector<Value>>(std::move(elems));
        return v;
    }
    static Value Dict(std::vector<std::pair<std::string, Value>> entries) {
        Value v; v.type = ValueType::DICT;
        v.dict = std::make_shared<std::vector<std::pair<std::string, Value>>>(std::move(entries));
        return v;
    }

    // Finds an entry by key, returns nullptr if this isn't a dict or the key is absent.
    Value* findDictEntry(const std::string& key) const {
        if (type != ValueType::DICT || !dict) return nullptr;
        for (auto& kv : *dict) if (kv.first == key) return &kv.second;
        return nullptr;
    }

    bool isTruthy() const {
        if (type == ValueType::NIL) return false;
        if (type == ValueType::BOOL) return boolean;
        if (type == ValueType::NUMBER) return number != 0;
        if (type == ValueType::STRING || type == ValueType::HTML) return !str.empty();
        return true;
    }

    std::string typeName() const {
        switch (type) {
            case ValueType::NUMBER: return "int/float";
            case ValueType::STRING: return "string";
            case ValueType::HTML: return "html";
            case ValueType::BOOL: return "bool";
            case ValueType::ARRAY: return "array";
            case ValueType::DICT: return "dict";
            case ValueType::NIL: return "nil";
            default: return "function";
        }
    }

    std::string toString() const {
        switch (type) {
            case ValueType::NIL: return "nil";
            case ValueType::BOOL: return boolean ? "true" : "false";
            case ValueType::NUMBER: {
                double intpart;
                if (std::modf(number, &intpart) == 0.0) return std::to_string((long long)number);
                return std::to_string(number);
            }
            case ValueType::STRING: return str;
            case ValueType::HTML: return str;
            case ValueType::FUNCTION: return "<ton.function " + (function ? function->name : "") + ">";
            case ValueType::NATIVE_FUNCTION: return "<native function>";
            case ValueType::ARRAY: {
                std::string out = "[";
                if (array) {
                    for (size_t i = 0; i < array->size(); i++) {
                        if (i > 0) out += ", ";
                        out += (*array)[i].toString();
                    }
                }
                out += "]";
                return out;
            }
            case ValueType::DICT: {
                std::string out = "{";
                if (dict) {
                    for (size_t i = 0; i < dict->size(); i++) {
                        if (i > 0) out += ", ";
                        out += (*dict)[i].first + ": " + (*dict)[i].second.toString();
                    }
                }
                out += "}";
                return out;
            }
        }
        return "";
    }

    // Represente la valeur en JSON (utilise par ton.json())
    std::string toJson() const {
        switch (type) {
            case ValueType::NIL: return "null";
            case ValueType::BOOL: return boolean ? "true" : "false";
            case ValueType::NUMBER: {
                double intpart;
                if (std::modf(number, &intpart) == 0.0) return std::to_string((long long)number);
                return std::to_string(number);
            }
            case ValueType::STRING:
            case ValueType::HTML: {
                std::string escaped = "\"";
                for (char c : str) {
                    if (c == '"' || c == '\\') escaped += '\\';
                    if (c == '\n') { escaped += "\\n"; continue; }
                    escaped += c;
                }
                escaped += "\"";
                return escaped;
            }
            case ValueType::ARRAY: {
                std::string out = "[";
                if (array) {
                    for (size_t i = 0; i < array->size(); i++) {
                        if (i > 0) out += ",";
                        out += (*array)[i].toJson();
                    }
                }
                out += "]";
                return out;
            }
            case ValueType::DICT: {
                std::string out = "{";
                if (dict) {
                    for (size_t i = 0; i < dict->size(); i++) {
                        if (i > 0) out += ",";
                        // Escape the key the same way string values are escaped.
                        std::string key = "\"";
                        for (char c : (*dict)[i].first) {
                            if (c == '"' || c == '\\') key += '\\';
                            key += c;
                        }
                        key += "\"";
                        out += key + ":" + (*dict)[i].second.toJson();
                    }
                }
                out += "}";
                return out;
            }
            default: return "null";
        }
    }

    // Same shape as toJson(), but indented across multiple lines — used by
    // ton.json's json_pretty() (src/Interpreter.cpp) for human-readable output.
    std::string toJsonPretty(int indent = 0) const {
        std::string pad(indent * 2, ' ');
        std::string padInner((indent + 1) * 2, ' ');
        switch (type) {
            case ValueType::ARRAY: {
                if (!array || array->empty()) return "[]";
                std::string out = "[\n";
                for (size_t i = 0; i < array->size(); i++) {
                    out += padInner + (*array)[i].toJsonPretty(indent + 1);
                    if (i + 1 < array->size()) out += ",";
                    out += "\n";
                }
                out += pad + "]";
                return out;
            }
            case ValueType::DICT: {
                if (!dict || dict->empty()) return "{}";
                std::string out = "{\n";
                for (size_t i = 0; i < dict->size(); i++) {
                    std::string key = "\"";
                    for (char c : (*dict)[i].first) {
                        if (c == '"' || c == '\\') key += '\\';
                        key += c;
                    }
                    key += "\"";
                    out += padInner + key + ": " + (*dict)[i].second.toJsonPretty(indent + 1);
                    if (i + 1 < dict->size()) out += ",";
                    out += "\n";
                }
                out += pad + "}";
                return out;
            }
            default: return toJson();
        }
    }
};
