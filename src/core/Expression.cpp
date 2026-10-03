#include "core/Expression.h"

#include <cmath>
#include <cstdlib>
#include <functional>

namespace keyflow {

namespace {

/// A deterministic value-noise source.
///
/// `wiggle` needs a smooth pseudo-random signal that is stable for a given
/// (seed, time) pair. A hash of the quantised time gives exactly that: the
/// value is constant while the playhead is parked, changes smoothly as it
/// moves, and is identical on every render of the same frame.
double hashNoise(unsigned seed, double t)
{
    // Quantise to the noise period so the signal has a wavelength rather than
    // being white noise, which would just look like a flicker.
    const double cell = std::floor(t);
    const double frac = t - cell;

    auto hash = [seed](double n) {
        unsigned h = seed ^ static_cast<unsigned>(static_cast<long long>(n) * 374761393u);
        h = (h ^ (h >> 13)) * 1274126177u;
        h = h ^ (h >> 16);
        return static_cast<double>(h % 100000u) / 100000.0 * 2.0 - 1.0;
    };

    // Smoothstep between neighbouring cells, so the signal has no corners.
    const double a = hash(cell);
    const double b = hash(cell + 1.0);
    const double s = frac * frac * (3.0 - 2.0 * frac);
    return a + (b - a) * s;
}

/// Bounds. An expression that loops or recurses past these is rejected rather
/// than allowed to consume the render thread.
constexpr int kMaxSteps = 100000;
constexpr int kMaxDepth = 32;

class Evaluator
{
public:
    Evaluator(const ExpressionContext& context) : _ctx(context) {}

    ExpressionResult run(const std::string& source)
    {
        _src = source;
        _i = 0;
        _steps = 0;
        _error.clear();

        const double value = parseExpression(0);
        if (!_error.empty()) {
            return {0.0, false, _error};
        }
        skipSpace();
        if (_i != _src.size()) {
            return {0.0, false, "unexpected '" + _src.substr(_i, 1) + "' at position "
                                + std::to_string(_i)};
        }
        if (std::isnan(value) || std::isinf(value)) {
            return {0.0, false, "the expression evaluated to a non-finite number"};
        }
        return {value, true, {}};
    }

private:
    bool fail(const std::string& what)
    {
        if (_error.empty()) {
            _error = what + " at position " + std::to_string(_i);
        }
        return false;
    }

    bool budget()
    {
        if (++_steps > kMaxSteps) {
            fail("the expression is too complex");
            return false;
        }
        return true;
    }

    void skipSpace()
    {
        while (_i < _src.size()) {
            const char c = _src[_i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++_i;
            else break;
        }
    }

    bool match(char c)
    {
        skipSpace();
        if (_i < _src.size() && _src[_i] == c) {
            ++_i;
            return true;
        }
        return false;
    }

    bool matchWord(const char* word)
    {
        skipSpace();
        const std::size_t n = std::char_traits<char>::length(word);
        if (_src.compare(_i, n, word) != 0) return false;
        // Reject a prefix of a longer identifier: `timeout` is not `time`.
        if (_i + n < _src.size()) {
            const char next = _src[_i + n];
            if (std::isalnum(static_cast<unsigned char>(next)) || next == '_') return false;
        }
        _i += n;
        return true;
    }

    // --- grammar ----------------------------------------------------------
    //
    // expression := ternary
    // ternary    := or ( '?' ternary ':' ternary )?
    // or         := and ( '||' and )*
    // and        := compare ( '&&' compare )*
    // compare    := add ( ('<'|'>'|'<='|'>='|'=='|'!=') add )*
    // add        := mul ( ('+'|'-') mul )*
    // mul        := unary ( ('*'|'/'|'%') unary )*
    // unary      := ('-'|'+'|'!') unary | power
    // power      := postfix ( '^' unary )?
    // postfix    := primary ( '[' expression ']' | '.' identifier )*
    // primary    := number | identifier | call | '(' expression ')'

    double parseExpression(int depth)
    {
        if (depth > kMaxDepth) { fail("nested too deeply"); return 0.0; }
        return parseTernary(depth);
    }

    double parseTernary(int depth)
    {
        const double condition = parseOr(depth);
        if (!_error.empty()) return 0.0;
        skipSpace();
        if (_i < _src.size() && _src[_i] == '?') {
            ++_i;
            const double a = parseTernary(depth + 1);
            if (!match(':')) { fail("expected ':' in the conditional"); return 0.0; }
            const double b = parseTernary(depth + 1);
            return condition != 0.0 ? a : b;
        }
        return condition;
    }

    double parseOr(int depth)
    {
        double left = parseAnd(depth);
        for (;;) {
            skipSpace();
            if (_src.compare(_i, 2, "||") == 0) {
                _i += 2;
                const double right = parseAnd(depth);
                left = (left != 0.0 || right != 0.0) ? 1.0 : 0.0;
            } else {
                return left;
            }
        }
    }

    double parseAnd(int depth)
    {
        double left = parseCompare(depth);
        for (;;) {
            skipSpace();
            if (_src.compare(_i, 2, "&&") == 0) {
                _i += 2;
                const double right = parseCompare(depth);
                left = (left != 0.0 && right != 0.0) ? 1.0 : 0.0;
            } else {
                return left;
            }
        }
    }

    double parseCompare(int depth)
    {
        double left = parseAdd(depth);
        for (;;) {
            skipSpace();
            if (_src.compare(_i, 2, "<=") == 0)      { _i += 2; left = left <= parseAdd(depth) ? 1.0 : 0.0; }
            else if (_src.compare(_i, 2, ">=") == 0) { _i += 2; left = left >= parseAdd(depth) ? 1.0 : 0.0; }
            else if (_src.compare(_i, 2, "==") == 0) { _i += 2; left = left == parseAdd(depth) ? 1.0 : 0.0; }
            else if (_src.compare(_i, 2, "!=") == 0) { _i += 2; left = left != parseAdd(depth) ? 1.0 : 0.0; }
            else if (_i < _src.size() && _src[_i] == '<') { ++_i; left = left < parseAdd(depth) ? 1.0 : 0.0; }
            else if (_i < _src.size() && _src[_i] == '>') { ++_i; left = left > parseAdd(depth) ? 1.0 : 0.0; }
            else return left;
        }
    }

    double parseAdd(int depth)
    {
        double left = parseMul(depth);
        for (;;) {
            skipSpace();
            if (_i < _src.size() && _src[_i] == '+') { ++_i; left += parseMul(depth); }
            else if (_i < _src.size() && _src[_i] == '-') { ++_i; left -= parseMul(depth); }
            else return left;
        }
    }

    double parseMul(int depth)
    {
        double left = parseUnary(depth);
        for (;;) {
            skipSpace();
            if (_i < _src.size() && _src[_i] == '*') { ++_i; left *= parseUnary(depth); }
            else if (_i < _src.size() && _src[_i] == '/') {
                ++_i;
                const double right = parseUnary(depth);
                // Division by zero yields zero rather than an error: an
                // expression that divides by a keyframed value passes through
                // zero mid-animation, and failing the whole expression for
                // that one frame would flash the parameter to its fallback.
                left = (right == 0.0) ? 0.0 : left / right;
            } else if (_i < _src.size() && _src[_i] == '%') {
                ++_i;
                const double right = parseUnary(depth);
                left = (right == 0.0) ? 0.0 : std::fmod(left, right);
            } else return left;
        }
    }

    double parseUnary(int depth)
    {
        if (!budget()) return 0.0;
        skipSpace();
        if (_i < _src.size() && _src[_i] == '-') { ++_i; return -parseUnary(depth + 1); }
        if (_i < _src.size() && _src[_i] == '+') { ++_i; return parseUnary(depth + 1); }
        if (_i < _src.size() && _src[_i] == '!') { ++_i; return parseUnary(depth + 1) == 0.0 ? 1.0 : 0.0; }
        return parsePower(depth);
    }

    double parsePower(int depth)
    {
        const double base = parsePostfix(depth);
        skipSpace();
        if (_i < _src.size() && _src[_i] == '^') {
            ++_i;
            const double exponent = parseUnary(depth + 1);
            // pow() of a negative base with a fractional exponent is NaN in
            // C. Guarding here rather than at the top keeps the error message
            // useful.
            if (base < 0.0 && std::floor(exponent) != exponent) {
                fail("a negative base with a fractional exponent is undefined");
                return 0.0;
            }
            return std::pow(base, exponent);
        }
        return base;
    }

    double parsePostfix(int depth)
    {
        double value = parsePrimary(depth);
        for (;;) {
            skipSpace();
            if (_i < _src.size() && _src[_i] == '[') {
                ++_i;
                const double index = parseExpression(depth + 1);
                if (!match(']')) { fail("expected ']'"); return 0.0; }
                // Indexing a scalar: [0] is the value, anything else is zero.
                // Real vector indexing arrives when the model carries vectors;
                // until then this keeps `position[0]` evaluating rather than
                // failing, which is what an expression copied from a tutorial
                // will contain.
                if (index != 0.0) value = 0.0;
            } else if (_i < _src.size() && _src[_i] == '.') {
                ++_i;
                skipSpace();
                const std::string member = parseIdentifier();
                if (member.empty()) { fail("expected a name after '.'"); return 0.0; }
                value = memberValue(member, value);
            } else {
                return value;
            }
        }
    }

    /// Resolve `x.member`. Only a small set of members exists, and an unknown
    /// one yields zero rather than failing, so an expression written against
    /// a newer build still produces a number.
    double memberValue(const std::string& member, double base)
    {
        if (member == "x") return _ctx.positionX;
        if (member == "y") return _ctx.positionY;
        if (member == "width") return _ctx.compWidth;
        if (member == "height") return _ctx.compHeight;
        if (member == "value") return base;
        if (member == "time") return _ctx.time;
        return 0.0;
    }

    std::string parseIdentifier()
    {
        skipSpace();
        const std::size_t start = _i;
        while (_i < _src.size()) {
            const char c = _src[_i];
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') ++_i;
            else break;
        }
        return _src.substr(start, _i - start);
    }

    double parsePrimary(int depth)
    {
        if (!budget()) return 0.0;
        if (depth > kMaxDepth) { fail("nested too deeply"); return 0.0; }
        skipSpace();

        if (_i >= _src.size()) {
            fail("unexpected end of the expression");
            return 0.0;
        }

        // A number. Handles 1, 1.5, .5 and 1e3.
        if (std::isdigit(static_cast<unsigned char>(_src[_i])) || _src[_i] == '.') {
            const char* begin = _src.c_str() + _i;
            char* end = nullptr;
            const double value = std::strtod(begin, &end);
            if (end == begin) { fail("expected a number"); return 0.0; }
            _i += static_cast<std::size_t>(end - begin);
            return value;
        }

        if (_src[_i] == '(') {
            ++_i;
            const double value = parseExpression(depth + 1);
            if (!match(')')) { fail("expected ')'"); return 0.0; }
            return value;
        }

        const std::string name = parseIdentifier();
        if (name.empty()) {
            fail("expected a value");
            return 0.0;
        }

        skipSpace();
        if (_i < _src.size() && _src[_i] == '(') {
            ++_i;
            std::vector<double> args;
            skipSpace();
            if (_i < _src.size() && _src[_i] == ')') {
                ++_i;
            } else {
                for (;;) {
                    args.push_back(parseExpression(depth + 1));
                    if (!_error.empty()) return 0.0;
                    if (match(',')) continue;
                    if (match(')')) break;
                    fail("expected ',' or ')' in the argument list");
                    return 0.0;
                }
            }
            return callFunction(name, args);
        }

        return variable(name);
    }

    double variable(const std::string& name)
    {
        if (name == "time") return _ctx.time;
        if (name == "frame") return static_cast<double>(_ctx.frame);
        if (name == "fps") return _ctx.fps;
        if (name == "value") return _ctx.value;
        if (name == "pi" || name == "PI") return 3.14159265358979323846;
        if (name == "e") return 2.71828182845904523536;
        if (name == "thisLayer" || name == "thisComp") {
            // The object itself: its members are reached through `.`, and
            // using it bare yields the layer's own value.
            return _ctx.value;
        }
        if (name == "compWidth") return _ctx.compWidth;
        if (name == "compHeight") return _ctx.compHeight;
        if (name == "layerIndex") return static_cast<double>(_ctx.layerIndex);
        if (name == "positionX") return _ctx.positionX;
        if (name == "positionY") return _ctx.positionY;
        if (name == "scaleX") return _ctx.scaleX;
        if (name == "scaleY") return _ctx.scaleY;
        if (name == "rotation") return _ctx.rotation;
        if (name == "opacity") return _ctx.opacity;
        if (name == "seed") return static_cast<double>(_ctx.seed);

        const auto it = _ctx.named.find(name);
        if (it != _ctx.named.end()) return it->second;

        // An unknown name is an error, not zero. Silently returning zero turns
        // a typo into a parameter that stops moving, which is far harder to
        // find than a message saying the name is not known.
        fail("unknown name '" + name + "'");
        return 0.0;
    }

    double callFunction(const std::string& name, const std::vector<double>& args)
    {
        auto arg = [&](std::size_t i, double fallback) {
            return i < args.size() ? args[i] : fallback;
        };

        if (name == "sin")   return std::sin(arg(0, 0.0));
        if (name == "cos")   return std::cos(arg(0, 0.0));
        if (name == "tan")   return std::tan(arg(0, 0.0));
        if (name == "asin")  return std::asin(arg(0, 0.0));
        if (name == "acos")  return std::acos(arg(0, 0.0));
        if (name == "atan")  return std::atan(arg(0, 0.0));
        if (name == "atan2") return std::atan2(arg(0, 0.0), arg(1, 1.0));
        if (name == "sqrt")  return std::sqrt(std::max(0.0, arg(0, 0.0)));
        if (name == "abs")   return std::fabs(arg(0, 0.0));
        if (name == "floor") return std::floor(arg(0, 0.0));
        if (name == "ceil")  return std::ceil(arg(0, 0.0));
        if (name == "round") return std::floor(arg(0, 0.0) + 0.5);
        if (name == "sign")  return arg(0, 0.0) > 0.0 ? 1.0 : (arg(0, 0.0) < 0.0 ? -1.0 : 0.0);
        if (name == "exp")   return std::exp(arg(0, 0.0));
        if (name == "log")   return arg(0, 0.0) > 0.0 ? std::log(arg(0, 0.0)) : 0.0;
        if (name == "log10") return arg(0, 0.0) > 0.0 ? std::log10(arg(0, 0.0)) : 0.0;
        if (name == "pow")   return std::pow(arg(0, 0.0), arg(1, 1.0));
        if (name == "min")   return std::min(arg(0, 0.0), arg(1, 0.0));
        if (name == "max")   return std::max(arg(0, 0.0), arg(1, 0.0));
        if (name == "clamp") {
            const double v = arg(0, 0.0);
            return std::min(std::max(v, arg(1, 0.0)), arg(2, 1.0));
        }
        if (name == "lerp" || name == "mix") {
            const double t = arg(2, 0.0);
            return arg(0, 0.0) + (arg(1, 0.0) - arg(0, 0.0)) * t;
        }
        if (name == "mod")   return arg(1, 1.0) == 0.0 ? 0.0 : std::fmod(arg(0, 0.0), arg(1, 1.0));
        if (name == "radians") return arg(0, 0.0) * 3.14159265358979323846 / 180.0;
        if (name == "degrees") return arg(0, 0.0) * 180.0 / 3.14159265358979323846;

        // The temporal functions. `wiggle(frequency, amplitude)` is the one
        // every motion-graphics expression uses, and it has to be stable for a
        // parked playhead or the preview flickers.
        if (name == "wiggle") {
            const double frequency = arg(0, 1.0);
            const double amplitude = arg(1, 10.0);
            return hashNoise(_ctx.seed, _ctx.time * frequency) * amplitude;
        }
        if (name == "noise") {
            return hashNoise(_ctx.seed, arg(0, _ctx.time));
        }
        if (name == "random") {
            // Seeded by the frame, so it is random-looking but reproducible.
            const double t = static_cast<double>(_ctx.frame);
            return (hashNoise(_ctx.seed ^ 0x9E3779B9u, t) + 1.0) * 0.5;
        }
        if (name == "smoothstep") {
            const double edge0 = arg(0, 0.0);
            const double edge1 = arg(1, 1.0);
            const double x = arg(2, 0.0);
            if (edge1 == edge0) return x < edge0 ? 0.0 : 1.0;
            const double t = std::min(std::max((x - edge0) / (edge1 - edge0), 0.0), 1.0);
            return t * t * (3.0 - 2.0 * t);
        }

        fail("unknown function '" + name + "'");
        return 0.0;
    }

    const ExpressionContext& _ctx;
    std::string _src;
    std::size_t _i = 0;
    int _steps = 0;
    std::string _error;
};

} // namespace

ExpressionResult evaluateExpression(const std::string& source,
                                    const ExpressionContext& context)
{
    if (source.empty()) {
        return {0.0, false, "the expression is empty"};
    }
    Evaluator evaluator(context);
    return evaluator.run(source);
}

ExpressionResult checkExpression(const std::string& source)
{
    // Parsing is the check. The context is a plain default, so an expression
    // that reads a name the default context does not carry reports it — which
    // is what the editor wants to show while the field is being typed.
    ExpressionContext context;
    return evaluateExpression(source, context);
}

} // namespace keyflow
