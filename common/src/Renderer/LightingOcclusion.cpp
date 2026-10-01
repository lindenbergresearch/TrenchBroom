#include "LightingOcclusion.h"

#include "Model/Brush.h"
#include "Model/BrushFace.h"
#include "Model/BrushNode.h"
#include "Model/EntityNode.h"
#include "Model/GroupNode.h"
#include "Model/LayerNode.h"
#include "Model/PatchNode.h"
#include "Model/WorldNode.h"
#include "kdl/overload.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace TrenchBroom::Renderer {
namespace {
bool intersects(const vm::bbox3f& bounds, const vm::vec3f& origin,
    const vm::vec3f& direction, float far) {
    auto near = 0.0f;
    for (size_t axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < 1e-12f) {
            if (origin[axis] < bounds.min[axis] || origin[axis] > bounds.max[axis]) return false;
        } else {
            auto a = (bounds.min[axis] - origin[axis]) / direction[axis];
            auto b = (bounds.max[axis] - origin[axis]) / direction[axis];
            if (a > b) std::swap(a, b);
            near = std::max(near, a);
            far = std::min(far, b);
            if (near > far) return false;
        }
    }
    return true;
}

bool castsShadow(const Model::Entity& entity) {
    if (const auto* value = entity.property("_shadow"); value && *value == "0") return false;
    const auto& name = entity.classname();
    return name.compare(0, 8, "trigger_") != 0 && name != "func_illusionary"
        && name != "func_detail_illusionary";
}

bool transparent(std::string_view texture) {
    const auto slash = texture.find_last_of('/');
    if (slash != std::string_view::npos) texture.remove_prefix(slash + 1);
    return texture.empty() ? false : texture.front() == '*' || texture.front() == '!'
        || texture == "clip" || texture == "skip" || texture == "hint"
        || texture == "origin" || texture == "trigger" || texture == "playerclip"
        || texture == "monsterclip" || texture == "nodraw";
}
}

bool isPreviewSky(std::string_view texture) {
    const auto slash = texture.find_last_of('/');
    if (slash != std::string_view::npos) texture.remove_prefix(slash + 1);
    return texture.substr(0, 3) == "sky";
}

LightingOcclusion::LightingOcclusion(std::vector<Triangle> triangles)
    : m_triangles{std::move(triangles)} {
    if (!m_triangles.empty()) build(0, m_triangles.size());
}

size_t LightingOcclusion::build(const size_t begin, const size_t end) {
    auto bounds = vm::bbox3f::builder{};
    for (auto i = begin; i < end; ++i) {
        bounds.add(m_triangles[i].a);
        bounds.add(m_triangles[i].b);
        bounds.add(m_triangles[i].c);
    }
    const auto index = m_branches.size();
    m_branches.push_back({bounds.bounds(), begin, end});
    if (end - begin > 8) {
        const auto size = bounds.bounds().size();
        const size_t axis = size.x() >= size.y() && size.x() >= size.z() ? 0 : size.y() >= size.z() ? 1 : 2;
        const auto middle = begin + (end - begin) / 2;
        std::nth_element(m_triangles.begin() + static_cast<std::ptrdiff_t>(begin),
            m_triangles.begin() + static_cast<std::ptrdiff_t>(middle),
            m_triangles.begin() + static_cast<std::ptrdiff_t>(end), [axis](const auto& a, const auto& b) {
                return a.a[axis] + a.b[axis] + a.c[axis] < b.a[axis] + b.b[axis] + b.c[axis];
            });
        const auto left = build(begin, middle);
        const auto right = build(middle, end);
        m_branches[index].left = left;
        m_branches[index].right = right;
    }
    return index;
}

void LightingOcclusion::trace(const size_t index, const vm::vec3f& origin,
    const vm::vec3f& direction, float& distance, Hit& hit) const {
    const auto& branch = m_branches[index];
    if (!intersects(branch.bounds, origin, direction, distance)) return;
    if (branch.left != 0) {
        trace(branch.left, origin, direction, distance, hit);
        trace(branch.right, origin, direction, distance, hit);
        return;
    }
    for (auto i = branch.begin; i < branch.end; ++i) {
        const auto& triangle = m_triangles[i];
        const auto ab = triangle.b - triangle.a;
        const auto ac = triangle.c - triangle.a;
        const auto p = vm::cross(direction, ac);
        const auto determinant = vm::dot(ab, p);
        if (std::abs(determinant) < 1e-8f) continue;
        const auto inv = 1.0f / determinant;
        const auto offset = origin - triangle.a;
        const auto u = vm::dot(offset, p) * inv;
        if (u < -1e-6f || u > 1.000001f) continue;
        const auto q = vm::cross(offset, ab);
        const auto v = vm::dot(direction, q) * inv;
        if (v < -1e-6f || u + v > 1.000001f) continue;
        const auto d = vm::dot(ac, q) * inv;
        if (d > 0.001f && d < distance) {
            distance = d;
            hit = triangle.sky ? Hit::Sky : Hit::Solid;
        }
    }
}

LightingOcclusion::Hit LightingOcclusion::trace(const vm::vec3f& origin,
    const vm::vec3f& direction, float distance) const {
    auto hit = Hit::None;
    if (!m_branches.empty()) trace(0, origin, direction, distance, hit);
    return hit;
}

std::shared_ptr<const LightingOcclusion> buildLightingOcclusion(const Model::Node& root) {
    auto triangles = std::vector<LightingOcclusion::Triangle>{};
    auto visit = std::function<void(const Model::Node&)>();
    visit = [&](const Model::Node& node) {
        node.accept(kdl::overload(
            [&](const Model::WorldNode*) {}, [&](const Model::LayerNode*) {},
            [&](const Model::GroupNode*) {}, [&](const Model::EntityNode*) {},
            [&](const Model::BrushNode* brush) {
                if (!castsShadow(brush->entity()->entity())) return;
                for (const auto& face : brush->brush().faces()) {
                    const auto& name = face.attributes().textureName();
                    if (transparent(name)) continue;
                    const auto vertices = face.vertexPositions();
                    for (size_t i = 2; i < vertices.size(); ++i)
                        triangles.push_back({vm::vec3f(vertices[0]), vm::vec3f(vertices[i - 1]),
                            vm::vec3f(vertices[i]), isPreviewSky(name)});
                }
            }, [&](const Model::PatchNode* patch) {
                if (!castsShadow(patch->entity()->entity()) || transparent(patch->patch().textureName())) return;
                const auto& grid = patch->grid();
                for (size_t row = 0; row < grid.quadRowCount(); ++row) {
                    for (size_t col = 0; col < grid.quadColumnCount(); ++col) {
                        const auto a = vm::vec3f(grid.point(row, col).position);
                        const auto b = vm::vec3f(grid.point(row, col + 1).position);
                        const auto c = vm::vec3f(grid.point(row + 1, col + 1).position);
                        const auto d = vm::vec3f(grid.point(row + 1, col).position);
                        const auto sky = isPreviewSky(patch->patch().textureName());
                        triangles.push_back({a, b, c, sky});
                        triangles.push_back({c, d, a, sky});
                    }
                }
            }));
        for (const auto* child : node.children()) visit(*child);
    };
    visit(root);
    return std::make_shared<const LightingOcclusion>(std::move(triangles));
}
}
