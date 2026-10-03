#include "TestFramework.h"

#include "app/Bridge.h"

using namespace keyflow;

namespace {

/// A host that records what it was asked to do.
///
/// The bridge's job is to turn messages into document changes and host calls;
/// both halves are worth checking, and a recording host checks the second
/// without a window or a file dialog.
class RecordingHost : public BridgeHost
{
public:
    std::string openPath;
    std::string savePath;

    int renderStarts = 0;
    std::string renderOutput;
    std::string renderCodec;
    int renderQuality = 0;

    int cancels = 0;
    int quits = 0;
    std::vector<std::string> statuses;

    std::string pickOpenFile(const std::string&, const std::string&) override
    {
        return openPath;
    }
    std::string pickSaveFile(const std::string&, const std::string&,
                             const std::string&) override
    {
        return savePath;
    }
    std::string ffmpegPath() override { return "ffmpeg"; }
    std::string ffprobePath() override { return "ffprobe"; }

    void startRender(const std::string& outputPath, const std::string& videoCodec,
                     int quality, bool, int, bool, int, int) override
    {
        ++renderStarts;
        renderOutput = outputPath;
        renderCodec = videoCodec;
        renderQuality = quality;
    }
    void cancelRender() override { ++cancels; }
    void requestQuit() override { ++quits; }
    void reportStatus(const std::string& message) override { statuses.push_back(message); }
};

/// A bridge wired to a fresh project, a recording host, and a captured outbox.
struct Fixture
{
    Project document = Project::createDefault("Test");
    RecordingHost host;
    std::vector<std::string> outbox;
    Bridge bridge;

    Fixture()
        : bridge(document, [this](const std::string& text) { outbox.push_back(text); }, &host)
    {
        // The page has to say hello before anything is sent.
        bridge.HandleMessage(R"({"type":"ready"})");
        // Keep the handshake replies: several tests check what ready sends.
        // Tests that care about a later reply use lastOfType, which searches
        // from the end.
    }

    /// Send a message and return the parsed reply of a given type, or a null
    /// value when no such reply arrived.
    json::Value lastOfType(const std::string& type) const
    {
        for (auto it = outbox.rbegin(); it != outbox.rend(); ++it) {
            std::string error;
            const json::Value parsed = json::parse(*it, &error);
            if (!error.empty()) continue;
            if (parsed["type"].asString() == type) return parsed;
        }
        return {};
    }

    bool sawError() const { return !lastOfType("error").isNull(); }
    std::string lastError() const { return lastOfType("error")["message"].asString(); }

    /// The first layer of the active composition.
    Layer& firstLayer() { return document.activeComposition()->layers.front(); }
};

} // namespace

TEST("bridge: nothing is sent before the page says ready")
{
    Project document = Project::createDefault();
    RecordingHost host;
    std::vector<std::string> outbox;
    Bridge bridge(document, [&](const std::string& t) { outbox.push_back(t); }, &host);

    bridge.HandleMessage(R"({"type":"requestDocument"})");
    CHECK(outbox.empty());
    CHECK(!bridge.PageReady());

    bridge.HandleMessage(R"({"type":"ready"})");
    CHECK(bridge.PageReady());
    CHECK(!outbox.empty());
}

TEST("bridge: ready sends the catalogue and the document")
{
    Fixture fixture;
    const json::Value catalogue = fixture.lastOfType("catalogue");
    const json::Value document = fixture.lastOfType("document");
    CHECK(!catalogue.isNull());
    CHECK(!document.isNull());
    CHECK(catalogue["effects"].size() > 30);
    CHECK(!document["document"].isNull());
}

TEST("bridge: a malformed message produces an error, not a crash")
{
    Fixture fixture;
    CHECK(!fixture.bridge.HandleMessage("this is not json"));
    CHECK(fixture.sawError());

    fixture.bridge.HandleMessage(R"([1,2,3])");
    CHECK(fixture.sawError());

    fixture.bridge.HandleMessage(R"({"noType":true})");
    CHECK(fixture.sawError());

    fixture.bridge.HandleMessage(R"({"type":"notARealMessage"})");
    CHECK(fixture.lastError().find("unknown message type") != std::string::npos);
}

TEST("bridge: adding a layer puts it above the selection and selects it")
{
    Fixture fixture;
    CHECK(fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})"));

    Composition* comp = fixture.document.activeComposition();
    REQUIRE(comp->layers.size() == 1);
    CHECK_EQ(comp->layers[0].name, std::string("Solid 1"));
    CHECK_EQ(fixture.bridge.selectedLayerId(), comp->layers[0].id);

    // A second layer goes above the first, which is selected.
    CHECK(fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"text"})"));
    REQUIRE(comp->layers.size() == 2);
    CHECK_EQ(comp->layers[1].name, std::string("Text 1"));
    CHECK_EQ(comp->layers[0].name, std::string("Solid 1"));
}

TEST("bridge: layer kind names map to the right kind")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"shape"})");
    CHECK(fixture.firstLayer().kind == LayerKind::Shape);

    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"adjustment"})");
    CHECK(fixture.document.activeComposition()->layers.back().kind
          == LayerKind::Adjustment);
}

TEST("bridge: removing a layer moves the selection to a neighbour")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");

    Composition* comp = fixture.document.activeComposition();
    REQUIRE(comp->layers.size() == 2);
    const std::string second = comp->layers[1].id;

    fixture.bridge.HandleMessage(
        R"({"type":"removeLayer","layerId":")" + second + R"("})");

    REQUIRE(comp->layers.size() == 1);
    // The selection must not still name the layer that was removed.
    CHECK(fixture.bridge.selectedLayerId() != second);
    CHECK_EQ(fixture.bridge.selectedLayerId(), comp->layers[0].id);
}

TEST("bridge: removing the last layer clears the selection")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    const std::string id = fixture.firstLayer().id;
    fixture.bridge.HandleMessage(R"({"type":"removeLayer","layerId":")" + id + R"("})");
    CHECK(fixture.bridge.selectedLayerId().empty());
}

TEST("bridge: reordering layers moves the right one")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid","name":"A"})");
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid","name":"B"})");
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid","name":"C"})");

    Composition* comp = fixture.document.activeComposition();
    REQUIRE(comp->layers.size() == 3);
    // Bottom to top: A, B, C.
    CHECK_EQ(comp->layers[0].name, std::string("A"));

    const std::string idC = comp->layers[2].id;
    fixture.bridge.HandleMessage(
        R"({"type":"moveLayer","layerId":")" + idC + R"(","index":0})");

    CHECK_EQ(comp->layers[0].name, std::string("C"));
    CHECK_EQ(comp->layers[1].name, std::string("A"));
    CHECK_EQ(comp->layers[2].name, std::string("B"));
}

TEST("bridge: duplicating a layer gives the copy fresh ids")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");

    Composition* comp = fixture.document.activeComposition();
    const std::string originalId = comp->layers[0].id;

    fixture.bridge.HandleMessage(
        R"({"type":"addEffect","layerId":")" + originalId + R"(",)"
        R"("specId":"blur_simple"})");

    REQUIRE(comp->layers[0].effects.size() == 1);
    const std::string originalEffectId = comp->layers[0].effects[0].instanceId;

    fixture.bridge.HandleMessage(
        R"({"type":"duplicateLayer","layerId":")" + originalId + R"("})");

    REQUIRE(fixture.document.activeComposition()->layers.size() == 2);
    const Layer& copy = fixture.document.activeComposition()->layers[1];
    CHECK(copy.id != originalId);
    CHECK_EQ(copy.name, std::string("Solid 1 copy"));
    REQUIRE(copy.effects.size() == 1);
    CHECK(copy.effects[0].instanceId != originalEffectId);
}

TEST("bridge: adding an effect applies the spec's defaults")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    const std::string layerId = fixture.firstLayer().id;

    fixture.bridge.HandleMessage(
        R"({"type":"addEffect","layerId":")" + layerId + R"(",)"
        R"("specId":"chroma_key"})");

    REQUIRE(fixture.firstLayer().effects.size() == 1);
    const Effect& effect = fixture.firstLayer().effects[0];
    CHECK_EQ(effect.specId, std::string("chroma_key"));
    CHECK(effect.enabled);

    // The defaults are the spec's, not zero. A chroma key added with
    // everything at zero keys nothing at all.
    CHECK_NEAR(effect.parameter("keyG", -1.0), 1.0, 1e-9);
    CHECK_NEAR(effect.parameter("tolerance", -1.0), 0.25, 1e-9);
    CHECK_NEAR(effect.parameter("spill", -1.0), 1.0, 1e-9);
}

TEST("bridge: adding an unknown effect is refused")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    const std::string layerId = fixture.firstLayer().id;

    fixture.bridge.HandleMessage(
        R"({"type":"addEffect","layerId":")" + layerId + R"(",)"
        R"("specId":"not_a_real_effect"})");

    CHECK(fixture.sawError());
    CHECK(fixture.firstLayer().effects.empty());
}

TEST("bridge: setting a parameter on a static value changes the constant")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    const std::string layerId = fixture.firstLayer().id;
    fixture.bridge.HandleMessage(
        R"({"type":"addEffect","layerId":")" + layerId + R"(",)"
        R"("specId":"box_blur"})");

    const std::string instanceId = fixture.firstLayer().effects[0].instanceId;
    fixture.bridge.HandleMessage(
        R"({"type":"setParameter","layerId":")" + layerId + R"(",)"
        R"("instanceId":")" + instanceId + R"(",)"
        R"("name":"radius","value":42})");

    CHECK_NEAR(fixture.firstLayer().effects[0].parameter("radius", -1.0), 42.0, 1e-9);
    CHECK(!fixture.firstLayer().effects[0].parameters.at("radius").animated());
}

TEST("bridge: setting a parameter on an animated value adds a keyframe")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    const std::string layerId = fixture.firstLayer().id;
    fixture.bridge.HandleMessage(
        R"({"type":"addEffect","layerId":")" + layerId + R"(",)"
        R"("specId":"box_blur"})");
    const std::string instanceId = fixture.firstLayer().effects[0].instanceId;

    // Put the playhead somewhere and add a keyframe, then move it and set a
    // different value. The second edit must become a second keyframe rather
    // than overwriting the constant, which would change nothing on screen.
    fixture.bridge.HandleMessage(R"({"type":"setPlayhead","time":1.0})");
    fixture.bridge.HandleMessage(
        R"({"type":"setKeyframe","layerId":")" + layerId + R"(",)"
        R"("instanceId":")" + instanceId + R"(",)"
        R"("name":"radius","value":0})");

    fixture.bridge.HandleMessage(R"({"type":"setPlayhead","time":3.0})");
    fixture.bridge.HandleMessage(
        R"({"type":"setParameter","layerId":")" + layerId + R"(",)"
        R"("instanceId":")" + instanceId + R"(",)"
        R"("name":"radius","value":100})");

    const AnimatedValue& radius = fixture.firstLayer().effects[0].parameters.at("radius");
    REQUIRE(radius.keyframes.size() == 2);
    CHECK_EQ(radius.keyframes[0].time, 1.0);
    CHECK_EQ(radius.keyframes[0].value, 0.0);
    CHECK_EQ(radius.keyframes[1].time, 3.0);
    CHECK_EQ(radius.keyframes[1].value, 100.0);
}

TEST("bridge: setting an unknown parameter is refused")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    const std::string layerId = fixture.firstLayer().id;
    fixture.bridge.HandleMessage(
        R"({"type":"addEffect","layerId":")" + layerId + R"(",)"
        R"("specId":"box_blur"})");
    const std::string instanceId = fixture.firstLayer().effects[0].instanceId;

    fixture.bridge.HandleMessage(
        R"({"type":"setParameter","layerId":")" + layerId + R"(",)"
        R"("instanceId":")" + instanceId + R"(",)"
        R"("name":"notAParameter","value":1})");

    CHECK(fixture.sawError());
}

TEST("bridge: transform parameters are addressed through the transform instance")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    const std::string layerId = fixture.firstLayer().id;

    fixture.bridge.HandleMessage(
        R"({"type":"setParameter","layerId":")" + layerId + R"(",)"
        R"("instanceId":"transform","name":"opacity","value":0.25})");

    CHECK_NEAR(fixture.firstLayer().opacity.constant, 0.25, 1e-9);
}

TEST("bridge: resetting the transform clears its keyframes too")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    const std::string layerId = fixture.firstLayer().id;

    fixture.bridge.HandleMessage(
        R"({"type":"setParameter","layerId":")" + layerId + R"(",)"
        R"("instanceId":"transform","name":"opacity","value":0.5})");
    fixture.bridge.HandleMessage(R"({"type":"setPlayhead","time":1.0})");
    fixture.bridge.HandleMessage(
        R"({"type":"setKeyframe","layerId":")" + layerId + R"(",)"
        R"("instanceId":"transform","name":"opacity","value":0.5})");

    REQUIRE(fixture.firstLayer().opacity.animated());

    fixture.bridge.HandleMessage(
        R"({"type":"resetTransform","layerId":")" + layerId + R"("})");

    // A reset that only cleared the constant would leave the layer exactly
    // where it was, because evaluate() reads the keyframes.
    CHECK(!fixture.firstLayer().opacity.animated());
    CHECK_NEAR(fixture.firstLayer().opacity.constant, 1.0, 1e-9);
}

TEST("bridge: removing the last keyframe leaves the value where it was")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    const std::string layerId = fixture.firstLayer().id;

    fixture.bridge.HandleMessage(R"({"type":"setPlayhead","time":2.0})");
    fixture.bridge.HandleMessage(
        R"({"type":"setKeyframe","layerId":")" + layerId + R"(",)"
        R"("instanceId":"transform","name":"opacity","value":0.4})");

    fixture.bridge.HandleMessage(
        R"({"type":"removeKeyframe","layerId":")" + layerId + R"(",)"
        R"("instanceId":"transform","name":"opacity","time":2.0})");

    CHECK(!fixture.firstLayer().opacity.animated());
    // The value must not jump to the default when the last keyframe goes.
    CHECK_NEAR(fixture.firstLayer().opacity.constant, 0.4, 1e-9);
}

TEST("bridge: a valid expression is stored and an invalid one refused")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    const std::string layerId = fixture.firstLayer().id;
    fixture.bridge.HandleMessage(
        R"({"type":"addEffect","layerId":")" + layerId + R"(",)"
        R"("specId":"box_blur"})");
    const std::string instanceId = fixture.firstLayer().effects[0].instanceId;

    fixture.bridge.HandleMessage(
        R"({"type":"setExpression","layerId":")" + layerId + R"(",)"
        R"("instanceId":")" + instanceId + R"(",)"
        R"("name":"radius","source":"time * 20"})");

    REQUIRE(fixture.firstLayer().effects[0].expressions.count("radius") == 1);
    CHECK_EQ(fixture.firstLayer().effects[0].expressions.at("radius").source,
             std::string("time * 20"));

    fixture.bridge.HandleMessage(
        R"({"type":"setExpression","layerId":")" + layerId + R"(",)"
        R"("instanceId":")" + instanceId + R"(",)"
        R"("name":"radius","source":"time *"})");
    CHECK(fixture.sawError());
}

TEST("bridge: toggling an effect flips its enabled flag")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    const std::string layerId = fixture.firstLayer().id;
    fixture.bridge.HandleMessage(
        R"({"type":"addEffect","layerId":")" + layerId + R"(",)"
        R"("specId":"box_blur"})");
    const std::string instanceId = fixture.firstLayer().effects[0].instanceId;

    fixture.bridge.HandleMessage(
        R"({"type":"toggleEffect","layerId":")" + layerId + R"(",)"
        R"("instanceId":")" + instanceId + R"(","enabled":false})");
    CHECK(!fixture.firstLayer().effects[0].enabled);
}

TEST("bridge: reordering effects moves the right one")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    const std::string layerId = fixture.firstLayer().id;

    fixture.bridge.HandleMessage(
        R"({"type":"addEffect","layerId":")" + layerId + R"(",)"
        R"("specId":"box_blur"})");
    fixture.bridge.HandleMessage(
        R"({"type":"addEffect","layerId":")" + layerId + R"(",)"
        R"("specId":"brightness_contrast"})");

    // Look the layer up again rather than holding a reference across the
    // messages: pushUndo copies the document, and a reference taken before
    // that can point into the old vector.
    Layer& layer = fixture.firstLayer();
    REQUIRE(layer.effects.size() == 2);
    CHECK_EQ(layer.effects[0].specId, std::string("box_blur"));
    CHECK_EQ(layer.effects[1].specId, std::string("brightness_contrast"));

    const std::string second = layer.effects[1].instanceId;
    fixture.bridge.HandleMessage(
        R"({"type":"moveEffect","layerId":")" + layerId + R"(",)"
        R"("instanceId":")" + second + R"(","index":0})");

    const Layer& moved = fixture.firstLayer();
    CHECK_EQ(moved.effects[0].specId, std::string("brightness_contrast"));
    CHECK_EQ(moved.effects[1].specId, std::string("box_blur"));
}

TEST("bridge: undo restores the previous document")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"text"})");

    // Undo replaces the whole document, so any pointer taken before it is
    // stale. Re-fetch after each message rather than holding one across.
    REQUIRE(fixture.document.activeComposition()->layers.size() == 2);

    fixture.bridge.HandleMessage(R"({"type":"undo"})");
    CHECK_EQ(fixture.document.activeComposition()->layers.size(), std::size_t(1));

    fixture.bridge.HandleMessage(R"({"type":"redo"})");
    CHECK_EQ(fixture.document.activeComposition()->layers.size(), std::size_t(2));
}

TEST("bridge: undo clears the selection when the layer is gone")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    const std::string id = fixture.firstLayer().id;
    CHECK_EQ(fixture.bridge.selectedLayerId(), id);

    fixture.bridge.HandleMessage(R"({"type":"undo"})");
    // The layer the selection named no longer exists, so the selection must
    // not still point at it.
    CHECK(fixture.bridge.selectedLayerId().empty());
}

TEST("bridge: undo with nothing to undo reports rather than crashing")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"undo"})");
    CHECK(fixture.sawError());
}

TEST("bridge: a new edit clears the redo branch")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    fixture.bridge.HandleMessage(R"({"type":"undo"})");
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"text"})");
    fixture.bridge.HandleMessage(R"({"type":"redo"})");
    // The redo must not bring back a future that no longer follows.
    CHECK(fixture.sawError());
}

TEST("bridge: quitting asks the host")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"quit"})");
    CHECK_EQ(fixture.host.quits, 1);
}

TEST("bridge: a non-http link is refused")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(
        R"({"type":"openExternal","url":"file:///C:/Windows/System32/cmd.exe"})");
    CHECK(fixture.sawError());
}

TEST("bridge: playhead clamps at zero")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"setPlayhead","time":-5})");
    CHECK_EQ(fixture.bridge.playhead(), 0.0);

    fixture.bridge.HandleMessage(R"({"type":"setPlayhead","time":2.5})");
    CHECK_EQ(fixture.bridge.playhead(), 2.5);
}

TEST("bridge: the dirty flag follows edits")
{
    Fixture fixture;
    CHECK(!fixture.bridge.dirty());
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    CHECK(fixture.bridge.dirty());
}

TEST("bridge: a message that changes nothing does not mark the document dirty")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"solid"})");
    fixture.bridge.clearDirty();

    // A selection change is not a document edit. Marking it dirty would make
    // a freshly opened project look modified the moment the user clicked a
    // layer.
    fixture.bridge.HandleMessage(R"({"type":"selectLayer","layerId":"layer-1"})");
    fixture.bridge.HandleMessage(R"({"type":"setPlayhead","time":1.0})");
    CHECK(!fixture.bridge.dirty());
}

TEST("bridge: composition settings are applied and clamped")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(
        R"({"type":"setComposition","width":3840,"height":2160,"duration":30,)"
        R"("fpsNumerator":24,"fpsDenominator":1})");

    const Composition* comp = fixture.document.activeComposition();
    CHECK_EQ(comp->width, 3840);
    CHECK_EQ(comp->height, 2160);
    CHECK_EQ(comp->duration, 30.0);
    CHECK_EQ(comp->fpsNumerator, 24);

    // A zero or negative size would divide by zero in the renderer.
    fixture.bridge.HandleMessage(
        R"({"type":"setComposition","width":0,"height":-10,"duration":0})");
    CHECK(fixture.document.activeComposition()->width >= 1);
    CHECK(fixture.document.activeComposition()->height >= 1);
    CHECK(fixture.document.activeComposition()->duration > 0.0);
}

TEST("bridge: a message for a layer that does not exist is refused")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(
        R"({"type":"renameLayer","layerId":"nope","name":"x"})");
    CHECK(fixture.sawError());
    CHECK(fixture.lastError().find("no layer") != std::string::npos);
}

TEST("bridge: the document snapshot round-trips through the page protocol")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"text"})");

    const json::Value snapshot = fixture.lastOfType("document");
    REQUIRE(!snapshot.isNull());

    std::string error;
    const auto reloaded = projectFromJson(snapshot["document"], &error);
    REQUIRE(reloaded.has_value());
    CHECK_EQ(reloaded->compositions[0].layers.size(), std::size_t(1));
    CHECK(reloaded->compositions[0].layers[0].kind == LayerKind::Text);
}

TEST("bridge: render start forwards the settings to the host")
{
    Fixture fixture;
    fixture.host.savePath = "C:/out/movie.mp4";
    fixture.bridge.HandleMessage(
        R"({"type":"startRender","outputPath":"C:/out/movie.mp4",)"
        R"("videoCodec":"h264_nvenc","quality":21,"useBitrate":true,"bitrateKbps":20000})");

    CHECK_EQ(fixture.host.renderStarts, 1);
    CHECK_EQ(fixture.host.renderOutput, std::string("C:/out/movie.mp4"));
    CHECK_EQ(fixture.host.renderCodec, std::string("h264_nvenc"));
    CHECK_EQ(fixture.host.renderQuality, 21);
}

TEST("bridge: render is refused when media is missing")
{
    Fixture fixture;
    // A layer that references a file which does not exist.
    fixture.bridge.HandleMessage(R"({"type":"addLayer","kind":"media"})");
    Asset asset;
    asset.id = "asset-missing";
    asset.name = "gone.mp4";
    asset.path = "C:/definitely/not/here.mp4";
    fixture.document.assets.push_back(asset);
    fixture.firstLayer().assetId = "asset-missing";

    fixture.bridge.HandleMessage(
        R"({"type":"startRender","outputPath":"C:/out/movie.mp4"})");

    // Starting a long render that fails at frame 200 is worse than refusing
    // before it starts.
    CHECK_EQ(fixture.host.renderStarts, 0);
    CHECK(fixture.sawError());
    CHECK(fixture.lastError().find("missing") != std::string::npos);
}

TEST("bridge: cancelling a render asks the host")
{
    Fixture fixture;
    fixture.bridge.HandleMessage(R"({"type":"cancelRender"})");
    CHECK_EQ(fixture.host.cancels, 1);
}

TEST("bridge: the catalogue carries the parameters the inspector needs")
{
    Fixture fixture;
    const json::Value catalogue = fixture.lastOfType("catalogue");
    REQUIRE(!catalogue.isNull());

    bool foundChroma = false;
    for (const json::Value& effect : catalogue["effects"].asArray()) {
        if (effect["id"].asString() != "chroma_key") continue;
        foundChroma = true;
        CHECK(!effect["name"].asString().empty());
        CHECK(!effect["category"].asString().empty());
        CHECK(effect["params"].size() >= 10);

        bool foundTolerance = false;
        for (const json::Value& p : effect["params"].asArray()) {
            if (p["name"].asString() != "tolerance") continue;
            foundTolerance = true;
            CHECK_EQ(p["kind"].asString(), std::string("scalar"));
            CHECK(p["max"].asNumber() > p["min"].asNumber());
        }
        CHECK(foundTolerance);
    }
    CHECK(foundChroma);
}

TEST("bridge: a choice parameter carries its labels")
{
    Fixture fixture;
    const json::Value catalogue = fixture.lastOfType("catalogue");
    REQUIRE(!catalogue.isNull());

    bool checked = false;
    for (const json::Value& effect : catalogue["effects"].asArray()) {
        if (effect["id"].asString() != "blend_backdrop") continue;
        for (const json::Value& p : effect["params"].asArray()) {
            if (p["name"].asString() != "mode") continue;
            CHECK_EQ(p["kind"].asString(), std::string("choice"));
            CHECK_EQ(p["choices"].size(), std::size_t(21));
            CHECK_EQ(p["choices"][0].asString(), std::string("Normal"));
            checked = true;
        }
    }
    CHECK(checked);
}
