#pragma once
#include <memory>
#include <vector>
#include <set>
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
// `break`/`continue`/`return` are implemented as C++ exceptions
// (BreakException/ContinueException/ReturnException below) that unwind the
// C++ call stack until a loop or function call catches them — this mirrors
// how those constructs "jump" out of nested blocks in the AST itself.
//
// Native functions (print, len, push, json, math/string/array helpers, the
// HTTP server hooks...) are registered in defineNatives() (src/Interpreter.cpp)
// as C++ lambdas stored as ordinary Values — see Value::NATIVE_FUNCTION. This
// is also where you'd add a brand-new built-in function to the language.
// ============================================================================

struct ReturnException { Value value; };
struct BreakException {};
struct ContinueException {};

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

    Interpreter();
    void interpret(const std::vector<StmtPtr>& statements);

    void execute(const StmtPtr& stmt, std::shared_ptr<Environment> env);
    Value evaluate(const ExprPtr& expr, std::shared_ptr<Environment> env);
    Value callFunction(const Value& callee, std::vector<Value>& args, int line);

private:
    void executeBlock(const std::vector<StmtPtr>& stmts, std::shared_ptr<Environment> env);
    void defineNatives();
    void runImport(const std::string& moduleName, int line);
    void checkType(VarKind kind, const Value& v, const std::string& varName, int line);

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

    [[noreturn]] void runtimeError(int line, const std::string& msg);
};
