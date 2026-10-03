// ---------------------------------------------------------------------------
// Expressions.
//
// A parameter can be driven by a small arithmetic expression instead of by
// keyframes: `time * 100`, `wiggle(2, 30)`, `thisComp.layer(1).position[0]`.
// The Android app has this and it is the feature that separates a keyframe
// editor from a motion-graphics tool.
//
// The evaluator is a recursive-descent parser over a fixed grammar. It is NOT
// a scripting host: there is no I/O, no host access, no way to reach the file
// system or the network, and evaluation is bounded in both depth and step
// count. An expression is a formula over the layer's own context, and the
// worst a bad one can do is return a wrong number.
// ---------------------------------------------------------------------------

#pragma once

#include <map>
#include <string>
#include <vector>

namespace keyflow {

/// What an expression can read.
struct ExpressionContext
{
    /// Seconds from the composition start.
    double time = 0.0;

    /// Frame index at the composition's rate.
    int frame = 0;

    /// Frames per second.
    double fps = 30.0;

    /// This layer's index in the composition, 1-based, counting from the top
    /// of the stack. Matches the editor's layer numbering.
    int layerIndex = 1;

    /// The composition's size.
    double compWidth = 1920.0;
    double compHeight = 1080.0;

    /// The layer's own transform, so an expression can read what keyframes
    /// already produce and offset from it.
    double value = 0.0;
    double positionX = 0.0;
    double positionY = 0.0;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double rotation = 0.0;
    double opacity = 1.0;

    /// Named values the layer's own effects expose, keyed by parameter name.
    std::map<std::string, double> named;

    /// A deterministic pseudo-random source, seeded per layer and per frame so
    /// `wiggle` is stable when the playhead is parked and reproducible across
    /// a render. A true RNG would make a render differ from its preview.
    unsigned seed = 0;
};

/// The result of evaluating an expression.
struct ExpressionResult
{
    double value = 0.0;

    /// True when the expression evaluated cleanly. When false the caller
    /// should fall back to the parameter's keyframed or constant value, and
    /// `error` says why.
    bool ok = false;

    std::string error;
};

/// Evaluate `source` against `context`.
///
/// Never throws and never runs unbounded: a malformed expression, a division
/// by zero or a runaway loop produces ok=false with a message. That is the
/// right failure for a UI field that is edited one keystroke at a time — the
/// half-typed expression is invalid for a moment and the parameter simply
/// holds its previous value.
ExpressionResult evaluateExpression(const std::string& source,
                                    const ExpressionContext& context);

/// Check an expression parses, without evaluating it. For the editor's
/// as-you-type validation, which should not depend on the playhead.
ExpressionResult checkExpression(const std::string& source);

} // namespace keyflow
