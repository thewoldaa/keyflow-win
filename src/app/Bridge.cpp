#include "app/Bridge.h"

#include "core/EffectCatalog.h"
#include "core/Expression.h"
#include "media/Media.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace keyflow {

namespace {

/// The layer kinds the page can create, by name.
LayerKind kindFromName(const std::string& name)
{
    if (name == "solid")      return LayerKind::Solid;
    if (name == "text")       return LayerKind::Text;
    if (name == "shape")      return LayerKind::Shape;
    if (name == "adjustment") return LayerKind::Adjustment;
    if (name == "null")       return LayerKind::Null;
    if (name == "audio")      return LayerKind::Audio;
    return LayerKind::Media;
}

const char* kindName(LayerKind kind)
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

/// The default name for a new layer of a given kind, numbered from the
/// existing layers so the second solid is "Solid 2" rather than another
/// "Solid 1".
std::string defaultLayerName(const Composition& comp, LayerKind kind)
{
    const char* base = "Layer";
    switch (kind) {
    case LayerKind::Media:      base = "Media"; break;
    case LayerKind::Solid:      base = "Solid"; break;
    case LayerKind::Text:       base = "Text"; break;
    case LayerKind::Shape:      base = "Shape"; break;
    case LayerKind::Adjustment: base = "Adjustment"; break;
    case LayerKind::Null:       base = "Null"; break;
    case LayerKind::Audio:      base = "Audio"; break;
    }

    int count = 0;
    for (const Layer& layer : comp.layers) {
        if (layer.name.rfind(base, 0) == 0) ++count;
    }
    return std::string(base) + " " + std::to_string(count + 1);
}

} // namespace

Bridge::Bridge(Project& document, Sender sender, BridgeHost* host)
    : _document(document), _sender(std::move(sender)), _host(host)
{
}

// --- outgoing -------------------------------------------------------------

void Bridge::Send(const std::string& jsonText)
{
    // Nothing goes out before the page has said hello. ExecuteScript calls
    // made during navigation are dropped silently, and the page would appear
    // to load with no data.
    if (!_pageReady) return;
    if (_sender) _sender(jsonText);
}

void Bridge::SendError(const std::string& message)
{
    json::Value out;
    out.set("type", "error");
    out.set("message", message);
    Send(json::write(out));
}

void Bridge::SendDocument()
{
    json::Value out;
    out.set("type", "document");
    out.set("document", projectToJson(_document));
    out.set("playhead", _playhead);
    out.set("selectedLayerId", _selectedLayerId);
    out.set("selectedEffectId", _selectedEffectId);
    out.set("dirty", _dirty);
    out.set("projectPath", _projectPath);
    out.set("canUndo", !_undoStack.empty());
    out.set("canRedo", !_redoStack.empty());
    Send(json::write(out));
}

void Bridge::SendCatalogue()
{
    json::Value out;
    out.set("type", "catalogue");

    json::Array categories;
    for (const std::string& category : EffectCatalog::instance().categories()) {
        categories.push_back(json::Value(category));
    }
    out.set("categories", json::Value(std::move(categories)));

    json::Array effects;
    for (const EffectSpec& spec : EffectCatalog::instance().effects()) {
        json::Value je;
        je.set("id", spec.id);
        je.set("name", spec.name);
        je.set("category", spec.category);
        je.set("needsPreviousFrame", spec.needsPreviousFrame);
        je.set("needsBackdrop", spec.needsBackdrop);
        je.set("compositionWide", spec.compositionWide);
        je.set("passes", spec.passes);

        json::Array params;
        for (const ParamSpec& p : spec.params) {
            json::Value jp;
            jp.set("name", p.name);
            jp.set("label", p.label);
            jp.set("group", p.group);
            jp.set("default", p.default_);
            jp.set("min", p.min);
            jp.set("max", p.max);
            jp.set("logarithmic", p.logarithmic);
            jp.set("unbounded", p.unbounded);

            const char* kind = "scalar";
            switch (p.kind) {
            case ParamKind::Scalar: kind = "scalar"; break;
            case ParamKind::Angle:  kind = "angle"; break;
            case ParamKind::Colour: kind = "colour"; break;
            case ParamKind::Toggle: kind = "toggle"; break;
            case ParamKind::Choice: kind = "choice"; break;
            }
            jp.set("kind", kind);

            if (!p.choices.empty()) {
                json::Array choices;
                for (const std::string& choice : p.choices) {
                    choices.push_back(json::Value(choice));
                }
                jp.set("choices", json::Value(std::move(choices)));
            }
            params.push_back(std::move(jp));
        }
        je.set("params", json::Value(std::move(params)));
        effects.push_back(std::move(je));
    }
    out.set("effects", json::Value(std::move(effects)));
    Send(json::write(out));
}

void Bridge::SendState()
{
    json::Value out;
    out.set("type", "state");
    out.set("playhead", _playhead);
    out.set("selectedLayerId", _selectedLayerId);
    out.set("selectedEffectId", _selectedEffectId);
    out.set("dirty", _dirty);
    out.set("projectPath", _projectPath);
    out.set("canUndo", !_undoStack.empty());
    out.set("canRedo", !_redoStack.empty());
    Send(json::write(out));
}

void Bridge::sendRenderProgress(int frame, int totalFrames, const std::string& stage)
{
    json::Value out;
    out.set("type", "renderProgress");
    out.set("frame", frame);
    out.set("total", totalFrames);
    out.set("stage", stage);
    Send(json::write(out));
}

void Bridge::sendRenderComplete(const std::string& outputPath, bool success,
                                const std::string& message)
{
    json::Value out;
    out.set("type", "renderComplete");
    out.set("path", outputPath);
    out.set("success", success);
    out.set("message", message);
    Send(json::write(out));
}

// --- helpers --------------------------------------------------------------

Layer* Bridge::layerFor(const json::Value& message)
{
    const std::string id = message["layerId"].asString();
    if (id.empty()) {
        SendError("the message names no layer");
        return nullptr;
    }
    Composition* comp = _document.activeComposition();
    if (comp == nullptr) {
        SendError("there is no active composition");
        return nullptr;
    }
    Layer* layer = comp->findLayer(id);
    if (layer == nullptr) {
        SendError("there is no layer with id '" + id + "'");
        return nullptr;
    }
    return layer;
}

Effect* Bridge::effectFor(const json::Value& message, Layer* layer)
{
    if (layer == nullptr) return nullptr;
    const std::string id = message["instanceId"].asString();
    for (Effect& effect : layer->effects) {
        if (effect.instanceId == id) return &effect;
    }
    SendError("there is no effect with instance id '" + id + "'");
    return nullptr;
}

AnimatedValue* Bridge::resolveParameter(const json::Value& message, Layer* layer)
{
    if (layer == nullptr) return nullptr;

    const std::string name = message["name"].asString();
    if (name.empty()) {
        SendError("the message names no parameter");
        return nullptr;
    }

    // "transform" names the layer's own transform rather than an effect in the
    // stack. The page addresses it the same way because a transform is an
    // effect the compositor applies rather than a shader pass, and a second
    // message shape for one concept would be two protocols to keep in step.
    if (message["instanceId"].asString() == "transform") {
        if (name == "positionX") return &layer->positionX;
        if (name == "positionY") return &layer->positionY;
        if (name == "positionZ") return &layer->positionZ;
        if (name == "scaleX")    return &layer->scaleX;
        if (name == "scaleY")    return &layer->scaleY;
        if (name == "rotation")  return &layer->rotation;
        if (name == "rotationX") return &layer->rotationX;
        if (name == "rotationY") return &layer->rotationY;
        if (name == "opacity")   return &layer->opacity;
        if (name == "anchorX")   return &layer->anchorX;
        if (name == "anchorY")   return &layer->anchorY;
        SendError("the transform has no parameter called '" + name + "'");
        return nullptr;
    }

    Effect* effect = effectFor(message, layer);
    if (effect == nullptr) return nullptr;

    const auto it = effect->parameters.find(name);
    if (it == effect->parameters.end()) {
        SendError("the effect has no parameter called '" + name + "'");
        return nullptr;
    }
    return &it->second;
}

void Bridge::applyDefaults(Effect& effect, const std::string& specId)
{
    const EffectSpec* spec = EffectCatalog::instance().find(specId);
    if (spec == nullptr) return;

    // Every parameter the spec declares gets its declared default. An effect
    // added with no parameters would render as though every control were
    // zero, which for a blur is no blur and for an opacity is invisible —
    // both look like the effect failed to add.
    for (const ParamSpec& p : spec->params) {
        AnimatedValue value;
        value.constant = p.default_;
        effect.parameters[p.name] = value;
    }
}

void Bridge::pushUndo(const std::string& label)
{
    (void)label;
    _undoStack.push_back(_document);
    if (_undoStack.size() > kMaxUndoDepth) {
        _undoStack.erase(_undoStack.begin());
    }
    // A new edit invalidates the redo branch. Keeping it would let the user
    // redo into a future that no longer follows from the present.
    _redoStack.clear();
}

// --- dispatch -------------------------------------------------------------

bool Bridge::Dispatch(const std::string& type, const json::Value& message)
{
    _changed = false;

    // --- session ----------------------------------------------------------

    if (type == "ready")                    { HandleReady(); return false; }
    if (type == "requestDocument")          { HandleRequestDocument(); return false; }
    if (type == "requestCatalogue")         { HandleRequestCatalogue(); return false; }
    if (type == "setPlayhead")              { HandleSetPlayhead(message); return false; }
    if (type == "selectLayer")              { HandleSelectLayer(message); return false; }
    if (type == "quit")                     { HandleQuit(); return false; }
    if (type == "openExternal")             { HandleOpenExternal(message); return false; }
    if (type == "cancelRender")             { HandleCancelRender(message); return false; }

    // --- document structure ----------------------------------------------

    if (type == "addLayer")                 { HandleAddLayer(message); return _changed; }
    if (type == "removeLayer")              { HandleRemoveLayer(message); return _changed; }
    if (type == "moveLayer")                { HandleMoveLayer(message); return _changed; }
    if (type == "renameLayer")              { HandleRenameLayer(message); return _changed; }
    if (type == "duplicateLayer")           { HandleDuplicateLayer(message); return _changed; }
    if (type == "setLayerProperty")         { HandleSetLayerProperty(message); return _changed; }

    // --- effects ----------------------------------------------------------

    if (type == "addEffect")                { HandleAddEffect(message); return _changed; }
    if (type == "removeEffect")             { HandleRemoveEffect(message); return _changed; }
    if (type == "moveEffect")               { HandleMoveEffect(message); return _changed; }
    if (type == "toggleEffect")             { HandleToggleEffect(message); return _changed; }
    if (type == "resetEffect")              { HandleResetEffect(message); return _changed; }

    // --- parameters -------------------------------------------------------

    if (type == "setParameter")             { HandleSetParameter(message); return _changed; }
    if (type == "setKeyframe")              { HandleSetKeyframe(message); return _changed; }
    if (type == "removeKeyframe")           { HandleRemoveKeyframe(message); return _changed; }
    if (type == "moveKeyframe")             { HandleMoveKeyframe(message); return _changed; }
    if (type == "setInterpolation")         { HandleSetInterpolation(message); return _changed; }
    if (type == "setExpression")            { HandleSetExpression(message); return _changed; }

    // --- transform --------------------------------------------------------

    if (type == "setTransform")             { HandleSetTransform(message); return _changed; }
    if (type == "resetTransform")           { HandleResetTransform(message); return _changed; }

    // --- composition ------------------------------------------------------

    if (type == "setComposition")           { HandleSetComposition(message); return _changed; }
    if (type == "addComposition")           { HandleAddComposition(message); return _changed; }

    // --- files ------------------------------------------------------------

    if (type == "newProject")               { HandleNewProject(message); return _changed; }
    if (type == "openProject")              { HandleOpenProject(message); return _changed; }
    if (type == "saveProject")              { HandleSaveProject(message); return _changed; }
    if (type == "saveProjectAs")            { HandleSaveProjectAs(message); return _changed; }
    if (type == "importMedia")              { HandleImportMedia(message); return _changed; }
    if (type == "relinkAsset")              { HandleRelinkAsset(message); return _changed; }
    if (type == "exportProject")            { HandleExportProject(message); return _changed; }
    if (type == "startRender")              { HandleStartRender(message); return false; }

    // --- undo -------------------------------------------------------------

    if (type == "undo")                     { HandleUndo(); return _changed; }
    if (type == "redo")                     { HandleRedo(); return _changed; }

    // An unknown type is a protocol mismatch between the page and the host,
    // which is worth saying out loud rather than ignoring.
    SendError("unknown message type '" + type + "'");
    return false;
}

bool Bridge::HandleMessage(const std::string& text)
{
    ++_messagesHandled;

    std::string parseError;
    const json::Value message = json::parse(text, &parseError);
    if (!parseError.empty()) {
        SendError("the page sent something that is not JSON: " + parseError);
        return false;
    }
    if (!message.isObject()) {
        SendError("the page sent a JSON value that is not an object");
        return false;
    }

    const std::string type = message["type"].asString();
    if (type.empty()) {
        SendError("the page sent a message with no type");
        return false;
    }

    const bool changed = Dispatch(type, message);
    if (changed) {
        _dirty = true;
        SendDocument();
    }
    return changed;
}

// --- session --------------------------------------------------------------

void Bridge::HandleReady()
{
    _pageReady = true;
    SendCatalogue();
    SendDocument();
}

void Bridge::HandleRequestDocument() { SendDocument(); }
void Bridge::HandleRequestCatalogue() { SendCatalogue(); }

void Bridge::HandleSetPlayhead(const json::Value& message)
{
    _playhead = std::max(0.0, message["time"].asNumber(0.0));
}

void Bridge::HandleSelectLayer(const json::Value& message)
{
    _selectedLayerId = message["layerId"].asString();
    _selectedEffectId = message["instanceId"].asString();
    SendState();
}

void Bridge::HandleQuit()
{
    if (_host) _host->requestQuit();
}

void Bridge::HandleOpenExternal(const json::Value& message)
{
    // The page must not be able to open an arbitrary URL — a message arriving
    // from a compromised page would become a way to launch anything. Only the
    // project's own documentation link is allowed through.
    const std::string url = message["url"].asString();
    if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0) {
        SendError("only http and https links can be opened");
        return;
    }
    if (_host) _host->reportStatus("Open " + url + " in a browser.");
}

void Bridge::HandleCancelRender(const json::Value& message)
{
    (void)message;
    if (_host) _host->cancelRender();
}

// --- document structure ---------------------------------------------------

void Bridge::HandleAddLayer(const json::Value& message)
{
    Composition* comp = _document.activeComposition();
    if (comp == nullptr) {
        SendError("there is no active composition");
        return;
    }

    pushUndo("Add layer");

    Layer layer;
    layer.id = _document.newId("layer");
    layer.kind = kindFromName(message["kind"].asString("solid"));
    layer.name = message["name"].asString();
    if (layer.name.empty()) layer.name = defaultLayerName(*comp, layer.kind);
    layer.outPoint = comp->duration;

    // A new layer is placed above the selected one when there is a selection,
    // and on top otherwise. Inserting at the end always would put a new solid
    // behind the artwork the user is looking at.
    int insertAt = static_cast<int>(comp->layers.size());
    if (!_selectedLayerId.empty()) {
        const int selected = comp->indexOfLayer(_selectedLayerId);
        if (selected >= 0) insertAt = selected + 1;
    }
    comp->layers.insert(comp->layers.begin() + insertAt, std::move(layer));

    _selectedLayerId = comp->layers[static_cast<std::size_t>(insertAt)].id;
    _changed = true;
}

void Bridge::HandleRemoveLayer(const json::Value& message)
{
    Composition* comp = _document.activeComposition();
    if (comp == nullptr) return;

    const std::string id = message["layerId"].asString();
    const int index = comp->indexOfLayer(id);
    if (index < 0) {
        SendError("there is no layer with id '" + id + "'");
        return;
    }

    pushUndo("Remove layer");
    comp->layers.erase(comp->layers.begin() + index);

    if (_selectedLayerId == id) {
        // Select the layer that took its place, or the new last one. Leaving
        // the selection pointing at a layer that no longer exists makes the
        // inspector edit nothing with no explanation.
        if (!comp->layers.empty()) {
            const std::size_t next = std::min<std::size_t>(
                static_cast<std::size_t>(index), comp->layers.size() - 1);
            _selectedLayerId = comp->layers[next].id;
        } else {
            _selectedLayerId.clear();
        }
        _selectedEffectId.clear();
    }
    _changed = true;
}

void Bridge::HandleMoveLayer(const json::Value& message)
{
    Composition* comp = _document.activeComposition();
    if (comp == nullptr) return;

    const std::string id = message["layerId"].asString();
    const int from = comp->indexOfLayer(id);
    if (from < 0) {
        SendError("there is no layer with id '" + id + "'");
        return;
    }

    int to = static_cast<int>(message["index"].asNumber(from));
    to = std::max(0, std::min(to, static_cast<int>(comp->layers.size()) - 1));
    if (to == from) return;

    pushUndo("Reorder layers");
    Layer moved = std::move(comp->layers[static_cast<std::size_t>(from)]);
    comp->layers.erase(comp->layers.begin() + from);
    comp->layers.insert(comp->layers.begin() + to, std::move(moved));
    _changed = true;
}

void Bridge::HandleRenameLayer(const json::Value& message)
{
    Layer* layer = layerFor(message);
    if (layer == nullptr) return;

    const std::string name = message["name"].asString();
    if (name == layer->name) return;

    pushUndo("Rename layer");
    layer->name = name;
    _changed = true;
}

void Bridge::HandleDuplicateLayer(const json::Value& message)
{
    Composition* comp = _document.activeComposition();
    if (comp == nullptr) return;

    const std::string id = message["layerId"].asString();
    const int index = comp->indexOfLayer(id);
    if (index < 0) {
        SendError("there is no layer with id '" + id + "'");
        return;
    }

    pushUndo("Duplicate layer");

    Layer copy = comp->layers[static_cast<std::size_t>(index)];
    copy.id = _document.newId("layer");
    copy.name += " copy";

    // The effect instance ids must be fresh too. Two effects sharing an
    // instance id means the page cannot address one of them, and edits land
    // on whichever the lookup finds first.
    for (Effect& effect : copy.effects) {
        effect.instanceId = _document.newId("effect");
    }

    comp->layers.insert(comp->layers.begin() + index + 1, std::move(copy));
    _selectedLayerId = comp->layers[static_cast<std::size_t>(index + 1)].id;
    _changed = true;
}

void Bridge::HandleSetLayerProperty(const json::Value& message)
{
    Layer* layer = layerFor(message);
    if (layer == nullptr) return;

    const std::string property = message["property"].asString();
    if (property.empty()) {
        SendError("setLayerProperty needs a property name");
        return;
    }

    pushUndo("Set " + property);

    if (property == "visible")        layer->visible = message["value"].asBool(true);
    else if (property == "locked")    layer->locked = message["value"].asBool(false);
    else if (property == "trackMatte")layer->trackMatte = message["value"].asBool(false);
    else if (property == "inPoint")   layer->inPoint = message["value"].asNumber(0.0);
    else if (property == "outPoint")  layer->outPoint = message["value"].asNumber(0.0);
    else if (property == "blend")     layer->blend = blendModeFromName(message["value"].asString("normal"));
    else if (property == "blendStrength") layer->blendStrength = message["value"].asNumber(1.0);
    else if (property == "motionBlurEnabled") layer->motionBlurEnabled = message["value"].asBool(false);
    else if (property == "motionBlurShutter") layer->motionBlurShutter = message["value"].asNumber(0.0);
    else if (property == "assetId")   layer->assetId = message["value"].asString();
    else if (property == "text")      layer->text = message["value"].asString();
    else if (property == "textSize")  layer->textSize = message["value"].asNumber(72.0);
    else if (property == "pathFilled")layer->pathFilled = message["value"].asBool(true);
    else if (property == "pathStroked") layer->pathStroked = message["value"].asBool(false);
    else if (property == "strokeWidth") layer->strokeWidth = message["value"].asNumber(4.0);
    else {
        SendError("unknown layer property '" + property + "'");
        return;
    }
    _changed = true;
}

// --- effects --------------------------------------------------------------

void Bridge::HandleAddEffect(const json::Value& message)
{
    Layer* layer = layerFor(message);
    if (layer == nullptr) return;

    const std::string specId = message["specId"].asString();
    const EffectSpec* spec = EffectCatalog::instance().find(specId);
    if (spec == nullptr) {
        SendError("there is no effect called '" + specId + "'");
        return;
    }

    pushUndo("Add effect");

    Effect effect;
    effect.specId = specId;
    effect.instanceId = _document.newId("effect");
    effect.enabled = true;
    applyDefaults(effect, specId);

    // Insert above the selected effect when one is selected, on top otherwise.
    // Same reasoning as layers: a new effect goes where the user is looking.
    int insertAt = static_cast<int>(layer->effects.size());
    if (!_selectedEffectId.empty()) {
        for (std::size_t i = 0; i < layer->effects.size(); ++i) {
            if (layer->effects[i].instanceId == _selectedEffectId) {
                insertAt = static_cast<int>(i) + 1;
                break;
            }
        }
    }
    layer->effects.insert(layer->effects.begin() + insertAt, std::move(effect));

    _selectedEffectId = layer->effects[static_cast<std::size_t>(insertAt)].instanceId;
    _changed = true;
}

void Bridge::HandleRemoveEffect(const json::Value& message)
{
    Layer* layer = layerFor(message);
    if (layer == nullptr) return;

    const std::string id = message["instanceId"].asString();
    for (auto it = layer->effects.begin(); it != layer->effects.end(); ++it) {
        if (it->instanceId == id) {
            pushUndo("Remove effect");
            layer->effects.erase(it);
            if (_selectedEffectId == id) _selectedEffectId.clear();
            _changed = true;
            return;
        }
    }
    SendError("there is no effect with instance id '" + id + "'");
}

void Bridge::HandleMoveEffect(const json::Value& message)
{
    Layer* layer = layerFor(message);
    if (layer == nullptr) return;

    const std::string id = message["instanceId"].asString();
    int from = -1;
    for (std::size_t i = 0; i < layer->effects.size(); ++i) {
        if (layer->effects[i].instanceId == id) {
            from = static_cast<int>(i);
            break;
        }
    }
    if (from < 0) {
        SendError("there is no effect with instance id '" + id + "'");
        return;
    }

    int to = static_cast<int>(message["index"].asNumber(from));
    to = std::max(0, std::min(to, static_cast<int>(layer->effects.size()) - 1));
    if (to == from) return;

    pushUndo("Reorder effects");
    Effect moved = std::move(layer->effects[static_cast<std::size_t>(from)]);
    layer->effects.erase(layer->effects.begin() + from);
    layer->effects.insert(layer->effects.begin() + to, std::move(moved));
    _changed = true;
}

void Bridge::HandleToggleEffect(const json::Value& message)
{
    Layer* layer = layerFor(message);
    if (layer == nullptr) return;
    Effect* effect = effectFor(message, layer);
    if (effect == nullptr) return;

    pushUndo("Toggle effect");
    effect->enabled = message["enabled"].asBool(!effect->enabled);
    _changed = true;
}

void Bridge::HandleResetEffect(const json::Value& message)
{
    Layer* layer = layerFor(message);
    if (layer == nullptr) return;
    Effect* effect = effectFor(message, layer);
    if (effect == nullptr) return;

    pushUndo("Reset effect");
    // Clear the parameters rather than overwriting them, so applyDefaults
    // fills in the spec's current defaults — which is what "reset" means when
    // a default has changed since the effect was added.
    effect->parameters.clear();
    applyDefaults(*effect, effect->specId);
    _changed = true;
}

// --- parameters -----------------------------------------------------------

void Bridge::HandleSetParameter(const json::Value& message)
{
    Layer* layer = layerFor(message);
    if (layer == nullptr) return;

    const std::string name = message["name"].asString();
    if (name.empty()) {
        SendError("setParameter needs a parameter name");
        return;
    }

    const double value = message["value"].asNumber(0.0);

    // resolveParameter handles both an effect in the stack and the layer's own
    // transform, which the page addresses with the instance id "transform".
    AnimatedValue* target = resolveParameter(message, layer);
    if (target == nullptr) return;

    pushUndo("Set parameter");

    if (target->animated()) {
        // With keyframes present, an edit becomes a keyframe at the playhead.
        // Overwriting `constant` instead would change nothing visible, because
        // evaluate() reads the keyframes when there are any.
        const bool atExisting = std::any_of(
            target->keyframes.begin(), target->keyframes.end(),
            [&](const Keyframe& k) { return std::fabs(k.time - _playhead) < 1e-9; });

        Keyframe key;
        key.time = _playhead;
        key.value = value;
        key.interpolation = Interpolation::Ease;
        if (!atExisting && !target->keyframes.empty()) {
            // Inherit the interpolation of the keyframe before this one, so
            // adding a keyframe mid-animation does not change the shape of the
            // curve on either side of it.
            for (const Keyframe& existing : target->keyframes) {
                if (existing.time <= _playhead) key.interpolation = existing.interpolation;
            }
        }
        target->setKeyframe(key);
    } else {
        target->constant = value;
    }
    _changed = true;
}

void Bridge::HandleSetKeyframe(const json::Value& message)
{
    Layer* layer = layerFor(message);
    if (layer == nullptr) return;
    const std::string name = message["name"].asString();
    AnimatedValue* target = resolveParameter(message, layer);
    if (target == nullptr) return;

    pushUndo("Add keyframe");

    Keyframe key;
    key.time = message["time"].asNumber(_playhead);
    // A keyframe added at the playhead takes the value the parameter has right
    // now, which is what makes the stopwatch button non-destructive.
    key.value = message["value"].asNumber(target->evaluate(key.time));
    key.interpolation = Interpolation::Ease;
    target->setKeyframe(key);
    _changed = true;
}

void Bridge::HandleRemoveKeyframe(const json::Value& message)
{
    Layer* layer = layerFor(message);
    if (layer == nullptr) return;

    const double time = message["time"].asNumber(_playhead);

    AnimatedValue* target = resolveParameter(message, layer);
    if (target == nullptr) return;

    // Read the value BEFORE removing the keyframe. Afterwards the keyframe is
    // gone, so evaluating at `time` would return whatever the remaining curve
    // happens to give there — which for the last keyframe is the default, and
    // the picture jumps.
    const double valueAtTime = target->evaluate(time);

    pushUndo("Remove keyframe");
    if (!target->removeKeyframe(time)) {
        SendError("there is no keyframe at that time");
        return;
    }

    // Removing the last keyframe leaves the parameter at the value it had
    // there, so the picture does not jump when the stopwatch is switched off.
    if (!target->animated()) {
        target->constant = valueAtTime;
    }
    _changed = true;
}

void Bridge::HandleMoveKeyframe(const json::Value& message)
{
    Layer* layer = layerFor(message);
    if (layer == nullptr) return;
    const std::string name = message["name"].asString();
    AnimatedValue* target = resolveParameter(message, layer);
    if (target == nullptr) return;

    const double from = message["from"].asNumber(0.0);
    const double to = message["to"].asNumber(0.0);

    // Find the keyframe, take a copy, remove it, and reinsert at the new time.
    // Mutating `time` in place would break the sort order and every lookup
    // after it.
    for (const Keyframe& key : target->keyframes) {
        if (std::fabs(key.time - from) < 1e-9) {
            pushUndo("Move keyframe");
            Keyframe moved = key;
            moved.time = to;
            target->removeKeyframe(from);
            target->setKeyframe(moved);
            _changed = true;
            return;
        }
    }
    SendError("there is no keyframe at that time");
}

void Bridge::HandleSetInterpolation(const json::Value& message)
{
    Layer* layer = layerFor(message);
    if (layer == nullptr) return;
    const std::string name = message["name"].asString();
    AnimatedValue* target = resolveParameter(message, layer);
    if (target == nullptr) return;

    const std::string kind = message["interpolation"].asString("ease");
    const double time = message["time"].asNumber(-1.0);

    pushUndo("Set interpolation");
    for (Keyframe& key : target->keyframes) {
        // A time of -1 means "every keyframe on this parameter", which is what
        // the inspector's easing menu applies to.
        if (time < 0.0 || std::fabs(key.time - time) < 1e-9) {
            if (kind == "hold")        key.interpolation = Interpolation::Hold;
            else if (kind == "linear") key.interpolation = Interpolation::Linear;
            else if (kind == "bezier") key.interpolation = Interpolation::Bezier;
            else                       key.interpolation = Interpolation::Ease;
        }
    }
    _changed = true;
}

void Bridge::HandleSetExpression(const json::Value& message)
{
    Layer* layer = layerFor(message);
    if (layer == nullptr) return;
    Effect* effect = effectFor(message, layer);
    if (effect == nullptr) return;

    const std::string name = message["name"].asString();
    const std::string source = message["source"].asString();
    const bool enabled = message["enabled"].asBool(!source.empty());

    pushUndo("Set expression");

    if (source.empty()) {
        effect->expressions.erase(name);
        _changed = true;
        return;
    }

    // Validate before storing. Storing an expression that cannot evaluate
    // would leave the parameter silently falling back with no indication of
    // why.
    const ExpressionResult check = checkExpression(source);
    if (!check.ok) {
        SendError("the expression is not valid: " + check.error);
        return;
    }

    Expression expression;
    expression.source = source;
    expression.enabled = enabled;
    effect->expressions[name] = std::move(expression);
    _changed = true;
}

// --- transform ------------------------------------------------------------

void Bridge::HandleSetTransform(const json::Value& message)
{
    Layer* layer = layerFor(message);
    if (layer == nullptr) return;

    pushUndo("Transform layer");

    // Position is sent as a pair, because a drag produces both at once and two
    // messages per mouse move would double the traffic and could tear.
    if (message.has("x")) layer->positionX.constant = message["x"].asNumber(0.0);
    if (message.has("y")) layer->positionY.constant = message["y"].asNumber(0.0);
    if (message.has("scaleX")) layer->scaleX.constant = message["scaleX"].asNumber(1.0);
    if (message.has("scaleY")) layer->scaleY.constant = message["scaleY"].asNumber(1.0);
    if (message.has("rotation")) layer->rotation.constant = message["rotation"].asNumber(0.0);
    if (message.has("opacity")) layer->opacity.constant = message["opacity"].asNumber(1.0);
    if (message.has("anchorX")) layer->anchorX.constant = message["anchorX"].asNumber(0.5);
    if (message.has("anchorY")) layer->anchorY.constant = message["anchorY"].asNumber(0.5);
    _changed = true;
}

void Bridge::HandleResetTransform(const json::Value& message)
{
    Layer* layer = layerFor(message);
    if (layer == nullptr) return;

    pushUndo("Reset transform");
    // Clear the keyframes too, or the reset appears to do nothing: evaluate()
    // reads the keyframes when there are any, so a reset that only set the
    // constants would leave the layer exactly where it was.
    layer->positionX = AnimatedValue{0.0, {}};
    layer->positionY = AnimatedValue{0.0, {}};
    layer->positionZ = AnimatedValue{0.0, {}};
    layer->scaleX = AnimatedValue{1.0, {}};
    layer->scaleY = AnimatedValue{1.0, {}};
    layer->rotation = AnimatedValue{0.0, {}};
    layer->rotationX = AnimatedValue{0.0, {}};
    layer->rotationY = AnimatedValue{0.0, {}};
    layer->opacity = AnimatedValue{1.0, {}};
    layer->anchorX = AnimatedValue{0.5, {}};
    layer->anchorY = AnimatedValue{0.5, {}};
    _changed = true;
}

// --- composition ----------------------------------------------------------

void Bridge::HandleSetComposition(const json::Value& message)
{
    Composition* comp = _document.activeComposition();
    if (comp == nullptr) return;

    pushUndo("Set composition");

    if (message.has("name"))   comp->name = message["name"].asString();
    if (message.has("width"))  comp->width = std::max(1, static_cast<int>(message["width"].asNumber(1920)));
    if (message.has("height")) comp->height = std::max(1, static_cast<int>(message["height"].asNumber(1080)));
    if (message.has("duration")) comp->duration = std::max(0.01, message["duration"].asNumber(10.0));
    if (message.has("fpsNumerator")) comp->fpsNumerator = std::max(1, static_cast<int>(message["fpsNumerator"].asNumber(30)));
    if (message.has("fpsDenominator")) comp->fpsDenominator = std::max(1, static_cast<int>(message["fpsDenominator"].asNumber(1)));
    if (message.has("backgroundR")) comp->backgroundR = message["backgroundR"].asNumber(0.0);
    if (message.has("backgroundG")) comp->backgroundG = message["backgroundG"].asNumber(0.0);
    if (message.has("backgroundB")) comp->backgroundB = message["backgroundB"].asNumber(0.0);
    if (message.has("hasCamera")) comp->hasCamera = message["hasCamera"].asBool(false);
    _changed = true;
}

void Bridge::HandleAddComposition(const json::Value& message)
{
    pushUndo("Add composition");

    Composition comp;
    comp.id = _document.newId("comp");
    comp.name = message["name"].asString();
    if (comp.name.empty()) {
        comp.name = "Composition " + std::to_string(_document.compositions.size() + 1);
    }
    comp.width = static_cast<int>(message["width"].asNumber(1920));
    comp.height = static_cast<int>(message["height"].asNumber(1080));
    comp.duration = message["duration"].asNumber(10.0);

    _document.compositions.push_back(std::move(comp));
    _document.activeCompositionId = _document.compositions.back().id;
    _selectedLayerId.clear();
    _selectedEffectId.clear();
    _changed = true;
}

// --- files ----------------------------------------------------------------

void Bridge::HandleNewProject(const json::Value& message)
{
    (void)message;
    pushUndo("New project");
    _document = Project::createDefault("Untitled");
    _projectPath.clear();
    _selectedLayerId.clear();
    _selectedEffectId.clear();
    _playhead = 0.0;
    _undoStack.clear();
    _redoStack.clear();
    _changed = true;
}

void Bridge::HandleOpenProject(const json::Value& message)
{
    std::string path = message["path"].asString();
    if (path.empty()) {
        if (_host == nullptr) {
            SendError("no file dialog is available");
            return;
        }
        path = _host->pickOpenFile("Keyflow project", "*.kfproj");
        if (path.empty()) return; // cancelled
    }

    std::string error;
    const auto loaded = loadProject(path, &error);
    if (!loaded.has_value()) {
        SendError(error.empty() ? "the project could not be opened" : error);
        return;
    }

    pushUndo("Open project");
    _document = *loaded;
    _projectPath = path;
    _selectedLayerId.clear();
    _selectedEffectId.clear();
    _playhead = 0.0;
    _undoStack.clear();
    _redoStack.clear();
    _changed = true;
    if (_host) _host->reportStatus("Opened " + path);
}

void Bridge::HandleSaveProject(const json::Value& message)
{
    (void)message;
    std::string path = _projectPath;
    if (path.empty()) {
        if (_host == nullptr) {
            SendError("no file dialog is available");
            return;
        }
        path = _host->pickSaveFile("Keyflow project", "*.kfproj",
                                   _document.name.empty() ? "Untitled" : _document.name);
        if (path.empty()) return; // cancelled
    }

    std::string error;
    if (!saveProject(_document, path, &error)) {
        SendError(error.empty() ? "the project could not be saved" : error);
        return;
    }

    _projectPath = path;
    _dirty = false;
    if (_host) _host->reportStatus("Saved " + path);
    SendState();
}

void Bridge::HandleSaveProjectAs(const json::Value& message)
{
    (void)message;
    if (_host == nullptr) {
        SendError("no file dialog is available");
        return;
    }
    const std::string path = _host->pickSaveFile(
        "Keyflow project", "*.kfproj",
        _document.name.empty() ? "Untitled" : _document.name);
    if (path.empty()) return;

    std::string error;
    if (!saveProject(_document, path, &error)) {
        SendError(error.empty() ? "the project could not be saved" : error);
        return;
    }
    _projectPath = path;
    _dirty = false;
    if (_host) _host->reportStatus("Saved " + path);
    SendState();
}

void Bridge::HandleExportProject(const json::Value& message)
{
    (void)message;
    // Exporting the project file itself is the same operation as saving, just
    // to a chosen path. Kept separate from saveProjectAs so the page can label
    // the two differently.
    HandleSaveProjectAs(message);
}

void Bridge::HandleImportMedia(const json::Value& message)
{
    std::vector<std::string> paths;

    const json::Value& list = message["paths"];
    if (list.isArray()) {
        for (const json::Value& entry : list.asArray()) {
            const std::string path = entry.asString();
            if (!path.empty()) paths.push_back(path);
        }
    }
    if (paths.empty() && _host != nullptr) {
        const std::string path = _host->pickOpenFile(
            "Media", "*.mp4;*.mov;*.mkv;*.webm;*.avi;*.png;*.jpg;*.jpeg;*.webp;*.bmp");
        if (!path.empty()) paths.push_back(path);
    }
    if (paths.empty()) return;

    if (_host == nullptr) {
        SendError("no media probe is available");
        return;
    }

    FFmpegPaths tools;
    tools.ffmpeg = _host->ffmpegPath();
    tools.ffprobe = _host->ffprobePath();

    Composition* comp = _document.activeComposition();
    if (comp == nullptr) {
        SendError("there is no active composition");
        return;
    }

    pushUndo("Import media");

    int imported = 0;
    std::vector<std::string> failures;

    for (const std::string& path : paths) {
        const MediaInfo info = probeMedia(tools, path);
        if (!info.ok) {
            failures.push_back(std::filesystem::path(path).filename().string()
                               + ": " + info.error);
            continue;
        }

        Asset asset;
        asset.id = _document.newId("asset");
        asset.name = std::filesystem::path(path).filename().string();
        asset.path = path;
        asset.duration = info.duration;
        asset.width = info.width;
        asset.height = info.height;
        asset.hasAudio = info.hasAudio;
        asset.still = info.still;
        _document.assets.push_back(asset);

        Layer layer;
        layer.id = _document.newId("layer");
        layer.name = asset.name;
        layer.kind = LayerKind::Media;
        layer.assetId = asset.id;
        layer.outPoint = info.still ? comp->duration
                                    : std::max(comp->duration, info.duration);

        // Centre the layer and scale it to fit the composition. Dropping a
        // 4K clip into a 1080p composition at 100% shows a quarter of it, with
        // no indication that the rest is off-screen.
        if (info.width > 0 && info.height > 0 && comp->width > 0 && comp->height > 0) {
            const double fitX = static_cast<double>(comp->width) / info.width;
            const double fitY = static_cast<double>(comp->height) / info.height;
            const double fit = std::min(fitX, fitY);
            if (fit < 1.0) {
                layer.scaleX.constant = fit;
                layer.scaleY.constant = fit;
            }
        }
        layer.positionX.constant = comp->width * 0.5;
        layer.positionY.constant = comp->height * 0.5;

        comp->layers.push_back(std::move(layer));
        _selectedLayerId = comp->layers.back().id;
        ++imported;
    }

    if (imported > 0) _changed = true;

    if (!failures.empty()) {
        std::string report = "could not import ";
        report += std::to_string(failures.size());
        report += failures.size() == 1 ? " file: " : " files: ";
        for (std::size_t i = 0; i < failures.size(); ++i) {
            if (i) report += "; ";
            report += failures[i];
        }
        SendError(report);
    }
    if (imported > 0 && _host) {
        _host->reportStatus("Imported " + std::to_string(imported)
                            + (imported == 1 ? " file" : " files"));
    }
}

void Bridge::HandleRelinkAsset(const json::Value& message)
{
    const std::string assetId = message["assetId"].asString();
    Asset* asset = nullptr;
    for (Asset& candidate : _document.assets) {
        if (candidate.id == assetId) {
            asset = &candidate;
            break;
        }
    }
    if (asset == nullptr) {
        SendError("there is no asset with id '" + assetId + "'");
        return;
    }

    std::string path = message["path"].asString();
    if (path.empty() && _host != nullptr) {
        path = _host->pickOpenFile("Media",
                                   "*.mp4;*.mov;*.mkv;*.webm;*.avi;*.png;*.jpg;*.jpeg;*.webp");
        if (path.empty()) return;
    }
    if (path.empty()) return;

    pushUndo("Relink media");
    asset->path = path;
    if (asset->name.empty()) {
        asset->name = std::filesystem::path(path).filename().string();
    }
    _changed = true;
}

void Bridge::HandleStartRender(const json::Value& message)
{
    if (_host == nullptr) {
        SendError("no renderer is available");
        return;
    }

    std::string path = message["outputPath"].asString();
    if (path.empty()) {
        path = _host->pickSaveFile("Video", "*.mp4", _document.name + ".mp4");
        if (path.empty()) return;
    }

    const Composition* comp = _document.activeComposition();
    if (comp == nullptr) {
        SendError("there is no active composition to render");
        return;
    }

    // Warn about missing media before starting, rather than failing part-way
    // through a long render.
    std::vector<std::string> missing;
    for (const Asset& asset : _document.assets) {
        if (!std::filesystem::exists(asset.path)) {
            missing.push_back(asset.name.empty() ? asset.path : asset.name);
        }
    }
    if (!missing.empty()) {
        std::string report = "these files are missing: ";
        for (std::size_t i = 0; i < missing.size(); ++i) {
            if (i) report += ", ";
            report += missing[i];
        }
        SendError(report);
        return;
    }

    _host->startRender(
        path,
        message["videoCodec"].asString("libx264"),
        static_cast<int>(message["quality"].asNumber(18)),
        message["useBitrate"].asBool(false),
        static_cast<int>(message["bitrateKbps"].asNumber(12000)),
        message["resize"].asBool(false),
        static_cast<int>(message["width"].asNumber(comp->width)),
        static_cast<int>(message["height"].asNumber(comp->height)));
}

// --- undo -----------------------------------------------------------------

void Bridge::HandleUndo()
{
    if (_undoStack.empty()) {
        SendError("there is nothing to undo");
        return;
    }
    _redoStack.push_back(_document);
    _document = std::move(_undoStack.back());
    _undoStack.pop_back();
    _changed = true;

    // The selection may name a layer the restored document does not have.
    Composition* comp = _document.activeComposition();
    if (comp != nullptr && !_selectedLayerId.empty()
        && comp->findLayer(_selectedLayerId) == nullptr) {
        _selectedLayerId.clear();
        _selectedEffectId.clear();
    }
}

void Bridge::HandleRedo()
{
    if (_redoStack.empty()) {
        SendError("there is nothing to redo");
        return;
    }
    _undoStack.push_back(_document);
    _document = std::move(_redoStack.back());
    _redoStack.pop_back();
    _changed = true;

    Composition* comp = _document.activeComposition();
    if (comp != nullptr && !_selectedLayerId.empty()
        && comp->findLayer(_selectedLayerId) == nullptr) {
        _selectedLayerId.clear();
        _selectedEffectId.clear();
    }
}

void Bridge::HandleReplaceDocument(const json::Value& message)
{
    std::string error;
    const auto loaded = projectFromJson(message["document"], &error);
    if (!loaded.has_value()) {
        SendError(error.empty() ? "the document could not be read" : error);
        return;
    }
    pushUndo("Replace document");
    _document = *loaded;
    _changed = true;
}

} // namespace keyflow
