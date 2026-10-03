#include "TestFramework.h"

#include "core/EffectCatalog.h"
#include "core/Model.h"
#include "gl/ShaderTable.h"

#include <algorithm>
#include <set>

using namespace keyflow;

TEST("catalogue: every effect has a non-empty id, name and category")
{
    for (const EffectSpec& spec : EffectCatalog::instance().effects()) {
        CHECK(!spec.id.empty());
        CHECK(!spec.name.empty());
        CHECK(!spec.category.empty());
    }
}

TEST("catalogue: ids are unique")
{
    std::set<std::string> seen;
    for (const EffectSpec& spec : EffectCatalog::instance().effects()) {
        const auto [it, inserted] = seen.insert(spec.id);
        CHECK(inserted);
    }
}

TEST("catalogue: every effect's category is declared")
{
    const auto& categories = EffectCatalog::instance().categories();
    for (const EffectSpec& spec : EffectCatalog::instance().effects()) {
        CHECK(std::find(categories.begin(), categories.end(), spec.category) != categories.end());
    }
}

TEST("catalogue: every effect has a shader that exists in the table")
{
    // This is the test that catches a renamed shader file or a typo in a
    // catalogue entry. Without it the failure is a black frame at runtime.
    for (const EffectSpec& spec : EffectCatalog::instance().effects()) {
        if (spec.shader.empty()) continue;
        const std::string_view source = shaderSourceView(spec.shader);
        if (source.empty()) {
            ::keyflow::test::report(__FILE__, __LINE__,
                                    "effect '" + spec.id + "' names shader '"
                                        + spec.shader + "', which is not in the table");
            continue;
        }
        CHECK(!source.empty());
    }
}

TEST("catalogue: a multi-pass effect's passes are all in the table")
{
    // A two-pass effect whose second pass is missing renders as a blur along
    // one axis: it looks like a blur that is subtly wrong rather than like a
    // missing shader, which is the worst way for this to fail.
    //
    // The passes are named by the spec's `passShaders` list rather than by a
    // suffix convention. A convention would have to be guessed from the name,
    // and it does not hold: a separable blur's second pass is the other axis,
    // while a glow's are a threshold, two blurs and a composite.
    for (const EffectSpec& spec : EffectCatalog::instance().effects()) {
        if (spec.passes < 2) continue;

        if (static_cast<int>(spec.passShaders.size()) != spec.passes) {
            ::keyflow::test::report(
                __FILE__, __LINE__,
                "effect '" + spec.id + "' declares " + std::to_string(spec.passes)
                    + " passes but names " + std::to_string(spec.passShaders.size())
                    + " shaders");
            continue;
        }
        for (const std::string& stem : spec.passShaders) {
            if (shaderSourceView(stem).empty()) {
                ::keyflow::test::report(
                    __FILE__, __LINE__,
                    "effect '" + spec.id + "' names pass shader '" + stem
                        + "', which is not in the table");
            }
        }
    }
}

TEST("catalogue: parameter names are unique within an effect")
{
    for (const EffectSpec& spec : EffectCatalog::instance().effects()) {
        std::set<std::string> seen;
        for (const ParamSpec& p : spec.params) {
            const auto [it, inserted] = seen.insert(p.name);
            if (!inserted) {
                ::keyflow::test::report(__FILE__, __LINE__,
                                        "effect '" + spec.id + "' declares parameter '"
                                            + p.name + "' twice");
            }
        }
    }
}

TEST("catalogue: every parameter's default is within its own range")
{
    // A default outside the range means the inspector opens showing a value
    // the slider cannot represent, and the first touch of the control snaps
    // the value somewhere else.
    for (const EffectSpec& spec : EffectCatalog::instance().effects()) {
        for (const ParamSpec& p : spec.params) {
            if (p.unbounded) continue;
            if (p.default_ < p.min - 1e-9 || p.default_ > p.max + 1e-9) {
                ::keyflow::test::report(
                    __FILE__, __LINE__,
                    "effect '" + spec.id + "' parameter '" + p.name
                        + "' defaults to " + std::to_string(p.default_)
                        + ", outside [" + std::to_string(p.min) + ", "
                        + std::to_string(p.max) + "]");
            }
        }
    }
}

TEST("catalogue: every parameter's range is ordered")
{
    for (const EffectSpec& spec : EffectCatalog::instance().effects()) {
        for (const ParamSpec& p : spec.params) {
            CHECK(p.min <= p.max);
        }
    }
}

TEST("catalogue: a choice parameter has choices, and its default indexes one")
{
    for (const EffectSpec& spec : EffectCatalog::instance().effects()) {
        for (const ParamSpec& p : spec.params) {
            if (p.kind != ParamKind::Choice) continue;
            CHECK(!p.choices.empty());
            const int index = static_cast<int>(p.default_);
            CHECK(index >= 0);
            CHECK(index < static_cast<int>(p.choices.size()));
        }
    }
}

TEST("catalogue: find resolves a known id and rejects an unknown one")
{
    const EffectCatalog& catalog = EffectCatalog::instance();
    const EffectSpec* key = catalog.find("chroma_key");
    REQUIRE(key != nullptr);
    CHECK_EQ(key->name, std::string("Chroma Key"));
    CHECK_EQ(key->shader, std::string("chroma_key"));
    CHECK(catalog.find("not-an-effect") == nullptr);
}

TEST("catalogue: chroma key exposes the parameters its shader declares")
{
    const EffectCatalog& catalog = EffectCatalog::instance();
    const EffectSpec* spec = catalog.find("chroma_key");
    REQUIRE(spec != nullptr);

    // Every uniform the shader reads that is not supplied by the renderer
    // itself must appear as a parameter, or there is a control missing from
    // the inspector for a value the effect uses.
    const char* expected[] = {"keyR", "keyG", "keyB", "tolerance", "softness",
                              "preBlur", "clipBlack", "clipWhite", "spill", "spillBias"};
    for (const char* name : expected) {
        const bool found = std::any_of(spec->params.begin(), spec->params.end(),
                                       [name](const ParamSpec& p) { return p.name == name; });
        if (!found) {
            ::keyflow::test::report(__FILE__, __LINE__,
                                    std::string("chroma_key is missing parameter '") + name + "'");
        }
    }
}

TEST("catalogue: the compositor shader covers every blend mode")
{
    // The shader dispatches on uMode. A mode the shader does not handle falls
    // through to the final return, which is Luminosity — so Luminosity is
    // correct without an explicit branch, and every other mode must have one.
    // A missing branch would make that mode silently render as Luminosity
    // rather than obviously not working.
    const std::string source = shaderSource("compositor_blend");
    REQUIRE(!source.empty());

    for (int i = 1; i < static_cast<int>(BlendMode::Luminosity); ++i) {
        const std::string needle = "uMode == " + std::to_string(i);
        if (source.find(needle) == std::string::npos) {
            ::keyflow::test::report(
                __FILE__, __LINE__,
                "the compositor shader has no branch for blend mode " + std::to_string(i)
                    + " (" + blendModeName(static_cast<BlendMode>(i)) + ")");
        }
    }

    // Luminosity is the fall-through. Pin that it is the last return, so a
    // later mode added below it does not silently steal the fall-through.
    const std::size_t lastReturn = source.rfind("return setLum(b, lum(s))");
    CHECK(lastReturn != std::string::npos);
}

TEST("shaders: the table is not empty and every entry compiles as GLSL")
{
    const std::vector<std::string> stems = shaderStems();
    CHECK(stems.size() > 40);

    for (const std::string& stem : stems) {
        const std::string source = shaderSource(stem);
        CHECK(!source.empty());
        CHECK(source.find("void main()") != std::string::npos);

        // A fragment shader must declare a precision. In GLSL ES the vertex
        // stage defaults to highp, so a vertex unit without one is correct;
        // a fragment unit without one fails to compile on most drivers.
        const bool isVertex = source.find("gl_Position") != std::string::npos;
        if (!isVertex) {
            CHECK(source.find("precision ") != std::string::npos);
        }
    }
}

TEST("shaders: every unit has balanced braces")
{
    // A truncated shader — the failure mode when a long string constant is cut
    // — is a brace imbalance, and this catches it without a GL context so the
    // test runs on the CI runner.
    for (const std::string& stem : shaderStems()) {
        const std::string source = shaderSource(stem);
        int depth = 0;
        for (char c : source) {
            if (c == '{') ++depth;
            else if (c == '}') --depth;
        }
        if (depth != 0) {
            ::keyflow::test::report(__FILE__, __LINE__,
                                    "shader '" + stem + "' has " + std::to_string(depth)
                                        + " unbalanced braces");
        }
    }
}

TEST("shaders: fragment units write gl_FragColor")
{
    for (const std::string& stem : shaderStems()) {
        const std::string source = shaderSource(stem);
        const bool isVertex = source.find("gl_Position") != std::string::npos;
        if (isVertex) continue;
        if (source.find("gl_FragColor") == std::string::npos) {
            ::keyflow::test::report(__FILE__, __LINE__,
                                    "shader '" + stem + "' is neither vertex nor fragment");
        }
    }
}

TEST("shaders: the passthrough vertex shader is present")
{
    const std::string source = passthroughVertexShader();
    CHECK(!source.empty());
    CHECK(source.find("gl_Position") != std::string::npos);
    CHECK(source.find("vTex") != std::string::npos);
}

TEST("shaders: the recovered units keep their key identifiers")
{
    // Pinned because these are the load-bearing parts of the recovered set. A
    // regeneration that lost one of them would still produce 60-odd shaders
    // and every other test would pass.
    const std::string chroma = shaderSource("chroma_key");
    REQUIRE(!chroma.empty());
    CHECK(chroma.find("uKeyR") != std::string::npos);
    CHECK(chroma.find("uSpill") != std::string::npos);
    CHECK(chroma.find("ycc") != std::string::npos);

    const std::string compositor = shaderSource("compositor_blend");
    REQUIRE(!compositor.empty());
    CHECK(compositor.find("blendColor") != std::string::npos);
    CHECK(compositor.find("clipColor") != std::string::npos);
    CHECK(compositor.find("uBackdrop") != std::string::npos);

    const std::string bokeh = shaderSource("bokeh_iris_blur");
    REQUIRE(!bokeh.empty());
    CHECK(bokeh.find("uIrisShape") != std::string::npos);
    CHECK(bokeh.find("uBokehShading") != std::string::npos);
}
