// ---------------------------------------------------------------------------
// The OpenGL renderer.
//
// One hidden window with a GL context, used for two things: the interactive
// preview and the offline render. Both go through the same code path, which is
// the point — a preview that composites differently from the render is a
// preview that lies about what the export will look like.
//
// The pipeline is the Android app's, recovered:
//
//   1. Each visible layer is drawn to its own texture in layer-local space.
//   2. The layer's effect stack runs over that texture, ping-ponging between
//      two targets, one pass per effect.
//   3. The result composites onto the frame so far, using the compositor
//      shader with the layer's blend mode.
//
// Premultiplied alpha throughout. That is not a style choice: the blend maths
// and the un-multiply in the chroma key both assume it, and a layer that
// arrives straight-alpha composites with a halo.
// ---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/EffectCatalog.h"
#include "core/Model.h"
#include "media/Media.h"

namespace keyflow {

/// A GL texture handle. Opaque outside the renderer.
using TextureId = unsigned int;

/// One layer's evaluated state at one time, ready to draw.
struct RenderLayer
{
    const Layer* layer = nullptr;

    /// The layer's own texture, premultiplied.
    TextureId texture = 0;
    int width = 0;
    int height = 0;

    /// The evaluated transform.
    Layer::Transform transform;

    /// The evaluated parameter values for each effect in the stack, keyed by
    /// effect instance id then parameter name. Evaluated once per frame rather
    /// than once per effect pass, so a parameter read by two passes cannot
    /// disagree with itself within a frame.
    std::vector<std::vector<double>> effectValues;

    /// Evaluated expressions, keyed the same way. Empty when the parameter is
    /// driven by keyframes.
    std::vector<std::vector<bool>> effectExpressionActive;
};

/// Settings for a render pass.
struct RenderSettings
{
    int width = 1920;
    int height = 1080;

    /// The frame's time in seconds.
    double time = 0.0;

    /// The frame index, for effects that need to know.
    int frame = 0;

    /// True to render at preview quality: smaller intermediate targets and
    /// fewer blur taps. The preview is interactive and the render is not.
    bool preview = true;
};

/// The renderer.
///
/// Owns the GL context and every texture it has created. Non-copyable: the
/// context is a single resource and copying the handle would give two owners.
class Renderer
{
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /// Create the hidden window and the GL context.
    ///
    /// Returns false and fills `error` when the context could not be created.
    /// A machine without a usable GL driver is a real case, and the app should
    /// say so rather than showing a black canvas.
    bool initialise(std::string& error);

    /// True once initialise has succeeded.
    bool ready() const { return _ready; }

    /// Report the driver, for the diagnostics panel.
    std::string driverInfo() const { return _driverInfo; }

    /// Render one frame of a composition.
    ///
    /// `frames` supplies the decoded picture for each media layer, indexed the
    /// same way as the composition's layer list. A layer whose entry is
    /// invalid draws nothing, which is what a missing asset looks like.
    bool renderFrame(const Composition& comp, const RenderSettings& settings,
                     const std::vector<Frame>& frames, std::string& error);

    /// Read the last rendered frame back as RGBA, top row first.
    ///
    /// Top-down because every consumer writes it to a file or hands it to
    /// ffmpeg, and both want top-down.
    bool readback(Frame& out, std::string& error);

    /// Upload a frame as a texture. Returns 0 on failure.
    ///
    /// The renderer owns the texture; the caller does not free it. Textures
    /// are pooled and reused across frames, because allocating one per frame
    /// at 30fps is a way to fragment the driver's memory within a minute.
    TextureId upload(const Frame& frame);

    /// Release every texture and target. Called when a project closes.
    void releaseResources();

    /// Render a two-layer composition and report whether the blend modes
    /// composite correctly.
    ///
    /// Returns false and fills `error` when a mode produces the wrong pixel.
    /// The blend maths has three inputs that can each be wrong independently —
    /// the mode uniform, the backdrop sampler, and the backdrop's own alpha —
    /// and all three produce the same symptom: every mode rendering as a plain
    /// source-over. Reading the result is the only check that covers all three,
    /// which is how all three were found.
    bool checkBlendModes(std::string& error);

    /// Compile every shader in the table and report what failed.
    ///
    /// Public because the only way to know a shader compiles is to hand it to
    /// a driver, and that cannot be done in the unit tests — they have no GL
    /// context. The self-test calls this; a diagnostic panel would too.
    /// Returns the stems that failed, empty when they all compiled.
    std::vector<std::string> validateShaders(std::string& firstError);

private:
    /// A render target: a texture plus the framebuffer that draws into it.
    struct Target
    {
        TextureId texture = 0;
        unsigned int framebuffer = 0;
        int width = 0;
        int height = 0;
    };

    /// Get a target of the given size, reusing one when the pool has it.
    Target acquireTarget(int width, int height);

    /// Run one effect over a source texture into a destination target.
    bool applyEffect(const Effect& effect, const EffectSpec& spec,
                     TextureId source, const Target& destination,
                     const Layer& layer, const Composition& comp,
                     const RenderSettings& settings,
                     const std::vector<double>& values,
                     TextureId backdrop, TextureId previousFrame,
                     std::string& error);

    /// Composite a layer's finished texture onto the frame so far.
    bool compositeLayer(TextureId layerTexture, const Layer& layer,
                        const Composition& comp, const RenderSettings& settings,
                        int targetWidth, int targetHeight, std::string& error);

    /// Draw a full-screen quad. Every pass is a full-screen quad.
    void drawFullscreenQuad();

    /// Build the texture for a layer that has no source file.
    ///
    /// A solid is a flat fill, a shape is its path rasterised, and text is
    /// drawn with the platform's own text engine. Returns false for a layer
    /// kind that has no picture — an adjustment, a null or an audio layer.
    bool generateLayerFrame(const Layer& layer, int width, int height, Frame& out);

    /// Even-odd point-in-polygon, for shape rasterisation.
    static bool pointInPolygon(double x, double y,
                               const std::vector<std::pair<double, double>>& points);

    /// Draw a text layer into a frame using the platform's text engine.
    bool rasteriseText(const Layer& layer, int width, int height, Frame& out);

    /// Compile a program, or return 0 and fill `error`.
    unsigned int compileProgram(const std::string& vertexSource,
                                const std::string& fragmentSource,
                                std::string& error);

    /// Find or build the program for a shader stem.
    unsigned int programFor(const std::string& stem, std::string& error);

    /// Set a float uniform by name. Silently ignores a missing uniform: a
    /// shader that does not read a parameter is not an error, it is a shader
    /// with fewer knobs than the spec offers.
    void setUniform(unsigned int program, const std::string& name, double value);

    /// Set an integer uniform by name.
    ///
    /// Separate from the float overload because the two use different GL
    /// calls, and passing a float to an integer uniform is an error the driver
    /// swallows — the uniform keeps its default of zero and the shader takes a
    /// branch nobody asked for.
    void setUniformInt(unsigned int program, const std::string& name, int value);
    void setUniform(unsigned int program, const std::string& name, float x, float y);
    void setUniform(unsigned int program, const std::string& name,
                    float x, float y, float z, float w);
    void setUniformMatrix3(unsigned int program, const std::string& name,
                           const float* values);

    void beginFrame(int width, int height);

    bool _ready = false;
    std::string _driverInfo;

    /// The hidden window and its device context.
    void* _window = nullptr;       // HWND
    void* _deviceContext = nullptr; // HDC
    void* _glContext = nullptr;    // HGLRC

    /// Compiled programs, keyed by shader stem.
    std::vector<std::pair<std::string, unsigned int>> _programs;

    /// Textures available for reuse.
    std::vector<TextureId> _texturePool;

    /// Render targets available for reuse, keyed by size.
    std::vector<Target> _targetPool;

    /// The frame being built.
    Target _frame;

    /// Where a composite lands before it is copied back into the frame.
    ///
    /// A composite reads the frame as its backdrop and writes a new frame, so
    /// the two cannot be the same texture: reading a texture attached to the
    /// bound framebuffer is undefined, and the driver returns a zero alpha for
    /// it. That zero is what made every blend mode silently composite as a
    /// plain source-over.
    Target _composite;

    Target _scratchA;
    Target _scratchB;

    /// The previous frame, for temporal effects.
    Target _previous;

    /// The unit quad's vertex buffer.
    unsigned int _quadBuffer = 0;
};

} // namespace keyflow
