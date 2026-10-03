// ---------------------------------------------------------------------------
// The document model.
//
// A Project holds one or more Compositions. A Composition holds Layers. A
// Layer holds an effect stack. Everything else — keyframes, expressions,
// vector paths, pivots — hangs off a Layer.
//
// This is the recovered shape of the Android app's model, kept because it is
// the right shape for the problem: an effect stack that can be reordered is
// what makes a compositor composable, and separating the layer's own transform
// from the effects applied to it is what lets a transform be animated without
// every effect having to know about it.
//
// Everything here is plain data. No OpenGL, no Win32, no WebView2. That is
// what makes it testable without a window, and the tests do exactly that.
// ---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/json/JsonValue.h"

namespace keyflow {

/// How a parameter's value changes over time.
enum class Interpolation
{
    Hold,     ///< Step: the previous keyframe's value until the next one.
    Linear,   ///< Straight line between keyframes.
    Bezier,   ///< Cubic with the two handles below.
    Ease,     ///< The default S-curve; equivalent to Bezier with handles at 1/3.
};

/// One point on an animated parameter.
struct Keyframe
{
    /// Time in seconds from the composition's start.
    double time = 0.0;

    /// The value at this time.
    double value = 0.0;

    Interpolation interpolation = Interpolation::Ease;

    /// Bezier handles, as a fraction of the gap to the neighbouring keyframe.
    /// Ignored unless `interpolation` is Bezier.
    double inHandle = 0.33;
    double outHandle = 0.33;
};

/// A parameter that may be static or animated.
///
/// One type for both, rather than a variant, because every effect parameter
/// goes through the same code path: read a value at a time, and the path is
/// the same whether that is a constant or an interpolation.
struct AnimatedValue
{
    /// The value when there are no keyframes.
    double constant = 0.0;

    /// Sorted by time. An empty vector means the parameter is static.
    std::vector<Keyframe> keyframes;

    /// The value at `time`.
    double evaluate(double time) const;

    /// True when the parameter has keyframes.
    bool animated() const { return !keyframes.empty(); }

    /// Insert a keyframe, keeping the vector sorted. Replaces a keyframe
    /// already at this time rather than adding a duplicate: two keyframes at
    /// one time make the curve ambiguous and the value undefined.
    void setKeyframe(Keyframe k);

    /// Remove the keyframe at exactly `time`, if there is one.
    bool removeKeyframe(double time);

    /// Bounds of the keyframes. Undefined when not animated.
    double firstTime() const;
    double lastTime() const;
};

/// An expression that drives a parameter.
///
/// Kept as source text plus a compiled form. The evaluator is a small
/// recursive-descent parser over the arithmetic subset the app's own
/// expressions use — no general-purpose scripting, no host access.
struct Expression
{
    std::string source;
    bool enabled = false;
};

/// A point on a vector path.
struct PathPoint
{
    double x = 0.0;
    double y = 0.0;
    /// Control handles, relative to the point.
    double inX = 0.0;
    double inY = 0.0;
    double outX = 0.0;
    double outY = 0.0;
};

/// A vector path, in the layer's own coordinate space.
struct VectorPath
{
    std::vector<PathPoint> points;
    bool closed = false;

    /// Build a path from a JSON array of points.
    static VectorPath fromJson(const json::Value& v);

    /// Serialise to a JSON array of points.
    json::Value toJson() const;
};

/// What a layer draws.
enum class LayerKind
{
    Media,       ///< A decoded image, video frame or generated frame.
    Solid,       ///< A flat colour, optionally with a gradient.
    Text,        ///< Rendered text.
    Shape,       ///< A vector path, filled and/or stroked.
    Adjustment,  ///< Applies its effect stack to everything below it.
    Null,        ///< A transform-only parent for other layers.
    Audio,       ///< Audio with no picture.
};

/// How a layer's effect stack combines with what is beneath it.
///
/// The numbering matches the compositor shader's `uMode` uniform, so the value
/// here is what gets uploaded. Renumbering would silently change the meaning
/// of every saved project.
enum class BlendMode
{
    Normal = 0,
    Darken = 1,
    Multiply = 2,
    ColorBurn = 3,
    LinearBurn = 4,
    Lighten = 5,
    Screen = 6,
    ColorDodge = 7,
    LinearDodge = 8,
    Overlay = 9,
    SoftLight = 10,
    HardLight = 11,
    VividLight = 12,
    Difference = 13,
    Exclusion = 14,
    Subtract = 15,
    Divide = 16,
    Hue = 17,
    Saturation = 18,
    Color = 19,
    Luminosity = 20,
};

/// Human-readable name for a blend mode, for the page and for project files.
const char* blendModeName(BlendMode mode);
/// Parse a blend mode name. Unknown names fall back to Normal.
BlendMode blendModeFromName(const std::string& name);

/// One instance of an effect in a layer's stack.
struct Effect
{
    /// The effect's spec id, e.g. "chroma_key" or "blur_box". This names an
    /// entry in the effect catalogue, which owns the shader and the parameter
    /// defaults. An id with no catalogue entry is kept rather than dropped:
    /// opening a project from a newer version should not silently delete work.
    std::string specId;

    /// Distinguishes two instances of the same effect in one stack, so the
    /// page can address one of them.
    std::string instanceId;

    /// Parameter values, keyed by the spec's parameter name.
    std::map<std::string, AnimatedValue> parameters;

    /// Per-instance expressions, keyed by parameter name.
    std::map<std::string, Expression> expressions;

    /// False when the effect is present but bypassed.
    bool enabled = true;

    /// Read a parameter, falling back to `fallback` when the effect does not
    /// carry one. A project saved before a parameter existed opens with the
    /// spec's default rather than zero.
    double parameter(const std::string& name, double fallback) const;
};

/// One item in a composition.
struct Layer
{
    std::string id;
    std::string name;
    LayerKind kind = LayerKind::Media;

    /// The media this layer draws, for LayerKind::Media. Empty otherwise.
    std::string assetId;

    /// Whether the layer is drawn at all.
    bool visible = true;

    /// Whether the layer's transform is locked in the editor. A locked layer
    /// can still be selected and its effects edited; only the transform is
    /// held. That distinction is what makes it useful while animating.
    bool locked = false;

    /// Whether the layer's own alpha is used as a matte for the layers below.
    bool trackMatte = false;

    /// In and out points, in seconds.
    double inPoint = 0.0;
    double outPoint = 10.0;

    // --- transform --------------------------------------------------------
    //
    // Animatable, like every effect parameter, because a transform is just an
    // effect that happens to be applied by the compositor rather than by a
    // shader pass.

    AnimatedValue positionX{0.0, {}};
    AnimatedValue positionY{0.0, {}};
    AnimatedValue scaleX{1.0, {}};
    AnimatedValue scaleY{1.0, {}};
    AnimatedValue rotation{0.0, {}};
    AnimatedValue opacity{1.0, {}};

    /// The point the layer rotates and scales about, in normalised layer
    /// coordinates: (0,0) is the top-left of the layer, (1,1) the bottom-right.
    /// The default centres it.
    AnimatedValue anchorX{0.5, {}};
    AnimatedValue anchorY{0.5, {}};

    /// 3D placement, used when the composition has a camera.
    AnimatedValue positionZ{0.0, {}};
    AnimatedValue rotationX{0.0, {}};
    AnimatedValue rotationY{0.0, {}};

    // --- content ----------------------------------------------------------

    /// Solid colour, 0..1 per channel. Used by LayerKind::Solid.
    double solidR = 1.0;
    double solidG = 1.0;
    double solidB = 1.0;
    double solidA = 1.0;

    /// Text content and size, used by LayerKind::Text.
    std::string text;
    double textSize = 72.0;

    /// The path, used by LayerKind::Shape.
    VectorPath path;
    bool pathFilled = true;
    bool pathStroked = false;
    double strokeWidth = 4.0;
    double fillR = 1.0;
    double fillG = 1.0;
    double fillB = 1.0;
    double strokeR = 0.0;
    double strokeG = 0.0;
    double strokeB = 0.0;

    /// The effect stack, applied in order.
    std::vector<Effect> effects;

    /// The blend mode this layer composites with.
    BlendMode blend = BlendMode::Normal;

    /// Strength of the blend, 0..1. At 0 the layer composites normally.
    double blendStrength = 1.0;

    /// Motion blur, in seconds of shutter. Zero disables it.
    double motionBlurShutter = 0.0;

    /// True when this layer's motion blur is on. Separate from the shutter so
    /// the shutter length survives toggling it off.
    bool motionBlurEnabled = false;

    /// Evaluate the layer's transform at a time.
    struct Transform
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        double scaleX = 1.0;
        double scaleY = 1.0;
        double rotation = 0.0;
        double rotationX = 0.0;
        double rotationY = 0.0;
        double opacity = 1.0;
        double anchorX = 0.5;
        double anchorY = 0.5;
    };

    Transform transformAt(double time) const;

    /// True when the layer is live at `time`.
    bool activeAt(double time) const
    {
        return visible && time >= inPoint && time < outPoint;
    }
};

/// A composition: the canvas, its frame rate, and the layers on it.
struct Composition
{
    std::string id;
    std::string name;

    int width = 1920;
    int height = 1080;

    /// Frames per second. Kept as a rational pair so 29.97 is exact.
    int fpsNumerator = 30;
    int fpsDenominator = 1;

    /// Duration in seconds.
    double duration = 10.0;

    /// Background, 0..1 per channel.
    double backgroundR = 0.0;
    double backgroundG = 0.0;
    double backgroundB = 0.0;
    double backgroundA = 1.0;

    /// Layers, bottom of the stack first. The compositor draws them in order,
    /// so index 0 is the backdrop and the last entry is on top.
    std::vector<Layer> layers;

    /// The active 3D camera, when the composition has one.
    bool hasCamera = false;
    AnimatedValue cameraX{0.0, {}};
    AnimatedValue cameraY{0.0, {}};
    AnimatedValue cameraZ{-1000.0, {}};
    AnimatedValue cameraZoom{1.0, {}};

    double fps() const
    {
        return fpsDenominator > 0
            ? static_cast<double>(fpsNumerator) / static_cast<double>(fpsDenominator)
            : 30.0;
    }

    /// Total frames at this frame rate. At least one, so an empty composition
    /// still renders a frame rather than dividing by zero downstream.
    int frameCount() const;

    /// Find a layer by id. Null when absent.
    Layer* findLayer(const std::string& id);
    const Layer* findLayer(const std::string& id) const;

    /// Find a layer's index. -1 when absent.
    int indexOfLayer(const std::string& id) const;
};

/// An imported file.
struct Asset
{
    std::string id;

    /// Display name, without a path.
    std::string name;

    /// Absolute path on disk. The project file stores this, so moving a
    /// project to another machine asks the user to relink. Storing the bytes
    /// would make project files enormous and is not what the Android app does.
    std::string path;

    /// Seconds. Zero for a still image.
    double duration = 0.0;

    int width = 0;
    int height = 0;

    /// True when the file has audio the editor can decode.
    bool hasAudio = false;

    /// True for a still image, which has no timeline of its own.
    bool still = false;
};

/// The whole document.
struct Project
{
    /// Format version. Bumped when a change cannot be read by an older build.
    /// Loading a file with a higher version than this build understands
    /// refuses rather than guessing, because guessing loses work.
    static constexpr int kFormatVersion = 1;

    std::string name;
    std::vector<Asset> assets;
    std::vector<Composition> compositions;

    /// The composition the editor opens on.
    std::string activeCompositionId;

    /// Find a composition by id. Null when absent.
    Composition* findComposition(const std::string& id);
    const Composition* findComposition(const std::string& id) const;

    /// The active composition, or the first one, or null when there are none.
    Composition* activeComposition();
    const Composition* activeComposition() const;

    /// Every asset id used by any layer. Used to warn about missing media
    /// before a render rather than failing part-way through one.
    std::vector<std::string> referencedAssetIds() const;

    /// Create a composition with a fresh id and one layer.
    static Project createDefault(const std::string& name = "Untitled");

    /// Generate an id that is unique within this project.
    ///
    /// Ids are opaque strings, not indices: an index changes meaning when a
    /// layer is deleted, and every reference to it — a matte target, a
    /// keyframe lane, an expression — would silently point somewhere else.
    std::string newId(const std::string& prefix) const;
};

// --- project file ---------------------------------------------------------

/// Serialise a project to JSON.
json::Value projectToJson(const Project& project);

/// Read a project from JSON.
///
/// Returns nullopt and fills `error` when the text is not a project or its
/// version is from the future. A structurally valid project with unknown
/// fields keeps them in the JSON but not in the model, which means saving
/// drops them — acceptable because the alternative is refusing to open a file
/// that is only slightly newer.
std::optional<Project> projectFromJson(const json::Value& root, std::string* error);

/// Load a project from a file. Returns nullopt and fills `error` on failure.
std::optional<Project> loadProject(const std::string& path, std::string* error);

/// Save a project to a file. Returns false and fills `error` on failure.
///
/// Written to a temporary file next to the target and then renamed, so a crash
/// or a full disk during the write leaves the previous save intact rather than
/// a truncated file where the project used to be.
bool saveProject(const Project& project, const std::string& path, std::string* error);

} // namespace keyflow
