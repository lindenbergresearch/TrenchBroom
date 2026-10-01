#pragma once

#include "vm/vec.h"
#include "vm/forward.h"
#include "vm/bbox.h"

#include <cstddef>
#include <vector>
#include <cstdint>
#include <memory>

namespace TrenchBroom::Model {
class Entity;
class EntityNodeBase;
}

namespace TrenchBroom::Renderer {
class ActiveShader;
class RenderContext;
class LightingOcclusion;

// Must match Lighting.fragsh. Each spatial region has its own bounded light list.
inline constexpr size_t MaxRenderLights = 16;

enum class PreviewLightType { Point, Spot, Directional };
enum class LightFalloff { Linear, Inverse, InverseSquare, Constant, LocalMin, InverseSquareOffset };
enum class LightingProfile { Quake, HalfLife };

struct PreviewLight {
    vm::vec3f position{0.0f, 0.0f, 0.0f};
    vm::vec3f color{1.0f, 1.0f, 1.0f};
    vm::vec3f direction{0.0f, 0.0f, -1.0f};
    float brightness = 300.0f;
    PreviewLightType type = PreviewLightType::Point;
    LightFalloff falloff = LightFalloff::Linear;
    float distanceScale = 1.0f;
    float falloffDistance = 0.0f;
    float angleScale = 0.5f;
    float outerConeCos = 1.0f;
    float innerConeCos = 1.0f;
    bool operator==(const PreviewLight&) const = default;
};

struct PreviewLighting {
    uint64_t revision = 0;
    std::vector<PreviewLight> lights;
    vm::vec3f minLight{0.0f, 0.0f, 0.0f};
    float brightnessScale = 1.0f / 255.0f;
    float gamma = 1.0f;
    std::shared_ptr<const LightingOcclusion> occlusion;
};

// Cached per spatial draw region, independent of camera position. Packed arrays
// can be uploaded in two calls and retained by the shader program between draws.
struct LightSelection {
    uint64_t revision = 0;
    bool valid = false;
    bool testOcclusion = false;
    vm::bbox3f bounds;
    std::vector<vm::vec4f> linearLights;
    std::vector<vm::vec4f> otherLights;
};

// Quake / ericw entity conventions. Cached lightmaps add visibility testing;
// lightstyles, emissive surfaces and bounced light require a later preview stage.
PreviewLighting buildPreviewLighting(
    const std::vector<Model::EntityNodeBase*>& nodes, const Model::Entity* world,
    LightingProfile profile = LightingProfile::Quake);

void configureLightingShader(ActiveShader& shader, const RenderContext& context);
void selectPreviewLights(const PreviewLighting& lighting, const vm::bbox3f& bounds, LightSelection& selection,
    bool testOcclusion = false);
void configureLightingRegion(ActiveShader& shader, const RenderContext& context,
    const vm::bbox3f& bounds, LightSelection& selection, bool testOcclusion = false);

// Background lightmaps use all lights reaching the surface, without a 16-light cap.
std::vector<size_t> previewLightCandidates(const PreviewLighting& lighting, const vm::bbox3f& bounds);
vm::vec3f samplePreviewLighting(const PreviewLighting& lighting, const std::vector<size_t>& candidates,
    const vm::vec3f& position, const vm::vec3f& normal);
}
