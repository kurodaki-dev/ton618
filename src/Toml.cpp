#include "Toml.hpp"
#include <cctype>
#include <cstdlib>
#include <stdexcept>

namespace {

using Table = std::shared_ptr<std::vector<std::pair<std::string, Value>>>;

struct Reader {
    const std::string& s;
    const std::string& what;
    size_t i = 0;
    int line = 1;

    Reader(const std::string& text, const std::string& w) : s(text), what(w) {}

    [[noreturn]] void fail(const std::string& msg) const {
        throw std::runtime_error(what + " line " + std::to_string(line) + ": " + msg);
    }

    bool atEnd() const { return i >= s.size(); }
    char peek(size_t k = 0) const { return i + k < s.size() ? s[i + k] : '\0'; }

    void skipSpaces() { while (peek() == ' ' || peek() == '\t') i++; }
    void skipComment() { if (peek() == '#') while (!atEnd() && peek() != '\n') i++; }
    // Spaces, comments and line breaks (inside arrays and between statements).
    void skipBlank() {
        while (!atEnd()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r') i++;
            else if (c == '\n') { i++; line++; }
            else if (c == '#') skipComment();
            else break;
        }
    }

    static bool bareChar(char c) { return std::isalnum((unsigned char)c) || c == '_' || c == '-'; }

    static void putUtf8(std::string& out, unsigned long cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }

    // After the opening quote(s): reads a basic string, handling escapes.
    std::string basicString(bool multi) {
        std::string out;
        if (multi && peek() == '\r') i++;
        if (multi && peek() == '\n') { i++; line++; } // the first newline of """ is dropped
        while (true) {
            if (atEnd()) fail("unterminated string");
            char c = s[i];
            if (multi) {
                if (c == '"' && peek(1) == '"' && peek(2) == '"') {
                    i += 3;
                    return out;
                }
            } else if (c == '"') { i++; return out; }
            if (c == '\n') {
                if (!multi) fail("a basic string can't contain a line break (use \"\"\" for several lines)");
                line++;
            }
            if (c == '\\') {
                i++;
                char e = peek();
                if (multi && (e == '\n' || e == ' ' || e == '\t' || e == '\r')) {
                    // "\" at the end of a line: swallow the whitespace and line breaks that follow
                    while (!atEnd() && (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n')) { if (peek() == '\n') line++; i++; }
                    continue;
                }
                i++;
                switch (e) {
                    case 'b': out += '\b'; break;
                    case 't': out += '\t'; break;
                    case 'n': out += '\n'; break;
                    case 'f': out += '\f'; break;
                    case 'r': out += '\r'; break;
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case 'u': case 'U': {
                        int n = e == 'u' ? 4 : 8;
                        if (i + (size_t)n > s.size()) fail("invalid unicode escape");
                        unsigned long cp = 0;
                        for (int k = 0; k < n; k++) {
                            char h = s[i++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= (unsigned long)(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= (unsigned long)(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= (unsigned long)(h - 'A' + 10);
                            else fail("invalid unicode escape");
                        }
                        if (cp > 0x10FFFF || (cp >= 0xD800 && cp < 0xE000)) fail("invalid unicode escape");
                        putUtf8(out, cp);
                        break;
                    }
                    default: fail(std::string("invalid escape '\\") + e + "'");
                }
                continue;
            }
            out += c;
            i++;
        }
    }

    std::string literalString(bool multi) {
        std::string out;
        if (multi && peek() == '\r') i++;
        if (multi && peek() == '\n') { i++; line++; }
        while (true) {
            if (atEnd()) fail("unterminated string");
            char c = s[i];
            if (multi) {
                if (c == '\'' && peek(1) == '\'' && peek(2) == '\'') { i += 3; return out; }
            } else if (c == '\'') { i++; return out; }
            if (c == '\n') {
                if (!multi) fail("a literal string can't contain a line break (use ''' for several lines)");
                line++;
            }
            out += c;
            i++;
        }
    }

    std::string quotedString() {
        char q = s[i];
        bool multi = peek(1) == q && peek(2) == q;
        i += multi ? 3 : 1;
        return q == '"' ? basicString(multi) : literalString(multi);
    }

    std::string key() {
        skipSpaces();
        if (peek() == '"' || peek() == '\'') return quotedString();
        size_t start = i;
        while (bareChar(peek())) i++;
        if (i == start) fail("expected a key name");
        return s.substr(start, i - start);
    }

    std::vector<std::string> keyPath() {
        std::vector<std::string> path{key()};
        while (true) {
            skipSpaces();
            if (peek() != '.') break;
            i++;
            path.push_back(key());
        }
        return path;
    }

    Value number() {
        size_t start = i;
        if (peek() == '+' || peek() == '-') i++;
        if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'o' || peek(1) == 'b')) {
            int base = peek(1) == 'x' ? 16 : peek(1) == 'o' ? 8 : 2;
            i += 2;
            std::string digits;
            while (std::isxdigit((unsigned char)peek()) || peek() == '_') { if (peek() != '_') digits += peek(); i++; }
            if (digits.empty()) fail("invalid number");
            return Value::Number((double)std::strtoull(digits.c_str(), nullptr, base));
        }
        std::string text;
        if (s[start] == '-') text += '-';
        bool isFloat = false;
        while (true) {
            char c = peek();
            if (std::isdigit((unsigned char)c)) text += c;
            else if (c == '_') {}
            else if (c == '.' || c == 'e' || c == 'E') { isFloat = true; text += c; }
            else if ((c == '+' || c == '-') && (text.back() == 'e' || text.back() == 'E')) text += c;
            else break;
            i++;
        }
        if (peek() == '-' || peek() == ':') fail("dates and times are not supported");
        if (text.empty() || text == "-" || (!std::isdigit((unsigned char)text.back()))) fail("invalid number");
        (void)isFloat;
        return Value::Number(std::strtod(text.c_str(), nullptr));
    }

    Value value() {
        skipSpaces();
        char c = peek();
        if (c == '"' || c == '\'') return Value::String(quotedString());
        if (c == '[') return array();
        if (c == '{') return inlineTable();
        if (s.compare(i, 4, "true") == 0 && !bareChar(peek(4))) { i += 4; return Value::Bool(true); }
        if (s.compare(i, 5, "false") == 0 && !bareChar(peek(5))) { i += 5; return Value::Bool(false); }
        if (s.compare(i, 3, "inf") == 0 || s.compare(i, 3, "nan") == 0) fail("inf and nan are not supported");
        if (std::isdigit((unsigned char)c) || c == '+' || c == '-') return number();
        fail("expected a value");
    }

    Value array() {
        i++; // [
        std::vector<Value> items;
        while (true) {
            skipBlank();
            if (atEnd()) fail("unterminated array");
            if (peek() == ']') { i++; break; }
            items.push_back(value());
            skipBlank();
            if (peek() == ',') { i++; continue; }
            if (peek() == ']') { i++; break; }
            fail("expected ',' or ']' in the array");
        }
        return Value::Array(items);
    }

    Value inlineTable() {
        i++; // {
        Value table = Value::Dict({});
        skipSpaces();
        if (peek() == '}') { i++; return table; }
        while (true) {
            skipSpaces();
            std::vector<std::string> path = keyPath();
            skipSpaces();
            if (peek() != '=') fail("expected '=' after the key");
            i++;
            assign(table.dict, path, value());
            skipSpaces();
            if (peek() == ',') { i++; continue; }
            if (peek() == '}') { i++; break; }
            fail("expected ',' or '}' in the inline table");
        }
        return table;
    }

    // The table named by `path` under `root`, creating the missing ones.
    Table descend(Table root, const std::vector<std::string>& path, size_t count) {
        Table cur = root;
        for (size_t k = 0; k < count; k++) {
            Value* next = nullptr;
            for (auto& kv : *cur) if (kv.first == path[k]) { next = &kv.second; break; }
            if (!next) {
                cur->push_back({path[k], Value::Dict({})});
                next = &cur->back().second;
            }
            if (next->type != ValueType::DICT) fail("'" + path[k] + "' is already defined as a value, not a table");
            cur = next->dict;
        }
        return cur;
    }

    void assign(Table table, const std::vector<std::string>& path, Value v) {
        Table parent = descend(table, path, path.size() - 1);
        for (auto& kv : *parent) if (kv.first == path.back()) fail("the key '" + path.back() + "' is defined twice");
        parent->push_back({path.back(), v});
    }

    void endOfStatement() {
        skipSpaces();
        skipComment();
        if (atEnd()) return;
        if (peek() == '\r') i++;
        if (peek() != '\n') fail("unexpected text after the value");
    }

    Value document() {
        Value root = Value::Dict({});
        Table current = root.dict;
        while (true) {
            skipBlank();
            if (atEnd()) break;
            if (peek() == '[') {
                if (peek(1) == '[') fail("[[arrays of tables]] are not supported");
                i++;
                skipSpaces();
                std::vector<std::string> path = keyPath();
                skipSpaces();
                if (peek() != ']') fail("expected ']' to close the table name");
                i++;
                current = descend(root.dict, path, path.size());
                endOfStatement();
                continue;
            }
            std::vector<std::string> path = keyPath();
            skipSpaces();
            if (peek() != '=') fail("expected '=' after the key '" + path.back() + "'");
            i++;
            assign(current, path, value());
            endOfStatement();
        }
        return root;
    }
};

} // namespace

Value parseToml(const std::string& text, const std::string& what) {
    Reader r(text, what);
    return r.document();
}
