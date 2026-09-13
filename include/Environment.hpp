#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include "Value.hpp"

// ============================================================================
// Environment.hpp — a variable scope: a name -> Value map, plus a link to the
// enclosing (parent) scope.
//
// Every block `{ ... }`, function call, and loop iteration gets its own
// Environment chained to the scope it was created in. Looking up or assigning
// a name walks up the parent chain until it's found (or the chain runs out).
// A ton.function's closure is simply the Environment that was active when the
// function was declared — that's what lets it "remember" outer variables.
// ============================================================================

struct Environment : std::enable_shared_from_this<Environment> {
    std::unordered_map<std::string, Value> values;
    std::shared_ptr<Environment> parent;

    explicit Environment(std::shared_ptr<Environment> parentEnv = nullptr)
        : parent(std::move(parentEnv)) {}

    void define(const std::string& name, const Value& v) { values[name] = v; }

    bool assign(const std::string& name, const Value& v) {
        auto it = values.find(name);
        if (it != values.end()) { it->second = v; return true; }
        if (parent) return parent->assign(name, v);
        return false;
    }

    bool get(const std::string& name, Value& out) {
        auto it = values.find(name);
        if (it != values.end()) { out = it->second; return true; }
        if (parent) return parent->get(name, out);
        return false;
    }

    bool has(const std::string& name) {
        if (values.count(name)) return true;
        if (parent) return parent->has(name);
        return false;
    }
};
