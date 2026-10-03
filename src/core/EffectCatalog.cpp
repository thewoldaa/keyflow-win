#include "core/EffectCatalog.h"

#include "gl/ShaderTable.h"

#include <algorithm>
#include <map>

namespace keyflow {

namespace {

/// Shorthand for building the parameter tables.
///
/// A lambda rather than a designated initialiser because ParamSpec has eight
/// fields and most parameters set three: the name, the label and the default.
ParamSpec param(
    const std::string& name,
    const std::string& label,
    double default_,
    double min,
    double max,
    ParamKind kind = ParamKind::Scalar,
    const std::string& group = {})
{
    ParamSpec p;
    p.name = name;
    p.label = label;
    p.default_ = default_;
    p.min = min;
    p.max = max;
    p.kind = kind;
    p.group = group;
    return p;
}

ParamSpec toggle(const std::string& name, const std::string& label, bool default_,
                 const std::string& group = {})
{
    ParamSpec p = param(name, label, default_ ? 1.0 : 0.0, 0.0, 1.0, ParamKind::Toggle, group);
    return p;
}

ParamSpec choice(const std::string& name, const std::string& label, int default_,
                 std::vector<std::string> choices, const std::string& group = {})
{
    ParamSpec p = param(name, label, static_cast<double>(default_), 0.0,
                        static_cast<double>(choices.size() - 1), ParamKind::Choice, group);
    p.choices = std::move(choices);
    return p;
}

ParamSpec angle(const std::string& name, const std::string& label, double default_,
                const std::string& group = {})
{
    return param(name, label, default_, -360.0, 360.0, ParamKind::Angle, group);
}

ParamSpec unbounded(const std::string& name, const std::string& label, double default_,
                    double min, double max, const std::string& group = {})
{
    ParamSpec p = param(name, label, default_, min, max, ParamKind::Scalar, group);
    p.unbounded = true;
    return p;
}

ParamSpec logScale(const std::string& name, const std::string& label, double default_,
                   double min, double max, const std::string& group = {})
{
    ParamSpec p = param(name, label, default_, min, max, ParamKind::Scalar, group);
    p.logarithmic = true;
    return p;
}

/// Build an EffectSpec. A helper rather than aggregate initialisation because
/// the trailing flags have defaults and naming them at each call site is what
/// keeps `false, true, false, 4` from being a puzzle.
EffectSpec effect(std::string id, std::string name, std::string category,
                  std::string shader, std::vector<ParamSpec> params,
                  bool needsPreviousFrame = false, bool needsBackdrop = false,
                  bool compositionWide = false, int passes = 1,
                  std::vector<std::string> passShaders = {})
{
    EffectSpec spec;
    spec.id = std::move(id);
    spec.name = std::move(name);
    spec.category = std::move(category);
    spec.shader = std::move(shader);
    spec.params = std::move(params);
    spec.needsPreviousFrame = needsPreviousFrame;
    spec.needsBackdrop = needsBackdrop;
    spec.compositionWide = compositionWide;
    spec.passes = passes;
    spec.passShaders = std::move(passShaders);
    if (spec.passShaders.empty()) spec.passShaders.push_back(spec.shader);
    return spec;
}

/// Three consecutive scalars the page draws as one colour swatch.
///
/// Kept as three parameters rather than one vec3 because the shader takes
/// three floats (`uColorR`, `uColorG`, `uColorB`), and a colour that binds as
/// three uniforms should be stored as three values. The `group` field ties
/// them back together for the inspector.
std::vector<ParamSpec> colourParams(const std::string& base, const std::string& label,
                                    double r, double g, double b, const std::string& group)
{
    return {
        param(base + "R", label + " R", r, 0.0, 1.0, ParamKind::Colour, group),
        param(base + "G", label + " G", g, 0.0, 1.0, ParamKind::Colour, group),
        param(base + "B", label + " B", b, 0.0, 1.0, ParamKind::Colour, group),
    };
}

void append(std::vector<ParamSpec>& into, std::vector<ParamSpec> from)
{
    into.insert(into.end(), std::make_move_iterator(from.begin()),
                std::make_move_iterator(from.end()));
}

} // namespace

EffectCatalog::EffectCatalog()
{
    auto add = [this](EffectSpec spec) {
        _effects.push_back(std::move(spec));
    };

    // -----------------------------------------------------------------------
    // Colour
    // -----------------------------------------------------------------------

    add(effect("brightness_contrast", "Brightness & Contrast", "Colour", "brightness_contrast",
         {
             param("brightness", "Brightness", 0.0, -1.0, 1.0),
             param("contrast", "Contrast", 1.0, 0.0, 3.0),
         }));

    add(effect("exposure_gamma", "Exposure & Gamma", "Colour", "exposure_gamma",
         {
             param("exposure", "Exposure", 0.0, -5.0, 5.0),
             param("offset", "Offset", 0.0, -1.0, 1.0),
             param("gamma", "Gamma", 1.0, 0.1, 4.0),
         }));

    add(effect("hue_saturation", "Hue & Saturation", "Colour", "hue_saturation",
         {
             angle("hue", "Hue", 0.0),
             param("saturation", "Saturation", 1.0, 0.0, 3.0),
             param("lightness", "Lightness", 1.0, 0.0, 3.0),
         }));

    add(effect("hsl_adjust", "HSL Adjust", "Colour", "hsl_adjust",
         {
             angle("hue", "Hue Shift", 0.0),
             param("saturation", "Saturation", 0.0, -1.0, 1.0),
             param("lightness", "Lightness", 0.0, -1.0, 1.0),
         }));

    add(effect("levels", "Levels", "Colour", "levels",
         {
             param("inBlack", "Input Black", 0.0, 0.0, 1.0),
             param("inWhite", "Input White", 1.0, 0.0, 1.0),
             param("gamma", "Gamma", 1.0, 0.1, 4.0),
             param("outBlack", "Output Black", 0.0, 0.0, 1.0),
             param("outWhite", "Output White", 1.0, 0.0, 1.0),
         }));

    add(effect("lut_apply", "Colour LUT", "Colour", "lut_apply",
         {
             logScale("strength", "Strength", 1.0, 0.0, 1.0),
         }));

    add(effect("invert", "Invert", "Colour", "invert",
         {
             param("width", "Blend Width", 0.5, 0.0, 1.0),
             param("intensity", "Intensity", 1.0, 0.0, 1.0),
             toggle("blend", "Blend With Original", false),
         }));

    add(effect("solid_color", "Fill Colour", "Colour", "solid_color",
         [&] {
             std::vector<ParamSpec> p = colourParams("color", "Colour", 1.0, 1.0, 1.0, "Colour");
             append(p, {param("opacity", "Opacity", 1.0, 0.0, 1.0)});
             return p;
         }()));

    add(effect("opacity", "Opacity", "Colour", "opacity",
         {
             param("alpha", "Opacity", 1.0, 0.0, 1.0),
         }));

    // -----------------------------------------------------------------------
    // Keying
    // -----------------------------------------------------------------------

    add(effect("chroma_key", "Chroma Key", "Keying", "chroma_key",
         [&] {
             std::vector<ParamSpec> p = colourParams("key", "Key Colour", 0.0, 1.0, 0.0, "Key");
             append(p, {
                 logScale("tolerance", "Tolerance", 0.25, 0.001, 1.0, "Matte"),
                 logScale("softness", "Softness", 0.08, 0.0, 1.0, "Matte"),
                 logScale("preBlur", "Pre-Blur", 0.0, 0.0, 40.0, "Matte"),
                 param("clipBlack", "Clip Black", 0.0, 0.0, 1.0, ParamKind::Scalar, "Matte"),
                 param("clipWhite", "Clip White", 1.0, 0.0, 1.0, ParamKind::Scalar, "Matte"),
                 param("spill", "Spill Suppression", 1.0, 0.0, 1.0, ParamKind::Scalar, "Spill"),
                 param("spillBias", "Spill Bias", 0.5, 0.0, 1.0, ParamKind::Scalar, "Spill"),
             });
             return p;
         }()));

    // -----------------------------------------------------------------------
    // Blur
    // -----------------------------------------------------------------------

    add(effect("box_blur", "Box Blur", "Blur", "box_blur",
         {
             logScale("radius", "Radius", 8.0, 0.0, 200.0),
             param("scaleX", "Stretch X", 1.0, 0.0, 4.0),
             param("scaleY", "Stretch Y", 1.0, 0.0, 4.0),
             toggle("repeatEdge", "Repeat Edge Pixels", true),
         }));

    add(effect("bokeh_iris_blur", "Bokeh Blur", "Blur", "bokeh_iris_blur",
         {
             logScale("radius", "Radius", 12.0, 0.0, 200.0),
             param("scaleX", "Stretch X", 1.0, 0.0, 4.0),
             param("scaleY", "Stretch Y", 1.0, 0.0, 4.0),
             choice("irisShape", "Aperture", 0,
                    {"Circle", "Triangle", "Square", "Pentagon", "Hexagon",
                     "Heptagon", "Octagon", "Star"}),
             param("irisCurvature", "Blade Curvature", 0.0, -1.0, 1.0),
             angle("rotation", "Rotation", 0.0),
             toggle("bokeh", "Flat Disc", true),
             param("bokehShading", "Bokeh Shading", 0.0, -1.0, 1.0),
             toggle("repeatEdge", "Repeat Edge Pixels", true),
         }));

    // The separable blur is two passes: one along each axis. Two programs that
    // differ by a constant direction vector are clearer as two programs than
    // as one with a branch, and the renderer runs the pair in order.
    add(effect("blur_simple", "Blur", "Blur", "blur_simple_x",
         {
             logScale("radius", "Radius", 8.0, 0.0, 200.0),
             toggle("repeatEdge", "Repeat Edge Pixels", true),
         }, false, false, false, 2, {"blur_simple_x", "blur_simple_y"}));

    add(effect("blur_masked", "Masked Blur", "Blur", "blur_masked",
         {
             logScale("radius", "Radius", 8.0, 0.0, 200.0),
         }));

    add(effect("matte_choke", "Matte Choke", "Blur", "matte_choke",
         {
             param("choke", "Choke", 0.0, -40.0, 40.0),
         }));

    // A "Soften" effect belongs here. Its shader was chunked by the Android
    // build's shrinker, and the join between the two surviving halves was
    // deduplicated away — the head ends mid-expression at
    // "vTex + o * uTexelSize * " and what the loop multiplies by is gone.
    // Both candidates (a horizontal or a vertical axis) are consistent with
    // what survives, so shipping either would be a guess. See docs/SHADERS.md.

    // -----------------------------------------------------------------------
    // Glow
    // -----------------------------------------------------------------------

    add(effect("glow_bloom", "Glow", "Glow", "glow_bloom",
         [&] {
             std::vector<ParamSpec> p = {
                 logScale("radius", "Radius", 24.0, 0.0, 400.0, "Glow"),
                 param("exposure", "Exposure", 0.0, -5.0, 5.0, ParamKind::Scalar, "Glow"),
                 param("chromatic", "Chromatic Aberration", 0.0, 0.0, 1.0, ParamKind::Scalar, "Glow"),
                 param("glowOnly", "Glow Only", 0.0, 0.0, 1.0, ParamKind::Toggle, "Glow"),
                 param("addMode", "Additive", 1.0, 0.0, 1.0, ParamKind::Toggle, "Glow"),
                 param("headroom", "Headroom", 1.0, 0.0, 4.0, ParamKind::Scalar, "Advanced"),
             };
             append(p, colourParams("tint", "Tint", 1.0, 1.0, 1.0, "Tint"));
             append(p, {
                 toggle("tintEnabled", "Tint", false, "Tint"),
                 param("tintAmount", "Tint Amount", 0.0, 0.0, 1.0, ParamKind::Scalar, "Tint"),
             });
             return p;
         }(), false, true, false, 1));

    add(effect("highlight_recovery", "Highlight Recovery", "Glow", "highlight_recovery",
         {
             param("headroom", "Headroom", 1.0, 0.0, 8.0),
             param("gamma", "Gamma", 1.0, 0.1, 4.0),
             choice("highlightMode", "Mode", 0, {"Recover", "Boost", "Both"}),
             param("highlightBoost", "Boost", 0.0, 0.0, 4.0),
             param("highlightThreshold", "Highlight Threshold", 0.75, 0.0, 1.0),
             param("suppressThreshold", "Suppress Threshold", 0.9, 0.0, 1.0),
             param("boostSoften", "Boost Softness", 0.1, 0.0, 1.0),
         }));

    add(effect("highlight_suppress", "Highlight Suppress", "Glow", "highlight_suppress",
         {
             param("headroom", "Headroom", 1.0, 0.0, 8.0),
             param("threshold", "Threshold", 0.8, 0.0, 1.0),
             param("softness", "Softness", 0.1, 0.0, 1.0),
         }));

    // -----------------------------------------------------------------------
    // Stylise
    // -----------------------------------------------------------------------

    add(effect("halftone_cmyk", "Halftone (CMYK)", "Stylise", "halftone_cmyk",
         [&] {
             std::vector<ParamSpec> p = {
                 logScale("cellSize", "Cell Size", 8.0, 1.0, 64.0, "Screen"),
                 angle("angle", "Screen Angle", 45.0, "Screen"),
                 param("cmyk", "CMYK", 1.0, 0.0, 1.0, ParamKind::Toggle, "Screen"),
                 param("gain", "Gain", 1.0, 0.0, 4.0, ParamKind::Scalar, "Screen"),
                 param("softness", "Dot Softness", 0.1, 0.0, 1.0, ParamKind::Scalar, "Screen"),
             };
             append(p, colourParams("ink", "Ink", 0.0, 0.0, 0.0, "Ink"));
             append(p, {
                 toggle("paperEnabled", "Paper", false, "Paper"),
             });
             append(p, colourParams("paper", "Paper", 1.0, 1.0, 1.0, "Paper"));
             return p;
         }()));

    add(effect("pixelate_cells", "Pixelate", "Stylise", "pixelate_cells",
         [&] {
             std::vector<ParamSpec> p = {
                 logScale("cellSize", "Cell Size", 16.0, 1.0, 256.0),
                 param("gap", "Cell Gap", 0.0, 0.0, 0.5),
                 toggle("roundCells", "Round Cells", false),
                 param("shade", "Cell Shading", 0.0, -1.0, 1.0),
                 param("vignette", "Vignette", 0.0, 0.0, 1.0),
                 toggle("backEnabled", "Background", false, "Background"),
             };
             append(p, colourParams("back", "Background", 0.0, 0.0, 0.0, "Background"));
             return p;
         }()));

    add(effect("scanlines", "Scanlines", "Stylise", "scanlines",
         {
             toggle("invert", "Invert", false),
             logScale("width", "Width", 2.0, 0.5, 32.0),
             param("intensity", "Intensity", 0.5, 0.0, 1.0),
             toggle("blend", "Blend With Original", false),
         }));

    add(effect("noise_fractal", "Fractal Noise", "Stylise", "noise_fractal",
         {
             choice("fractalType", "Type", 0, {"Basic", "Turbulent", "Threads"}),
             choice("noiseType", "Noise", 0, {"Soft", "Smooth", "Sharp"}),
             toggle("invert", "Invert", false),
             param("contrast", "Contrast", 1.0, 0.0, 4.0),
             param("brightness", "Brightness", 0.0, -1.0, 1.0),
             choice("overflow", "Overflow", 0, {"Clip", "Wrap"}),
             angle("rotation", "Rotation", 0.0),
             logScale("scale", "Scale", 100.0, 1.0, 1000.0),
             param("scaleWidth", "Scale Width", 1.0, 0.0, 4.0),
             param("scaleHeight", "Scale Height", 1.0, 0.0, 4.0),
             unbounded("offsetX", "Offset X", 0.0, -2000.0, 2000.0),
             unbounded("offsetY", "Offset Y", 0.0, -2000.0, 2000.0),
             param("complexity", "Complexity", 1.0, 1.0, 8.0, ParamKind::Scalar, "Sub"),
             param("subInfluence", "Sub Influence", 0.5, 0.0, 1.0, ParamKind::Scalar, "Sub"),
             param("subScaling", "Sub Scaling", 0.5, 0.0, 1.0, ParamKind::Scalar, "Sub"),
             angle("subRotation", "Sub Rotation", 0.0, "Sub"),
             unbounded("subOffsetX", "Sub Offset X", 0.0, -2000.0, 2000.0, "Sub"),
             unbounded("subOffsetY", "Sub Offset Y", 0.0, -2000.0, 2000.0, "Sub"),
             angle("evolution", "Evolution", 0.0),
             param("randomSeed", "Random Seed", 1.0, 0.0, 1000.0),
             toggle("fitAlpha", "Fit To Layer Alpha", false),
             param("opacity", "Opacity", 1.0, 0.0, 1.0),
         }));

    add(effect("pattern_ascii", "ASCII", "Stylise", "pattern_ascii",
         {
             logScale("tileWidth", "Cell Width", 8.0, 2.0, 64.0, "Grid"),
             logScale("tileHeight", "Cell Height", 12.0, 2.0, 64.0, "Grid"),
             unbounded("tileCenterX", "Centre X", 0.5, -1.0, 2.0, "Grid"),
             unbounded("tileCenterY", "Centre Y", 0.5, -1.0, 2.0, "Grid"),
             toggle("mirrorEdges", "Mirror Edges", false, "Grid"),
             angle("phase", "Phase", 0.0, "Glyph"),
             param("horizontalPhaseShift", "Horizontal Shift", 0.0, -1.0, 1.0, ParamKind::Scalar, "Glyph"),
             param("outputWidth", "Output Width", 0.0, 0.0, 1.0, ParamKind::Scalar, "Output"),
             param("outputHeight", "Output Height", 0.0, 0.0, 1.0, ParamKind::Scalar, "Output"),
         }));

    add(effect("sharpen", "Sharpen", "Stylise", "sharpen",
         {
             logScale("edgeThickness", "Radius", 2.0, 0.1, 40.0),
         }));

    // -----------------------------------------------------------------------
    // Generate
    // -----------------------------------------------------------------------

    add(effect("audio_spectrum", "Audio Spectrum", "Generate", "audio_spectrum",
         [&] {
             std::vector<ParamSpec> p = {
                 param("bands", "Bands", 64.0, 8.0, 256.0, ParamKind::Scalar, "Layout"),
                 param("startX", "Start X", 0.5, 0.0, 1.0, ParamKind::Scalar, "Layout"),
                 param("startY", "Start Y", 0.5, 0.0, 1.0, ParamKind::Scalar, "Layout"),
                 param("endX", "End X", 0.5, 0.0, 1.0, ParamKind::Scalar, "Layout"),
                 param("endY", "End Y", 1.0, 0.0, 1.0, ParamKind::Scalar, "Layout"),
                 toggle("polar", "Polar", false, "Layout"),
                 param("maxHeight", "Max Height", 0.3, 0.0, 1.0, ParamKind::Scalar, "Shape"),
                 param("thickness", "Thickness", 0.6, 0.0, 1.0, ParamKind::Scalar, "Shape"),
                 param("softness", "Softness", 0.1, 0.0, 1.0, ParamKind::Scalar, "Shape"),
                 choice("display", "Display", 0, {"Bars", "Dots", "Lines"}, "Shape"),
                 choice("side", "Side", 0, {"Both", "Left", "Right"}, "Shape"),
             };
             append(p, colourParams("in", "Low Colour", 0.2, 0.6, 1.0, "Colour"));
             append(p, colourParams("out", "High Colour", 1.0, 0.3, 0.6, "Colour"));
             append(p, {
                 toggle("hueInterp", "Hue Interpolate", true, "Colour"),
                 toggle("dynamicHue", "Dynamic Hue", false, "Colour"),
                 toggle("colorSymmetry", "Colour Symmetry", true, "Colour"),
                 toggle("compositeOnOriginal", "Composite On Original", true, "Output"),
             });
             return p;
         }(), false, false, false, 1));

    add(effect("gradient_4color", "4-Colour Gradient", "Generate", "gradient_4color",
         [&] {
             std::vector<ParamSpec> p = {
                 unbounded("P1x", "Point 1 X", 0.0, -1.0, 2.0, "Points"),
                 unbounded("P1y", "Point 1 Y", 0.0, -1.0, 2.0, "Points"),
                 unbounded("P2x", "Point 2 X", 1.0, -1.0, 2.0, "Points"),
                 unbounded("P2y", "Point 2 Y", 0.0, -1.0, 2.0, "Points"),
                 unbounded("P3x", "Point 3 X", 0.0, -1.0, 2.0, "Points"),
                 unbounded("P3y", "Point 3 Y", 1.0, -1.0, 2.0, "Points"),
                 unbounded("P4x", "Point 4 X", 1.0, -1.0, 2.0, "Points"),
                 unbounded("P4y", "Point 4 Y", 1.0, -1.0, 2.0, "Points"),
             };
             append(p, colourParams("C1", "Colour 1", 1.0, 0.0, 0.0, "Colours"));
             append(p, colourParams("C2", "Colour 2", 0.0, 1.0, 0.0, "Colours"));
             append(p, colourParams("C3", "Colour 3", 0.0, 0.0, 1.0, "Colours"));
             append(p, colourParams("C4", "Colour 4", 1.0, 1.0, 0.0, "Colours"));
             append(p, {
                 param("blend", "Blend", 1.0, 0.0, 1.0, ParamKind::Scalar, "Output"),
                 toggle("fitAlpha", "Fit To Layer Alpha", false, "Output"),
                 toggle("blendOriginal", "Blend With Original", false, "Output"),
             });
             return p;
         }()));

    add(effect("gradient_linear_radial", "Gradient", "Generate", "gradient_linear_radial",
         [&] {
             std::vector<ParamSpec> p = {
                 unbounded("startX", "Start X", 0.0, -1.0, 2.0, "Points"),
                 unbounded("startY", "Start Y", 0.0, -1.0, 2.0, "Points"),
                 unbounded("endX", "End X", 1.0, -1.0, 2.0, "Points"),
                 unbounded("endY", "End Y", 1.0, -1.0, 2.0, "Points"),
                 toggle("radial", "Radial", false, "Shape"),
             };
             append(p, colourParams("start", "Start Colour", 0.0, 0.0, 0.0, "Colours"));
             append(p, colourParams("end", "End Colour", 1.0, 1.0, 1.0, "Colours"));
             append(p, {
                 toggle("fitAlpha", "Fit To Layer Alpha", false, "Output"),
                 toggle("blendOriginal", "Blend With Original", false, "Output"),
             });
             return p;
         }()));

    // Shapes share a parameter block; the shader differs in how it measures.
    auto shapeParams = [&](const char* sizeName, const char* sizeLabel, double sizeDefault,
                           bool hasBorder, bool hasSoftness, bool hasCount) {
        std::vector<ParamSpec> p;
        if (hasCount) {
            p.push_back(param("count", "Count", 6.0, 1.0, 64.0, ParamKind::Scalar, "Shape"));
        }
        if (sizeName[0] == 's') {
            p.push_back(param("size", sizeLabel, sizeDefault, 0.0, 2.0, ParamKind::Scalar, "Shape"));
        } else {
            p.push_back(param("width", "Width", sizeDefault, 0.0, 2.0, ParamKind::Scalar, "Shape"));
            p.push_back(param("height", "Height", sizeDefault, 0.0, 2.0, ParamKind::Scalar, "Shape"));
        }
        if (hasCount) {
            p.push_back(param("innerRadius", "Inner Radius", 0.2, 0.0, 1.0, ParamKind::Scalar, "Shape"));
            p.push_back(param("radius", "Radius", 0.4, 0.0, 1.0, ParamKind::Scalar, "Shape"));
            p.push_back(param("width", "Stroke Width", 0.05, 0.0, 1.0, ParamKind::Scalar, "Shape"));
        }
        p.push_back(unbounded("anchorX", "Anchor X", 0.5, -1.0, 2.0, "Transform"));
        p.push_back(unbounded("anchorY", "Anchor Y", 0.5, -1.0, 2.0, "Transform"));
        p.push_back(angle("rotation", "Rotation", 0.0, "Transform"));
        if (hasBorder) {
            p.push_back(param("border", "Border", 0.0, 0.0, 0.5, ParamKind::Scalar, "Shape"));
        }
        if (hasSoftness) {
            p.push_back(param("softness", "Softness", 0.0, 0.0, 1.0, ParamKind::Scalar, "Shape"));
        }
        p.push_back(logScale("feather", "Feather", 0.0, 0.0, 1.0, "Shape"));
        p.push_back(toggle("invert", "Invert", false, "Shape"));
        append(p, colourParams("color", "Colour", 1.0, 1.0, 1.0, "Fill"));
        p.push_back(param("opacity", "Opacity", 1.0, 0.0, 1.0, ParamKind::Scalar, "Fill"));
        p.push_back(toggle("bgFill", "Fill Background", false, "Background"));
        append(p, colourParams("bg", "Background", 0.0, 0.0, 0.0, "Background"));
        p.push_back(toggle("fitAlpha", "Fit To Layer Alpha", false, "Output"));
        return p;
    };

    add(effect("shape_circle", "Circle", "Generate", "shape_circle",
         shapeParams("size", "Size", 0.5, false, true, false)));

    add(effect("shape_rect", "Rectangle", "Generate", "shape_rect",
         shapeParams("width", "Width", 0.6, true, false, false)));

    add(effect("shape_ellipse", "Ellipse", "Generate", "shape_ellipse",
         shapeParams("width", "Width", 0.6, false, false, false)));

    add(effect("shape_radial_repeat", "Radial Repeat", "Generate", "shape_radial_repeat",
         shapeParams("size", "Size", 0.5, false, true, true)));

    add(effect("light_flare", "Light Flare", "Generate", "light_flare",
         [&] {
             std::vector<ParamSpec> p = {
                 unbounded("centerX", "Centre X", 0.5, -1.0, 2.0, "Position"),
                 unbounded("centerY", "Centre Y", 0.5, -1.0, 2.0, "Position"),
                 angle("direction", "Direction", 0.0, "Shape"),
                 choice("shape", "Shape", 0, {"Round", "Anamorphic", "Star"}, "Shape"),
                 logScale("width", "Width", 0.3, 0.01, 2.0, "Shape"),
                 param("sweepIntensity", "Sweep Intensity", 0.5, 0.0, 1.0, ParamKind::Scalar, "Glow"),
                 param("edgeIntensity", "Edge Intensity", 0.5, 0.0, 1.0, ParamKind::Scalar, "Glow"),
                 logScale("edgeThickness", "Edge Thickness", 0.1, 0.001, 1.0, "Glow"),
                 param("lightReception", "Light Reception", 1.0, 0.0, 1.0, ParamKind::Scalar, "Glow"),
             };
             append(p, colourParams("color", "Colour", 1.0, 0.9, 0.7, "Colour"));
             return p;
         }()));

    // -----------------------------------------------------------------------
    // Motion
    // -----------------------------------------------------------------------

    add(effect("motion_wiggle", "Wiggle", "Motion", "motion_wiggle",
         {
             param("speed", "Speed", 1.0, 0.0, 10.0, ParamKind::Scalar, "Motion"),
             param("strength", "Strength", 0.1, 0.0, 1.0, ParamKind::Scalar, "Motion"),
             angle("angle", "Angle", 0.0, "Motion"),
             angle("phase", "Phase", 0.0, "Motion"),
             param("decay", "Decay", 0.0, 0.0, 1.0, ParamKind::Scalar, "Motion"),
         }));

    add(effect("motion_ripple", "Ripple", "Motion", "motion_ripple",
         {
             param("frequency", "Frequency", 8.0, 0.1, 100.0, ParamKind::Scalar, "Wave"),
             param("strength", "Strength", 0.05, 0.0, 0.5, ParamKind::Scalar, "Wave"),
             angle("rotation", "Rotation", 0.0, "Wave"),
             param("scale", "Scale", 1.0, 0.0, 4.0, ParamKind::Scalar, "Wave"),
             param("soften", "Soften", 0.0, 0.0, 1.0, ParamKind::Scalar, "Wave"),
             param("decay", "Decay", 0.0, 0.0, 1.0, ParamKind::Scalar, "Wave"),
             param("seed", "Random Seed", 1.0, 0.0, 1000.0, ParamKind::Scalar, "Wave"),
         }));

    add(effect("motion_swirl", "Swirl", "Motion", "motion_swirl",
         {
             param("speed", "Speed", 1.0, 0.0, 10.0, ParamKind::Scalar, "Swirl"),
             angle("angle", "Angle", 0.0, "Swirl"),
             unbounded("pivotX", "Pivot X", 0.5, -1.0, 2.0, "Swirl"),
             unbounded("pivotY", "Pivot Y", 0.5, -1.0, 2.0, "Swirl"),
             angle("phase", "Phase", 0.0, "Swirl"),
             param("decay", "Decay", 0.0, 0.0, 1.0, ParamKind::Scalar, "Swirl"),
         }));

    add(effect("transform_2d", "Transform", "Motion", "transform_2d",
         {
             unbounded("positionX", "Position X", 0.0, -4000.0, 4000.0, "Transform"),
             unbounded("positionY", "Position Y", 0.0, -4000.0, 4000.0, "Transform"),
             param("scale", "Scale", 1.0, 0.0, 10.0, ParamKind::Scalar, "Transform"),
             angle("rotation", "Rotation", 0.0, "Transform"),
             unbounded("anchorX", "Anchor X", 0.5, -1.0, 2.0, "Transform"),
             unbounded("anchorY", "Anchor Y", 0.5, -1.0, 2.0, "Transform"),
             param("opacity", "Opacity", 1.0, 0.0, 1.0, ParamKind::Scalar, "Transform"),
         }));

    add(effect("motion_blur_directional", "Directional Motion Blur", "Motion",
         "motion_blur_directional",
         {
             logScale("maxVelPx", "Max Velocity", 40.0, 0.0, 400.0),
             param("samples", "Samples", 12.0, 1.0, 64.0),
         }));

    // -----------------------------------------------------------------------
    // Transition
    // -----------------------------------------------------------------------

    add(effect("wipe_rect", "Rectangular Wipe", "Transition", "wipe_rect",
         {
             param("left", "Left", 0.0, 0.0, 1.0, ParamKind::Scalar, "Bounds"),
             param("top", "Top", 0.0, 0.0, 1.0, ParamKind::Scalar, "Bounds"),
             param("right", "Right", 1.0, 0.0, 1.0, ParamKind::Scalar, "Bounds"),
             param("bottom", "Bottom", 1.0, 0.0, 1.0, ParamKind::Scalar, "Bounds"),
         }));

    add(effect("wipe_linear", "Linear Wipe", "Transition", "wipe_linear",
         {
             param("completion", "Completion", 0.5, 0.0, 1.0),
             angle("angle", "Angle", 0.0),
             logScale("feather", "Feather", 0.05, 0.0, 1.0),
         }));

    add(effect("wipe_radial", "Radial Wipe", "Transition", "wipe_radial",
         {
             param("completion", "Completion", 0.5, 0.0, 1.0),
             angle("startAngle", "Start Angle", 0.0),
             unbounded("centerX", "Centre X", 0.5, -1.0, 2.0),
             unbounded("centerY", "Centre Y", 0.5, -1.0, 2.0),
             toggle("clockwise", "Clockwise", true),
             logScale("feather", "Feather", 0.05, 0.0, 1.0),
         }));

    // -----------------------------------------------------------------------
    // Distort
    // -----------------------------------------------------------------------

    add(effect("chromatic_aberration", "Chromatic Aberration", "Distort",
         "chromatic_aberration",
         {
             param("spread", "Spread", 0.01, 0.0, 0.2),
             param("shift", "Shift", 0.0, -0.1, 0.1),
             angle("angle", "Angle", 0.0),
         }));

    add(effect("grain", "Grain", "Distort", "grain",
         {
             param("amount", "Amount", 0.1, 0.0, 1.0),
             logScale("size", "Size", 1.0, 0.1, 20.0),
             param("softness", "Softness", 0.5, 0.0, 1.0),
         }));

    add(effect("edge_outline", "Outline", "Distort", "edge_outline",
         {
             angle("direction", "Direction", 0.0),
             logScale("distance", "Distance", 4.0, 0.0, 100.0),
             param("softness", "Softness", 0.0, 0.0, 1.0),
         }));

    add(effect("edge_detect", "Edge Detect", "Distort", "edge_detect",
         {
             logScale("width", "Width", 2.0, 0.1, 40.0),
         }));

    add(effect("stroke_glow", "Stroke", "Distort", "stroke_glow",
         [&] {
             std::vector<ParamSpec> p = {
                 logScale("width", "Width", 4.0, 0.1, 100.0),
             };
             append(p, colourParams("color", "Colour", 1.0, 1.0, 1.0, "Colour"));
             return p;
         }()));

    add(effect("drop_shadow", "Drop Shadow", "Distort", "drop_shadow",
         [&] {
             std::vector<ParamSpec> p = colourParams("shadow", "Shadow", 0.0, 0.0, 0.0, "Shadow");
             append(p, {
                 param("opacity", "Opacity", 0.5, 0.0, 1.0, ParamKind::Scalar, "Shadow"),
                 logScale("softness", "Softness", 8.0, 0.0, 100.0, "Shadow"),
             });
             return p;
         }()));

    // -----------------------------------------------------------------------
    // Composite
    // -----------------------------------------------------------------------

    add(effect("blend_backdrop", "Blend With Backdrop", "Composite", "blend_backdrop",
         {
             param("alpha", "Opacity", 1.0, 0.0, 1.0),
             param("strength", "Blend Strength", 1.0, 0.0, 1.0),
             choice("mode", "Mode", 0,
                    {"Normal", "Darken", "Multiply", "Colour Burn", "Linear Burn",
                     "Lighten", "Screen", "Colour Dodge", "Linear Dodge", "Overlay",
                     "Soft Light", "Hard Light", "Vivid Light", "Difference",
                     "Exclusion", "Subtract", "Divide", "Hue", "Saturation",
                     "Colour", "Luminosity"}),
         }, false, true, false, 1));

    add(effect("adjustment_layer", "Adjustment Layer", "Composite", "adjustment_layer",
         {
             param("alpha", "Opacity", 1.0, 0.0, 1.0),
             param("strength", "Strength", 1.0, 0.0, 1.0),
             choice("mode", "Mode", 0,
                    {"Normal", "Darken", "Multiply", "Colour Burn", "Linear Burn",
                     "Lighten", "Screen", "Colour Dodge", "Linear Dodge", "Overlay",
                     "Soft Light", "Hard Light", "Vivid Light", "Difference",
                     "Exclusion", "Subtract", "Divide", "Hue", "Saturation",
                     "Colour", "Luminosity"}),
             toggle("useMask", "Use Mask", false),
         }, false, true, true, 1));

    add(effect("mask_apply", "Apply Mask", "Composite", "mask_apply",
         {
             choice("maskMode", "Mask Source", 0,
                    {"Alpha", "Luma", "Alpha Inverted", "Luma Inverted"}),
         }, false, true, true, 1));

    // -----------------------------------------------------------------------
    // Category order for the browser. Declared here rather than derived from
    // the effects above, because a category with no effects yet should still
    // hold its place in the list.
    // -----------------------------------------------------------------------

    _categories = {
        "Colour", "Keying", "Blur", "Glow", "Stylise", "Generate",
        "Motion", "Transition", "Distort", "Composite",
    };

    // Stable order within the catalogue: by category, then by name. The
    // browser sorts for display anyway; this makes the order deterministic for
    // the tests and for anything that walks the list.
    std::stable_sort(_effects.begin(), _effects.end(),
                     [&](const EffectSpec& a, const EffectSpec& b) {
                         const auto ca = std::find(_categories.begin(), _categories.end(), a.category);
                         const auto cb = std::find(_categories.begin(), _categories.end(), b.category);
                         if (ca != cb) return ca < cb;
                         return a.name < b.name;
                     });
}

const EffectCatalog& EffectCatalog::instance()
{
    // Function-local static: constructed on first use, thread-safe since C++11,
    // and no static-initialisation-order problem with the shader table.
    static const EffectCatalog catalog;
    return catalog;
}

const EffectSpec* EffectCatalog::find(const std::string& id) const
{
    for (const EffectSpec& spec : _effects) {
        if (spec.id == id) return &spec;
    }
    return nullptr;
}

} // namespace keyflow
