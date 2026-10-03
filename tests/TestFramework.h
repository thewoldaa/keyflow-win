// ---------------------------------------------------------------------------
// A test framework small enough to read in one sitting.
//
// A macro registers a function; main runs them all, prints what failed and
// returns non-zero if anything did. No fixtures, no parameterised cases, no
// assertion library — the tests in this project are all of the form "given
// this input, is the output right", and a framework that supports more than
// that would be more code to maintain than the tests it runs.
// ---------------------------------------------------------------------------

#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace keyflow::test {

struct Case
{
    std::string name;
    std::function<void()> run;
};

/// The registry. A function-local static so registration order across
/// translation units is well-defined: the first test to run constructs it, and
/// everything after appends.
inline std::vector<Case>& registry()
{
    static std::vector<Case> cases;
    return cases;
}

/// Set by a failing assertion; checked after each case.
inline bool& failed()
{
    static bool flag = false;
    return flag;
}

/// Set when a case should stop. A failed CHECK does not stop the case — the
/// remaining assertions still run, which usually tells you more about what
/// broke than the first one does. A failed REQUIRE does stop it.
inline bool& aborted()
{
    static bool flag = false;
    return flag;
}

struct Registrar
{
    Registrar(const char* name, std::function<void()> fn)
    {
        registry().push_back({name, std::move(fn)});
    }
};

/// Format a value for a failure message. Overloads rather than a template so
/// the common types print readably and a type without an overload still
/// compiles through the fallback.
inline std::string describe(bool v) { return v ? "true" : "false"; }
inline std::string describe(const std::string& v) { return '"' + v + '"'; }
inline std::string describe(const char* v) { return v ? std::string("\"") + v + "\"" : "(null)"; }
inline std::string describe(double v)
{
    std::ostringstream os;
    os.precision(17);
    os << v;
    return os.str();
}
inline std::string describe(int v) { return std::to_string(v); }
inline std::string describe(long long v) { return std::to_string(v); }
inline std::string describe(unsigned long v) { return std::to_string(v); }
inline std::string describe(std::size_t v) { return std::to_string(v); }

template <typename T>
std::string describe(const T&)
{
    return "(unprintable)";
}

inline void report(const char* file, int line, const std::string& message)
{
    std::cout << "    " << file << ":" << line << ": " << message << "\n";
    failed() = true;
}

/// Floating-point comparison with a tolerance. Exact equality on doubles is
/// almost never what a test means, and a test that fails on the last bit is a
/// test that gets deleted.
inline bool nearlyEqual(double a, double b, double epsilon = 1e-9)
{
    if (std::isnan(a) || std::isnan(b)) return false;
    const double diff = std::fabs(a - b);
    if (diff <= epsilon) return true;
    return diff <= epsilon * std::max(std::fabs(a), std::fabs(b));
}

} // namespace keyflow::test

// --- macros ---------------------------------------------------------------

#define KF_CONCAT_INNER(a, b) a##b
#define KF_CONCAT(a, b) KF_CONCAT_INNER(a, b)

/// Define a test case.
#define TEST(name)                                                             \
    static void KF_CONCAT(kf_test_, __LINE__)();                               \
    static ::keyflow::test::Registrar KF_CONCAT(kf_registrar_, __LINE__)(      \
        name, &KF_CONCAT(kf_test_, __LINE__));                                 \
    static void KF_CONCAT(kf_test_, __LINE__)()

/// Assert a condition. Records a failure and continues.
#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ::keyflow::test::report(__FILE__, __LINE__, "CHECK failed: " #cond); \
        }                                                                      \
    } while (false)

/// Assert two values are equal. Records a failure and continues.
#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        const auto kf_a = (a);                                                 \
        const auto kf_b = (b);                                                 \
        if (!(kf_a == kf_b)) {                                                 \
            ::keyflow::test::report(                                           \
                __FILE__, __LINE__,                                            \
                std::string("CHECK_EQ failed: " #a " == " #b "\n      left:  ") \
                    + ::keyflow::test::describe(kf_a) + "\n      right: "      \
                    + ::keyflow::test::describe(kf_b));                        \
        }                                                                      \
    } while (false)

/// Assert two doubles are equal within a tolerance.
#define CHECK_NEAR(a, b, eps)                                                  \
    do {                                                                       \
        const double kf_a = (a);                                               \
        const double kf_b = (b);                                               \
        if (!::keyflow::test::nearlyEqual(kf_a, kf_b, eps)) {                  \
            ::keyflow::test::report(                                           \
                __FILE__, __LINE__,                                            \
                std::string("CHECK_NEAR failed: " #a " ~= " #b "\n      left:  ") \
                    + ::keyflow::test::describe(kf_a) + "\n      right: "      \
                    + ::keyflow::test::describe(kf_b));                        \
        }                                                                      \
    } while (false)

/// Assert a condition, stopping the case when it fails. For the setup step a
/// later assertion would crash on — dereferencing a null the first check
/// proved was null.
#define REQUIRE(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ::keyflow::test::report(__FILE__, __LINE__, "REQUIRE failed: " #cond); \
            ::keyflow::test::aborted() = true;                                 \
            return;                                                            \
        }                                                                      \
    } while (false)
