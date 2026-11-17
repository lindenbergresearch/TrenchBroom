#include "Renderer/Lighting.h"
#include "Renderer/LightingOcclusion.h"

#include <catch2/catch.hpp>

namespace TrenchBroom::Renderer {
namespace {
LightingOcclusion::Triangle plane(const float z, const bool sky = false) {
    return {{-1024, -1024, z}, {1024, -1024, z}, {0, 1024, z}, sky};
}
PreviewLighting pointScene() {
    auto scene = PreviewLighting{};
    auto light = PreviewLight{};
    light.position = {0, 0, 100};
    scene.lights.push_back(light);
    return scene;
}
vm::vec3f sample(const PreviewLighting& scene) {
    return samplePreviewLighting(scene, previewLightCandidates(scene,
        vm::bbox3f{{-1, -1, -1}, {1, 1, 1}}), {0, 0, 0}, {0, 0, 1});
}
}

TEST_CASE("LightingPreview walls block light but not rays ending before them", "[lighting]") {
    const auto scene = LightingOcclusion{{plane(50)}};
    CHECK(scene.trace({0, 0, 0}, {0, 0, 1}, 100) == LightingOcclusion::Hit::Solid);
    CHECK(scene.trace({0, 0, 0}, {0, 0, 1}, 49) == LightingOcclusion::Hit::None);
    CHECK(scene.trace({0, 0, 100}, {0, 0, -1}, 100) == LightingOcclusion::Hit::Solid);
    CHECK(scene.trace({0, 0, 0}, {1, 0, 0}, 100) == LightingOcclusion::Hit::None);
    CHECK(scene.trace({2048, 0, 0}, {0, 0, 1}, 100) == LightingOcclusion::Hit::None);
}

TEST_CASE("LightingPreview ray tree returns closest sky or solid", "[lighting]") {
    auto triangles = std::vector<LightingOcclusion::Triangle>{};
    for (int i = 0; i < 32; ++i) triangles.push_back(plane(100.0f + i * 10.0f, true));
    triangles.push_back(plane(50));
    auto scene = LightingOcclusion{triangles};
    CHECK(scene.trace({0, 0, 0}, {0, 0, 1}, 1000) == LightingOcclusion::Hit::Solid);
    CHECK(scene.trace({0, 0, 60}, {0, 0, 1}, 1000) == LightingOcclusion::Hit::Sky);
}

TEST_CASE("LightingPreview preserves Quake falloff and handles occlusion", "[lighting]") {
    auto scene = pointScene();
    CHECK(sample(scene).x() == Approx(200));
    scene.occlusion = std::make_shared<LightingOcclusion>(std::vector{plane(50)});
    CHECK(sample(scene).x() == Approx(0));
    scene.minLight = {10, 20, 30};
    const auto dark = sample(scene);
    CHECK(dark.x() == Approx(10));
    CHECK(dark.y() == Approx(20));
    CHECK(dark.z() == Approx(30));
    scene.occlusion = std::make_shared<LightingOcclusion>(std::vector{plane(0)});
    CHECK(sample(scene).x() == Approx(200)); // no self-shadow acne
}

TEST_CASE("LightingPreview sunlight requires visible sky", "[lighting]") {
    auto scene = pointScene();
    scene.lights[0].type = PreviewLightType::Directional;
    scene.occlusion = std::make_shared<LightingOcclusion>(std::vector<LightingOcclusion::Triangle>{});
    CHECK(sample(scene).x() == Approx(0));
    scene.occlusion = std::make_shared<LightingOcclusion>(std::vector{plane(100, true)});
    CHECK(sample(scene).x() == Approx(300));
    scene.occlusion = std::make_shared<LightingOcclusion>(std::vector{plane(100, true), plane(50)});
    CHECK(sample(scene).x() == Approx(0));
}

TEST_CASE("LightingPreview local minlight is occluded and negative lights subtract", "[lighting]") {
    auto scene = pointScene();
    auto negative = scene.lights.front();
    negative.brightness = -50;
    negative.falloff = LightFalloff::Constant;
    scene.lights.push_back(negative);
    CHECK(sample(scene).x() == Approx(150));
    scene.lights[0].falloff = LightFalloff::LocalMin;
    CHECK(sample(scene).x() == Approx(300));
    scene.occlusion = std::make_shared<LightingOcclusion>(std::vector{plane(50)});
    CHECK(sample(scene).x() == Approx(0));
}

TEST_CASE("LightingPreview cached lightmaps include more than sixteen sources", "[lighting]") {
    auto scene = pointScene();
    scene.lights.resize(20, scene.lights.front());
    CHECK(sample(scene).x() == Approx(4000));
    auto selection = LightSelection{};
    selectPreviewLights(scene, vm::bbox3f{{-1, -1, -1}, {1, 1, 1}}, selection);
    CHECK(selection.linearLights.size() == 32);
    scene.occlusion = std::make_shared<LightingOcclusion>(std::vector{plane(50)});
    selectPreviewLights(scene, vm::bbox3f{{-1, -1, -1}, {1, 1, 1}}, selection, true);
    CHECK(selection.linearLights.empty());
}
}
