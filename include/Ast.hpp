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
    TERNARY, DICT, FUNCTION_EXPR, SPREAD,
    // beta-2.0.0
    GET,      // object.name  /  object?.name   (assignment: "value" is set -> obj.name = value)
    SUPER,    // super.name — a method of the enclosing class's parent, bound to self
    RANGE,    // a..b / a..=b — "left"/"right" are the bounds, "inclusive" says which
    SLICE,    // target[start:end] — either bound may be null
    FSTRING   // f"Hello {name}" — "elements" alternates literal and expression parts
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
    int sym = -1; // "name" interned (see Symbols, Environment.hpp) — filled lazily by the interpreter

    // GET — "object.name" (the property name is in "name"). With "value" set,
    // it's an assignment (obj.name = value). "optional" marks "?." — the
    // result is nil instead of an error when the object is nil (also used by
    // INDEX "a?.[i]" and CALL "f?.()").
    ExprPtr object;
    bool optional = false;

    // RANGE — inclusive is true for "..=".
    bool inclusive = false;

    // SLICE — "indexTarget" is the sliced value; either bound may be null.
    ExprPtr sliceStart, sliceEnd;

    // BINARY / LOGICAL
    ExprPtr left, right;
    TokenType op;

    // UNARY / SPREAD ("...expr" inside an array or dict literal — see ARRAY/DICT below)
    ExprPtr operand;

    // CALL
    ExprPtr callee;
    std::vector<ExprPtr> args;

    // GROUPING
    ExprPtr inner;

    // ARRAY — an element may itself be a SPREAD node ("...arr"), splicing that
    // array's items in place instead of contributing a single element.
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
    // A spread entry ("...dict") is stored as {nullptr, spreadNode} where
    // spreadNode->type == SPREAD — its keys/values are merged in, later entries
    // (spread or not) overriding earlier ones with the same key.
    std::vector<std::pair<ExprPtr, ExprPtr>> dictEntries;

    // FUNCTION_EXPR — an inline "ton.function(params) { ... }" value, most often
    // passed straight into map()/filter()/reduce()/get()/post() as a callback.
    // Reuses "name" for an optional debug name and "params"/"body"-shaped fields below.
    // fnParamDefaults is parallel to fnParams (nullptr = no default value); if
    // fnHasRestParam is true, the LAST entry of fnParams collects every extra
    // argument into a ton.array instead of binding a single value.
    std::vector<std::string> fnParams;
    std::vector<ExprPtr> fnParamDefaults;
    bool fnHasRestParam = false;
    StmtPtr fnBody;
};

// CLASS = "ton.Point p = ..." (declared with a class name: Stmt::typeName).
// ANY   = "ton.let x = ..." / "ton.const X = ..." (no type check).
enum class VarKind { NONE, INT, FLOAT, STRING, BOOL, ARRAY, HTML, DICT, CLASS, ANY };

enum class StmtType {
    EXPR_STMT, VAR_DECL, BLOCK, IF, WHILE, FOR, FOR_IN, FN_DECL, RETURN, PRINT,
    BREAK, CONTINUE, IMPORT, TRY_CATCH, THROW, SWITCH,
    CLASS_DECL // ton.class Name extends Parent { fields and methods } — see below
};

struct Stmt {
    StmtType type;
    int line = 0;

    ExprPtr expr;

    // VAR_DECL
    std::string name;
    ExprPtr initializer;
    VarKind declaredType = VarKind::NONE;
    bool isConst = false;      // ton.const — reassigning it is an error
    std::string typeName;      // VarKind::CLASS: the class the value must be an instance of
    // Destructuring: "ton.let [a, b, ...rest] = arr" (destructKind 1) or
    // "ton.let {name, age: years} = dict" (destructKind 2). Each entry is
    // (dict key or "" for arrays, variable name); destructRest is the
    // "...rest" name ("" if none).
    int destructKind = 0;
    std::vector<std::pair<std::string, std::string>> destructNames;
    std::string destructRest;

    int sym = -1, sym2 = -1;   // interned name / name2, filled lazily by the interpreter

    // BLOCK — needsScope is false when the block declares nothing itself, so
    // the interpreter can skip allocating a new Environment for it.
    // CLASS_DECL also stores its body (field VAR_DECLs and method FN_DECLs) here.
    std::vector<StmtPtr> statements;
    bool needsScope = true;

    // FOR_IN — the optional second loop variable: "for ton.k, ton.v in dict"
    // (key, value) or "for ton.i, ton.x in array" (index, value).
    std::string name2;

    // CLASS_DECL — "name" is the class name, superName the parent ("" if none).
    std::string superName;

    // IF
    // Also reused by:
    //   FOR_IN    — condition = the collection expression, thenBranch = the loop body, name = the item variable
    //   TRY_CATCH — thenBranch = the try block, elseBranch = the catch block, name = the caught-error variable
    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;

    // TRY_CATCH — the optional "finally { ... }" block, run exactly once no
    // matter what happens in the try/catch above (success, a caught error, an
    // error raised from inside the catch block itself, or a break/continue/
    // return unwinding through it). Null if there's no finally clause.
    StmtPtr finallyBranch;

    // FOR
    StmtPtr forInit;
    ExprPtr forCondition;
    ExprPtr forIncrement;
    StmtPtr forBody;

    // FN_DECL — paramDefaults is parallel to params (nullptr = no default
    // value); if hasRestParam is true, the LAST entry of params collects
    // every extra argument into a ton.array instead of binding a single value.
    std::string fnName;
    std::vector<std::string> params;
    std::vector<ExprPtr> paramDefaults;
    bool hasRestParam = false;
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
