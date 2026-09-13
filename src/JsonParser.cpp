#include "JsonParser.hpp"
#include <stdexcept>
#include <cctype>
#include <cmath>

namespace {

// A tiny hand-written recursive-descent JSON parser. It's deliberately
// minimal (no streaming, no big-number handling) since it only needs to
// round-trip the same JSON shapes Value::toJson() can produce, plus whatever
// reasonable JSON a script might read from a file or an HTTP response.
struct JsonReader {
    const std::string& s;
    size_t pos = 0;

    explicit JsonReader(const std::string& src) : s(src) {}

    [[noreturn]] void fail(const std::string& msg) {
        throw std::runtime_error("json_parse: " + msg + " at offset " + std::to_string(pos) + ".");
    }

    char peek() const { return pos < s.size() ? s[pos] : '\0'; }
    char advance() { return s[pos++]; }
    bool atEnd() const { return pos >= s.size(); }

    void skipWhitespace() {
        while (!atEnd() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r')) pos++;
    }

    bool matchLiteral(const char* lit) {
        size_t len = 0;
        while (lit[len]) len++;
        if (s.compare(pos, len, lit) == 0) { pos += len; return true; }
        return false;
    }

    Value parseValue() {
        skipWhitespace();
        if (atEnd()) fail("unexpected end of input");
        char c = peek();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return Value::String(parseString());
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') { if (!matchLiteral("null")) fail("invalid literal"); return Value::Nil(); }
        if (c == '-' || std::isdigit((unsigned char)c)) return parseNumber();
        fail(std::string("unexpected character '") + c + "'");
    }

    Value parseObject() {
        advance(); // '{'
        std::vector<std::pair<std::string, Value>> entries;
        skipWhitespace();
        if (peek() == '}') { advance(); return Value::Dict(entries); }
        while (true) {
            skipWhitespace();
            if (peek() != '"') fail("expected a string key");
            std::string key = parseString();
            skipWhitespace();
            if (peek() != ':') fail("expected ':' after object key");
            advance();
            Value val = parseValue();
            entries.push_back({key, val});
            skipWhitespace();
            if (peek() == ',') { advance(); continue; }
            if (peek() == '}') { advance(); break; }
            fail("expected ',' or '}' in object");
        }
        return Value::Dict(entries);
    }

    Value parseArray() {
        advance(); // '['
        std::vector<Value> elems;
        skipWhitespace();
        if (peek() == ']') { advance(); return Value::Array(elems); }
        while (true) {
            elems.push_back(parseValue());
            skipWhitespace();
            if (peek() == ',') { advance(); continue; }
            if (peek() == ']') { advance(); break; }
            fail("expected ',' or ']' in array");
        }
        return Value::Array(elems);
    }

    std::string parseString() {
        advance(); // opening '"'
        std::string out;
        while (true) {
            if (atEnd()) fail("unterminated string");
            char c = advance();
            if (c == '"') break;
            if (c == '\\') {
                if (atEnd()) fail("unterminated escape sequence");
                char esc = advance();
                switch (esc) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u': {
                        // Minimal \uXXXX support: only the common ASCII-range case, no
                        // surrogate-pair handling — sufficient for typical script/API JSON.
                        if (pos + 4 > s.size()) fail("invalid \\u escape");
                        std::string hex = s.substr(pos, 4);
                        pos += 4;
                        int code = std::stoi(hex, nullptr, 16);
                        if (code < 0x80) out += (char)code;
                        else out += '?'; // non-ASCII code point, not worth a full UTF-8 encoder here
                        break;
                    }
                    default: fail("invalid escape sequence");
                }
                continue;
            }
            out += c;
        }
        return out;
    }

    Value parseBool() {
        if (matchLiteral("true")) return Value::Bool(true);
        if (matchLiteral("false")) return Value::Bool(false);
        fail("invalid literal");
    }

    Value parseNumber() {
        size_t start = pos;
        if (peek() == '-') advance();
        while (std::isdigit((unsigned char)peek())) advance();
        if (peek() == '.') { advance(); while (std::isdigit((unsigned char)peek())) advance(); }
        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') advance();
            while (std::isdigit((unsigned char)peek())) advance();
        }
        std::string numStr = s.substr(start, pos - start);
        try { return Value::Number(std::stod(numStr)); }
        catch (...) { fail("invalid number '" + numStr + "'"); }
    }
};

} // namespace

Value parseJson(const std::string& text) {
    JsonReader reader(text);
    Value result = reader.parseValue();
    reader.skipWhitespace();
    if (!reader.atEnd()) reader.fail("unexpected trailing content");
    return result;
}
