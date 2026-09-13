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

// Scans a "double-quoted" string literal. A "${expression}" anywhere inside
// it is string interpolation: "Hello ${ton.name}!" desugars, right here at
// the token level, into the same tokens as ("Hello " + (ton.name) + "!") —
// the parser and interpreter need no changes at all, since string
// concatenation via '+' (see Interpreter::evaluate) already stringifies any
// value type. The expression inside "${...}" can be anything a normal
// expression can (it's recursively lexed by a fresh Lexer over that
// substring), including nested strings and braces (both are tracked so
// e.g. ${ {a:1}["a"] } or ${ strings_pad_left(str(x), 3, "0") } work).
void Lexer::string() {
    std::vector<std::string> literalParts; // always exprParts.size() + 1
    std::vector<std::string> exprParts;
    std::string chunk;

    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\\') {
            advance();
            char esc = advance();
            switch (esc) {
                case 'n': chunk += '\n'; break;
                case 't': chunk += '\t'; break;
                case 'r': chunk += '\r'; break;
                case '"': chunk += '"'; break;
                case '\\': chunk += '\\'; break;
                case '$': chunk += '$'; break;
                default: chunk += esc; break;
            }
            continue;
        }

        if (peek() == '$' && peekNext() == '{') {
            advance(); advance(); // consume "${"
            literalParts.push_back(chunk);
            chunk.clear();

            int depth = 1;
            int exprStart = current;
            while (!isAtEnd() && depth > 0) {
                char c = peek();
                if (c == '"') {
                    // Skip a nested string literal so its own braces/quotes
                    // can't be mistaken for the interpolation's boundaries.
                    advance();
                    while (!isAtEnd() && peek() != '"') {
                        if (peek() == '\\') advance();
                        if (peek() == '\n') line++;
                        advance();
                    }
                    if (!isAtEnd()) advance();
                    continue;
                }
                if (c == '{') depth++;
                else if (c == '}') { depth--; if (depth == 0) break; }
                else if (c == '\n') line++;
                advance();
            }
            if (isAtEnd()) throw std::runtime_error("Ligne " + std::to_string(line) + ": interpolation '${...}' non terminee.");
            exprParts.push_back(source.substr(exprStart, current - exprStart));
            advance(); // consume closing '}'
            continue;
        }

        if (peek() == '\n') line++;
        chunk += advance();
    }
    if (isAtEnd()) throw std::runtime_error("Ligne " + std::to_string(line) + ": chaine non terminee.");
    advance(); // consume closing '"'
    literalParts.push_back(chunk);

    // The common case (no interpolation at all): a single plain STRING
    // token, exactly as before this feature existed.
    if (exprParts.empty()) {
        addToken(TokenType::STRING, literalParts[0]);
        return;
    }

    tokens.emplace_back(TokenType::LPAREN, "(", line);
    for (size_t i = 0; i < exprParts.size(); i++) {
        Token lit(TokenType::STRING, "\"...\"", line);
        lit.stringValue = literalParts[i];
        tokens.push_back(lit);
        tokens.emplace_back(TokenType::PLUS, "+", line);

        tokens.emplace_back(TokenType::LPAREN, "(", line);
        Lexer sub(exprParts[i]);
        for (auto& t : sub.scanTokens()) {
            if (t.type == TokenType::END_OF_FILE) continue;
            Token copy = t;
            copy.line = line; // approximate: attribute it to the "${" line
            tokens.push_back(copy);
        }
        tokens.emplace_back(TokenType::RPAREN, ")", line);
        tokens.emplace_back(TokenType::PLUS, "+", line);
    }
    Token lastLit(TokenType::STRING, "\"...\"", line);
    lastLit.stringValue = literalParts.back();
    tokens.push_back(lastLit);
    tokens.emplace_back(TokenType::RPAREN, ")", line);
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
        case '.': addToken(TokenType::DOT); break;
        case '?': addToken(TokenType::QUESTION); break;
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
