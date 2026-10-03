#include "TestFramework.h"

#include "core/Model.h"

#include <filesystem>
#include <fstream>

using namespace keyflow;

namespace {

/// A project with enough variety to exercise every branch of the serialiser.
Project buildRichProject()
{
    Project project;
    project.name = "Test Project";
    project.activeCompositionId = "comp-1";

    Asset asset;
    asset.id = "asset-1";
    asset.name = "clip.mp4";
    asset.path = "C:/media/clip.mp4";
    asset.duration = 12.5;
    asset.width = 1920;
    asset.height = 1080;
    asset.hasAudio = true;
    project.assets.push_back(asset);

    Composition comp;
    comp.id = "comp-1";
    comp.name = "Main";
    comp.width = 1280;
    comp.height = 720;
    comp.fpsNumerator = 30000;
    comp.fpsDenominator = 1001;
    comp.duration = 8.25;
    comp.backgroundR = 0.1;
    comp.backgroundG = 0.2;
    comp.backgroundB = 0.3;
    comp.hasCamera = true;
    comp.cameraZ.constant = -800.0;

    Layer media;
    media.id = "layer-1";
    media.name = "Clip";
    media.kind = LayerKind::Media;
    media.assetId = "asset-1";
    media.blend = BlendMode::Screen;
    media.blendStrength = 0.75;
    media.motionBlurEnabled = true;
    media.motionBlurShutter = 0.5;
    media.inPoint = 1.0;
    media.outPoint = 5.0;
    media.positionX.setKeyframe({0.0, 0.0, Interpolation::Linear, 0.33, 0.33});
    media.positionX.setKeyframe({2.0, 100.0, Interpolation::Bezier, 0.25, 0.75});
    media.opacity.constant = 0.9;

    Effect key;
    key.specId = "chroma_key";
    key.instanceId = "effect-1";
    key.enabled = true;
    key.parameters["tolerance"].constant = 0.3;
    key.parameters["keyG"].constant = 0.9;
    Expression expr;
    expr.source = "time * 10";
    expr.enabled = true;
    key.expressions["softness"] = expr;
    media.effects.push_back(key);

    Layer shape;
    shape.id = "layer-2";
    shape.name = "Star";
    shape.kind = LayerKind::Shape;
    shape.pathFilled = true;
    shape.pathStroked = true;
    shape.strokeWidth = 3.5;
    shape.fillR = 1.0;
    shape.fillG = 0.5;
    shape.fillB = 0.0;
    shape.path.closed = true;
    shape.path.points.push_back({0.0, 0.0, -1.0, -1.0, 1.0, 1.0});
    shape.path.points.push_back({10.0, 20.0, 0.0, 0.0, 0.0, 0.0});

    Layer text;
    text.id = "layer-3";
    text.name = "Title";
    text.kind = LayerKind::Text;
    text.text = "Hello, world";
    text.textSize = 96.0;

    Layer solid;
    solid.id = "layer-4";
    solid.name = "BG";
    solid.kind = LayerKind::Solid;
    solid.solidR = 0.2;
    solid.solidG = 0.4;
    solid.solidB = 0.6;
    solid.solidA = 1.0;

    comp.layers = {solid, media, shape, text};
    project.compositions.push_back(comp);
    return project;
}

} // namespace

TEST("project io: a rich project survives a round trip")
{
    const Project original = buildRichProject();
    const std::string text = keyflow::json::write(projectToJson(original), 2);

    std::string error;
    const auto loaded = projectFromJson(keyflow::json::parse(text, &error), &error);
    REQUIRE(loaded.has_value());

    CHECK_EQ(loaded->name, original.name);
    CHECK_EQ(loaded->assets.size(), std::size_t(1));
    CHECK_EQ(loaded->compositions.size(), std::size_t(1));

    const Composition& comp = loaded->compositions[0];
    CHECK_EQ(comp.id, std::string("comp-1"));
    CHECK_EQ(comp.width, 1280);
    CHECK_EQ(comp.height, 720);
    CHECK_EQ(comp.fpsNumerator, 30000);
    CHECK_EQ(comp.fpsDenominator, 1001);
    CHECK_NEAR(comp.duration, 8.25, 1e-12);
    CHECK_NEAR(comp.backgroundG, 0.2, 1e-12);
    CHECK(comp.hasCamera);
    CHECK_NEAR(comp.cameraZ.constant, -800.0, 1e-12);
    CHECK_EQ(comp.layers.size(), std::size_t(4));
}

TEST("project io: keyframes survive with their interpolation and handles")
{
    const Project original = buildRichProject();
    std::string error;
    const auto loaded = projectFromJson(projectToJson(original), &error);
    REQUIRE(loaded.has_value());

    const Layer& media = loaded->compositions[0].layers[1];
    REQUIRE(media.positionX.keyframes.size() == 2);
    CHECK_EQ(media.positionX.keyframes[0].time, 0.0);
    CHECK_EQ(media.positionX.keyframes[0].value, 0.0);
    CHECK(media.positionX.keyframes[0].interpolation == Interpolation::Linear);
    CHECK_EQ(media.positionX.keyframes[1].time, 2.0);
    CHECK_EQ(media.positionX.keyframes[1].value, 100.0);
    CHECK(media.positionX.keyframes[1].interpolation == Interpolation::Bezier);
    CHECK_NEAR(media.positionX.keyframes[1].inHandle, 0.25, 1e-12);
    CHECK_NEAR(media.positionX.keyframes[1].outHandle, 0.75, 1e-12);
}

TEST("project io: effects survive with their parameters and expressions")
{
    const Project original = buildRichProject();
    std::string error;
    const auto loaded = projectFromJson(projectToJson(original), &error);
    REQUIRE(loaded.has_value());

    const Layer& media = loaded->compositions[0].layers[1];
    REQUIRE(media.effects.size() == 1);
    const Effect& effect = media.effects[0];
    CHECK_EQ(effect.specId, std::string("chroma_key"));
    CHECK_EQ(effect.instanceId, std::string("effect-1"));
    CHECK(effect.enabled);
    CHECK_NEAR(effect.parameter("tolerance", -1.0), 0.3, 1e-12);
    CHECK_NEAR(effect.parameter("keyG", -1.0), 0.9, 1e-12);

    REQUIRE(effect.expressions.count("softness") == 1);
    CHECK_EQ(effect.expressions.at("softness").source, std::string("time * 10"));
    CHECK(effect.expressions.at("softness").enabled);
}

TEST("project io: a missing parameter falls back rather than reading zero")
{
    Effect effect;
    effect.specId = "chroma_key";
    // The parameter is absent. Reading it must give the caller's fallback, not
    // zero, or a project saved before a parameter existed would open with the
    // parameter at zero instead of at the spec's default.
    CHECK_NEAR(effect.parameter("tolerance", 0.25), 0.25, 1e-12);
}

TEST("project io: paths and shapes survive")
{
    const Project original = buildRichProject();
    std::string error;
    const auto loaded = projectFromJson(projectToJson(original), &error);
    REQUIRE(loaded.has_value());

    CHECK_EQ(loaded->assets[0].path, std::string("C:/media/clip.mp4"));
    CHECK_EQ(loaded->assets[0].duration, 12.5);
    CHECK(loaded->assets[0].hasAudio);

    const Layer& shape = loaded->compositions[0].layers[2];
    CHECK(shape.kind == LayerKind::Shape);
    REQUIRE(shape.path.points.size() == 2);
    CHECK_EQ(shape.path.points[0].x, 0.0);
    CHECK_EQ(shape.path.points[0].inX, -1.0);
    CHECK_EQ(shape.path.points[1].y, 20.0);
    CHECK(shape.path.closed);
    CHECK_NEAR(shape.strokeWidth, 3.5, 1e-12);
}

TEST("project io: text and solid content survive")
{
    const Project original = buildRichProject();
    std::string error;
    const auto loaded = projectFromJson(projectToJson(original), &error);
    REQUIRE(loaded.has_value());

    const Layer& text = loaded->compositions[0].layers[3];
    CHECK(text.kind == LayerKind::Text);
    CHECK_EQ(text.text, std::string("Hello, world"));
    CHECK_NEAR(text.textSize, 96.0, 1e-12);

    const Layer& solid = loaded->compositions[0].layers[0];
    CHECK(solid.kind == LayerKind::Solid);
    CHECK_NEAR(solid.solidG, 0.4, 1e-12);
}

TEST("project io: refuses a file that is not a project")
{
    std::string error;
    const auto loaded = projectFromJson(keyflow::json::parse(R"({"format":"other"})", &error), &error);
    CHECK(!loaded.has_value());
    CHECK(error.find("not a Keyflow project") != std::string::npos);
}

TEST("project io: refuses a version from the future")
{
    std::string error;
    const auto loaded = projectFromJson(
        keyflow::json::parse(R"({"format":"keyflow-project","version":99})", &error), &error);
    CHECK(!loaded.has_value());
    CHECK(error.find("newer version") != std::string::npos);
}

TEST("project io: a project with no compositions gets one")
{
    // The editor has to open something. An empty compositions array is
    // recoverable, so it is repaired rather than refused.
    std::string error;
    const auto loaded = projectFromJson(
        keyflow::json::parse(R"({"format":"keyflow-project","version":1})", &error), &error);
    REQUIRE(loaded.has_value());
    CHECK_EQ(loaded->compositions.size(), std::size_t(1));
    CHECK(!loaded->activeCompositionId.empty());
    CHECK(loaded->activeComposition() != nullptr);
}

TEST("project io: an active composition that does not exist falls back")
{
    std::string error;
    const auto loaded = projectFromJson(
        keyflow::json::parse(
            R"({"format":"keyflow-project","version":1,)"
            R"("activeCompositionId":"gone",)"
            R"("compositions":[{"id":"comp-9","layers":[]}]})",
            &error),
        &error);
    REQUIRE(loaded.has_value());
    CHECK_EQ(loaded->activeCompositionId, std::string("comp-9"));
}

TEST("project io: out-of-order keyframes are sorted on load")
{
    // A hand-edited project can carry keys out of order. Evaluating against
    // the wrong pair would produce a value that is wrong in a way that is very
    // hard to see, so the loader sorts rather than trusting the file.
    const char* text = R"({
        "format": "keyflow-project", "version": 1,
        "compositions": [{
            "id": "c", "layers": [{
                "id": "l", "name": "L",
                "transform": {
                    "opacity": {"value": 1.0, "keys": [
                        {"time": 3.0, "value": 3.0},
                        {"time": 1.0, "value": 1.0},
                        {"time": 2.0, "value": 2.0}
                    ]}
                }
            }]
        }]
    })";

    std::string error;
    const auto loaded = projectFromJson(keyflow::json::parse(text, &error), &error);
    REQUIRE(loaded.has_value());
    const Layer& layer = loaded->compositions[0].layers[0];
    REQUIRE(layer.opacity.keyframes.size() == 3);
    CHECK_EQ(layer.opacity.keyframes[0].time, 1.0);
    CHECK_EQ(layer.opacity.keyframes[1].time, 2.0);
    CHECK_EQ(layer.opacity.keyframes[2].time, 3.0);
}

TEST("project io: save and load through the filesystem")
{
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "keyflow-test-project";
    std::filesystem::create_directories(dir);
    const std::filesystem::path file = dir / "roundtrip.kfproj";

    const Project original = buildRichProject();
    std::string error;
    REQUIRE(saveProject(original, file.string(), &error));

    const auto loaded = loadProject(file.string(), &error);
    REQUIRE(loaded.has_value());
    CHECK_EQ(loaded->name, original.name);
    CHECK_EQ(loaded->compositions[0].layers.size(), std::size_t(4));

    std::filesystem::remove_all(dir);
}

TEST("project io: saving over an existing file replaces it atomically")
{
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "keyflow-test-overwrite";
    std::filesystem::create_directories(dir);
    const std::filesystem::path file = dir / "twice.kfproj";

    std::string error;
    Project first = Project::createDefault("First");
    REQUIRE(saveProject(first, file.string(), &error));

    Project second = Project::createDefault("Second");
    REQUIRE(saveProject(second, file.string(), &error));

    const auto loaded = loadProject(file.string(), &error);
    REQUIRE(loaded.has_value());
    CHECK_EQ(loaded->name, std::string("Second"));

    // The temporary file the writer used must not be left behind.
    CHECK(!std::filesystem::exists(file.string() + ".tmp"));

    std::filesystem::remove_all(dir);
}

TEST("project io: loading a file that does not exist fails cleanly")
{
    std::string error;
    const auto loaded = loadProject("C:/definitely/not/here.kfproj", &error);
    CHECK(!loaded.has_value());
    CHECK(!error.empty());
}
