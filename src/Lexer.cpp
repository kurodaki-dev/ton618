#include "Lexer.hpp"
#include <cctype>
#include <stdexcept>

const std::unordered_map<std::string, TokenType> Lexer::keywords = {
    {"if", TokenType::IF},
    {"else", TokenType::ELSE},
    {"while", TokenType::WHILE},
    {"for", TokenType::FOR},
    {"in", TokenType::IN},
    {"function", TokenType::FUNCTION},
    {"return", TokenType::RETURN},
    {"break", TokenType::BREAK},
    {"continue", TokenType::CONTINUE},
    {"try", TokenType::TRY},
    {"catch", TokenType::CATCH},
    {"throw", TokenType::THROW},
    {"finally", TokenType::FINALLY},
    {"switch", TokenType::SWITCH},
    {"case", TokenType::CASE},
    {"default", TokenType::DEFAULT},
    {"true", TokenType::TRUE},
    {"false", TokenType::FALSE},
    {"nil", TokenType::NIL},
    {"and", TokenType::AND},
    {"or", TokenType::OR},
    {"print", TokenType::PRINT},
    // types (only used after "ton.")
    {"int", TokenType::TYPE_INT},
    {"string", TokenType::TYPE_STRING},
    {"bool", TokenType::TYPE_BOOL},
    {"float", TokenType::TYPE_FLOAT},
    {"array", TokenType::TYPE_ARRAY},
    {"html", TokenType::TYPE_HTML},
    {"dict", TokenType::TYPE_DICT},
};

Lexer::Lexer(std::string src) : source(std::move(src)) {}

bool Lexer::isAtEnd() const { return current >= (int)source.size(); }
char Lexer::advance() { return source[current++]; }

bool Lexer::match(char expected) {
    if (isAtEnd() || source[current] != expected) return false;
    current++;
    return true;
}

char Lexer::peek() const { return isAtEnd() ? '\0' : source[current]; }
char Lexer::peekNext() const {
    return (current + 1 >= (int)source.size()) ? '\0' : source[current + 1];
}
char Lexer::peekAt(int offset) const {
    int idx = current + offset;
    return (idx >= (int)source.size()) ? '\0' : source[idx];
}

void Lexer::addToken(TokenType type) {
    tokens.emplace_back(type, source.substr(start, current - start), line);
}
void Lexer::addToken(TokenType type, const std::string& strVal) {
    Token t(type, source.substr(start, current - start), line);
    t.stringValue = strVal;
    tokens.push_back(t);
}
void Lexer::addToken(TokenType type, double numVal) {
    Token t(type, source.substr(start, current - start), line);
    t.numberValue = numVal;
    tokens.push_back(t);
}

void Lexer::skipLineComment() {
    while (peek() != '\n' && !isAtEnd()) advance();
}

void Lexer::skipBlockComment() {
    while (!isAtEnd()) {
        if (peek() == '*' && peekNext() == '/') { advance(); advance(); return; }
        if (peek() == '\n') line++;
        advance();
    }
}

// Scans a "double-quoted" string literal. Plain and simple on purpose — for
// building a string out of variables, use the always-available format()
// native (src/Interpreter.cpp) instead of embedded expression syntax here:
// format("Hello {}, you are {} next year!", name, age + 1).
void Lexer::string() {
    std::string value;
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\\') {
            advance();
            char esc = advance();
            switch (esc) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '"': value += '"'; break;
                case '\\': value += '\\'; break;
                default: value += esc; break;
            }
            continue;
        }
        if (peek() == '\n') line++;
        value += advance();
    }
    if (isAtEnd()) throw std::runtime_error("Ligne " + std::to_string(line) + ": chaine non terminee.");
    advance();
    addToken(TokenType::STRING, value);
}

void Lexer::number() {
    while (std::isdigit(peek())) advance();
    if (peek() == '.' && std::isdigit(peekNext())) {
        advance();
        while (std::isdigit(peek())) advance();
    }
    addToken(TokenType::NUMBER, std::stod(source.substr(start, current - start)));
}

void Lexer::identifierOrTon() {
    while (std::isalnum(peek()) || peek() == '_') advance();
    std::string text = source.substr(start, current - start);

    if (text == "ton") {
        addToken(TokenType::TON);
        return;
    }
    if (text == "IMPORT") {
        // Expect "://" right after
        if (peek() == ':' && peekNext() == '/' && peekAt(2) == '/') {
            advance(); advance(); advance(); // consomme "://"
            addToken(TokenType::IMPORT_ARROW);
            return;
        }
    }

    auto it = keywords.find(text);
    if (it != keywords.end()) {
        addToken(it->second);
    } else {
        addToken(TokenType::IDENTIFIER);
    }
}

void Lexer::scanToken() {
    char c = advance();
    switch (c) {
        case '(': addToken(TokenType::LPAREN); break;
        case ')': addToken(TokenType::RPAREN); break;
        case '{': addToken(TokenType::LBRACE); break;
        case '}': addToken(TokenType::RBRACE); break;
        case '[': addToken(TokenType::LBRACKET); break;
        case ']': addToken(TokenType::RBRACKET); break;
        case ',': addToken(TokenType::COMMA); break;
        case ';': addToken(TokenType::SEMICOLON); break;
        case '.':
            if (peek() == '.' && peekNext() == '.') { advance(); advance(); addToken(TokenType::ELLIPSIS); }
            else addToken(TokenType::DOT);
            break;
        case '?':
            addToken(match('?') ? TokenType::QUESTION_QUESTION : TokenType::QUESTION);
            break;
        case ':': addToken(TokenType::COLON); break;
        case '+':
            if (match('+')) addToken(TokenType::PLUS_PLUS);
            else if (match('=')) addToken(TokenType::PLUS_EQUAL);
            else addToken(TokenType::PLUS);
            break;
        case '-':
            if (match('-')) addToken(TokenType::MINUS_MINUS);
            else if (match('=')) addToken(TokenType::MINUS_EQUAL);
            else addToken(TokenType::MINUS);
            break;
        case '*':
            addToken(match('=') ? TokenType::STAR_EQUAL : TokenType::STAR);
            break;
        case '%':
            addToken(match('=') ? TokenType::PERCENT_EQUAL : TokenType::PERCENT);
            break;
        case '/':
            if (match('/')) skipLineComment();
            else if (match('*')) skipBlockComment();
            else if (match('=')) addToken(TokenType::SLASH_EQUAL);
            else addToken(TokenType::SLASH);
            break;
        case '=':
            addToken(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL);
            break;
        case '!':
            addToken(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
            break;
        case '<':
            addToken(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);
            break;
        case '>':
            addToken(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
            break;
        case '&':
            if (match('&')) addToken(TokenType::AND);
            break;
        case '|':
            if (match('|')) addToken(TokenType::OR);
            break;
        case ' ': case '\r': case '\t':
            break;
        case '\n':
            line++;
            break;
        case '"':
            string();
            break;
        default:
            if (std::isdigit(c)) number();
            else if (std::isalpha(c) || c == '_') identifierOrTon();
            else throw std::runtime_error("Ligne " + std::to_string(line) +
                    ": caractere inattendu '" + std::string(1, c) + "'.");
            break;
    }
}

std::vector<Token> Lexer::scanTokens() {
    while (!isAtEnd()) {
        start = current;
        scanToken();
    }
    tokens.emplace_back(TokenType::END_OF_FILE, "", line);
    return tokens;
}
