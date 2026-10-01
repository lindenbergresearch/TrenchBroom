#include "Lighting.h"

#include "Assets/EntityDefinition.h"
#include "Assets/PropertyDefinition.h"
#include "Model/Entity.h"
#include "Model/EntityNodeBase.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/ActiveShader.h"
#include "Renderer/RenderContext.h"
#include "Renderer/LightingOcclusion.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <initializer_list>
#include <locale>
#include <optional>
#include <sstream>
#include <string>

namespace TrenchBroom::Renderer {namespace {
        // Explicit properties take precedence over ALL definition defaults, including
        // aliases (a default for _light must not mask an explicitly specified light).
        std::string property(const Model::Entity& entity, std::initializer_list<const char *> keys) {
            for (const auto* key: keys) { if (const auto* value = entity.property(key); value && !value->empty()) return *value; }
            if (const auto* definition = entity.definition()) {
                for (const auto* key: keys) {
                    if (const auto* def = definition->propertyDefinition(key)) {
                        auto value = Assets::PropertyDefinition::defaultValue(*def);
                        if (!value.empty()) return value;
                    }
                }
            }
            return {};
        }

        std::vector<float> numbers(const std::string& text) {
            auto stream = std::istringstream(text);
            stream.imbue(std::locale::classic());
            auto result = std::vector<float>{};
            float value;
            while (true) {
                stream >> std::ws;
                if (stream.eof()) break;
                if (!(stream >> value) || !std::isfinite(value) || result.size() == 4) return {};
                result.push_back(value);
            }
            // Reject partial values such as "300garbage" as well as NaN and infinity.
            return result;
        }

        float scalar(const Model::Entity& entity, std::initializer_list<const char *> keys, float fallback) {
            const auto values = numbers(property(entity, keys));
            return values.size() == 1 ? values[0] : fallback;
        }

        vm::vec3f color(const std::vector<float>& values, bool byteColor = false) {
            const auto largest = std::max({values[0], values[1], values[2]});
            const auto divisor = byteColor || largest > 1.0f ? 255.0f : 1.0f;
            return vm::vec3f{
                std::clamp(values[0] / divisor, 0.0f, 1.0f),
                std::clamp(values[1] / divisor, 0.0f, 1.0f),
                std::clamp(values[2] / divisor, 0.0f, 1.0f)
            };
        }

        vm::vec3f colorProperty(const Model::Entity& entity, std::initializer_list<const char *> keys) {
            const auto values = numbers(property(entity, keys));
            return values.size() == 3 ? color(values) : vm::vec3f{1.0f, 1.0f, 1.0f};
        }

        vm::vec3f direction(float yaw, float pitch) {
            constexpr auto radians = float(M_PI / 180.0);
            yaw = std::remainder(yaw, 360.0f) * radians;
            pitch = std::remainder(pitch, 360.0f) * radians;

            return vm::vec3f{
                std::cos(pitch) * std::cos(yaw),
                std::cos(pitch) * std::sin(yaw),
                std::sin(pitch)
            };
        }

        bool finite(const vm::vec3f& v) { return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z()); }

        std::optional<vm::vec3f> spotDirection(const Model::EntityNodeBase& node) {
            const auto& entity = node.entity();

            // Only the actual target key is an aiming target; target2 etc. can be triggers.
            if (const auto* target = entity.property("target")) {
                for (const auto* candidate: node.linkTargets()) {
                    const auto* name = candidate->entity().property("targetname");
                    if (!name || *name != *target) continue;
                    const auto delta = vm::vec3f(candidate->entity().origin() - entity.origin());
                    const auto length = vm::length(delta);
                    if (finite(delta) && std::isfinite(length) && length > 0.001f) return delta / length;
                }
            }

            const auto mangle = numbers(property(entity, {"mangle"}));
            if (mangle.size() == 3) return direction(mangle[0], mangle[1]);

            return std::nullopt;
        }

        void setCone(PreviewLight& light, float outer, float inner) {
            constexpr auto halfRadians = float(M_PI / 360.0f);
            outer = std::clamp(outer, 1.0f, 179.0f);

            // A missing/zero soft angle means a hard cone, not a fade from its center.
            inner = inner > 0.0f ? std::min(inner, outer) : outer;
            light.outerConeCos = std::cos(outer * halfRadians);
            light.innerConeCos = std::cos(inner * halfRadians);
        }

        // An upper estimate of contribution, ignoring surface normals and cones. Used
        // only when the scene exceeds the preview budget; ordinary scenes keep ALL lights.
        float contribution(const PreviewLight& light, const float distance) {
            const auto brightness = std::abs(light.brightness);
            const auto d = distance * light.distanceScale;

            if (light.type == PreviewLightType::Directional) return brightness;
            switch (light.falloff) {
                case LightFalloff::Linear:
                    return light.falloffDistance > 0.0f
                               ? brightness * std::max(0.0f, 1.0f - distance / light.falloffDistance)
                               : std::max(0.0f, brightness - d);
                case LightFalloff::Inverse:
                    return brightness * 128.0f / std::max(d, 1.0f);
                case LightFalloff::InverseSquare:
                    return brightness * 16384.0f / std::max(d * d, 1.0f);
                case LightFalloff::InverseSquareOffset:
                    return brightness * 16384.0f / ((d + 128.0f) * (d + 128.0f));
                case LightFalloff::Constant:
                case LightFalloff::LocalMin:
                    return brightness;
            }

            return 0.0f;
        }

        float distanceToBounds(const vm::vec3f& point, const vm::bbox3f& bounds) {
            auto distanceSquared = 0.0f;
            for (size_t axis = 0; axis < 3; ++axis) {
                const auto d = point[axis] - std::clamp(point[axis], bounds.min[axis], bounds.max[axis]);
                distanceSquared += d * d;
            }

            return std::sqrt(distanceSquared);
        }

        bool visibleLight(const PreviewLighting& lighting, const PreviewLight& light, const vm::vec3f& position) {
            if (!lighting.occlusion) return true;
            if (light.type == PreviewLightType::Directional) return lighting.occlusion->trace(position, -light.direction, 10000000.0f) == LightingOcclusion::Hit::Sky;

            const auto delta = light.position - position;
            const auto distance = vm::length(delta);

            return distance < 0.002f || lighting.occlusion->trace(position, delta / distance, distance - 0.001f)
                   == LightingOcclusion::Hit::None;
        }
    }

    PreviewLighting buildPreviewLighting(
        const std::vector<Model::EntityNodeBase *>& nodes, const Model::Entity* world,
        const LightingProfile profile
    ) {
        auto result = PreviewLighting{};
        static std::atomic<uint64_t> nextRevision{1};
        result.revision = nextRevision.fetch_add(1, std::memory_order_relaxed);
        const auto halfLife = profile == LightingProfile::HalfLife;
        const auto distanceScale = world ? std::clamp(scalar(*world, {"_dist"}, 1.0f), 0.001f, 1000.0f) : 1.0f;
        const auto angleScale = world ? std::clamp(scalar(*world, {"_anglescale", "_anglesense"}, 0.5f), 0.0f, 1.0f) : 0.5f;
        if (world) {
            result.brightnessScale *= std::clamp(scalar(*world, {"_range"}, 0.5f), 0.0f, 100.0f) / 0.5f;
            result.gamma = std::clamp(scalar(*world, {"_gamma"}, 1.0f), 0.1f, 10.0f);
            result.minLight = colorProperty(*world, {"_minlight_color", "_mincolor"})
                              * std::clamp(scalar(*world, {"_minlight", "light"}, 0.0f), 0.0f, 65536.0f);
            const auto sunlight = scalar(*world, {"_sunlight", "_sun_light"}, 0.0f);
            if (sunlight != 0.0f) {
                auto sun = PreviewLight{};
                sun.type = PreviewLightType::Directional;
                sun.brightness = std::clamp(sunlight, -65536.0f, 65536.0f);
                sun.color = colorProperty(*world, {"_sunlight_color", "_sun_color"});
                sun.angleScale = angleScale;
                const auto angles = numbers(property(*world, {"_sunlight_mangle", "_sun_mangle", "_sun_angle"}));
                if (angles.size() == 3) sun.direction = direction(angles[0], angles[1]);
                result.lights.push_back(sun);
            }
        }

        for (const auto* node: nodes) {
            const auto& entity = node->entity();
            // The index's substring query can also return trigger_relight etc.
            if (entity.classname().compare(0, 5, "light") != 0) continue;
            // A surface-light template is not a point light at the entity origin.
            if (!property(entity, {"_surface"}).empty()) continue;
            if (scalar(entity, {"_nostaticlight"}, 0.0f) != 0.0f) continue;
            auto light = PreviewLight{};
            light.brightness = halfLife ? 200.0f : 300.0f;
            light.position = vm::vec3f(entity.origin());
            if (!finite(light.position)) continue;
            const auto values = numbers(property(entity, {"_light", "light"}));
            if (values.size() == 1) light.brightness = values[0];
            else if (values.size() == 3 || values.size() == 4) {
                // Half-Life uses byte colors; Quake extensions also accept 0..1 RGB.
                light.color = color(values, halfLife);
                if (values.size() == 4) light.brightness = values[3];
            }
            light.brightness = std::clamp(light.brightness, -65536.0f, 65536.0f);
            if (light.brightness == 0.0f) continue;
            const auto separateColor = numbers(property(entity, {"_color", "color"}));
            if (separateColor.size() == 3) light.color = color(separateColor);

            light.distanceScale = distanceScale * std::clamp(scalar(entity, {"wait"}, 1.0f), 0.001f, 1000.0f);
            light.falloff = static_cast<LightFalloff>(static_cast<int>(std::clamp(scalar(entity, {"delay"}, 0.0f), 0.0f, 5.0f)));
            light.falloffDistance = std::clamp(scalar(entity, {"_falloff"}, 0.0f), 0.0f, 1000000.0f);
            light.angleScale = std::clamp(scalar(entity, {"_anglescale", "_anglesense"}, angleScale), 0.0f, 1.0f);
            if (halfLife) {
                // In ZHLT this key selects a formula, while in Quake it is a radius.
                // Keep this compatibility path separate from the Quake preview model.
                light.falloffDistance = 0.0f;
                light.falloff = scalar(entity, {"_falloff"}, 0.0f) == 1.0f ? LightFalloff::Inverse : LightFalloff::InverseSquare;
                light.distanceScale = std::clamp(scalar(entity, {"_fade"}, 1.0f), 0.001f, 1000.0f);
                light.angleScale = 1.0f;
            }

            if (const auto aim = spotDirection(*node)) {
                light.type = PreviewLightType::Spot;
                light.direction = *aim;
                setCone(light, scalar(entity, {"angle"}, 40.0f), scalar(entity, {"_softangle"}, 0.0f));
            }
            // Also accept explicit spot/environment classes found in game definitions.
            else if (entity.classname() == "light_spot" || entity.classname() == "light_environment") {
                const auto angles = numbers(property(entity, {"angles"}));
                const auto yaw = angles.size() == 3 ? angles[1] : scalar(entity, {"angle"}, 0.0f);
                const auto pitch = scalar(entity, {"pitch"}, angles.size() == 3 ? -angles[0] : -90.0f);
                light.direction = direction(yaw, pitch);
                light.type = PreviewLightType::Spot;
                setCone(light, 2.0f * scalar(entity, {"_cone2"}, 45.0f), 2.0f * scalar(entity, {"_cone"}, 30.0f));
            }
            if (scalar(entity, {"_sun"}, 0.0f) != 0.0f || entity.classname() == "light_environment") { light.type = PreviewLightType::Directional; }
            if (halfLife && light.type == PreviewLightType::Spot) { setCone(light, 2.0f * scalar(entity, {"_cone2"}, 45.0f), 2.0f * scalar(entity, {"_cone"}, 30.0f)); }
            result.lights.push_back(light);
        }
        return result;
    }

    std::vector<size_t> previewLightCandidates(const PreviewLighting& lighting, const vm::bbox3f& bounds) {
        auto result = std::vector<size_t>{};
        for (size_t i = 0; i < lighting.lights.size(); ++i) { if (contribution(lighting.lights[i], distanceToBounds(lighting.lights[i].position, bounds)) > 0.0f) result.push_back(i); }
        return result;
    }

    vm::vec3f samplePreviewLighting(
        const PreviewLighting& lighting, const std::vector<size_t>& candidates,
        const vm::vec3f& position, const vm::vec3f& normal
    ) {
        const auto length = vm::length(normal);
        const auto n = length > 1e-6f ? normal / length : vm::vec3f{0, 0, 1};
        const auto origin = position + n * 0.1f;
        auto total = vm::vec3f::zero();
        auto minimum = lighting.minLight;
        for (const auto i: candidates) {
            const auto& light = lighting.lights[i];
            const auto delta = light.position - position;
            const auto distance = light.type == PreviewLightType::Directional ? 0.0f : vm::length(delta);
            const auto direction = light.type == PreviewLightType::Directional
                                       ? -light.direction
                                       : delta / std::max(distance, 1e-6f);
            const auto localMin = light.type != PreviewLightType::Directional && light.falloff == LightFalloff::LocalMin;
            const auto incidence = vm::dot(n, direction);
            if (!localMin && incidence <= 0.0f) continue;
            auto amount = contribution(light, distance);
            if (amount <= 0.0f) continue;
            if (light.type == PreviewLightType::Spot) {
                const auto angle = vm::dot(-direction, light.direction);
                if (angle < light.outerConeCos) continue;
                const auto width = light.innerConeCos - light.outerConeCos;
                if (width > 1e-6f) amount *= std::clamp((angle - light.outerConeCos) / width, 0.0f, 1.0f);
            }
            if (!visibleLight(lighting, light, origin)) continue;
            if (localMin) { if (light.brightness > 0.0f) minimum = vm::max(minimum, light.color * amount); }
            else {
                amount *= 1.0f - light.angleScale + incidence * light.angleScale;
                total = total + light.color * (light.brightness < 0.0f ? -amount : amount);
            }
        }
        return vm::max(vm::max(total, minimum), vm::vec3f::zero());
    }

    void selectPreviewLights(
        const PreviewLighting& lighting, const vm::bbox3f& bounds,
        LightSelection& selection, const bool testOcclusion
    ) {
        if (selection.valid && selection.revision == lighting.revision && selection.bounds == bounds
            && selection.testOcclusion == testOcclusion)
            return;
        selection.valid = true;
        selection.revision = lighting.revision;
        selection.bounds = bounds;
        selection.testOcclusion = testOcclusion;
        selection.linearLights.clear();
        selection.otherLights.clear();

        // Rank the strongest possible contribution anywhere in this region. In
        // particular, a light at an edge must not be culled by a center-only test.
        std::array<std::pair<float, size_t>, MaxRenderLights> ranks{};
        size_t count = 0;
        for (size_t i = 0; i < lighting.lights.size(); ++i) {
            const auto& light = lighting.lights[i];
            const auto peak = std::max({light.color.x(), light.color.y(), light.color.z()});
            auto score = peak * contribution(light, distanceToBounds(light.position, bounds));
            if (score <= 0.0f) continue;
            if (testOcclusion && !visibleLight(lighting, light, bounds.center())) continue;
            if (light.type == PreviewLightType::Directional) score += 1e20f;
            if (count < MaxRenderLights) ranks[count++] = {score, i};
            else {
                const auto worst = std::min_element(ranks.begin(), ranks.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
                if (score > worst->first) *worst = {score, i};
            }
        }
        std::sort(ranks.begin(), ranks.begin() + count, [](const auto& a, const auto& b) { return a.second < b.second; });
        for (size_t i = 0; i < count; ++i) {
            const auto& light = lighting.lights[ranks[i].second];
            if (light.type == PreviewLightType::Point && light.falloff == LightFalloff::Linear) {
                const auto radius = light.falloffDistance > 0.0f
                                        ? light.falloffDistance
                                        : std::abs(light.brightness) / light.distanceScale;
                selection.linearLights.emplace_back(light.position, radius);
                selection.linearLights.emplace_back(light.color * light.brightness, light.angleScale);
            }
            else {
                selection.otherLights.emplace_back(light.position, static_cast<float>(light.type));
                selection.otherLights.emplace_back(light.color, light.brightness);
                selection.otherLights.emplace_back(light.direction, light.outerConeCos);
                selection.otherLights.emplace_back(light.distanceScale, static_cast<float>(light.falloff), light.falloffDistance, light.angleScale);
                selection.otherLights.emplace_back(light.innerConeCos, 0.0f, 0.0f, 0.0f);
            }
        }
    }

    void configureLightingRegion(
        ActiveShader& shader, const RenderContext& context,
        const vm::bbox3f& bounds, LightSelection& selection, const bool testOcclusion
    ) {
        if (!context.render3D() || !pref(Preferences::EnableLightning)) return;
        selectPreviewLights(context.lighting(), bounds, selection, testOcclusion);
        shader.setVec4Array("LinearLights", selection.linearLights);
        shader.setVec4Array("OtherLights", selection.otherLights);
        shader.set("NumLinearLights", selection.linearLights.size() / 2);
        shader.set("NumLights", selection.otherLights.size() / 5);
    }

    void configureLightingShader(ActiveShader& shader, const RenderContext& context) {
        const auto& lighting = context.lighting();
        const auto enabled = context.render3D() && pref(Preferences::EnableLightning);
        // Region-specific counts are supplied before each spatial draw. Reset these
        // even for empty/unlit passes so no prior region can leak into another view.
        shader.set("NumLinearLights", 0);
        shader.set("NumLights", 0);
        const auto ambient = pref(Preferences::LightningAmbient);
        const auto intensity = pref(Preferences::LightningIntensity);
        shader.set("AmbientLight", vm::vec3f{ambient.r(), ambient.g(), ambient.b()});
        shader.set("MinimumLight", lighting.minLight);
        shader.set("LightScale", lighting.brightnessScale * (std::isfinite(intensity) ? std::clamp(intensity, 0.0f, 1000.0f) : 1.0f));
        shader.set("LightGamma", lighting.gamma);
        shader.set("EnableLighting", enabled);
    }
}
