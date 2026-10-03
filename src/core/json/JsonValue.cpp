#include "core/json/JsonValue.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace keyflow::json {

namespace {

const Value kNull;

/// Serialise a double so a round-trip preserves the value exactly.
///
/// %.17g is the shortest format guaranteed to read back as the same double.
/// Using %g instead loses precision on values like 0.1 and the project file
/// then drifts a little on every save.
std::string formatNumber(double n)
{
    if (std::isnan(n) || std::isinf(n)) {
        // JSON has no NaN or Infinity. Emitting them produces a file that no
        // parser accepts, so they become 0. The alternative — failing the save
        // — would lose the whole project over one bad keyframe.
        return "0";
    }
    // Integers print without a decimal point. 1920 rather than 1920.0 keeps
    // project files readable and diffs meaningful.
    if (n == std::floor(n) && std::fabs(n) < 1e15) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(n));
        return buf;
    }
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.17g", n);
    return buf;
}

void escapeInto(std::string& out, const std::string& s)
{
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                // UTF-8 passes through untouched. The file is UTF-8 and the
                // page is UTF-8, so re-encoding here would only be a chance to
                // get it wrong.
                out.push_back(static_cast<char>(c));
            }
        }
    }
    out.push_back('"');
}

void writeIndent(std::string& out, int depth)
{
    out.push_back('\n');
    out.append(static_cast<std::size_t>(depth) * 2, ' ');
}

void writeValue(std::string& out, const Value& v, int indent, int depth);

void writeValue(std::string& out, const Value& v, int indent, int depth)
{
    switch (v.type()) {
    case Type::Null:
        out += "null";
        break;
    case Type::Bool:
        out += v.asBool() ? "true" : "false";
        break;
    case Type::Number:
        out += formatNumber(v.asNumber());
        break;
    case Type::String:
        escapeInto(out, v.asString());
        break;
    case Type::Array: {
        const Array& a = v.asArray();
        if (a.empty()) {
            out += "[]";
            break;
        }
        out.push_back('[');
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (i) out.push_back(',');
            if (indent) writeIndent(out, depth + 1);
            writeValue(out, a[i], indent, depth + 1);
        }
        if (indent) writeIndent(out, depth);
        out.push_back(']');
        break;
    }
    case Type::Object: {
        const Object& o = v.asObject();
        if (o.empty()) {
            out += "{}";
            break;
        }
        out.push_back('{');
        for (std::size_t i = 0; i < o.size(); ++i) {
            if (i) out.push_back(',');
            if (indent) writeIndent(out, depth + 1);
            escapeInto(out, o[i].first);
            out.push_back(':');
            if (indent) out.push_back(' ');
            writeValue(out, o[i].second, indent, depth + 1);
        }
        if (indent) writeIndent(out, depth);
        out.push_back('}');
        break;
    }
    }
}

// --- parser ---------------------------------------------------------------

/// A recursive-descent parser over the text.
///
/// Depth-limited: a project file is nested maybe eight deep, and a hostile or
/// corrupt file that nests ten thousand deep would otherwise blow the stack.
/// A limit turns that into a parse error, which the loader already handles.
class Parser
{
public:
    Parser(const std::string& text, std::string& error)
        : _s(text), _error(error) {}

    bool run(Value& out)
    {
        skipSpace();
        if (!parseValue(out, 0)) return false;
        skipSpace();
        if (_i != _s.size()) return fail("trailing content after the top-level value");
        return true;
    }

private:
    static constexpr int kMaxDepth = 64;

    bool fail(const std::string& what)
    {
        if (_error.empty()) {
            std::ostringstream os;
            os << "at byte " << _i << ": " << what;
            _error = os.str();
        }
        return false;
    }

    void skipSpace()
    {
        while (_i < _s.size()) {
            const char c = _s[_i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++_i;
            } else {
                break;
            }
        }
    }

    bool literal(const char* word)
    {
        const std::size_t n = std::char_traits<char>::length(word);
        if (_s.compare(_i, n, word) != 0) return false;
        _i += n;
        return true;
    }

    bool parseValue(Value& out, int depth)
    {
        if (depth > kMaxDepth) return fail("nested too deeply");
        if (_i >= _s.size()) return fail("unexpected end of input");

        const char c = _s[_i];
        switch (c) {
        case '{': return parseObject(out, depth);
        case '[': return parseArray(out, depth);
        case '"': {
            std::string s;
            if (!parseString(s)) return false;
            out = Value(std::move(s));
            return true;
        }
        case 't':
            if (literal("true")) { out = Value(true); return true; }
            return fail("expected true");
        case 'f':
            if (literal("false")) { out = Value(false); return true; }
            return fail("expected false");
        case 'n':
            if (literal("null")) { out = Value(); return true; }
            return fail("expected null");
        default:
            return parseNumber(out);
        }
    }

    bool parseObject(Value& out, int depth)
    {
        ++_i; // '{'
        Object o;
        skipSpace();
        if (_i < _s.size() && _s[_i] == '}') {
            ++_i;
            out = Value(std::move(o));
            return true;
        }
        for (;;) {
            skipSpace();
            if (_i >= _s.size() || _s[_i] != '"') return fail("expected a key string");
            std::string key;
            if (!parseString(key)) return false;
            skipSpace();
            if (_i >= _s.size() || _s[_i] != ':') return fail("expected ':' after the key");
            ++_i;
            skipSpace();
            Value v;
            if (!parseValue(v, depth + 1)) return false;
            o.emplace_back(std::move(key), std::move(v));
            skipSpace();
            if (_i >= _s.size()) return fail("unterminated object");
            if (_s[_i] == ',') { ++_i; continue; }
            if (_s[_i] == '}') { ++_i; break; }
            return fail("expected ',' or '}'");
        }
        out = Value(std::move(o));
        return true;
    }

    bool parseArray(Value& out, int depth)
    {
        ++_i; // '['
        Array a;
        skipSpace();
        if (_i < _s.size() && _s[_i] == ']') {
            ++_i;
            out = Value(std::move(a));
            return true;
        }
        for (;;) {
            skipSpace();
            Value v;
            if (!parseValue(v, depth + 1)) return false;
            a.push_back(std::move(v));
            skipSpace();
            if (_i >= _s.size()) return fail("unterminated array");
            if (_s[_i] == ',') { ++_i; continue; }
            if (_s[_i] == ']') { ++_i; break; }
            return fail("expected ',' or ']'");
        }
        out = Value(std::move(a));
        return true;
    }

    /// Append a code point as UTF-8.
    static void appendUtf8(std::string& out, unsigned cp)
    {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool parseHex4(unsigned& out)
    {
        if (_i + 4 > _s.size()) return fail("truncated \\u escape");
        unsigned v = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = _s[_i + static_cast<std::size_t>(k)];
            v <<= 4;
            if (c >= '0' && c <= '9')      v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
            else return fail("bad hex digit in \\u escape");
        }
        _i += 4;
        out = v;
        return true;
    }

    bool parseString(std::string& out)
    {
        ++_i; // opening quote
        out.clear();
        for (;;) {
            if (_i >= _s.size()) return fail("unterminated string");
            const unsigned char c = static_cast<unsigned char>(_s[_i]);
            if (c == '"') {
                ++_i;
                return true;
            }
            if (c == '\\') {
                ++_i;
                if (_i >= _s.size()) return fail("unterminated escape");
                const char e = _s[_i++];
                switch (e) {
                case '"':  out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/'); break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'u': {
                    unsigned cp = 0;
                    if (!parseHex4(cp)) return false;
                    // A surrogate pair is two escapes that name one code point.
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (_i + 1 < _s.size() && _s[_i] == '\\' && _s[_i + 1] == 'u') {
                            _i += 2;
                            unsigned lo = 0;
                            if (!parseHex4(lo)) return false;
                            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            } else {
                                // A lone high surrogate: emit the replacement
                                // character rather than dropping the string.
                                cp = 0xFFFD;
                            }
                        } else {
                            cp = 0xFFFD;
                        }
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default:
                    return fail("unknown escape");
                }
                continue;
            }
            // Control characters must be escaped in JSON. Accepting them
            // silently would let a stray newline inside a string split the
            // value in two on the next save.
            if (c < 0x20) return fail("unescaped control character in string");
            out.push_back(static_cast<char>(c));
            ++_i;
        }
    }

    bool parseNumber(Value& out)
    {
        const std::size_t start = _i;
        if (_i < _s.size() && (_s[_i] == '-' || _s[_i] == '+')) ++_i;
        bool anyDigit = false;
        while (_i < _s.size() && _s[_i] >= '0' && _s[_i] <= '9') { ++_i; anyDigit = true; }
        if (_i < _s.size() && _s[_i] == '.') {
            ++_i;
            while (_i < _s.size() && _s[_i] >= '0' && _s[_i] <= '9') { ++_i; anyDigit = true; }
        }
        if (!anyDigit) return fail("expected a value");
        if (_i < _s.size() && (_s[_i] == 'e' || _s[_i] == 'E')) {
            ++_i;
            if (_i < _s.size() && (_s[_i] == '-' || _s[_i] == '+')) ++_i;
            bool expDigit = false;
            while (_i < _s.size() && _s[_i] >= '0' && _s[_i] <= '9') { ++_i; expDigit = true; }
            if (!expDigit) return fail("exponent has no digits");
        }
        const std::string text = _s.substr(start, _i - start);
        out = Value(std::strtod(text.c_str(), nullptr));
        return true;
    }

    const std::string& _s;
    std::string& _error;
    std::size_t _i = 0;
};

} // namespace

const Value& Value::operator[](const std::string& key) const
{
    if (_type == Type::Object) {
        for (const auto& [k, v] : _object) {
            if (k == key) return v;
        }
    }
    return kNull;
}

Value& Value::operator[](const std::string& key)
{
    if (_type != Type::Object) {
        _type = Type::Object;
        _object.clear();
    }
    for (auto& [k, v] : _object) {
        if (k == key) return v;
    }
    _object.emplace_back(key, Value());
    return _object.back().second;
}

const Value& Value::operator[](std::size_t index) const
{
    if (_type == Type::Array && index < _array.size()) return _array[index];
    return kNull;
}

bool Value::has(const std::string& key) const
{
    if (_type != Type::Object) return false;
    for (const auto& [k, v] : _object) {
        if (k == key) return true;
    }
    return false;
}

void Value::set(const std::string& key, Value v)
{
    if (_type != Type::Object) {
        _type = Type::Object;
        _object.clear();
    }
    for (auto& [k, existing] : _object) {
        if (k == key) {
            existing = std::move(v);
            return;
        }
    }
    _object.emplace_back(key, std::move(v));
}

void Value::remove(const std::string& key)
{
    if (_type != Type::Object) return;
    for (auto it = _object.begin(); it != _object.end(); ++it) {
        if (it->first == key) {
            _object.erase(it);
            return;
        }
    }
}

void Value::push(Value v)
{
    if (_type != Type::Array) {
        _type = Type::Array;
        _array.clear();
    }
    _array.push_back(std::move(v));
}

std::size_t Value::size() const
{
    if (_type == Type::Array) return _array.size();
    if (_type == Type::Object) return _object.size();
    return 0;
}

Value parse(const std::string& text, std::string* error)
{
    std::string localError;
    std::string& err = error ? *error : localError;
    err.clear();

    Value out;
    Parser p(text, err);
    if (!p.run(out)) return Value();
    return out;
}

std::string write(const Value& value, int indent)
{
    std::string out;
    // A rough reserve. Most project files are tens of kilobytes; growing from
    // zero costs a handful of reallocations and reserving wildly over-shoots.
    out.reserve(1024);
    writeValue(out, value, indent, 0);
    if (indent) out.push_back('\n');
    return out;
}

} // namespace keyflow::json
