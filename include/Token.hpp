#pragma once
#include <string>

// ============================================================================
// Token.hpp — the vocabulary of the TON618 language.
//
// A Token is the smallest unit the Lexer produces from raw source text (see
// Lexer.hpp). The Parser (see Parser.hpp) then consumes a flat list of Tokens
// and builds the AST (see Ast.hpp) out of them. If you're adding a new keyword
// or operator to the language, this is the first file to touch:
//   1. Add a TokenType here.
//   2. Teach the Lexer to produce it (src/Lexer.cpp — either in the `keywords`
//      map, or in `scanToken()` for a punctuation/operator character).
//   3. Teach the Parser to consume it (src/Parser.cpp).
// ============================================================================

enum class TokenType {
    // Literals
    NUMBER, STRING, IDENTIFIER, TRUE, FALSE, NIL,

    // Operators
    PLUS, MINUS, STAR, SLASH, PERCENT,
    EQUAL, EQUAL_EQUAL, BANG, BANG_EQUAL,
    LESS, LESS_EQUAL, GREATER, GREATER_EQUAL,
    AND, OR,

    // Compound assignment: += -= *= /= %=
    PLUS_EQUAL, MINUS_EQUAL, STAR_EQUAL, SLASH_EQUAL, PERCENT_EQUAL,
    // Compound assignment, bitwise: &= |= ^= <<= >>=
    AMPERSAND_EQUAL, PIPE_EQUAL, CARET_EQUAL, LESS_LESS_EQUAL, GREATER_GREATER_EQUAL,
    // Increment / decrement: ++ --
    PLUS_PLUS, MINUS_MINUS,
    // Ternary: cond ? a : b
    QUESTION, COLON,
    // Nil-coalescing: a ?? b
    QUESTION_QUESTION,
    // Rest parameter: ton.function f(a, ...rest) { ... }
    ELLIPSIS,
    // Bitwise: & | ^ ~ << >> (operate on numbers truncated to a 64-bit integer)
    AMPERSAND, PIPE, CARET, TILDE, LESS_LESS, GREATER_GREATER,

    // Symboles
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    COMMA, SEMICOLON, DOT,

    // TON618 specifique
    TON,            // le mot "ton" avant le point (ton.x, ton.int, ton.function...)
    IMPORT_ARROW,   // "IMPORT://"

    // Types
    TYPE_INT, TYPE_STRING, TYPE_BOOL, TYPE_FLOAT, TYPE_ARRAY, TYPE_HTML, TYPE_DICT,

    // Keywords
    IF, ELSE, WHILE, FOR, IN, FUNCTION, RETURN, BREAK, CONTINUE,
    TRY, CATCH, THROW, FINALLY,
    SWITCH, CASE, DEFAULT,
    PRINT,

    // Special
    NEWLINE, END_OF_FILE
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;

    std::string stringValue;
    double numberValue = 0;

    Token(TokenType t, std::string lex, int ln)
        : type(t), lexeme(std::move(lex)), line(ln) {}
};
