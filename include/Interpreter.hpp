#pragma once
#include <memory>
#include <vector>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include "Ast.hpp"
#include "Value.hpp"
#include "Environment.hpp"
#include "Debugger.hpp"

// ============================================================================
// Interpreter.hpp — walks the AST (see Ast.hpp) and actually runs the program:
// a tree-walking interpreter, the simplest and most flexible way to execute a
// language (no bytecode compilation step — every node is evaluated directly).
//
// The two core entry points are:
//   execute(stmt, env)   — runs a Stmt for its side effects (returns nothing)
//   evaluate(expr, env)  — evaluates an Expr and returns its Value
// Both take the Environment (see Environment.hpp) they should read/write
// variables in, which is how scoping (blocks, function calls, loops) works:
// each of those creates a new child Environment and passes it down.
//
// `break`/`continue`/`return` are signalled by execute()'s return value (see
// Signal below): a loop or function call checks it after every statement and
// stops early when it isn't Signal::NONE. (Before beta-2.0.0 they were C++
// exceptions, which made every function return cost microseconds.) Runtime
// errors, on the other hand, are still C++ exceptions — they're exceptional.
//
// Native functions (print, len, push, json, math/string/array helpers, the
// HTTP server hooks...) are registered in defineNatives() (src/Interpreter.cpp)
// as C++ lambdas stored as ordinary Values — see Value::NATIVE_FUNCTION. This
// is also where you'd add a brand-new built-in function to the language.
// ============================================================================

// What a statement asks its enclosing loop / function to do next.
enum class Signal { NONE, BREAK, CONTINUE, RETURN };

// A runtime error that already carries its "Error at line N: " prefix.
struct LineError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// "throw <value>": keeps the thrown value itself (a string, a dict, an
// instance...), so "catch (e)" receives exactly what was thrown.
struct ThrownValue : std::runtime_error {
    Value value;
    ThrownValue(const std::string& msg, Value v) : std::runtime_error(msg), value(std::move(v)) {}
};

// Structural equality shared by ==, switch/case, contains(), in, unique()...
bool valuesEqual(const Value& a, const Value& b);

struct TraceFrame { std::string function; int line; };

struct RegisteredRoute {
    std::string method; // "GET" or "POST"
    std::string path;
    Value handler;      // ton.function value
};

class Interpreter {
public:
    std::shared_ptr<Environment> globals;
    Debugger debugger;
    std::string scriptDir;
    std::set<std::string> importedModules;
    std::vector<RegisteredRoute> routes;
    // Extra command-line arguments passed after the script path, exposed to
    // scripts as sys_args() once "IMPORT://ton.sys" is used (see main.cpp).
    std::vector<std::string> scriptArgs;

    // Value of the last "return" (read by callFunction when execute() yields Signal::RETURN).
    Value returnValue;
    // Call stack captured when the first uncaught error was raised — printed
    // by main.cpp as a traceback. Cleared whenever a try/catch handles an error.
    std::vector<TraceFrame> errorTrace;
    int callDepth = 0;
    int maxCallDepth = 10000;

    Interpreter();
    void interpret(const std::vector<StmtPtr>& statements);
    // Runs one REPL entry: like interpret(), but returns the value of a
    // trailing expression statement (nil otherwise) so the REPL can echo it.
    Value interpretRepl(const std::vector<StmtPtr>& statements);

    Signal execute(const StmtPtr& stmt, const std::shared_ptr<Environment>& env);
    Value evaluate(const ExprPtr& expr, const std::shared_ptr<Environment>& env);
    // `self`, when given, binds "self" (and the method's class, for super) —
    // that's how a method call obj.method(args) runs.
    Value callFunction(const Value& callee, std::vector<Value>& args, int line, const Value* self = nullptr);

private:
    std::unordered_map<std::string, Value> moduleNamespaces; // "IMPORT://x as y" cache

    Signal executeBlock(const std::vector<StmtPtr>& stmts, const std::shared_ptr<Environment>& env);
    Value instantiate(const Value& klass, std::vector<Value>& args, int line);
    Value bindMethod(const std::shared_ptr<FunctionObj>& method, const Value& self);
    Value getProperty(const Value& object, const std::string& name, int line);
    Value callMethod(const ExprPtr& getExpr, std::vector<Value>& args, int line, const std::shared_ptr<Environment>& env);
    void evaluateArgs(const std::vector<ExprPtr>& argExprs, std::vector<Value>& out, const std::shared_ptr<Environment>& env);
    void defineVariable(const StmtPtr& stmt, const Value& val, const std::shared_ptr<Environment>& env);
    [[noreturn]] void undefinedName(const std::string& name, int line, const std::shared_ptr<Environment>& env);
    Value importNamespace(const std::string& moduleName, int line);
    std::string findModuleFile(const std::string& moduleName, std::string& source, int line);
    void defineNativesV2();
    void defineNatives();
    void runImport(const std::string& moduleName, int line);
    void checkType(VarKind kind, const Value& v, const std::string& varName, int line,
                   const std::string& typeName = "", const std::shared_ptr<Environment>& env = nullptr);

    // Built-in "system modules", each only defined once the matching
    // "IMPORT://ton.<name>" statement actually runs (see runImport in
    // src/Interpreter.cpp) — unlike the always-available natives from
    // defineNatives(), these stay invisible to a script that never imports them.
    void registerBuiltinSys();
    void registerBuiltinOs();
    void registerBuiltinRequests();
    void registerBuiltinRandom();
    void registerBuiltinTime();
    void registerBuiltinJson();
    void registerBuiltinMathUtils();
    void registerBuiltinStrings();
    void registerBuiltinEncoding();
    void registerBuiltinRegex();
    void registerBuiltinPath();
    void registerBuiltinTensor();

    [[noreturn]] void runtimeError(int line, const std::string& msg);
};
