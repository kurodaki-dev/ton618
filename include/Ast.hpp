#pragma once
#include <memory>
#include <vector>
#include <string>
#include "Token.hpp"

// ============================================================================
// Ast.hpp — the Abstract Syntax Tree: what a TON618 program looks like once
// parsed, before it is executed.
//
// There are two kinds of nodes:
//   - Expr  — anything that produces a value (a literal, a variable read, a
//             binary operation, a function call, an array/dict literal...).
//   - Stmt  — anything that performs an action (a variable declaration, an
//             if/while/for, a function declaration, a return...).
//
// Both are "fat structs": instead of one C++ class per node kind (the classic
// OOP visitor-pattern approach), every possible field lives directly on Expr
// or Stmt, and `type` says which ones are actually meaningful for a given node.
// This keeps the interpreter's switch-based execute()/evaluate() simple to
// read and extend, at the cost of a few unused fields per node — a deliberate
// trade-off for a small hobby language. Some node kinds are described as
// "reusing" another kind's fields (see the comments below) instead of adding
// brand-new fields — that's what keeps this struct from growing without bound
// every time a new statement/expression kind is added.
//
// Pipeline: Lexer (text -> Token) -> Parser (Token -> Expr/Stmt here)
//        -> Interpreter (Expr/Stmt -> a running program, see Interpreter.hpp).
// ============================================================================

struct Expr;
using ExprPtr = std::shared_ptr<Expr>;
struct Stmt;
using StmtPtr = std::shared_ptr<Stmt>;

enum class ExprType {
    LITERAL, VARIABLE, ASSIGN, BINARY, UNARY, LOGICAL, CALL, GROUPING, ARRAY, INDEX,
    TERNARY, DICT, FUNCTION_EXPR
};

struct Expr {
    ExprType type;
    int line = 0;

    // LITERAL
    std::string litString;
    double litNumber = 0;
    bool litBool = false;
    bool isNil = false, isString = false, isNumber = false, isBoolLit = false;

    // VARIABLE / ASSIGN — the stored name never includes the "ton." prefix
    // (the prefix is just a syntax requirement, not a separate scope)
    std::string name;
    ExprPtr value;

    // BINARY / LOGICAL
    ExprPtr left, right;
    TokenType op;

    // UNARY
    ExprPtr operand;

    // CALL
    ExprPtr callee;
    std::vector<ExprPtr> args;

    // GROUPING
    ExprPtr inner;

    // ARRAY
    std::vector<ExprPtr> elements;

    // INDEX
    ExprPtr indexTarget;
    ExprPtr indexValue;

    // TERNARY (cond ? left : right) — "condition" holds the test, "left"/"right"
    // are reused for the then/else branches since BINARY/LOGICAL never use them
    // together with TERNARY on the same node.
    ExprPtr condition;

    // DICT — one literal entry per (key expression, value expression) pair.
    // Keys are always evaluated to strings at parse time (bare identifier or string literal).
    std::vector<std::pair<ExprPtr, ExprPtr>> dictEntries;

    // FUNCTION_EXPR — an inline "ton.function(params) { ... }" value, most often
    // passed straight into map()/filter()/reduce()/get()/post() as a callback.
    // Reuses "name" for an optional debug name and "params"/"body"-shaped fields below.
    std::vector<std::string> fnParams;
    StmtPtr fnBody;
};

enum class VarKind { NONE, INT, FLOAT, STRING, BOOL, ARRAY, HTML, DICT };

enum class StmtType {
    EXPR_STMT, VAR_DECL, BLOCK, IF, WHILE, FOR, FOR_IN, FN_DECL, RETURN, PRINT,
    BREAK, CONTINUE, IMPORT, TRY_CATCH, THROW, SWITCH
};

struct Stmt {
    StmtType type;
    int line = 0;

    ExprPtr expr;

    // VAR_DECL
    std::string name;
    ExprPtr initializer;
    VarKind declaredType = VarKind::NONE;

    // BLOCK
    std::vector<StmtPtr> statements;

    // IF
    // Also reused by:
    //   FOR_IN    — condition = the collection expression, thenBranch = the loop body, name = the item variable
    //   TRY_CATCH — thenBranch = the try block, elseBranch = the catch block, name = the caught-error variable
    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;

    // FOR
    StmtPtr forInit;
    ExprPtr forCondition;
    ExprPtr forIncrement;
    StmtPtr forBody;

    // FN_DECL
    std::string fnName;
    std::vector<std::string> params;
    StmtPtr body;

    // IMPORT
    std::string moduleName;

    // SWITCH — "condition" (reused from IF above) holds the subject expression.
    // Each case can list several values (`case 1, 2:`); the first one that
    // equals the subject (by the same rules as "==") runs its block, and no
    // case falls through into the next. "elseBranch" (also reused) holds the
    // optional "default:" block, run when no case matches.
    std::vector<std::pair<std::vector<ExprPtr>, StmtPtr>> switchCases;

    // THROW uses the shared "expr" field above for the thrown value.
};
