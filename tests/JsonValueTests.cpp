#include "TestFramework.h"

#include "core/json/JsonValue.h"

using keyflow::json::Value;

TEST("json: parses scalars")
{
    std::string error;
    CHECK_EQ(keyflow::json::parse("42", &error).asNumber(), 42.0);
    CHECK_EQ(keyflow::json::parse("-3.5", &error).asNumber(), -3.5);
    CHECK_EQ(keyflow::json::parse("1e3", &error).asNumber(), 1000.0);
    CHECK_EQ(keyflow::json::parse("true", &error).asBool(), true);
    CHECK_EQ(keyflow::json::parse("false", &error).asBool(), false);
    CHECK(keyflow::json::parse("null", &error).isNull());
    CHECK(error.empty());
}

TEST("json: parses strings with escapes")
{
    std::string error;
    CHECK_EQ(keyflow::json::parse(R"("hello")", &error).asString(), std::string("hello"));
    CHECK_EQ(keyflow::json::parse(R"("a\nb")", &error).asString(), std::string("a\nb"));
    CHECK_EQ(keyflow::json::parse("\"q\\\"q\"", &error).asString(), std::string("q\"q"));
    CHECK_EQ(keyflow::json::parse("\"back\\\\slash\"", &error).asString(),
             std::string("back\\slash"));
    CHECK_EQ(keyflow::json::parse("\"\\u0041\"", &error).asString(), std::string("A"));
}

TEST("json: parses a surrogate pair into one code point")
{
    std::string error;
    // U+1F600, the grinning face, as the two escapes JSON requires.
    const Value v = keyflow::json::parse(R"("😀")", &error);
    REQUIRE(error.empty());
    // Four bytes of UTF-8.
    CHECK_EQ(v.asString().size(), std::size_t(4));
}

TEST("json: parses arrays and objects")
{
    std::string error;
    const Value v = keyflow::json::parse(R"({"a": [1, 2, 3], "b": {"c": "d"}})", &error);
    REQUIRE(error.empty());
    CHECK_EQ(v["a"].size(), std::size_t(3));
    CHECK_EQ(v["a"][1].asNumber(), 2.0);
    CHECK_EQ(v["b"]["c"].asString(), std::string("d"));
}

TEST("json: a missing key reads as null rather than crashing")
{
    std::string error;
    const Value v = keyflow::json::parse(R"({"a": 1})", &error);
    REQUIRE(error.empty());
    CHECK(v["nope"].isNull());
    // Chained reads through a missing key stay null.
    CHECK(v["nope"]["deeper"][3].isNull());
    CHECK_EQ(v["nope"].asNumber(7.0), 7.0);
}

TEST("json: rejects malformed input with a position")
{
    std::string error;
    keyflow::json::parse("{\"a\": }", &error);
    CHECK(!error.empty());
    CHECK(error.find("byte") != std::string::npos);

    error.clear();
    keyflow::json::parse("[1, 2", &error);
    CHECK(!error.empty());

    error.clear();
    keyflow::json::parse("{\"a\": 1} trailing", &error);
    CHECK(!error.empty());

    error.clear();
    keyflow::json::parse("\"unterminated", &error);
    CHECK(!error.empty());
}

TEST("json: rejects a nested document past the depth limit")
{
    std::string text;
    for (int i = 0; i < 200; ++i) text += "[";
    for (int i = 0; i < 200; ++i) text += "]";

    std::string error;
    keyflow::json::parse(text, &error);
    CHECK(!error.empty());
    CHECK(error.find("deeply") != std::string::npos);
}

TEST("json: round-trips through write")
{
    const char* source = R"({"name":"test","count":3,"nested":{"flag":true},"list":[1,2]})";
    std::string error;
    const Value first = keyflow::json::parse(source, &error);
    REQUIRE(error.empty());

    const std::string written = keyflow::json::write(first);
    const Value second = keyflow::json::parse(written, &error);
    REQUIRE(error.empty());

    CHECK_EQ(second["name"].asString(), std::string("test"));
    CHECK_EQ(second["count"].asNumber(), 3.0);
    CHECK_EQ(second["nested"]["flag"].asBool(), true);
    CHECK_EQ(second["list"].size(), std::size_t(2));
}

TEST("json: write preserves key order")
{
    Value v;
    v.set("zebra", 1);
    v.set("alpha", 2);
    v.set("middle", 3);

    const std::string text = keyflow::json::write(v);
    // The keys appear in insertion order, not sorted. A project file that
    // reorders itself on every save is a file nobody can review.
    CHECK(text.find("zebra") < text.find("alpha"));
    CHECK(text.find("alpha") < text.find("middle"));
}

TEST("json: write emits integers without a decimal point")
{
    Value v;
    v.set("width", 1920.0);
    v.set("scale", 1.5);
    const std::string text = keyflow::json::write(v);
    CHECK(text.find("1920,") != std::string::npos || text.find("1920}") != std::string::npos);
    CHECK(text.find("1.5") != std::string::npos);
}

TEST("json: a double survives a round trip exactly")
{
    // 0.1 is not representable in binary. A %.17g writer and a strtod reader
    // must reproduce the same double, or a project file drifts on every save.
    const double values[] = {0.1, 1.0 / 3.0, 2.718281828459045, 1e-300, 1e300};
    for (double original : values) {
        Value v;
        v.set("n", original);
        const std::string text = keyflow::json::write(v);
        std::string error;
        const Value parsed = keyflow::json::parse(text, &error);
        REQUIRE(error.empty());
        CHECK_EQ(parsed["n"].asNumber(), original);
    }
}

TEST("json: NaN and infinity write as zero")
{
    // JSON has no representation for either. Writing one produces a file no
    // parser accepts, which would lose the whole project rather than one value.
    Value v;
    v.set("a", std::nan(""));
    v.set("b", std::numeric_limits<double>::infinity());
    v.set("c", -std::numeric_limits<double>::infinity());

    const std::string text = keyflow::json::write(v);
    CHECK_EQ(text, std::string(R"({"a":0,"b":0,"c":0})"));

    // And the result parses, which is the property that matters.
    std::string error;
    const Value parsed = keyflow::json::parse(text, &error);
    REQUIRE(error.empty());
    CHECK_EQ(parsed["a"].asNumber(), 0.0);
    CHECK_EQ(parsed["b"].asNumber(), 0.0);
}

TEST("json: remove deletes a key and set replaces in place")
{
    Value v;
    v.set("a", 1);
    v.set("b", 2);
    v.set("a", 9);
    CHECK_EQ(v.size(), std::size_t(2));
    CHECK_EQ(v["a"].asNumber(), 9.0);

    v.remove("a");
    CHECK_EQ(v.size(), std::size_t(1));
    CHECK(v["a"].isNull());
    CHECK_EQ(v["b"].asNumber(), 2.0);
}
