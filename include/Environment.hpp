#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include "Value.hpp"

// ============================================================================
// Environment.hpp — a variable scope: a name -> Value map, plus a link to the
// enclosing (parent) scope.
//
// Every function call, loop, and block that declares something gets its own
// Environment chained to the scope it was created in. Looking up or assigning
// a name walks up the parent chain until it's found (or the chain runs out).
// A ton.function's closure is simply the Environment that was active when the
// function was declared — that's what lets it "remember" outer variables.
//
// Performance notes (beta-2.0.0):
//   - Names are interned into small integers ("symbols", see Symbols below)
//     once, so a lookup compares ints instead of hashing strings.
//   - A scope stores its variables in a small flat vector (most scopes hold a
//     handful of names, where a linear scan beats any hash map); only a scope
//     that grows large (the global scope, with its ~200 natives) builds a
//     hash index on top.
//   - Each slot can be marked constant (ton.const), which assign() refuses.
// ============================================================================

// Global string <-> int interning table for identifier names.
struct Symbols {
    static int intern(const std::string& name) {
        auto& t = table();
        auto it = t.ids.find(name);
        if (it != t.ids.end()) return it->second;
        int id = (int)t.names.size();
        t.names.push_back(name);
        t.ids.emplace(name, id);
        return id;
    }
    static const std::string& name(int id) { return table().names[id]; }

private:
    struct Table {
        std::unordered_map<std::string, int> ids;
        std::vector<std::string> names;
    };
    static Table& table() { static Table t; return t; }
};

enum class AssignResult { OK, UNDEFINED, CONSTANT };

struct Environment : std::enable_shared_from_this<Environment> {
    struct Slot {
        int id;
        Value value;
        bool isConst;
    };
    std::vector<Slot> slots;
    std::unordered_map<int, size_t> index; // only built once slots grows past kIndexThreshold
    std::shared_ptr<Environment> parent;

    static constexpr size_t kIndexThreshold = 24;

    explicit Environment(std::shared_ptr<Environment> parentEnv = nullptr)
        : parent(std::move(parentEnv)) {}

    Slot* findLocal(int id) {
        if (!index.empty()) {
            auto it = index.find(id);
            return it == index.end() ? nullptr : &slots[it->second];
        }
        for (auto& s : slots) if (s.id == id) return &s;
        return nullptr;
    }

    Slot* lookup(int id) {
        for (Environment* e = this; e; e = e->parent.get()) {
            if (Slot* s = e->findLocal(id)) return s;
        }
        return nullptr;
    }

    void define(int id, const Value& v, bool isConst = false) {
        if (Slot* s = findLocal(id)) { s->value = v; s->isConst = isConst; return; }
        slots.push_back({id, v, isConst});
        if (!index.empty()) index[id] = slots.size() - 1;
        else if (slots.size() > kIndexThreshold) {
            for (size_t i = 0; i < slots.size(); i++) index[slots[i].id] = i;
        }
    }

    AssignResult assign(int id, const Value& v) {
        Slot* s = lookup(id);
        if (!s) return AssignResult::UNDEFINED;
        if (s->isConst) return AssignResult::CONSTANT;
        s->value = v;
        return AssignResult::OK;
    }

    bool get(int id, Value& out) {
        if (Slot* s = lookup(id)) { out = s->value; return true; }
        return false;
    }

    // String-keyed convenience overloads (natives, the debugger, modules).
    void define(const std::string& name, const Value& v, bool isConst = false) { define(Symbols::intern(name), v, isConst); }
    bool assign(const std::string& name, const Value& v) { return assign(Symbols::intern(name), v) == AssignResult::OK; }
    bool get(const std::string& name, Value& out) { return get(Symbols::intern(name), out); }
    bool has(const std::string& name) { return lookup(Symbols::intern(name)) != nullptr; }
};
