#include "TestFramework.h"

#include "core/Expression.h"

using namespace keyflow;

namespace {

ExpressionContext contextAt(double time)
{
    ExpressionContext context;
    context.time = time;
    context.frame = static_cast<int>(time * 30.0);
    context.fps = 30.0;
    context.value = 42.0;
    context.compWidth = 1920.0;
    context.compHeight = 1080.0;
    context.positionX = 100.0;
    context.positionY = 200.0;
    context.layerIndex = 3;
    context.seed = 12345;
    return context;
}

double eval(const std::string& source, const ExpressionContext& context)
{
    const ExpressionResult result = evaluateExpression(source, context);
    if (!result.ok) {
        ::keyflow::test::report(__FILE__, __LINE__,
                                "expression failed to evaluate: " + source
                                    + " -- " + result.error);
        return 0.0;
    }
    return result.value;
}

} // namespace

TEST("expression: arithmetic")
{
    const ExpressionContext ctx = contextAt(0.0);
    CHECK_EQ(eval("1 + 2", ctx), 3.0);
    CHECK_EQ(eval("10 - 4", ctx), 6.0);
    CHECK_EQ(eval("3 * 4", ctx), 12.0);
    CHECK_EQ(eval("10 / 4", ctx), 2.5);
    CHECK_EQ(eval("2 + 3 * 4", ctx), 14.0);
    CHECK_EQ(eval("(2 + 3) * 4", ctx), 20.0);
    CHECK_EQ(eval("-5", ctx), -5.0);
    CHECK_EQ(eval("2 ^ 10", ctx), 1024.0);
    CHECK_EQ(eval("10 % 3", ctx), 1.0);
}

TEST("expression: division by zero yields zero rather than failing")
{
    // A keyframed divisor passes through zero mid-animation. Failing the
    // expression for that frame would flash the parameter to its fallback.
    const ExpressionContext ctx = contextAt(0.0);
    CHECK_EQ(eval("5 / 0", ctx), 0.0);
    CHECK_EQ(eval("5 % 0", ctx), 0.0);
}

TEST("expression: reads the context")
{
    const ExpressionContext ctx = contextAt(2.0);
    CHECK_EQ(eval("time", ctx), 2.0);
    CHECK_EQ(eval("time * 30", ctx), 60.0);
    CHECK_EQ(eval("frame", ctx), 60.0);
    CHECK_EQ(eval("fps", ctx), 30.0);
    CHECK_EQ(eval("value", ctx), 42.0);
    CHECK_EQ(eval("compWidth", ctx), 1920.0);
    CHECK_EQ(eval("compHeight", ctx), 1080.0);
    CHECK_EQ(eval("layerIndex", ctx), 3.0);
    CHECK_EQ(eval("positionX", ctx), 100.0);
}

TEST("expression: constants")
{
    const ExpressionContext ctx = contextAt(0.0);
    CHECK_NEAR(eval("pi", ctx), 3.14159265358979, 1e-9);
    CHECK_NEAR(eval("e", ctx), 2.71828182845905, 1e-9);
}

TEST("expression: functions")
{
    const ExpressionContext ctx = contextAt(0.0);
    CHECK_EQ(eval("abs(-7)", ctx), 7.0);
    CHECK_EQ(eval("floor(2.7)", ctx), 2.0);
    CHECK_EQ(eval("ceil(2.1)", ctx), 3.0);
    CHECK_EQ(eval("round(2.6)", ctx), 3.0);
    CHECK_EQ(eval("min(3, 8)", ctx), 3.0);
    CHECK_EQ(eval("max(3, 8)", ctx), 8.0);
    CHECK_EQ(eval("clamp(15, 0, 10)", ctx), 10.0);
    CHECK_EQ(eval("clamp(-5, 0, 10)", ctx), 0.0);
    CHECK_EQ(eval("sqrt(16)", ctx), 4.0);
    CHECK_EQ(eval("pow(2, 8)", ctx), 256.0);
    CHECK_EQ(eval("sign(-3)", ctx), -1.0);
    CHECK_EQ(eval("mod(7, 3)", ctx), 1.0);
}

TEST("expression: sqrt of a negative is zero, not NaN")
{
    const ExpressionContext ctx = contextAt(0.0);
    CHECK_EQ(eval("sqrt(-4)", ctx), 0.0);
}

TEST("expression: trigonometric functions and degrees")
{
    const ExpressionContext ctx = contextAt(0.0);
    CHECK_NEAR(eval("sin(0)", ctx), 0.0, 1e-12);
    CHECK_NEAR(eval("cos(0)", ctx), 1.0, 1e-12);
    CHECK_NEAR(eval("sin(radians(90))", ctx), 1.0, 1e-9);
    CHECK_NEAR(eval("degrees(pi)", ctx), 180.0, 1e-9);
}

TEST("expression: interpolation helpers")
{
    const ExpressionContext ctx = contextAt(0.0);
    CHECK_EQ(eval("lerp(0, 10, 0.5)", ctx), 5.0);
    CHECK_EQ(eval("mix(0, 10, 0.25)", ctx), 2.5);
    CHECK_EQ(eval("smoothstep(0, 1, 0.5)", ctx), 0.5);
    CHECK_EQ(eval("smoothstep(0, 1, -1)", ctx), 0.0);
    CHECK_EQ(eval("smoothstep(0, 1, 2)", ctx), 1.0);
}

TEST("expression: comparison and logic")
{
    const ExpressionContext ctx = contextAt(0.0);
    CHECK_EQ(eval("1 < 2", ctx), 1.0);
    CHECK_EQ(eval("2 < 1", ctx), 0.0);
    CHECK_EQ(eval("2 >= 2", ctx), 1.0);
    CHECK_EQ(eval("1 == 1", ctx), 1.0);
    CHECK_EQ(eval("1 != 1", ctx), 0.0);
    CHECK_EQ(eval("1 && 0", ctx), 0.0);
    CHECK_EQ(eval("1 || 0", ctx), 1.0);
    CHECK_EQ(eval("!0", ctx), 1.0);
}

TEST("expression: the conditional operator")
{
    const ExpressionContext ctx = contextAt(0.0);
    CHECK_EQ(eval("1 ? 10 : 20", ctx), 10.0);
    CHECK_EQ(eval("0 ? 10 : 20", ctx), 20.0);
}

TEST("expression: wiggle is stable for a parked playhead")
{
    // The property that matters. A wiggle that changes while the playhead is
    // still makes the preview flicker, and makes a render differ from the
    // preview.
    const ExpressionContext ctx = contextAt(4.0);
    const double first = eval("wiggle(2, 30)", ctx);
    const double second = eval("wiggle(2, 30)", ctx);
    CHECK_EQ(first, second);

    // And it stays within its amplitude.
    for (int i = 0; i < 50; ++i) {
        const ExpressionContext sample = contextAt(static_cast<double>(i) * 0.1);
        const double value = eval("wiggle(2, 30)", sample);
        CHECK(value >= -30.0 - 1e-9);
        CHECK(value <= 30.0 + 1e-9);
    }
}

TEST("expression: wiggle varies over time")
{
    // If it did not vary it would not be a wiggle. Sampled coarsely enough
    // that a smooth signal still moves.
    double minValue = 1e9;
    double maxValue = -1e9;
    for (int i = 0; i < 100; ++i) {
        const ExpressionContext sample = contextAt(static_cast<double>(i) * 0.05);
        const double value = eval("wiggle(4, 50)", sample);
        minValue = std::min(minValue, value);
        maxValue = std::max(maxValue, value);
    }
    CHECK(maxValue - minValue > 1.0);
}

TEST("expression: wiggle is reproducible across evaluations")
{
    // Same seed, same time, same answer — which is what makes a render match
    // its preview.
    const ExpressionContext a = contextAt(1.234);
    ExpressionContext b = contextAt(1.234);
    CHECK_EQ(eval("wiggle(3, 20)", a), eval("wiggle(3, 20)", b));
}

TEST("expression: random is seeded by the frame")
{
    // Reproducible per frame, which a true RNG would not be.
    const ExpressionContext a = contextAt(1.0);
    const ExpressionContext b = contextAt(1.0);
    CHECK_EQ(eval("random()", a), eval("random()", b));

    const double v = eval("random()", a);
    CHECK(v >= 0.0);
    CHECK(v <= 1.0);
}

TEST("expression: named values from the layer")
{
    ExpressionContext ctx = contextAt(0.0);
    ctx.named["blurRadius"] = 12.5;
    CHECK_EQ(eval("blurRadius * 2", ctx), 25.0);
}

TEST("expression: rejects an unknown name")
{
    // Returning zero would turn a typo into a parameter that silently stops
    // moving, which is much harder to find than a message.
    const ExpressionContext ctx = contextAt(0.0);
    const ExpressionResult result = evaluateExpression("notAName + 1", ctx);
    CHECK(!result.ok);
    CHECK(result.error.find("notAName") != std::string::npos);
}

TEST("expression: rejects an unknown function")
{
    const ExpressionContext ctx = contextAt(0.0);
    const ExpressionResult result = evaluateExpression("frobnicate(1)", ctx);
    CHECK(!result.ok);
    CHECK(!result.error.empty());
}

TEST("expression: rejects a malformed expression with a position")
{
    const ExpressionContext ctx = contextAt(0.0);
    for (const char* source : {"1 +", "(1 + 2", "1 2 3", "* 5", ""}) {
        const ExpressionResult result = evaluateExpression(source, ctx);
        CHECK(!result.ok);
    }
}

TEST("expression: rejects a non-finite result")
{
    const ExpressionContext ctx = contextAt(0.0);
    // A huge power overflows to infinity. Letting that reach a shader blanks
    // the frame, so it is reported instead.
    const ExpressionResult result = evaluateExpression("1e300 * 1e300", ctx);
    CHECK(!result.ok);
    CHECK(result.error.find("finite") != std::string::npos);
}

TEST("expression: a negative base with a fractional exponent is rejected")
{
    const ExpressionContext ctx = contextAt(0.0);
    const ExpressionResult result = evaluateExpression("(-8) ^ 0.5", ctx);
    CHECK(!result.ok);
}

TEST("expression: a deeply nested expression is rejected, not crashed on")
{
    std::string source;
    for (int i = 0; i < 200; ++i) source += "(";
    source += "1";
    for (int i = 0; i < 200; ++i) source += ")";

    const ExpressionContext ctx = contextAt(0.0);
    const ExpressionResult result = evaluateExpression(source, ctx);
    CHECK(!result.ok);
    CHECK(!result.error.empty());
}

TEST("expression: indexing a scalar yields the value at zero")
{
    // `position[0]` appears in expressions copied from tutorials. It should
    // evaluate rather than fail.
    const ExpressionContext ctx = contextAt(0.0);
    CHECK_EQ(eval("value[0]", ctx), 42.0);
}

TEST("expression: member access reads the layer's transform")
{
    const ExpressionContext ctx = contextAt(0.0);
    CHECK_EQ(eval("thisLayer.x", ctx), 100.0);
    CHECK_EQ(eval("thisLayer.y", ctx), 200.0);
}

TEST("expression: checkExpression parses without a real context")
{
    CHECK(checkExpression("time * 2 + 1").ok);
    CHECK(!checkExpression("time *").ok);
}
