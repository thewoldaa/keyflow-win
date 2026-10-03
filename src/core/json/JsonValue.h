// ---------------------------------------------------------------------------
// A small JSON value type.
//
// The project format is JSON and the page protocol is JSON, so this is the
// one type both sides agree on. It is deliberately not a general-purpose
// library: it parses and writes what a project file contains, it preserves
// key order (so saving a project does not reorder the file and produce a
// meaningless diff), and it reports parse errors with a position rather than
// throwing an exception nobody catches.
//
// Numbers are stored as double. A composition is 1920x1080 and a keyframe is
// 1.25, so double is more than enough precision, and it keeps the page's
// numbers and the file's numbers the same kind of thing.
// ---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace keyflow::json {

class Value;

/// An ordered object. std::map would sort keys; a project file that reorders
/// itself on every save is a file nobody can review.
using Object = std::vector<std::pair<std::string, Value>>;

/// An array.
using Array = std::vector<Value>;

/// What a Value currently holds.
enum class Type
{
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

/// A JSON value.
///
/// Copyable and assignable, because the model copies these around freely when
/// building snapshots for the page and the undo stack.
class Value
{
public:
    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : _type(Type::Bool), _bool(b) {}
    Value(double n) : _type(Type::Number), _number(n) {}
    Value(int n) : _type(Type::Number), _number(static_cast<double>(n)) {}
    Value(std::int64_t n) : _type(Type::Number), _number(static_cast<double>(n)) {}
    Value(const char* s) : _type(Type::String), _string(s) {}
    Value(std::string s) : _type(Type::String), _string(std::move(s)) {}
    Value(Array a) : _type(Type::Array), _array(std::move(a)) {}
    Value(Object o) : _type(Type::Object), _object(std::move(o)) {}

    Type type() const { return _type; }
    bool isNull() const { return _type == Type::Null; }
    bool isBool() const { return _type == Type::Bool; }
    bool isNumber() const { return _type == Type::Number; }
    bool isString() const { return _type == Type::String; }
    bool isArray() const { return _type == Type::Array; }
    bool isObject() const { return _type == Type::Object; }

    /// Scalar accessors. Each returns the fallback when the value holds
    /// something else, so a malformed project file produces a default rather
    /// than a crash. A file that is wrong should open with pieces missing, not
    /// take the editor down.
    bool asBool(bool fallback = false) const
    {
        return _type == Type::Bool ? _bool : fallback;
    }
    double asNumber(double fallback = 0.0) const
    {
        return _type == Type::Number ? _number : fallback;
    }
    std::string asString(const std::string& fallback = {}) const
    {
        return _type == Type::String ? _string : fallback;
    }

    /// Array access. An empty array is returned for a non-array so callers can
    /// iterate without checking.
    const Array& asArray() const { return _array; }
    Array& asArray() { return _array; }

    /// Object access.
    const Object& asObject() const { return _object; }
    Object& asObject() { return _object; }

    /// Look up a key. Returns a null Value when absent, which makes chained
    /// reads (`root["a"]["b"].asNumber()`) safe.
    const Value& operator[](const std::string& key) const;
    Value& operator[](const std::string& key);

    /// Array element. Out-of-range returns a null Value.
    const Value& operator[](std::size_t index) const;

    /// True when the key is present, regardless of its value.
    bool has(const std::string& key) const;

    /// Set a key, replacing it in place so the order is stable.
    void set(const std::string& key, Value v);

    /// Remove a key. No-op when absent.
    void remove(const std::string& key);

    /// Append to an array, converting this value to an array first if needed.
    void push(Value v);

    /// Number of array elements or object keys. Zero for scalars.
    std::size_t size() const;

private:
    Type _type = Type::Null;
    bool _bool = false;
    double _number = 0.0;
    std::string _string;
    Array _array;
    Object _object;
};

/// Parse JSON text.
///
/// On failure returns a null Value and fills `error` with a message naming the
/// byte offset. Never throws: the caller is a message handler or a file
/// loader, and neither has anywhere useful to propagate an exception to.
Value parse(const std::string& text, std::string* error = nullptr);

/// Serialise a value. `indent` of zero produces a single line.
std::string write(const Value& value, int indent = 0);

} // namespace keyflow::json
