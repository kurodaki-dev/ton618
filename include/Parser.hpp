#pragma once
#include <vector>
#include "Token.hpp"
#include "Ast.hpp"

// ============================================================================
// Parser.hpp — turns the flat list of Tokens (see Lexer.hpp/Token.hpp) into
// an AST (see Ast.hpp): a tree of Stmt/Expr nodes the Interpreter can execute.
//
// This is a classic recursive-descent parser: `parse()` reads one top-level
// `declaration()` at a time until the tokens run out. Expressions are parsed
// through a chain of methods ordered from loosest to tightest binding
// (assignment -> ternary -> logicOr -> ... -> primary), which is how operator
// precedence (why `2 + 3 * 4` is `2 + (3 * 4)`, not `(2 + 3) * 4`) is encoded
// without a precedence table: each method only calls the next-tighter one
// for its operands, so the tightest-binding operators end up deepest in the
// tree and get evaluated first.
//
// To add a new operator: add its Token in Token.hpp/Lexer.cpp, then hook it
// into the right precedence method here. To add a new statement kind: add a
// StmtType in Ast.hpp, add a parsing method here, and dispatch to it from
// `statement()` or `declaration()`.
// ============================================================================

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    std::vector<StmtPtr> parse();

private:
    std::vector<Token> tokens;
    int current = 0;

    bool isAtEnd() const;
    const Token& peek() const;
    const Token& peekNext() const;
    const Token& previous() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool checkNext(TokenType type) const;
    bool match(std::vector<TokenType> types);
    const Token& consume(TokenType type, const std::string& message);
    [[noreturn]] void error(const Token& token, const std::string& message);

    // Statements
    StmtPtr declaration();
    StmtPtr tonDeclaration();     // gere tout ce qui commence par "ton."
    StmtPtr varDeclFromType(VarKind kind);
    StmtPtr fnDeclaration();
    StmtPtr importDeclaration();
    StmtPtr statement();
    StmtPtr ifStatement();
    StmtPtr whileStatement();
    StmtPtr forStatement(); // also handles "for ton.item in ton.collection { ... }"
    StmtPtr tryStatement();
    StmtPtr throwStatement();
    StmtPtr switchStatement();
    StmtPtr returnStatement();
    StmtPtr printStatement();
    StmtPtr block();
    StmtPtr exprStatement();

    // Expressions, from loosest to tightest binding:
    // expression -> assignment -> ternary -> nilCoalesce -> logicOr -> logicAnd
    //            -> bitwise -> equality -> comparison -> shift -> term -> factor
    //            -> unary -> call -> primary
    ExprPtr expression();
    ExprPtr assignment();
    ExprPtr ternary();
    ExprPtr nilCoalesce(); // "a ?? b"
    ExprPtr logicOr();
    ExprPtr logicAnd();
    ExprPtr bitwise(); // "a & b", "a | b", "a ^ b" — all one precedence level, left to right
    ExprPtr equality();
    ExprPtr comparison(); // also handles "a in b" (array/dict/string membership)
    ExprPtr shift(); // "a << b", "a >> b"
    ExprPtr term();
    ExprPtr factor();
    ExprPtr unary(); // also handles "~a" (bitwise not)
    ExprPtr call();
    ExprPtr finishCall(ExprPtr callee);
    ExprPtr primary();
    ExprPtr tonReference(); // parse "ton.identifiant" en expression (variable ou appel)
    ExprPtr functionExpression(int line); // parse "ton.function(...) { ... }" as an inline value (a callback)

    // Parses "(a, b = default, ...rest)" — shared by fnDeclaration() and
    // functionExpression(). "..." may only appear on the last parameter.
    struct ParamList { std::vector<std::string> names; std::vector<ExprPtr> defaults; bool hasRest = false; };
    ParamList parseParamList();

    // Builds the ASSIGN (or INDEX-assign) node used by both "=" and by desugaring
    // compound assignment (+=, -=, ...) and postfix increment/decrement (++, --).
    ExprPtr makeAssignTarget(const ExprPtr& target, const ExprPtr& value, const Token& opToken);
};
