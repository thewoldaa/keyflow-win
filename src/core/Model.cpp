#include "core/Model.h"

#include <algorithm>
#include <cmath>

namespace keyflow {

namespace {

/// Cubic Bezier easing, solved for y at a given x.
///
/// A CSS-style cubic-bezier: the curve runs from (0,0) to (1,1) with control
/// points (x1,y1) and (x2,y2). x is the normalised time between the two
/// keyframes and y is the normalised value between them.
///
/// Solved by bisection rather than Newton. Bisection is slower per iteration
/// but it cannot diverge, and this runs per parameter per frame — a divergent
/// solve would produce a NaN value that propagates into the shader and blanks
/// the frame, which is far worse than a few extra iterations.
double cubicBezier(double x, double x1, double y1, double x2, double y2)
{
    if (x <= 0.0) return 0.0;
    if (x >= 1.0) return 1.0;

    auto sampleX = [&](double t) {
        const double u = 1.0 - t;
        return 3.0 * u * u * t * x1 + 3.0 * u * t * t * x2 + t * t * t;
    };
    auto sampleY = [&](double t) {
        const double u = 1.0 - t;
        return 3.0 * u * u * t * y1 + 3.0 * u * t * t * y2 + t * t * t;
    };

    double lo = 0.0;
    double hi = 1.0;
    double t = x; // a decent first guess; bisection converges from anywhere
    for (int i = 0; i < 24; ++i) {
        t = 0.5 * (lo + hi);
        const double sx = sampleX(t);
        if (sx < x) {
            lo = t;
        } else {
            hi = t;
        }
        if (hi - lo < 1e-7) break;
    }
    return sampleY(t);
}

double easeInOut(double t)
{
    // The standard S-curve the app uses for its default keyframes. Named
    // rather than inlined so the default is stated in one place.
    return t * t * (3.0 - 2.0 * t);
}

} // namespace

// --- AnimatedValue --------------------------------------------------------

double AnimatedValue::evaluate(double time) const
{
    if (keyframes.empty()) return constant;
    if (keyframes.size() == 1) return keyframes.front().value;

    // Before the first and after the last keyframe the value holds. Holding
    // rather than extrapolating is what every editor does, and it is the only
    // choice that does not produce runaway values from a linear fit.
    if (time <= keyframes.front().time) return keyframes.front().value;
    if (time >= keyframes.back().time) return keyframes.back().value;

    // Find the pair bracketing `time`. A linear scan is right here: effect
    // stacks have a handful of keyframes each, and the vector is sorted, so a
    // binary search would cost more in cache misses than it saves.
    std::size_t i = 0;
    while (i + 1 < keyframes.size() && keyframes[i + 1].time <= time) ++i;
    if (i + 1 >= keyframes.size()) return keyframes.back().value;

    const Keyframe& a = keyframes[i];
    const Keyframe& b = keyframes[i + 1];

    const double span = b.time - a.time;
    if (span <= 0.0) return b.value;
    const double t = (time - a.time) / span;

    switch (a.interpolation) {
    case Interpolation::Hold:
        return a.value;
    case Interpolation::Linear:
        return a.value + (b.value - a.value) * t;
    case Interpolation::Ease:
        return a.value + (b.value - a.value) * easeInOut(t);
    case Interpolation::Bezier: {
        // Handles are fractions of the gap. Clamped to keep the curve
        // monotonic in x: a handle past 1.0 makes the curve double back, and
        // the solve then has more than one answer.
        const double x1 = std::clamp(a.outHandle, 0.0, 1.0);
        const double x2 = std::clamp(1.0 - b.inHandle, 0.0, 1.0);
        return a.value + (b.value - a.value) * cubicBezier(t, x1, 0.0, x2, 1.0);
    }
    }
    return b.value;
}

void AnimatedValue::setKeyframe(Keyframe k)
{
    // Replace an existing keyframe at this time rather than adding a second.
    // Two keyframes at one time make `span` zero and the value undefined.
    for (auto& existing : keyframes) {
        if (std::fabs(existing.time - k.time) < 1e-9) {
            existing = k;
            return;
        }
    }
    keyframes.push_back(k);
    std::sort(keyframes.begin(), keyframes.end(),
              [](const Keyframe& x, const Keyframe& y) { return x.time < y.time; });
}

bool AnimatedValue::removeKeyframe(double time)
{
    for (auto it = keyframes.begin(); it != keyframes.end(); ++it) {
        if (std::fabs(it->time - time) < 1e-9) {
            keyframes.erase(it);
            return true;
        }
    }
    return false;
}

double AnimatedValue::firstTime() const
{
    return keyframes.empty() ? 0.0 : keyframes.front().time;
}

double AnimatedValue::lastTime() const
{
    return keyframes.empty() ? 0.0 : keyframes.back().time;
}

// --- VectorPath -----------------------------------------------------------

VectorPath VectorPath::fromJson(const json::Value& v)
{
    VectorPath path;
    path.closed = v["closed"].asBool(false);
    for (const json::Value& p : v["points"].asArray()) {
        PathPoint point;
        point.x = p["x"].asNumber(0.0);
        point.y = p["y"].asNumber(0.0);
        point.inX = p["inX"].asNumber(0.0);
        point.inY = p["inY"].asNumber(0.0);
        point.outX = p["outX"].asNumber(0.0);
        point.outY = p["outY"].asNumber(0.0);
        path.points.push_back(point);
    }
    return path;
}

json::Value VectorPath::toJson() const
{
    json::Array out;
    out.reserve(points.size());
    for (const PathPoint& p : points) {
        json::Value jp;
        jp.set("x", p.x);
        jp.set("y", p.y);
        jp.set("inX", p.inX);
        jp.set("inY", p.inY);
        jp.set("outX", p.outX);
        jp.set("outY", p.outY);
        out.push_back(std::move(jp));
    }
    json::Value result;
    result.set("closed", closed);
    result.set("points", json::Value(std::move(out)));
    return result;
}

// --- BlendMode ------------------------------------------------------------

const char* blendModeName(BlendMode mode)
{
    switch (mode) {
    case BlendMode::Normal:       return "normal";
    case BlendMode::Darken:       return "darken";
    case BlendMode::Multiply:     return "multiply";
    case BlendMode::ColorBurn:    return "color-burn";
    case BlendMode::LinearBurn:   return "linear-burn";
    case BlendMode::Lighten:      return "lighten";
    case BlendMode::Screen:       return "screen";
    case BlendMode::ColorDodge:   return "color-dodge";
    case BlendMode::LinearDodge:  return "linear-dodge";
    case BlendMode::Overlay:      return "overlay";
    case BlendMode::SoftLight:    return "soft-light";
    case BlendMode::HardLight:    return "hard-light";
    case BlendMode::VividLight:   return "vivid-light";
    case BlendMode::Difference:   return "difference";
    case BlendMode::Exclusion:    return "exclusion";
    case BlendMode::Subtract:     return "subtract";
    case BlendMode::Divide:       return "divide";
    case BlendMode::Hue:          return "hue";
    case BlendMode::Saturation:   return "saturation";
    case BlendMode::Color:        return "color";
    case BlendMode::Luminosity:   return "luminosity";
    }
    return "normal";
}

BlendMode blendModeFromName(const std::string& name)
{
    for (int i = 0; i <= static_cast<int>(BlendMode::Luminosity); ++i) {
        const auto mode = static_cast<BlendMode>(i);
        if (name == blendModeName(mode)) return mode;
    }
    return BlendMode::Normal;
}

// --- Effect ---------------------------------------------------------------

double Effect::parameter(const std::string& name, double fallback) const
{
    const auto it = parameters.find(name);
    return it == parameters.end() ? fallback : it->second.constant;
}

// --- Layer ----------------------------------------------------------------

Layer::Transform Layer::transformAt(double time) const
{
    Transform t;
    t.x = positionX.evaluate(time);
    t.y = positionY.evaluate(time);
    t.z = positionZ.evaluate(time);
    t.scaleX = scaleX.evaluate(time);
    t.scaleY = scaleY.evaluate(time);
    t.rotation = rotation.evaluate(time);
    t.rotationX = rotationX.evaluate(time);
    t.rotationY = rotationY.evaluate(time);
    t.opacity = opacity.evaluate(time);
    t.anchorX = anchorX.evaluate(time);
    t.anchorY = anchorY.evaluate(time);
    return t;
}

// --- Composition ----------------------------------------------------------

int Composition::frameCount() const
{
    const double rate = fps();
    if (rate <= 0.0 || duration <= 0.0) return 1;
    const int frames = static_cast<int>(std::lround(duration * rate));
    return frames < 1 ? 1 : frames;
}

Layer* Composition::findLayer(const std::string& layerId)
{
    for (Layer& layer : layers) {
        if (layer.id == layerId) return &layer;
    }
    return nullptr;
}

const Layer* Composition::findLayer(const std::string& layerId) const
{
    for (const Layer& layer : layers) {
        if (layer.id == layerId) return &layer;
    }
    return nullptr;
}

int Composition::indexOfLayer(const std::string& layerId) const
{
    for (std::size_t i = 0; i < layers.size(); ++i) {
        if (layers[i].id == layerId) return static_cast<int>(i);
    }
    return -1;
}

// --- Project --------------------------------------------------------------

Composition* Project::findComposition(const std::string& compositionId)
{
    for (Composition& comp : compositions) {
        if (comp.id == compositionId) return &comp;
    }
    return nullptr;
}

const Composition* Project::findComposition(const std::string& compositionId) const
{
    for (const Composition& comp : compositions) {
        if (comp.id == compositionId) return &comp;
    }
    return nullptr;
}

Composition* Project::activeComposition()
{
    if (Composition* found = findComposition(activeCompositionId)) return found;
    return compositions.empty() ? nullptr : &compositions.front();
}

const Composition* Project::activeComposition() const
{
    if (const Composition* found = findComposition(activeCompositionId)) return found;
    return compositions.empty() ? nullptr : &compositions.front();
}

std::vector<std::string> Project::referencedAssetIds() const
{
    std::vector<std::string> ids;
    for (const Composition& comp : compositions) {
        for (const Layer& layer : comp.layers) {
            if (layer.assetId.empty()) continue;
            if (std::find(ids.begin(), ids.end(), layer.assetId) == ids.end()) {
                ids.push_back(layer.assetId);
            }
        }
    }
    return ids;
}

std::string Project::newId(const std::string& prefix) const
{
    // A counter rather than a random id: project files stay readable, and
    // uniqueness only has to hold within one project.
    //
    // Every id in the document is checked, including effect instance ids. A
    // counter that only considered composition and layer ids would hand the
    // same id to two effects, and then the page cannot address one of them:
    // a lookup by instance id finds whichever comes first, so edits land on
    // the wrong effect and reordering silently moves nothing.
    for (int n = 1; n < 1000000; ++n) {
        const std::string candidate = prefix + "-" + std::to_string(n);
        bool taken = false;
        for (const Composition& comp : compositions) {
            if (comp.id == candidate) { taken = true; break; }
            for (const Layer& layer : comp.layers) {
                if (layer.id == candidate) { taken = true; break; }
                for (const Effect& effect : layer.effects) {
                    if (effect.instanceId == candidate) { taken = true; break; }
                }
                if (taken) break;
            }
            if (taken) break;
        }
        if (!taken) return candidate;
    }
    return prefix + "-overflow";
}

Project Project::createDefault(const std::string& name)
{
    Project project;
    project.name = name;

    Composition comp;
    comp.id = "comp-1";
    comp.name = "Composition 1";
    comp.width = 1920;
    comp.height = 1080;
    comp.duration = 10.0;
    project.compositions.push_back(std::move(comp));
    project.activeCompositionId = "comp-1";
    return project;
}

} // namespace keyflow
