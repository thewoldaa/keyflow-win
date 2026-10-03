#include "core/Model.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace keyflow {

namespace {

const char* layerKindName(LayerKind kind)
{
    switch (kind) {
    case LayerKind::Media:      return "media";
    case LayerKind::Solid:      return "solid";
    case LayerKind::Text:       return "text";
    case LayerKind::Shape:      return "shape";
    case LayerKind::Adjustment: return "adjustment";
    case LayerKind::Null:       return "null";
    case LayerKind::Audio:      return "audio";
    }
    return "media";
}

LayerKind layerKindFromName(const std::string& name)
{
    if (name == "solid")      return LayerKind::Solid;
    if (name == "text")       return LayerKind::Text;
    if (name == "shape")      return LayerKind::Shape;
    if (name == "adjustment") return LayerKind::Adjustment;
    if (name == "null")       return LayerKind::Null;
    if (name == "audio")      return LayerKind::Audio;
    return LayerKind::Media;
}

const char* interpolationName(Interpolation i)
{
    switch (i) {
    case Interpolation::Hold:   return "hold";
    case Interpolation::Linear: return "linear";
    case Interpolation::Bezier: return "bezier";
    case Interpolation::Ease:   return "ease";
    }
    return "ease";
}

Interpolation interpolationFromName(const std::string& name)
{
    if (name == "hold")   return Interpolation::Hold;
    if (name == "linear") return Interpolation::Linear;
    if (name == "bezier") return Interpolation::Bezier;
    return Interpolation::Ease;
}

json::Value animatedToJson(const AnimatedValue& v)
{
    json::Value out;
    out.set("value", v.constant);
    if (!v.keyframes.empty()) {
        json::Array keys;
        keys.reserve(v.keyframes.size());
        for (const Keyframe& k : v.keyframes) {
            json::Value jk;
            jk.set("time", k.time);
            jk.set("value", k.value);
            jk.set("interp", interpolationName(k.interpolation));
            if (k.interpolation == Interpolation::Bezier) {
                jk.set("inHandle", k.inHandle);
                jk.set("outHandle", k.outHandle);
            }
            keys.push_back(std::move(jk));
        }
        out.set("keys", json::Value(std::move(keys)));
    }
    return out;
}

AnimatedValue animatedFromJson(const json::Value& v, double fallback)
{
    AnimatedValue out;
    out.constant = v["value"].asNumber(fallback);
    for (const json::Value& jk : v["keys"].asArray()) {
        Keyframe k;
        k.time = jk["time"].asNumber(0.0);
        k.value = jk["value"].asNumber(out.constant);
        k.interpolation = interpolationFromName(jk["interp"].asString("ease"));
        k.inHandle = jk["inHandle"].asNumber(0.33);
        k.outHandle = jk["outHandle"].asNumber(0.33);
        out.keyframes.push_back(k);
    }
    // The file is not trusted to be sorted. A hand-edited project with keys
    // out of order would otherwise evaluate against the wrong pair.
    std::sort(out.keyframes.begin(), out.keyframes.end(),
              [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
    return out;
}

json::Value effectToJson(const Effect& effect)
{
    json::Value out;
    out.set("specId", effect.specId);
    out.set("instanceId", effect.instanceId);
    out.set("enabled", effect.enabled);

    json::Value params;
    for (const auto& [name, value] : effect.parameters) {
        params.set(name, animatedToJson(value));
    }
    out.set("params", std::move(params));

    if (!effect.expressions.empty()) {
        json::Value exprs;
        for (const auto& [name, expr] : effect.expressions) {
            json::Value je;
            je.set("source", expr.source);
            je.set("enabled", expr.enabled);
            exprs.set(name, std::move(je));
        }
        out.set("expressions", std::move(exprs));
    }
    return out;
}

Effect effectFromJson(const json::Value& v)
{
    Effect effect;
    effect.specId = v["specId"].asString();
    effect.instanceId = v["instanceId"].asString();
    effect.enabled = v["enabled"].asBool(true);
    for (const auto& [name, jv] : v["params"].asObject()) {
        effect.parameters[name] = animatedFromJson(jv, 0.0);
    }
    for (const auto& [name, jv] : v["expressions"].asObject()) {
        Expression expr;
        expr.source = jv["source"].asString();
        expr.enabled = jv["enabled"].asBool(false);
        effect.expressions[name] = std::move(expr);
    }
    return effect;
}

json::Value layerToJson(const Layer& layer)
{
    json::Value out;
    out.set("id", layer.id);
    out.set("name", layer.name);
    out.set("kind", layerKindName(layer.kind));
    if (!layer.assetId.empty()) out.set("assetId", layer.assetId);
    out.set("visible", layer.visible);
    out.set("locked", layer.locked);
    out.set("trackMatte", layer.trackMatte);
    out.set("inPoint", layer.inPoint);
    out.set("outPoint", layer.outPoint);

    json::Value transform;
    transform.set("positionX", animatedToJson(layer.positionX));
    transform.set("positionY", animatedToJson(layer.positionY));
    transform.set("positionZ", animatedToJson(layer.positionZ));
    transform.set("scaleX", animatedToJson(layer.scaleX));
    transform.set("scaleY", animatedToJson(layer.scaleY));
    transform.set("rotation", animatedToJson(layer.rotation));
    transform.set("rotationX", animatedToJson(layer.rotationX));
    transform.set("rotationY", animatedToJson(layer.rotationY));
    transform.set("opacity", animatedToJson(layer.opacity));
    transform.set("anchorX", animatedToJson(layer.anchorX));
    transform.set("anchorY", animatedToJson(layer.anchorY));
    out.set("transform", std::move(transform));

    if (layer.kind == LayerKind::Solid) {
        json::Value solid;
        solid.set("r", layer.solidR);
        solid.set("g", layer.solidG);
        solid.set("b", layer.solidB);
        solid.set("a", layer.solidA);
        out.set("solid", std::move(solid));
    }

    if (layer.kind == LayerKind::Text) {
        json::Value text;
        text.set("content", layer.text);
        text.set("size", layer.textSize);
        out.set("text", std::move(text));
    }

    if (layer.kind == LayerKind::Shape) {
        json::Value shape;
        shape.set("path", layer.path.toJson());
        shape.set("filled", layer.pathFilled);
        shape.set("stroked", layer.pathStroked);
        shape.set("strokeWidth", layer.strokeWidth);
        json::Value fill;
        fill.set("r", layer.fillR);
        fill.set("g", layer.fillG);
        fill.set("b", layer.fillB);
        shape.set("fill", std::move(fill));
        json::Value stroke;
        stroke.set("r", layer.strokeR);
        stroke.set("g", layer.strokeG);
        stroke.set("b", layer.strokeB);
        shape.set("stroke", std::move(stroke));
        out.set("shape", std::move(shape));
    }

    if (!layer.effects.empty()) {
        json::Array effects;
        effects.reserve(layer.effects.size());
        for (const Effect& effect : layer.effects) {
            effects.push_back(effectToJson(effect));
        }
        out.set("effects", json::Value(std::move(effects)));
    }

    out.set("blend", blendModeName(layer.blend));
    out.set("blendStrength", layer.blendStrength);
    out.set("motionBlurEnabled", layer.motionBlurEnabled);
    out.set("motionBlurShutter", layer.motionBlurShutter);
    return out;
}

Layer layerFromJson(const json::Value& v)
{
    Layer layer;
    layer.id = v["id"].asString();
    layer.name = v["name"].asString("Layer");
    layer.kind = layerKindFromName(v["kind"].asString("media"));
    layer.assetId = v["assetId"].asString();
    layer.visible = v["visible"].asBool(true);
    layer.locked = v["locked"].asBool(false);
    layer.trackMatte = v["trackMatte"].asBool(false);
    layer.inPoint = v["inPoint"].asNumber(0.0);
    layer.outPoint = v["outPoint"].asNumber(10.0);

    const json::Value& t = v["transform"];
    layer.positionX = animatedFromJson(t["positionX"], 0.0);
    layer.positionY = animatedFromJson(t["positionY"], 0.0);
    layer.positionZ = animatedFromJson(t["positionZ"], 0.0);
    layer.scaleX = animatedFromJson(t["scaleX"], 1.0);
    layer.scaleY = animatedFromJson(t["scaleY"], 1.0);
    layer.rotation = animatedFromJson(t["rotation"], 0.0);
    layer.rotationX = animatedFromJson(t["rotationX"], 0.0);
    layer.rotationY = animatedFromJson(t["rotationY"], 0.0);
    layer.opacity = animatedFromJson(t["opacity"], 1.0);
    layer.anchorX = animatedFromJson(t["anchorX"], 0.5);
    layer.anchorY = animatedFromJson(t["anchorY"], 0.5);

    const json::Value& solid = v["solid"];
    layer.solidR = solid["r"].asNumber(1.0);
    layer.solidG = solid["g"].asNumber(1.0);
    layer.solidB = solid["b"].asNumber(1.0);
    layer.solidA = solid["a"].asNumber(1.0);

    const json::Value& text = v["text"];
    layer.text = text["content"].asString();
    layer.textSize = text["size"].asNumber(72.0);

    const json::Value& shape = v["shape"];
    layer.path = VectorPath::fromJson(shape["path"]);
    layer.pathFilled = shape["filled"].asBool(true);
    layer.pathStroked = shape["stroked"].asBool(false);
    layer.strokeWidth = shape["strokeWidth"].asNumber(4.0);
    layer.fillR = shape["fill"]["r"].asNumber(1.0);
    layer.fillG = shape["fill"]["g"].asNumber(1.0);
    layer.fillB = shape["fill"]["b"].asNumber(1.0);
    layer.strokeR = shape["stroke"]["r"].asNumber(0.0);
    layer.strokeG = shape["stroke"]["g"].asNumber(0.0);
    layer.strokeB = shape["stroke"]["b"].asNumber(0.0);

    for (const json::Value& je : v["effects"].asArray()) {
        layer.effects.push_back(effectFromJson(je));
    }

    layer.blend = blendModeFromName(v["blend"].asString("normal"));
    layer.blendStrength = v["blendStrength"].asNumber(1.0);
    layer.motionBlurEnabled = v["motionBlurEnabled"].asBool(false);
    layer.motionBlurShutter = v["motionBlurShutter"].asNumber(0.0);
    return layer;
}

json::Value compositionToJson(const Composition& comp)
{
    json::Value out;
    out.set("id", comp.id);
    out.set("name", comp.name);
    out.set("width", comp.width);
    out.set("height", comp.height);
    out.set("fpsNumerator", comp.fpsNumerator);
    out.set("fpsDenominator", comp.fpsDenominator);
    out.set("duration", comp.duration);

    json::Value bg;
    bg.set("r", comp.backgroundR);
    bg.set("g", comp.backgroundG);
    bg.set("b", comp.backgroundB);
    bg.set("a", comp.backgroundA);
    out.set("background", std::move(bg));

    if (comp.hasCamera) {
        json::Value camera;
        camera.set("x", animatedToJson(comp.cameraX));
        camera.set("y", animatedToJson(comp.cameraY));
        camera.set("z", animatedToJson(comp.cameraZ));
        camera.set("zoom", animatedToJson(comp.cameraZoom));
        out.set("camera", std::move(camera));
    }

    json::Array layers;
    layers.reserve(comp.layers.size());
    for (const Layer& layer : comp.layers) {
        layers.push_back(layerToJson(layer));
    }
    out.set("layers", json::Value(std::move(layers)));
    return out;
}

Composition compositionFromJson(const json::Value& v)
{
    Composition comp;
    comp.id = v["id"].asString();
    comp.name = v["name"].asString("Composition");
    comp.width = static_cast<int>(v["width"].asNumber(1920));
    comp.height = static_cast<int>(v["height"].asNumber(1080));
    comp.fpsNumerator = static_cast<int>(v["fpsNumerator"].asNumber(30));
    comp.fpsDenominator = static_cast<int>(v["fpsDenominator"].asNumber(1));
    comp.duration = v["duration"].asNumber(10.0);
    comp.backgroundR = v["background"]["r"].asNumber(0.0);
    comp.backgroundG = v["background"]["g"].asNumber(0.0);
    comp.backgroundB = v["background"]["b"].asNumber(0.0);
    comp.backgroundA = v["background"]["a"].asNumber(1.0);

    if (v.has("camera")) {
        comp.hasCamera = true;
        comp.cameraX = animatedFromJson(v["camera"]["x"], 0.0);
        comp.cameraY = animatedFromJson(v["camera"]["y"], 0.0);
        comp.cameraZ = animatedFromJson(v["camera"]["z"], -1000.0);
        comp.cameraZoom = animatedFromJson(v["camera"]["zoom"], 1.0);
    }

    for (const json::Value& jl : v["layers"].asArray()) {
        comp.layers.push_back(layerFromJson(jl));
    }
    return comp;
}

} // namespace

json::Value projectToJson(const Project& project)
{
    json::Value root;
    root.set("format", "keyflow-project");
    root.set("version", Project::kFormatVersion);
    root.set("name", project.name);

    json::Array assets;
    assets.reserve(project.assets.size());
    for (const Asset& asset : project.assets) {
        json::Value ja;
        ja.set("id", asset.id);
        ja.set("name", asset.name);
        ja.set("path", asset.path);
        ja.set("duration", asset.duration);
        ja.set("width", asset.width);
        ja.set("height", asset.height);
        ja.set("hasAudio", asset.hasAudio);
        ja.set("still", asset.still);
        assets.push_back(std::move(ja));
    }
    root.set("assets", json::Value(std::move(assets)));

    json::Array comps;
    comps.reserve(project.compositions.size());
    for (const Composition& comp : project.compositions) {
        comps.push_back(compositionToJson(comp));
    }
    root.set("compositions", json::Value(std::move(comps)));
    root.set("activeCompositionId", project.activeCompositionId);
    return root;
}

std::optional<Project> projectFromJson(const json::Value& root, std::string* error)
{
    auto fail = [&](const std::string& message) -> std::optional<Project> {
        if (error) *error = message;
        return std::nullopt;
    };

    if (!root.isObject()) {
        return fail("the file is not a JSON object");
    }
    if (root["format"].asString() != "keyflow-project") {
        return fail("that file is not a Keyflow project");
    }

    const int version = static_cast<int>(root["version"].asNumber(0));
    if (version > Project::kFormatVersion) {
        // Refusing beats guessing. A newer file may carry effects or a layer
        // kind this build does not know, and opening it would silently drop
        // them on the next save.
        return fail("made by a newer version of Keyflow; update the app first");
    }

    Project project;
    project.name = root["name"].asString("Untitled");

    for (const json::Value& ja : root["assets"].asArray()) {
        Asset asset;
        asset.id = ja["id"].asString();
        asset.name = ja["name"].asString();
        asset.path = ja["path"].asString();
        asset.duration = ja["duration"].asNumber(0.0);
        asset.width = static_cast<int>(ja["width"].asNumber(0));
        asset.height = static_cast<int>(ja["height"].asNumber(0));
        asset.hasAudio = ja["hasAudio"].asBool(false);
        asset.still = ja["still"].asBool(false);
        project.assets.push_back(std::move(asset));
    }

    for (const json::Value& jc : root["compositions"].asArray()) {
        project.compositions.push_back(compositionFromJson(jc));
    }

    project.activeCompositionId = root["activeCompositionId"].asString();

    if (project.compositions.empty()) {
        project.compositions.push_back(Composition{});
        project.compositions.front().id = "comp-1";
        project.activeCompositionId = "comp-1";
    }
    if (project.findComposition(project.activeCompositionId) == nullptr) {
        project.activeCompositionId = project.compositions.front().id;
    }
    return project;
}

std::optional<Project> loadProject(const std::string& path, std::string* error)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "cannot open " + path;
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string text = buffer.str();

    std::string parseError;
    const json::Value root = json::parse(text, &parseError);
    if (root.isNull() && !parseError.empty()) {
        if (error) *error = "that file is not valid JSON: " + parseError;
        return std::nullopt;
    }
    return projectFromJson(root, error);
}

bool saveProject(const Project& project, const std::string& path, std::string* error)
{
    const std::string text = json::write(projectToJson(project), 2);

    // Write beside the target and rename. A crash or a full disk during the
    // write then leaves the previous save intact, rather than a half-written
    // file where the project used to be.
    const std::filesystem::path target(path);
    std::filesystem::path temp = target;
    temp += ".tmp";

    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            if (error) *error = "cannot write " + temp.string();
            return false;
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!out) {
            if (error) *error = "failed while writing " + temp.string();
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::rename(temp, target, ec);
    if (ec) {
        // rename does not replace an existing file on every platform. Remove
        // and retry, which is what the failure almost always means.
        std::filesystem::remove(target, ec);
        ec.clear();
        std::filesystem::rename(temp, target, ec);
        if (ec) {
            if (error) *error = "cannot replace " + path + ": " + ec.message();
            std::filesystem::remove(temp, ec);
            return false;
        }
    }
    return true;
}

} // namespace keyflow
