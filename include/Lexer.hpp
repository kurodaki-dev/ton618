#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "Token.hpp"

// ============================================================================
// Lexer.hpp — turns raw source text into a flat list of Tokens (see Token.hpp).
//
// This is the first stage of the pipeline: text -> Lexer -> Tokens -> Parser
// -> AST -> Interpreter. It knows nothing about grammar (it doesn't care that
// "ton.int x = 5" is a valid declaration and "5 = ton.int x" isn't) — it just
// recognizes characters that belong together (numbers, strings, identifiers,
// keywords, operators) and emits one Token per unit, tracking line numbers
// along the way for error messages and the debugger.
// ============================================================================

class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> scanTokens();

private:
    std::string source;
    std::vector<Token> tokens;
    int start = 0;
    int current = 0;
    int line = 1;

    static const std::unordered_map<std::string, TokenType> keywords;

    bool isAtEnd() const;
    char advance();
    bool match(char expected);
    char peek() const;
    char peekNext() const;
    char peekAt(int offset) const;
    void scanToken();
    void addToken(TokenType type);
    void addToken(TokenType type, const std::string& strVal);
    void addToken(TokenType type, double numVal);

    void string();
    void number();
    void identifierOrTon();
    void skipLineComment();
    void skipBlockComment();
};
