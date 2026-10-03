// ---------------------------------------------------------------------------
// The effect catalogue.
//
// Every effect the editor offers is one entry here: an id, a display name, a
// category, the shader that implements it, and the parameters with their
// defaults and ranges. The page builds its effect browser from this table, the
// inspector builds its controls from it, the renderer binds its uniforms from
// it, and a project file names effects by the ids here.
//
// One table, four consumers. The alternative — the page knowing its own list,
// the renderer knowing its own — is two lists that drift, and the symptom of
// drift is a control that moves something the renderer does not read.
//
// The shaders are the ones recovered from the Android build. The parameter
// names are the shader's own uniform names, so binding is mechanical: the
// renderer walks the parameter list and calls glUniform on `u` + capitalised
// name. A parameter whose name does not match its uniform is a bug that shows
// up immediately as a control that does nothing.
// ---------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>

namespace keyflow {

/// What kind of control the page draws for a parameter.
enum class ParamKind
{
    Scalar,   ///< A number, with a slider when the range is bounded.
    Angle,    ///< Degrees, drawn as a dial.
    Colour,   ///< An RGB triple. Three consecutive Scalar parameters.
    Toggle,   ///< 0 or 1.
    Choice,   ///< An index into `choices`.
};

/// One parameter of an effect.
struct ParamSpec
{
    /// Name without the `u` prefix. "tolerance" binds to `uTolerance`.
    std::string name;

    /// Label the page shows.
    std::string label;

    /// Group within the inspector panel. Empty means ungrouped.
    std::string group;

    ParamKind kind = ParamKind::Scalar;

    double default_ = 0.0;
    double min = 0.0;
    double max = 1.0;

    /// True when the slider should be logarithmic. For a radius that spans
    /// 0.1 to 100, a linear slider puts everything useful in the first
    /// millimetre.
    bool logarithmic = false;

    /// When true the range is a suggestion and the value may go outside it.
    /// Used by parameters like position, where the interesting values are
    /// often outside the frame.
    bool unbounded = false;

    /// Labels for ParamKind::Choice. The value is the index.
    std::vector<std::string> choices;
};

/// One effect.
struct EffectSpec
{
    /// Stable id, used in project files and in messages from the page. Never
    /// rename one: an old project would stop resolving it.
    std::string id;

    /// Display name.
    std::string name;

    /// Category the browser groups it under.
    std::string category;

    /// Shader file stem, without extension. Resolved against the embedded
    /// shader table.
    std::string shader;

    /// Parameters, in the order the inspector shows them.
    std::vector<ParamSpec> params;

    /// True when the effect needs the frame before it, for a temporal effect
    /// like a trail or an echo. The renderer then keeps the previous frame
    /// bound to `uPrev`.
    bool needsPreviousFrame = false;

    /// True when the effect reads what is already composited beneath the
    /// layer, bound to `uBackdrop`.
    bool needsBackdrop = false;

    /// True when the effect is applied to the whole composition rather than to
    /// a single layer.
    bool compositionWide = false;

    /// Number of passes. Multi-pass effects render to an intermediate target;
    /// each pass runs its own shader in order.
    ///
    /// `shader` is the first pass; `passShaders` lists them all and is what
    /// the renderer iterates. Two fields rather than one because a single-pass
    /// effect is the common case and should not have to spell out a
    /// one-element list.
    int passes = 1;

    /// The shader for each pass, in order. Empty means `shader` alone.
    std::vector<std::string> passShaders;
};

/// The catalogue.
///
/// A singleton because it is read-only after construction and every consumer
/// wants the same instance. Constructed on first use, which is cheap and
/// avoids a static-initialisation-order problem with the shader table.
class EffectCatalog
{
public:
    /// The one catalogue.
    static const EffectCatalog& instance();

    /// Every effect, in a stable order: grouped by category, then by name.
    const std::vector<EffectSpec>& effects() const { return _effects; }

    /// Category names, in display order.
    const std::vector<std::string>& categories() const { return _categories; }

    /// Look up an effect by id. Null when absent.
    const EffectSpec* find(const std::string& id) const;

private:
    EffectCatalog();

    std::vector<EffectSpec> _effects;
    std::vector<std::string> _categories;
};

/// The GLSL source for a shader stem, or an empty string when absent.
///
/// The shaders are compiled into the executable as string constants, so there
/// is no shader file to find at runtime and no way for the app to run against
/// a shader set that does not match its own catalogue.
std::string shaderSource(const std::string& stem);

/// The GLSL ES 1.00 source of the shared vertex shader every effect pass uses.
std::string passthroughVertexShader();

} // namespace keyflow
