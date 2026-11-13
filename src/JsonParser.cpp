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
                        // \uXXXX, including UTF-16 surrogate pairs, written out as UTF-8.
                        auto hex4 = [&]() -> unsigned {
                            if (pos + 4 > s.size()) fail("invalid \\u escape");
                            unsigned v = 0;
                            for (int k = 0; k < 4; k++) {
                                char h = s[pos++];
                                v <<= 4;
                                if (h >= '0' && h <= '9') v |= (unsigned)(h - '0');
                                else if (h >= 'a' && h <= 'f') v |= (unsigned)(h - 'a' + 10);
                                else if (h >= 'A' && h <= 'F') v |= (unsigned)(h - 'A' + 10);
                                else fail("invalid \\u escape");
                            }
                            return v;
                        };
                        unsigned code = hex4();
                        if (code >= 0xD800 && code < 0xDC00 && pos + 6 <= s.size() && s[pos] == '\\' && s[pos + 1] == 'u') {
                            size_t save = pos;
                            pos += 2;
                            unsigned low = hex4();
                            if (low >= 0xDC00 && low < 0xE000) code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                            else pos = save; // not a pair: keep the lone high surrogate as is
                        }
                        if (code < 0x80) out += (char)code;
                        else if (code < 0x800) { out += (char)(0xC0 | (code >> 6)); out += (char)(0x80 | (code & 0x3F)); }
                        else if (code < 0x10000) {
                            out += (char)(0xE0 | (code >> 12)); out += (char)(0x80 | ((code >> 6) & 0x3F)); out += (char)(0x80 | (code & 0x3F));
                        } else {
                            out += (char)(0xF0 | (code >> 18)); out += (char)(0x80 | ((code >> 12) & 0x3F));
                            out += (char)(0x80 | ((code >> 6) & 0x3F)); out += (char)(0x80 | (code & 0x3F));
                        }
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
