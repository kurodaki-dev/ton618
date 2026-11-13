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
                case '0': value += '\0'; break;
                case 'u': value += unicodeEscape(); break;
                // In an f-string, "\{" keeps a literal brace; the template
                // parser (Parser::fstring) sees it as the "{{" escape.
                case '{': value += fstringMode ? "{{" : "{"; break;
                case '}': value += fstringMode ? "}}" : "}"; break;
                default: value += esc; break;
            }
            continue;
        }
        if (peek() == '\n') line++;
        value += advance();
    }
    if (isAtEnd()) throw std::runtime_error("Line " + std::to_string(line) + ": unterminated string.");
    advance();
    addToken(fstringMode ? TokenType::FSTRING : TokenType::STRING, value);
}

// "\u{1F600}" or "\u00e9" -> the UTF-8 bytes of that code point.
std::string Lexer::unicodeEscape() {
    std::string hex;
    if (match('{')) {
        while (!isAtEnd() && peek() != '}') hex += advance();
        if (!match('}')) throw std::runtime_error("Line " + std::to_string(line) + ": unterminated \\u{...} escape.");
    } else {
        for (int i = 0; i < 4 && std::isxdigit((unsigned char)peek()); i++) hex += advance();
    }
    if (hex.empty()) throw std::runtime_error("Line " + std::to_string(line) + ": invalid \\u escape.");
    unsigned long cp = std::stoul(hex, nullptr, 16);
    std::string out;
    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F));
    }
    return out;
}

// `raw multi-line string` — no escape sequences at all, newlines kept as-is.
// Handy for embedded HTML, regexes and Windows paths.
void Lexer::rawString() {
    std::string value;
    while (peek() != '`' && !isAtEnd()) {
        if (peek() == '\n') line++;
        value += advance();
    }
    if (isAtEnd()) throw std::runtime_error("Line " + std::to_string(line) + ": unterminated `raw string`.");
    advance();
    addToken(TokenType::STRING, value);
}

// Numbers: 42, 3.14, 1_000_000, 1e9, 2.5e-3, 0xFF, 0b1010, 0o755.
// Underscores are digit separators and are ignored.
void Lexer::number() {
    char first = source[start];
    if (first == '0' && (peek() == 'x' || peek() == 'X' || peek() == 'b' || peek() == 'B' || peek() == 'o' || peek() == 'O')) {
        char kind = (char)std::tolower(advance());
        int base = kind == 'x' ? 16 : kind == 'b' ? 2 : 8;
        std::string digits;
        while (std::isxdigit((unsigned char)peek()) || peek() == '_') { char c = advance(); if (c != '_') digits += c; }
        if (digits.empty()) throw std::runtime_error("Line " + std::to_string(line) + ": malformed number literal.");
        try {
            addToken(TokenType::NUMBER, (double)std::stoull(digits, nullptr, base));
        } catch (...) {
            throw std::runtime_error("Line " + std::to_string(line) + ": malformed number literal '0" + std::string(1, kind) + digits + "'.");
        }
        return;
    }
    std::string text(1, first);
    auto digitsRun = [&]() { while (std::isdigit((unsigned char)peek()) || (peek() == '_' && std::isdigit((unsigned char)peekNext()))) { char c = advance(); if (c != '_') text += c; } };
    digitsRun();
    if (peek() == '.' && std::isdigit((unsigned char)peekNext())) {
        text += advance();
        digitsRun();
    }
    if ((peek() == 'e' || peek() == 'E') &&
        (std::isdigit((unsigned char)peekNext()) || ((peekNext() == '+' || peekNext() == '-') && std::isdigit((unsigned char)peekAt(2))))) {
        text += advance();
        if (peek() == '+' || peek() == '-') text += advance();
        digitsRun();
    }
    addToken(TokenType::NUMBER, std::stod(text));
}

void Lexer::identifierOrTon() {
    // f"..." — an f-string: a template with {expressions} inside.
    if (source[start] == 'f' && current == start + 1 && peek() == '"') {
        advance();
        fstringMode = true;
        string();
        fstringMode = false;
        return;
    }
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
            else if (match('.')) addToken(match('=') ? TokenType::DOT_DOT_EQUAL : TokenType::DOT_DOT);
            else addToken(TokenType::DOT);
            break;
        case '?':
            if (match('?')) addToken(match('=') ? TokenType::QUESTION_QUESTION_EQUAL : TokenType::QUESTION_QUESTION);
            // "?." is optional chaining — but "c ?.5 : 1" (a ternary with a
            // ".5" literal) is not, hence the digit check.
            else if (peek() == '.' && !std::isdigit((unsigned char)peekNext())) { advance(); addToken(TokenType::QUESTION_DOT); }
            else addToken(TokenType::QUESTION);
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
            if (match('*')) addToken(match('=') ? TokenType::STAR_STAR_EQUAL : TokenType::STAR_STAR);
            else addToken(match('=') ? TokenType::STAR_EQUAL : TokenType::STAR);
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
            if (match('=')) addToken(TokenType::EQUAL_EQUAL);
            else if (match('>')) addToken(TokenType::ARROW);
            else addToken(TokenType::EQUAL);
            break;
        case '!':
            addToken(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
            break;
        case '<':
            if (match('<')) addToken(match('=') ? TokenType::LESS_LESS_EQUAL : TokenType::LESS_LESS);
            else addToken(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);
            break;
        case '>':
            if (match('>')) addToken(match('=') ? TokenType::GREATER_GREATER_EQUAL : TokenType::GREATER_GREATER);
            else addToken(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
            break;
        case '&':
            if (match('&')) addToken(TokenType::AND);
            else addToken(match('=') ? TokenType::AMPERSAND_EQUAL : TokenType::AMPERSAND);
            break;
        case '|':
            if (match('|')) addToken(TokenType::OR);
            else if (match('>')) addToken(TokenType::PIPE_GREATER);
            else addToken(match('=') ? TokenType::PIPE_EQUAL : TokenType::PIPE);
            break;
        case '^':
            addToken(match('=') ? TokenType::CARET_EQUAL : TokenType::CARET);
            break;
        case '~':
            addToken(TokenType::TILDE);
            break;
        case ' ': case '\r': case '\t':
            break;
        case '\n':
            line++;
            break;
        case '"':
            string();
            break;
        case '`':
            rawString();
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
