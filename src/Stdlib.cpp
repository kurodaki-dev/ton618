// ============================================================================
// Stdlib.cpp — the always-available native functions added in beta-2.0.0
// (collections, strings, conversions, classes). Like defineNatives() in
// Interpreter.cpp, every function here is a C++ lambda stored as an ordinary
// Value in the global scope. Some of them replace an older, simpler version
// defined in defineNatives() (sort gained a comparator, map/filter/... pass
// the index to two-parameter callbacks, min/max accept an array, ...): they
// are defined afterwards, so the newer definition wins.
//
// Thanks to "uniform call syntax" (see Interpreter::callMethod), every one of
// these can also be called as a method on its first argument:
//     sum([1, 2, 3])        ==  [1, 2, 3].sum()
//     padStart("7", 3, "0") ==  "7".padStart(3, "0")
// ============================================================================
#include "Interpreter.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <stdexcept>

namespace {

[[noreturn]] void fail(const std::string& msg) { throw std::runtime_error(msg); }

bool isText(const Value& v) { return v.type == ValueType::STRING || v.type == ValueType::HTML; }

const std::vector<Value>& needArray(const std::vector<Value>& args, size_t i, const char* fn) {
    if (i >= args.size() || args[i].type != ValueType::ARRAY)
        fail(std::string(fn) + "() expects an array as argument " + std::to_string(i + 1) +
             (i < args.size() ? " (got " + args[i].typeName() + ")." : "."));
    return *args[i].array;
}

const std::string& needText(const std::vector<Value>& args, size_t i, const char* fn) {
    if (i >= args.size() || !isText(args[i]))
        fail(std::string(fn) + "() expects a string as argument " + std::to_string(i + 1) +
             (i < args.size() ? " (got " + args[i].typeName() + ")." : "."));
    return args[i].str;
}

double needNumber(const std::vector<Value>& args, size_t i, const char* fn) {
    if (i >= args.size() || args[i].type != ValueType::NUMBER)
        fail(std::string(fn) + "() expects a number as argument " + std::to_string(i + 1) +
             (i < args.size() ? " (got " + args[i].typeName() + ")." : "."));
    return args[i].number;
}

bool isCallable(const Value& v) {
    return v.type == ValueType::FUNCTION || v.type == ValueType::NATIVE_FUNCTION || v.type == ValueType::CLASS;
}

// Ordering used by sort()/sorted()/min()/max() when no comparator is given:
// numbers numerically, everything else by its text.
bool naturalLess(const Value& a, const Value& b) {
    if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER) return a.number < b.number;
    if (isText(a) && isText(b)) return a.str < b.str;
    return a.toString() < b.toString();
}

Value deepCopy(const Value& v) {
    switch (v.type) {
        case ValueType::ARRAY: {
            std::vector<Value> out;
            for (auto& x : *v.array) out.push_back(deepCopy(x));
            return Value::Array(out);
        }
        case ValueType::DICT: {
            std::vector<std::pair<std::string, Value>> out;
            for (auto& kv : *v.dict) out.push_back({kv.first, deepCopy(kv.second)});
            return Value::Dict(out);
        }
        case ValueType::INSTANCE: {
            auto inst = std::make_shared<InstanceObj>();
            inst->klass = v.instance->klass;
            for (auto& kv : v.instance->fields) inst->fields.push_back({kv.first, deepCopy(kv.second)});
            Value r; r.type = ValueType::INSTANCE; r.instance = inst;
            return r;
        }
        default: return v;
    }
}

} // namespace

void Interpreter::defineNativesV2() {
    auto def = [this](const std::string& name, NativeFn fn) {
        Value v; v.type = ValueType::NATIVE_FUNCTION;
        v.nativeFn = std::make_shared<NativeFn>(std::move(fn));
        globals->define(name, v);
    };

    // Calls a user callback with (item) — or (item, index) when the callback
    // declares two or more parameters (or is native): map((x, i) => ...).
    auto callEach = [this](const Value& fn, const Value& item, size_t index) {
        bool wantsIndex = fn.type == ValueType::FUNCTION &&
                          (fn.function->params.size() >= 2 || fn.function->hasRestParam);
        std::vector<Value> a = {item};
        if (wantsIndex) a.push_back(Value::Number((double)index));
        return callFunction(fn, a, 0);
    };
    auto needFn = [](const std::vector<Value>& args, size_t i, const char* name) -> const Value& {
        if (i >= args.size() || !isCallable(args[i]))
            fail(std::string(name) + "() expects a function as argument " + std::to_string(i + 1) + ".");
        return args[i];
    };

    // ---- Output & conversions ---------------------------------------------

    // print(a, b, ...) as a value (so it can be passed around: arr.forEach(print)).
    // print(...) as a statement is handled by the parser and behaves the same.
    def("print", [](std::vector<Value>& args) -> Value {
        for (size_t i = 0; i < args.size(); i++) {
            if (i) std::cout << ' ';
            std::cout << args[i].toString();
        }
        std::cout << '\n';
        return Value::Nil();
    });
    def("int", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Number(0);
        if (args[0].type == ValueType::NUMBER) return Value::Number(std::trunc(args[0].number));
        if (args[0].type == ValueType::BOOL) return Value::Number(args[0].boolean ? 1 : 0);
        if (isText(args[0])) {
            try {
                size_t used = 0;
                std::string s = args[0].str;
                int base = 10;
                if (args.size() >= 2) base = (int)needNumber(args, 1, "int");
                long long n = std::stoll(s, &used, base);
                return Value::Number((double)n);
            } catch (...) {
                fail("int(): cannot convert \"" + args[0].str + "\" to an integer.");
            }
        }
        fail("int() cannot convert a " + args[0].typeName() + ".");
    });
    def("float", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Number(0);
        if (args[0].type == ValueType::NUMBER) return args[0];
        if (args[0].type == ValueType::BOOL) return Value::Number(args[0].boolean ? 1 : 0);
        if (isText(args[0])) {
            try { return Value::Number(std::stod(args[0].str)); }
            catch (...) { fail("float(): cannot convert \"" + args[0].str + "\" to a number."); }
        }
        fail("float() cannot convert a " + args[0].typeName() + ".");
    });
    def("bool", [](std::vector<Value>& args) -> Value {
        return Value::Bool(!args.empty() && args[0].isTruthy());
    });
    // fixed(3.14159, 2) -> "3.14"
    def("fixed", [](std::vector<Value>& args) -> Value {
        double x = needNumber(args, 0, "fixed");
        int digits = args.size() >= 2 ? (int)needNumber(args, 1, "fixed") : 2;
        if (digits < 0 || digits > 20) fail("fixed(): digits must be between 0 and 20.");
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.*f", digits, x);
        return Value::String(buf);
    });
    // repr(x): like str(), but strings come back quoted — handy when debugging.
    def("repr", [](std::vector<Value>& args) -> Value {
        return Value::String(args.empty() ? "nil" : args[0].repr());
    });
    def("isinstance", [](std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[1].type != ValueType::CLASS) fail("isinstance(value, Class) expects a class as its second argument.");
        return Value::Bool(args[0].type == ValueType::INSTANCE && args[0].instance->klass->isSubclassOf(args[1].klass.get()));
    });
    def("copy", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Nil();
        const Value& v = args[0];
        if (v.type == ValueType::ARRAY) return Value::Array(*v.array);
        if (v.type == ValueType::DICT) return Value::Dict(*v.dict);
        if (v.type == ValueType::INSTANCE) {
            auto inst = std::make_shared<InstanceObj>(*v.instance);
            Value r; r.type = ValueType::INSTANCE; r.instance = inst;
            return r;
        }
        return v;
    });
    def("deepcopy", [](std::vector<Value>& args) -> Value { return args.empty() ? Value::Nil() : deepCopy(args[0]); });
    def("exit", [](std::vector<Value>& args) -> Value {
        std::cout.flush();
        std::exit(args.empty() ? 0 : (int)needNumber(args, 0, "exit"));
    });

    // ---- Numbers ------------------------------------------------------------

    // min/max accept either several numbers, or one array.
    auto extreme = [](std::vector<Value>& args, bool wantMax, const char* name) -> Value {
        const std::vector<Value>* items = &args;
        if (args.size() == 1 && args[0].type == ValueType::ARRAY) items = args[0].array.get();
        if (items->empty()) fail(std::string(name) + "() of nothing: give at least one value.");
        Value best = (*items)[0];
        for (auto& v : *items) if (wantMax ? naturalLess(best, v) : naturalLess(v, best)) best = v;
        return best;
    };
    def("min", [extreme](std::vector<Value>& args) -> Value { return extreme(args, false, "min"); });
    def("max", [extreme](std::vector<Value>& args) -> Value { return extreme(args, true, "max"); });
    def("clamp", [](std::vector<Value>& args) -> Value {
        double x = needNumber(args, 0, "clamp"), lo = needNumber(args, 1, "clamp"), hi = needNumber(args, 2, "clamp");
        return Value::Number(std::max(lo, std::min(hi, x)));
    });
    def("sign", [](std::vector<Value>& args) -> Value {
        double x = needNumber(args, 0, "sign");
        return Value::Number(x > 0 ? 1 : x < 0 ? -1 : 0);
    });
    // round(x) or round(x, digits)
    def("round", [](std::vector<Value>& args) -> Value {
        double x = needNumber(args, 0, "round");
        if (args.size() < 2) return Value::Number(std::round(x));
        double f = std::pow(10.0, needNumber(args, 1, "round"));
        return Value::Number(std::round(x * f) / f);
    });
    // range(end), range(start, end), range(start, end, step) -> array (end excluded)
    def("range", [](std::vector<Value>& args) -> Value {
        double start = 0, end, step = 1;
        if (args.size() == 1) end = needNumber(args, 0, "range");
        else { start = needNumber(args, 0, "range"); end = needNumber(args, 1, "range"); }
        if (args.size() >= 3) step = needNumber(args, 2, "range");
        if (step == 0) fail("range(): step can't be 0.");
        if (std::fabs((end - start) / step) > 1e8) fail("range(): too many elements.");
        std::vector<Value> out;
        if (step > 0) for (double i = start; i < end; i += step) out.push_back(Value::Number(i));
        else for (double i = start; i > end; i += step) out.push_back(Value::Number(i));
        return Value::Array(out);
    });
    def("sum", [](std::vector<Value>& args) -> Value {
        double total = 0;
        for (auto& v : needArray(args, 0, "sum")) {
            if (v.type != ValueType::NUMBER) fail("sum() expects an array of numbers (found a " + v.typeName() + ").");
            total += v.number;
        }
        return Value::Number(total);
    });
    def("avg", [](std::vector<Value>& args) -> Value {
        auto& arr = needArray(args, 0, "avg");
        if (arr.empty()) fail("avg() of an empty array.");
        double total = 0;
        for (auto& v : arr) {
            if (v.type != ValueType::NUMBER) fail("avg() expects an array of numbers (found a " + v.typeName() + ").");
            total += v.number;
        }
        return Value::Number(total / arr.size());
    });

    // ---- Arrays -------------------------------------------------------------

    def("len", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Number(0);
        const Value& v = args[0];
        if (isText(v)) return Value::Number((double)v.str.size());
        if (v.type == ValueType::ARRAY) return Value::Number((double)v.array->size());
        if (v.type == ValueType::DICT) return Value::Number((double)v.dict->size());
        if (v.type == ValueType::INSTANCE) return Value::Number((double)v.instance->fields.size());
        if (v.type == ValueType::NIL) return Value::Number(0);
        fail("len() of a " + v.typeName() + " makes no sense.");
    });
    def("map", [callEach, needFn](std::vector<Value>& args) -> Value {
        auto arr = needArray(args, 0, "map"); // copy: the callback may modify the array
        const Value& fn = needFn(args, 1, "map");
        std::vector<Value> out;
        out.reserve(arr.size());
        for (size_t i = 0; i < arr.size(); i++) out.push_back(callEach(fn, arr[i], i));
        return Value::Array(std::move(out));
    });
    def("filter", [callEach, needFn](std::vector<Value>& args) -> Value {
        auto arr = needArray(args, 0, "filter");
        const Value& fn = needFn(args, 1, "filter");
        std::vector<Value> out;
        for (size_t i = 0; i < arr.size(); i++) if (callEach(fn, arr[i], i).isTruthy()) out.push_back(arr[i]);
        return Value::Array(std::move(out));
    });
    def("forEach", [callEach, needFn](std::vector<Value>& args) -> Value {
        auto arr = needArray(args, 0, "forEach");
        const Value& fn = needFn(args, 1, "forEach");
        for (size_t i = 0; i < arr.size(); i++) callEach(fn, arr[i], i);
        return Value::Nil();
    });
    def("find", [callEach, needFn](std::vector<Value>& args) -> Value {
        auto arr = needArray(args, 0, "find");
        const Value& fn = needFn(args, 1, "find");
        for (size_t i = 0; i < arr.size(); i++) if (callEach(fn, arr[i], i).isTruthy()) return arr[i];
        return Value::Nil();
    });
    def("findIndex", [callEach, needFn](std::vector<Value>& args) -> Value {
        auto arr = needArray(args, 0, "findIndex");
        const Value& fn = needFn(args, 1, "findIndex");
        for (size_t i = 0; i < arr.size(); i++) if (callEach(fn, arr[i], i).isTruthy()) return Value::Number((double)i);
        return Value::Number(-1);
    });
    def("any", [callEach, needFn](std::vector<Value>& args) -> Value {
        auto arr = needArray(args, 0, "any");
        if (args.size() < 2) { for (auto& v : arr) if (v.isTruthy()) return Value::Bool(true); return Value::Bool(false); }
        const Value& fn = needFn(args, 1, "any");
        for (size_t i = 0; i < arr.size(); i++) if (callEach(fn, arr[i], i).isTruthy()) return Value::Bool(true);
        return Value::Bool(false);
    });
    def("all", [callEach, needFn](std::vector<Value>& args) -> Value {
        auto arr = needArray(args, 0, "all");
        if (args.size() < 2) { for (auto& v : arr) if (!v.isTruthy()) return Value::Bool(false); return Value::Bool(true); }
        const Value& fn = needFn(args, 1, "all");
        for (size_t i = 0; i < arr.size(); i++) if (!callEach(fn, arr[i], i).isTruthy()) return Value::Bool(false);
        return Value::Bool(true);
    });
    // reduce(arr, fn, [initial]) — without an initial value, starts from the first element.
    def("reduce", [this, needFn](std::vector<Value>& args) -> Value {
        auto arr = needArray(args, 0, "reduce");
        const Value& fn = needFn(args, 1, "reduce");
        size_t i = 0;
        Value acc;
        if (args.size() >= 3) acc = args[2];
        else {
            if (arr.empty()) fail("reduce() of an empty array with no initial value.");
            acc = arr[0];
            i = 1;
        }
        for (; i < arr.size(); i++) {
            std::vector<Value> a = {acc, arr[i]};
            acc = callFunction(fn, a, 0);
        }
        return acc;
    });
    def("flatMap", [callEach, needFn](std::vector<Value>& args) -> Value {
        auto arr = needArray(args, 0, "flatMap");
        const Value& fn = needFn(args, 1, "flatMap");
        std::vector<Value> out;
        for (size_t i = 0; i < arr.size(); i++) {
            Value r = callEach(fn, arr[i], i);
            if (r.type == ValueType::ARRAY) out.insert(out.end(), r.array->begin(), r.array->end());
            else out.push_back(r);
        }
        return Value::Array(std::move(out));
    });
    // count(arr, value) or count(arr, predicate); count(string, substring)
    def("count", [callEach](std::vector<Value>& args) -> Value {
        if (args.size() < 2) fail("count(collection, valueOrFunction) expects 2 arguments.");
        if (isText(args[0])) {
            const std::string& s = args[0].str;
            std::string sub = args[1].toString();
            if (sub.empty()) fail("count(): the substring can't be empty.");
            size_t n = 0;
            for (size_t p = s.find(sub); p != std::string::npos; p = s.find(sub, p + sub.size())) n++;
            return Value::Number((double)n);
        }
        auto arr = needArray(args, 0, "count");
        size_t n = 0;
        bool pred = args[1].type == ValueType::FUNCTION || args[1].type == ValueType::NATIVE_FUNCTION;
        for (size_t i = 0; i < arr.size(); i++)
            if (pred ? callEach(args[1], arr[i], i).isTruthy() : valuesEqual(arr[i], args[1])) n++;
        return Value::Number((double)n);
    });
    // sort(arr, [compare]) — in place. compare(a, b) returns a negative number
    // (or true) when a comes first: sort(people, (a, b) => a.age - b.age).
    def("sort", [this](std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::ARRAY) fail("sort() expects an array.");
        auto& arr = *args[0].array;
        if (args.size() >= 2) {
            Value cmp = args[1];
            std::vector<Value> tmp(arr);
            std::stable_sort(tmp.begin(), tmp.end(), [&](const Value& a, const Value& b) {
                std::vector<Value> ab = {a, b};
                Value r = callFunction(cmp, ab, 0);
                return r.type == ValueType::NUMBER ? r.number < 0 : r.isTruthy();
            });
            arr = tmp;
        } else {
            std::stable_sort(arr.begin(), arr.end(), naturalLess);
        }
        return args[0];
    });
    // sorted(arr, [key]) — a sorted copy; key(x) gives what to sort by:
    // sorted(words, w => len(w)).
    def("sorted", [this](std::vector<Value>& args) -> Value {
        auto arr = needArray(args, 0, "sorted");
        if (args.size() >= 2) {
            std::vector<std::pair<Value, Value>> keyed;
            for (auto& v : arr) { std::vector<Value> a = {v}; keyed.push_back({callFunction(args[1], a, 0), v}); }
            std::stable_sort(keyed.begin(), keyed.end(), [](auto& x, auto& y) { return naturalLess(x.first, y.first); });
            std::vector<Value> out;
            for (auto& kv : keyed) out.push_back(kv.second);
            return Value::Array(out);
        }
        std::stable_sort(arr.begin(), arr.end(), naturalLess);
        return Value::Array(arr);
    });
    def("reverse", [](std::vector<Value>& args) -> Value {
        if (args.empty()) fail("reverse() expects an array or a string.");
        if (isText(args[0])) {
            std::string s(args[0].str.rbegin(), args[0].str.rend());
            return Value::String(s);
        }
        if (args[0].type != ValueType::ARRAY) fail("reverse() expects an array or a string.");
        std::reverse(args[0].array->begin(), args[0].array->end());
        return args[0];
    });
    def("unique", [](std::vector<Value>& args) -> Value {
        std::vector<Value> out;
        for (auto& v : needArray(args, 0, "unique")) {
            bool seen = false;
            for (auto& o : out) if (valuesEqual(o, v)) { seen = true; break; }
            if (!seen) out.push_back(v);
        }
        return Value::Array(out);
    });
    // flatten(arr, [depth=1])
    def("flatten", [](std::vector<Value>& args) -> Value {
        int depth = args.size() >= 2 ? (int)needNumber(args, 1, "flatten") : 1;
        std::function<void(const std::vector<Value>&, int, std::vector<Value>&)> go =
            [&](const std::vector<Value>& in, int d, std::vector<Value>& out) {
                for (auto& v : in) {
                    if (v.type == ValueType::ARRAY && d > 0) go(*v.array, d - 1, out);
                    else out.push_back(v);
                }
            };
        std::vector<Value> out;
        go(needArray(args, 0, "flatten"), depth, out);
        return Value::Array(out);
    });
    def("zip", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Array({});
        size_t n = SIZE_MAX;
        for (size_t i = 0; i < args.size(); i++) n = std::min(n, needArray(args, i, "zip").size());
        std::vector<Value> out;
        for (size_t r = 0; r < n; r++) {
            std::vector<Value> row;
            for (auto& a : args) row.push_back((*a.array)[r]);
            out.push_back(Value::Array(row));
        }
        return Value::Array(out);
    });
    def("enumerate", [](std::vector<Value>& args) -> Value {
        std::vector<Value> out;
        auto& arr = needArray(args, 0, "enumerate");
        for (size_t i = 0; i < arr.size(); i++) out.push_back(Value::Array({Value::Number((double)i), arr[i]}));
        return Value::Array(out);
    });
    def("chunk", [](std::vector<Value>& args) -> Value {
        auto& arr = needArray(args, 0, "chunk");
        double size = needNumber(args, 1, "chunk");
        if (size < 1) fail("chunk(): the size must be at least 1.");
        std::vector<Value> out;
        for (size_t i = 0; i < arr.size(); i += (size_t)size)
            out.push_back(Value::Array(std::vector<Value>(arr.begin() + i, arr.begin() + std::min(arr.size(), i + (size_t)size))));
        return Value::Array(out);
    });
    def("groupBy", [this](std::vector<Value>& args) -> Value {
        auto arr = needArray(args, 0, "groupBy");
        std::vector<std::pair<std::string, Value>> groups;
        for (auto& v : arr) {
            std::vector<Value> a = {v};
            std::string key = callFunction(args.at(1), a, 0).toString();
            Value* g = nullptr;
            for (auto& kv : groups) if (kv.first == key) { g = &kv.second; break; }
            if (!g) { groups.push_back({key, Value::Array({})}); g = &groups.back().second; }
            g->array->push_back(v);
        }
        return Value::Dict(groups);
    });
    def("first", [](std::vector<Value>& args) -> Value {
        auto& arr = needArray(args, 0, "first");
        return arr.empty() ? Value::Nil() : arr.front();
    });
    def("last", [](std::vector<Value>& args) -> Value {
        auto& arr = needArray(args, 0, "last");
        return arr.empty() ? Value::Nil() : arr.back();
    });
    def("insert", [](std::vector<Value>& args) -> Value {
        if (args.size() < 3 || args[0].type != ValueType::ARRAY) fail("insert(array, index, value) expects 3 arguments.");
        auto& arr = *args[0].array;
        long long i = (long long)needNumber(args, 1, "insert");
        if (i < 0) i += (long long)arr.size() + 1;
        if (i < 0 || i > (long long)arr.size()) fail("insert(): index out of bounds.");
        arr.insert(arr.begin() + i, args[2]);
        return args[0];
    });
    def("removeAt", [](std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::ARRAY) fail("removeAt(array, index) expects an array.");
        auto& arr = *args[0].array;
        long long i = (long long)needNumber(args, 1, "removeAt");
        if (i < 0) i += (long long)arr.size();
        if (i < 0 || i >= (long long)arr.size()) fail("removeAt(): index out of bounds.");
        Value removed = arr[i];
        arr.erase(arr.begin() + i);
        return removed;
    });
    // remove(array, value) removes the first equal element; remove(dict, key)
    // removes a key. Returns whether something was removed.
    def("remove", [](std::vector<Value>& args) -> Value {
        if (args.size() < 2) fail("remove(collection, item) expects 2 arguments.");
        if (args[0].type == ValueType::ARRAY) {
            auto& arr = *args[0].array;
            for (size_t i = 0; i < arr.size(); i++)
                if (valuesEqual(arr[i], args[1])) { arr.erase(arr.begin() + i); return Value::Bool(true); }
            return Value::Bool(false);
        }
        if (args[0].type == ValueType::DICT) {
            auto& d = *args[0].dict;
            std::string key = args[1].toString();
            for (size_t i = 0; i < d.size(); i++)
                if (d[i].first == key) { d.erase(d.begin() + i); return Value::Bool(true); }
            return Value::Bool(false);
        }
        fail("remove() expects an array or a dict.");
    });
    def("clear", [](std::vector<Value>& args) -> Value {
        if (args.empty()) fail("clear() expects an array or a dict.");
        if (args[0].type == ValueType::ARRAY) args[0].array->clear();
        else if (args[0].type == ValueType::DICT) args[0].dict->clear();
        else fail("clear() expects an array or a dict.");
        return args[0];
    });
    def("isEmpty", [](std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::Bool(true);
        const Value& v = args[0];
        if (isText(v)) return Value::Bool(v.str.empty());
        if (v.type == ValueType::ARRAY) return Value::Bool(v.array->empty());
        if (v.type == ValueType::DICT) return Value::Bool(v.dict->empty());
        return Value::Bool(v.type == ValueType::NIL);
    });

    // ---- Dicts --------------------------------------------------------------

    auto fieldsOf = [](const Value& v, const char* fn) -> const std::vector<std::pair<std::string, Value>>& {
        if (v.type == ValueType::DICT) return *v.dict;
        if (v.type == ValueType::INSTANCE) return v.instance->fields;
        fail(std::string(fn) + "() expects a dict or an object (got " + v.typeName() + ").");
    };
    def("keys", [fieldsOf](std::vector<Value>& args) -> Value {
        if (args.empty()) fail("keys() expects a dict.");
        std::vector<Value> out;
        for (auto& kv : fieldsOf(args[0], "keys")) out.push_back(Value::String(kv.first));
        return Value::Array(out);
    });
    def("values", [fieldsOf](std::vector<Value>& args) -> Value {
        if (args.empty()) fail("values() expects a dict.");
        std::vector<Value> out;
        for (auto& kv : fieldsOf(args[0], "values")) out.push_back(kv.second);
        return Value::Array(out);
    });
    def("items", [fieldsOf](std::vector<Value>& args) -> Value {
        if (args.empty()) fail("items() expects a dict.");
        std::vector<Value> out;
        for (auto& kv : fieldsOf(args[0], "items")) out.push_back(Value::Array({Value::String(kv.first), kv.second}));
        return Value::Array(out);
    });
    def("has", [fieldsOf](std::vector<Value>& args) -> Value {
        if (args.size() < 2) fail("has(dict, key) expects 2 arguments.");
        std::string key = args[1].toString();
        for (auto& kv : fieldsOf(args[0], "has")) if (kv.first == key) return Value::Bool(true);
        return Value::Bool(false);
    });
    // dict([[key, value], ...]) — the inverse of items().
    def("dict", [](std::vector<Value>& args) -> Value {
        std::vector<std::pair<std::string, Value>> out;
        if (args.empty()) return Value::Dict(out);
        for (auto& pair : needArray(args, 0, "dict")) {
            if (pair.type != ValueType::ARRAY || pair.array->size() != 2) fail("dict() expects an array of [key, value] pairs.");
            std::string key = (*pair.array)[0].toString();
            bool replaced = false;
            for (auto& kv : out) if (kv.first == key) { kv.second = (*pair.array)[1]; replaced = true; break; }
            if (!replaced) out.push_back({key, (*pair.array)[1]});
        }
        return Value::Dict(out);
    });
    // merge(d1, d2, ...) — a new dict; later keys win.
    def("merge", [fieldsOf](std::vector<Value>& args) -> Value {
        std::vector<std::pair<std::string, Value>> out;
        for (auto& d : args) {
            for (auto& kv : fieldsOf(d, "merge")) {
                bool replaced = false;
                for (auto& o : out) if (o.first == kv.first) { o.second = kv.second; replaced = true; break; }
                if (!replaced) out.push_back(kv);
            }
        }
        return Value::Dict(out);
    });

    // ---- Strings ------------------------------------------------------------

    def("startsWith", [](std::vector<Value>& args) -> Value {
        const std::string& s = needText(args, 0, "startsWith");
        std::string p = args.size() >= 2 ? args[1].toString() : "";
        return Value::Bool(s.compare(0, p.size(), p) == 0 && s.size() >= p.size());
    });
    def("endsWith", [](std::vector<Value>& args) -> Value {
        const std::string& s = needText(args, 0, "endsWith");
        std::string p = args.size() >= 2 ? args[1].toString() : "";
        return Value::Bool(s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0);
    });
    def("repeat", [](std::vector<Value>& args) -> Value {
        double n = needNumber(args, 1, "repeat");
        if (n < 0) fail("repeat(): the count can't be negative.");
        if (!args.empty() && args[0].type == ValueType::ARRAY) {
            std::vector<Value> out;
            for (int i = 0; i < (int)n; i++) out.insert(out.end(), args[0].array->begin(), args[0].array->end());
            return Value::Array(out);
        }
        const std::string& s = needText(args, 0, "repeat");
        std::string out;
        for (int i = 0; i < (int)n; i++) out += s;
        return Value::String(out);
    });
    def("chars", [](std::vector<Value>& args) -> Value {
        std::vector<Value> out;
        for (char c : needText(args, 0, "chars")) out.push_back(Value::String(std::string(1, c)));
        return Value::Array(out);
    });
    def("lines", [](std::vector<Value>& args) -> Value {
        const std::string& s = needText(args, 0, "lines");
        std::vector<Value> out;
        size_t start = 0;
        while (start <= s.size()) {
            size_t nl = s.find('\n', start);
            if (nl == std::string::npos) { if (start < s.size()) out.push_back(Value::String(s.substr(start))); break; }
            std::string line = s.substr(start, nl - start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            out.push_back(Value::String(line));
            start = nl + 1;
        }
        return Value::Array(out);
    });
    auto pad = [](std::vector<Value>& args, bool atStart, const char* name) -> Value {
        std::string s = args.empty() ? "" : args[0].toString();
        size_t width = (size_t)std::max(0.0, needNumber(args, 1, name));
        std::string fill = args.size() >= 3 ? args[2].toString() : " ";
        if (fill.empty()) fail(std::string(name) + "(): the fill string can't be empty.");
        std::string padding;
        while (s.size() + padding.size() < width) padding += fill;
        padding = padding.substr(0, width > s.size() ? width - s.size() : 0);
        return Value::String(atStart ? padding + s : s + padding);
    };
    def("padStart", [pad](std::vector<Value>& args) -> Value { return pad(args, true, "padStart"); });
    def("padEnd", [pad](std::vector<Value>& args) -> Value { return pad(args, false, "padEnd"); });
    def("capitalize", [](std::vector<Value>& args) -> Value {
        std::string s = needText(args, 0, "capitalize");
        if (!s.empty()) s[0] = (char)std::toupper((unsigned char)s[0]);
        return Value::String(s);
    });
    def("lastIndexOf", [](std::vector<Value>& args) -> Value {
        if (args.size() < 2) fail("lastIndexOf(collection, item) expects 2 arguments.");
        if (isText(args[0])) {
            size_t p = args[0].str.rfind(args[1].toString());
            return Value::Number(p == std::string::npos ? -1 : (double)p);
        }
        auto& arr = needArray(args, 0, "lastIndexOf");
        for (size_t i = arr.size(); i-- > 0;) if (valuesEqual(arr[i], args[1])) return Value::Number((double)i);
        return Value::Number(-1);
    });
    def("ord", [](std::vector<Value>& args) -> Value {
        const std::string& s = needText(args, 0, "ord");
        if (s.empty()) fail("ord() of an empty string.");
        return Value::Number((double)(unsigned char)s[0]);
    });
    def("chr", [](std::vector<Value>& args) -> Value {
        double n = needNumber(args, 0, "chr");
        if (n < 0 || n > 255) fail("chr() expects a number between 0 and 255.");
        return Value::String(std::string(1, (char)(int)n));
    });
}
