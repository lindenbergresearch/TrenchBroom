#include "SpatialLightMesh.h"

#include "Assets/Texture.h"
#include "Renderer/ActiveShader.h"
#include "Renderer/PrimType.h"
#include "Renderer/RenderUtils.h"
#include "Renderer/RenderContext.h"
#include "Renderer/PreviewLightmap.h"
#include "Renderer/LightingOcclusion.h"

#include <algorithm>
#include <cmath>

namespace TrenchBroom::Renderer {
namespace {
constexpr float RegionSize = 512.0f;

SpatialLightMeshBuilder::Vertex midpoint(const SpatialLightMeshBuilder::Vertex& a,
    const SpatialLightMeshBuilder::Vertex& b) {
    // Interpolate the stored normal as well as UVs; normalizing it here would
    // change interpolation across an originally smooth patch triangle.
    return SpatialLightMeshBuilder::Vertex{(getVertexComponent<0>(a) + getVertexComponent<0>(b)) * 0.5f,
            (getVertexComponent<1>(a) + getVertexComponent<1>(b)) * 0.5f,
            (getVertexComponent<2>(a) + getVertexComponent<2>(b)) * 0.5f};
}
}

SpatialLightMesh::SpatialLightMesh(std::vector<GLVertexTypes::P3NT2::Vertex> vertices, std::vector<Batch> batches)
    : m_vertices{VertexArray::copy(vertices)}, m_batches{std::move(batches)} {
    auto surfaces = std::vector<PreviewLightmap::Surface>{};
    for (const auto& batch : m_batches) {
        m_materials.push_back(batch.texture);
        surfaces.push_back({static_cast<size_t>(batch.first), static_cast<size_t>(batch.count),
            batch.texture && isPreviewSky(batch.texture->name())});
    }
    m_lightmap = std::make_shared<PreviewLightmap>(std::move(vertices), std::move(surfaces));
}

bool SpatialLightMesh::empty() const { return m_batches.empty(); }

void SpatialLightMesh::prepare(VboManager& manager) {
    m_vertices.prepare(manager);
    if (m_lightmap) m_lightmap->prepare(manager);
}

void SpatialLightMesh::render(RenderContext& context, ActiveShader& shader, TextureRenderFunc& textures) {
    if (m_lightmap) {
        m_lightmap->update(context.lighting());
        if (m_lightmap->render(shader, textures, m_materials)) return;
    }
    if (!m_vertices.setup()) return;
    if (!m_groupsValid || m_lightRevision != context.lighting().revision) {
        m_groups.clear();
        for (size_t i = 0; i < m_batches.size(); ++i) {
            auto& batch = m_batches[i];
            selectPreviewLights(context.lighting(), batch.bounds, batch.lights);
            const auto* previous = m_groups.empty() ? nullptr : &m_batches[m_groups.back().batch];
            if (!previous || previous->texture != batch.texture
                || previous->lights.linearLights != batch.lights.linearLights
                || previous->lights.otherLights != batch.lights.otherLights) {
                m_groups.push_back({i, {}, {}});
            }
            m_groups.back().starts.push_back(batch.first);
            m_groups.back().counts.push_back(batch.count);
        }
        m_lightRevision = context.lighting().revision;
        m_groupsValid = true;
    }
    const Assets::Texture* currentTexture = nullptr;
    bool firstTexture = true;
    for (const auto& group : m_groups) {
        auto& batch = m_batches[group.batch];
        if (firstTexture || currentTexture != batch.texture) {
            if (!firstTexture) textures.after(currentTexture);
            currentTexture = batch.texture;
            firstTexture = false;
            shader.set("GridColor", gridColorForTexture(currentTexture));
            shader.set("EnableMasked", currentTexture != nullptr && currentTexture->masked());
            textures.before(currentTexture);
        }
        configureLightingRegion(shader, context, batch.bounds, batch.lights);
        if (group.starts.size() == 1)
            m_vertices.render(PrimType::Triangles, batch.first, batch.count);
        else
            m_vertices.render(PrimType::Triangles, group.starts, group.counts, static_cast<GLint>(group.starts.size()));
    }
    if (!firstTexture) textures.after(currentTexture);
    m_vertices.cleanup();
}

SpatialLightMeshBuilder::SpatialLightMeshBuilder(const bool preserveOrder)
    : m_preserveOrder{preserveOrder} {}

void SpatialLightMeshBuilder::addTriangle(const Assets::Texture* texture,
    const Vertex& a, const Vertex& b, const Vertex& c) {
    addTriangle(texture, a, b, c, 0);
}

void SpatialLightMeshBuilder::addTriangle(const Assets::Texture* texture,
    const Vertex& a, const Vertex& b, const Vertex& c, const unsigned depth) {
    const auto& pa = getVertexComponent<0>(a);
    const auto& pb = getVertexComponent<0>(b);
    const auto& pc = getVertexComponent<0>(c);
    const auto ab = vm::squared_length(pb - pa);
    const auto bc = vm::squared_length(pc - pb);
    const auto ca = vm::squared_length(pa - pc);
    // A room-sized floor must not receive one light list for its entire extent.
    // Limit subdivision for pathological, world-sized triangles (<=256 leaves).
    if (depth < 8 && std::max({ab, bc, ca}) > RegionSize * RegionSize) {
        if (ab >= bc && ab >= ca) {
            const auto m = midpoint(a, b);
            addTriangle(texture, a, m, c, depth + 1);
            addTriangle(texture, m, b, c, depth + 1);
        } else if (bc >= ca) {
            const auto m = midpoint(b, c);
            addTriangle(texture, a, b, m, depth + 1);
            addTriangle(texture, a, m, c, depth + 1);
        } else {
            const auto m = midpoint(c, a);
            addTriangle(texture, a, b, m, depth + 1);
            addTriangle(texture, m, b, c, depth + 1);
        }
        return;
    }
    const auto center = (pa + pb + pc) / (3.0f * RegionSize);
    const auto cell = Cell{std::floor(center.x()), std::floor(center.y()), std::floor(center.z())};
    size_t index;
    if (m_preserveOrder) {
        // Transparent surfaces retain the source triangle order.
        if (m_chunks.empty() || m_chunks.back().texture != texture || m_chunks.back().cell != cell)
            m_chunks.push_back({texture, cell, {}});
        index = m_chunks.size() - 1;
    } else {
        const auto [it, inserted] = m_lookup[texture].try_emplace(cell, m_chunks.size());
        if (inserted) m_chunks.push_back({texture, cell, {}});
        index = it->second;
    }
    auto& vertices = m_chunks[index].vertices;
    vertices.insert(vertices.end(), {a, b, c});
}

SpatialLightMesh SpatialLightMeshBuilder::build() {
    // Opaque chunks can be grouped by material to avoid additional texture binds.
    if (!m_preserveOrder) {
        std::stable_sort(m_chunks.begin(), m_chunks.end(), [](const auto& a, const auto& b) {
            return std::less<const Assets::Texture*>{}(a.texture, b.texture);
        });
    }
    auto vertices = std::vector<Vertex>{};
    size_t count = 0;
    for (const auto& chunk : m_chunks) count += chunk.vertices.size();
    vertices.reserve(count);
    auto batches = std::vector<SpatialLightMesh::Batch>{};
    batches.reserve(m_chunks.size());
    for (auto& chunk : m_chunks) {
        auto bounds = vm::bbox3f::builder{};
        for (const auto& vertex : chunk.vertices) bounds.add(getVertexComponent<0>(vertex));
        batches.push_back({chunk.texture, bounds.bounds(), static_cast<GLint>(vertices.size()),
            static_cast<GLsizei>(chunk.vertices.size()), {}});
        vertices.insert(vertices.end(), chunk.vertices.begin(), chunk.vertices.end());
    }
    return {std::move(vertices), std::move(batches)};
}
}
