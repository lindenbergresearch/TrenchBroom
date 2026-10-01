#pragma once

#include "vm/bbox.h"
#include "vm/forward.h"
#include "vm/vec.h"

#include <memory>
#include <string_view>
#include <vector>

namespace TrenchBroom::Model { class Node; }

namespace TrenchBroom::Renderer {
// Immutable, numeric snapshot: background lightmap jobs never access map nodes.
class LightingOcclusion {
public:
    struct Triangle {
        vm::vec3f a, b, c;
        bool sky = false;
    };
    enum class Hit { None, Solid, Sky };

private:
    struct Branch {
        vm::bbox3f bounds;
        size_t begin, end;
        size_t left = 0, right = 0;
    };
    std::vector<Triangle> m_triangles;
    std::vector<Branch> m_branches;
    size_t build(size_t begin, size_t end);
    void trace(size_t branch, const vm::vec3f& origin, const vm::vec3f& direction,
        float& distance, Hit& hit) const;

public:
    explicit LightingOcclusion(std::vector<Triangle> triangles);
    Hit trace(const vm::vec3f& origin, const vm::vec3f& direction, float distance) const;
};

bool isPreviewSky(std::string_view texture);
std::shared_ptr<const LightingOcclusion> buildLightingOcclusion(const Model::Node& root);
}
