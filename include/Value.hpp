#pragma once
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <cmath>
#include <cstdio>
#include <cstdlib>

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

enum class ValueType { NUMBER, STRING, BOOL, NIL, FUNCTION, NATIVE_FUNCTION, ARRAY, HTML, DICT, CLASS, INSTANCE };

struct ClassObj;

struct FunctionObj {
    std::string name;
    std::vector<std::string> params;
    std::vector<int> paramIds;      // interned params (see Symbols in Environment.hpp), filled lazily
    // Set for a method declared inside a ton.class body: the class that
    // declared it, which is where "super.method()" starts looking (its
    // superclass). Non-owning — the class is always kept alive by the
    // instance the method is called on.
    ClassObj* ownerClass = nullptr;
    bool isInitializer = false;     // the class's "init" method: returns self
    // Parallel to params (nullptr = no default value); if hasRestParam is
    // true, the LAST entry of params collects every extra argument into a
    // ton.array instead of binding a single value — see Interpreter::callFunction.
    std::vector<ExprPtr> paramDefaults;
    bool hasRestParam = false;
    StmtPtr body;
    std::shared_ptr<Environment> closure;
};

using NativeFn = std::function<Value(std::vector<Value>&)>;

// A user-defined type: "ton.class Name extends Parent { ... }".
struct ClassObj : std::enable_shared_from_this<ClassObj> {
    std::string name;
    std::shared_ptr<ClassObj> superclass;
    // Methods in declaration order (a class has few methods: a linear scan is fine).
    std::vector<std::pair<std::string, std::shared_ptr<FunctionObj>>> methods;
    // Field declarations ("ton.int x = 0" inside the class body), evaluated
    // for every new instance, before init() runs — parents' fields first.
    std::vector<StmtPtr> fieldInits;
    std::shared_ptr<Environment> closure;

    std::shared_ptr<FunctionObj> findMethod(const std::string& m) const {
        for (const ClassObj* c = this; c; c = c->superclass.get())
            for (auto& kv : c->methods) if (kv.first == m) return kv.second;
        return nullptr;
    }
    bool isSubclassOf(const ClassObj* other) const {
        for (const ClassObj* c = this; c; c = c->superclass.get()) if (c == other) return true;
        return false;
    }
};

struct InstanceObj;
// Defined at the end of this file, once InstanceObj is a complete type.
inline std::string instanceClassName(const Value& v);
inline std::string instanceDefaultString(const Value& v);
inline std::string instanceJson(const Value& v);

// Shortest text that reads back as exactly the same double: "0.1" rather
// than "0.100000", "3.14" rather than "3.140000", "1e+21" for huge values.
inline std::string formatNumber(double n) {
    if (std::isnan(n)) return "nan";
    if (std::isinf(n)) return n > 0 ? "inf" : "-inf";
    double intpart;
    if (std::modf(n, &intpart) == 0.0 && std::fabs(n) < 1e15) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%lld", (long long)n);
        return buf;
    }
    char buf[40];
    for (int prec = 1; prec <= 17; prec++) {
        std::snprintf(buf, sizeof buf, "%.*g", prec, n);
        if (std::strtod(buf, nullptr) == n) break;
    }
    return buf;
}

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
    std::shared_ptr<ClassObj> klass;        // CLASS
    std::shared_ptr<InstanceObj> instance;  // INSTANCE

    // Lets the interpreter route toString() of an instance through its
    // class's own "toString()" method, if it declares one (see Interpreter()).
    static std::function<std::string(const Value&)>& instanceToString() {
        static std::function<std::string(const Value&)> hook;
        return hook;
    }

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
            case ValueType::CLASS: return "class";
            case ValueType::INSTANCE: return instanceClassName(*this);
            default: return "function";
        }
    }

    std::string toString() const {
        switch (type) {
            case ValueType::NIL: return "nil";
            case ValueType::BOOL: return boolean ? "true" : "false";
            case ValueType::NUMBER: return formatNumber(number);
            case ValueType::STRING: return str;
            case ValueType::HTML: return str;
            case ValueType::CLASS: return "<ton.class " + (klass ? klass->name : "") + ">";
            case ValueType::INSTANCE: {
                if (instanceToString()) return instanceToString()(*this);
                return instanceDefaultString(*this);
            }
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

    // A JSON string literal, with every control character escaped.
    static std::string jsonQuote(const std::string& s) {
        std::string out = "\"";
        for (unsigned char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                default:
                    if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
                    else out += (char)c;
            }
        }
        return out + "\"";
    }

    // Like toString(), but strings are shown quoted — used for values nested
    // inside an instance's default text form, where "a" vs a matters.
    std::string repr() const {
        if (type == ValueType::STRING || type == ValueType::HTML) return jsonQuote(str);
        return toString();
    }

    // Represente la valeur en JSON (utilise par ton.json())
    std::string toJson() const {
        switch (type) {
            case ValueType::NIL: return "null";
            case ValueType::BOOL: return boolean ? "true" : "false";
            case ValueType::NUMBER:
                if (!std::isfinite(number)) return "null";
                return formatNumber(number);
            case ValueType::STRING:
            case ValueType::HTML: return jsonQuote(str);
            case ValueType::INSTANCE: return instanceJson(*this);
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
                        out += jsonQuote((*dict)[i].first) + ":" + (*dict)[i].second.toJson();
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
                    out += padInner + jsonQuote((*dict)[i].first) + ": " + (*dict)[i].second.toJsonPretty(indent + 1);
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

// An object created by calling a class: "ton.let p = Point(1, 2)".
struct InstanceObj {
    std::shared_ptr<ClassObj> klass;
    std::vector<std::pair<std::string, Value>> fields;   // see Value::dict: insertion-ordered

    Value* findField(const std::string& f) {
        for (auto& kv : fields) if (kv.first == f) return &kv.second;
        return nullptr;
    }
};

inline std::string instanceClassName(const Value& v) { return v.instance->klass->name; }
inline std::string instanceDefaultString(const Value& v) {
    std::string out = v.instance->klass->name + "{";
    for (size_t i = 0; i < v.instance->fields.size(); i++) {
        if (i > 0) out += ", ";
        out += v.instance->fields[i].first + ": " + v.instance->fields[i].second.repr();
    }
    return out + "}";
}
inline std::string instanceJson(const Value& v) {
    std::string out = "{";
    for (size_t i = 0; i < v.instance->fields.size(); i++) {
        if (i > 0) out += ",";
        out += Value::jsonQuote(v.instance->fields[i].first) + ":" + v.instance->fields[i].second.toJson();
    }
    return out + "}";
}
