#include "TestFramework.h"

#include "core/Model.h"

using namespace keyflow;

TEST("keyframe: a static value ignores time")
{
    AnimatedValue v;
    v.constant = 5.0;
    CHECK_EQ(v.evaluate(0.0), 5.0);
    CHECK_EQ(v.evaluate(100.0), 5.0);
    CHECK(!v.animated());
}

TEST("keyframe: a single keyframe holds everywhere")
{
    AnimatedValue v;
    v.setKeyframe({2.0, 7.0, Interpolation::Linear, 0.33, 0.33});
    CHECK_EQ(v.evaluate(0.0), 7.0);
    CHECK_EQ(v.evaluate(2.0), 7.0);
    CHECK_EQ(v.evaluate(50.0), 7.0);
}

TEST("keyframe: linear interpolates between two points")
{
    AnimatedValue v;
    v.setKeyframe({0.0, 0.0, Interpolation::Linear, 0.33, 0.33});
    v.setKeyframe({2.0, 10.0, Interpolation::Linear, 0.33, 0.33});

    CHECK_EQ(v.evaluate(0.0), 0.0);
    CHECK_EQ(v.evaluate(1.0), 5.0);
    CHECK_EQ(v.evaluate(2.0), 10.0);
    // Outside the range the value holds rather than extrapolating.
    CHECK_EQ(v.evaluate(-5.0), 0.0);
    CHECK_EQ(v.evaluate(99.0), 10.0);
}

TEST("keyframe: hold steps at the next keyframe")
{
    AnimatedValue v;
    v.setKeyframe({0.0, 1.0, Interpolation::Hold, 0.33, 0.33});
    v.setKeyframe({2.0, 9.0, Interpolation::Hold, 0.33, 0.33});

    CHECK_EQ(v.evaluate(0.0), 1.0);
    CHECK_EQ(v.evaluate(1.999), 1.0);
    CHECK_EQ(v.evaluate(2.0), 9.0);
}

TEST("keyframe: ease is monotonic and passes through both ends")
{
    AnimatedValue v;
    v.setKeyframe({0.0, 0.0, Interpolation::Ease, 0.33, 0.33});
    v.setKeyframe({1.0, 1.0, Interpolation::Ease, 0.33, 0.33});

    CHECK_NEAR(v.evaluate(0.0), 0.0, 1e-12);
    CHECK_NEAR(v.evaluate(1.0), 1.0, 1e-12);
    CHECK_NEAR(v.evaluate(0.5), 0.5, 1e-9);

    // Monotonic: each sample is at least the one before.
    double previous = -1.0;
    for (int i = 0; i <= 100; ++i) {
        const double t = static_cast<double>(i) / 100.0;
        const double value = v.evaluate(t);
        CHECK(value >= previous - 1e-12);
        previous = value;
    }
}

TEST("keyframe: bezier honours the handles")
{
    AnimatedValue v;
    // Both handles pinned to the start: the curve leaves late and arrives
    // fast, which is the "ease in" shape.
    v.setKeyframe({0.0, 0.0, Interpolation::Bezier, 1.0, 1.0});
    v.setKeyframe({1.0, 1.0, Interpolation::Bezier, 1.0, 1.0});

    CHECK_NEAR(v.evaluate(0.0), 0.0, 1e-9);
    CHECK_NEAR(v.evaluate(1.0), 1.0, 1e-9);
    // At the midpoint the late-leaving curve is behind the linear one.
    CHECK(v.evaluate(0.5) < 0.5);
}

TEST("keyframe: setting a keyframe at an existing time replaces it")
{
    AnimatedValue v;
    v.setKeyframe({1.0, 10.0, Interpolation::Linear, 0.33, 0.33});
    v.setKeyframe({1.0, 20.0, Interpolation::Linear, 0.33, 0.33});
    CHECK_EQ(v.keyframes.size(), std::size_t(1));
    CHECK_EQ(v.evaluate(1.0), 20.0);
}

TEST("keyframe: out-of-order inserts stay sorted")
{
    AnimatedValue v;
    v.setKeyframe({3.0, 3.0, Interpolation::Linear, 0.33, 0.33});
    v.setKeyframe({1.0, 1.0, Interpolation::Linear, 0.33, 0.33});
    v.setKeyframe({2.0, 2.0, Interpolation::Linear, 0.33, 0.33});

    REQUIRE(v.keyframes.size() == 3);
    CHECK_EQ(v.keyframes[0].time, 1.0);
    CHECK_EQ(v.keyframes[1].time, 2.0);
    CHECK_EQ(v.keyframes[2].time, 3.0);
    // The interpolation between 1 and 2 must use those two, not 3 and 1.
    CHECK_EQ(v.evaluate(1.5), 1.5);
}

TEST("keyframe: remove finds the exact time")
{
    AnimatedValue v;
    v.setKeyframe({1.0, 1.0, Interpolation::Linear, 0.33, 0.33});
    v.setKeyframe({2.0, 2.0, Interpolation::Linear, 0.33, 0.33});

    CHECK(v.removeKeyframe(1.0));
    CHECK_EQ(v.keyframes.size(), std::size_t(1));
    CHECK(!v.removeKeyframe(1.0));
    CHECK_EQ(v.firstTime(), 2.0);
    CHECK_EQ(v.lastTime(), 2.0);
}

TEST("layer: transformAt reads every channel")
{
    Layer layer;
    layer.positionX.constant = 100.0;
    layer.positionY.constant = 200.0;
    layer.scaleX.constant = 2.0;
    layer.scaleY.constant = 0.5;
    layer.rotation.constant = 45.0;
    layer.opacity.constant = 0.8;
    layer.anchorX.constant = 0.25;
    layer.anchorY.constant = 0.75;

    const Layer::Transform t = layer.transformAt(0.0);
    CHECK_EQ(t.x, 100.0);
    CHECK_EQ(t.y, 200.0);
    CHECK_EQ(t.scaleX, 2.0);
    CHECK_EQ(t.scaleY, 0.5);
    CHECK_EQ(t.rotation, 45.0);
    CHECK_EQ(t.opacity, 0.8);
    CHECK_EQ(t.anchorX, 0.25);
    CHECK_EQ(t.anchorY, 0.75);
}

TEST("layer: activeAt respects the in and out points")
{
    Layer layer;
    layer.inPoint = 1.0;
    layer.outPoint = 3.0;

    CHECK(!layer.activeAt(0.5));
    CHECK(layer.activeAt(1.0));
    CHECK(layer.activeAt(2.999));
    // The out point is exclusive: two adjacent layers must not both be live on
    // the boundary frame, or the frame is drawn twice.
    CHECK(!layer.activeAt(3.0));

    layer.visible = false;
    CHECK(!layer.activeAt(2.0));
}

TEST("composition: frameCount rounds and never returns zero")
{
    Composition comp;
    comp.fpsNumerator = 30;
    comp.fpsDenominator = 1;
    comp.duration = 10.0;
    CHECK_EQ(comp.frameCount(), 300);

    comp.duration = 0.0;
    CHECK_EQ(comp.frameCount(), 1);

    // 29.97 is exact as a rational, which a float would not be.
    comp.fpsNumerator = 30000;
    comp.fpsDenominator = 1001;
    comp.duration = 10.0;
    CHECK_NEAR(comp.fps(), 29.97002997, 1e-6);
    CHECK_EQ(comp.frameCount(), 300);
}

TEST("composition: findLayer and indexOfLayer agree")
{
    Composition comp;
    Layer a;
    a.id = "layer-a";
    Layer b;
    b.id = "layer-b";
    comp.layers.push_back(a);
    comp.layers.push_back(b);

    CHECK(comp.findLayer("layer-a") != nullptr);
    CHECK(comp.findLayer("nope") == nullptr);
    CHECK_EQ(comp.indexOfLayer("layer-a"), 0);
    CHECK_EQ(comp.indexOfLayer("layer-b"), 1);
    CHECK_EQ(comp.indexOfLayer("nope"), -1);
}

TEST("project: createDefault has one composition and selects it")
{
    const Project project = Project::createDefault("My Project");
    CHECK_EQ(project.name, std::string("My Project"));
    REQUIRE(project.compositions.size() == 1);
    CHECK_EQ(project.activeCompositionId, project.compositions[0].id);
    CHECK(project.activeComposition() != nullptr);
}

TEST("project: newId does not collide with existing ids")
{
    Project project = Project::createDefault();
    Composition comp;
    comp.id = "layer-1";
    project.compositions.push_back(comp);

    const std::string id = project.newId("layer");
    CHECK(id != "layer-1");
    CHECK_EQ(id, std::string("layer-2"));
}

TEST("project: referencedAssetIds is unique and ordered")
{
    Project project = Project::createDefault();
    Composition comp;

    Layer a;
    a.assetId = "asset-1";
    Layer b;
    b.assetId = "asset-2";
    Layer c;
    c.assetId = "asset-1";
    Layer d; // no asset
    comp.layers = {a, b, c, d};
    project.compositions.push_back(comp);

    const std::vector<std::string> ids = project.referencedAssetIds();
    REQUIRE(ids.size() == 2);
    CHECK_EQ(ids[0], std::string("asset-1"));
    CHECK_EQ(ids[1], std::string("asset-2"));
}

TEST("blend mode: names round-trip")
{
    for (int i = 0; i <= static_cast<int>(BlendMode::Luminosity); ++i) {
        const auto mode = static_cast<BlendMode>(i);
        CHECK_EQ(static_cast<int>(blendModeFromName(blendModeName(mode))), i);
    }
    CHECK_EQ(static_cast<int>(blendModeFromName("not-a-mode")),
             static_cast<int>(BlendMode::Normal));
}

TEST("blend mode: the numbering matches the compositor shader")
{
    // The shader compares uMode against these integers. Renumbering the enum
    // would silently change the meaning of every saved project, so the values
    // are pinned here rather than left implicit.
    CHECK_EQ(static_cast<int>(BlendMode::Normal), 0);
    CHECK_EQ(static_cast<int>(BlendMode::Multiply), 2);
    CHECK_EQ(static_cast<int>(BlendMode::Screen), 6);
    CHECK_EQ(static_cast<int>(BlendMode::Overlay), 9);
    CHECK_EQ(static_cast<int>(BlendMode::SoftLight), 10);
    CHECK_EQ(static_cast<int>(BlendMode::Difference), 13);
    CHECK_EQ(static_cast<int>(BlendMode::Hue), 17);
    CHECK_EQ(static_cast<int>(BlendMode::Luminosity), 20);
}
