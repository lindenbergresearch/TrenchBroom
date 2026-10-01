#pragma once

#include "Renderer/Lighting.h"
#include "Renderer/VertexArray.h"

#include <array>
#include <map>
#include <vector>

namespace TrenchBroom::Assets { class Texture; }

namespace TrenchBroom::Renderer {
class TextureRenderFunc;
class PreviewLightmap;

// Rendering-only subdivision: map topology, UVs, selection and picking are unchanged.
class SpatialLightMesh {
public:
    struct Batch {
        const Assets::Texture* texture;
        vm::bbox3f bounds;
        GLint first;
        GLsizei count;
        LightSelection lights;
    };

private:
    struct DrawGroup {
        size_t batch;
        GLIndices starts;
        GLCounts counts;
    };
    VertexArray m_vertices;
    std::shared_ptr<PreviewLightmap> m_lightmap;
    std::vector<const Assets::Texture*> m_materials;
    std::vector<Batch> m_batches;
    std::vector<DrawGroup> m_groups;
    uint64_t m_lightRevision = 0;
    bool m_groupsValid = false;

public:
    SpatialLightMesh() = default;
    SpatialLightMesh(std::vector<GLVertexTypes::P3NT2::Vertex> vertices, std::vector<Batch> batches);
    bool empty() const;
    void prepare(VboManager& manager);
    void render(RenderContext& context, ActiveShader& shader, TextureRenderFunc& textures);
};

class SpatialLightMeshBuilder {
public:
    using Vertex = GLVertexTypes::P3NT2::Vertex;

private:
    using Cell = std::array<float, 3>;
    struct Chunk {
        const Assets::Texture* texture;
        Cell cell;
        std::vector<Vertex> vertices;
    };
    bool m_preserveOrder;
    std::vector<Chunk> m_chunks;
    std::map<const Assets::Texture*, std::map<Cell, size_t>> m_lookup;

    void addTriangle(const Assets::Texture* texture, const Vertex& a, const Vertex& b,
        const Vertex& c, unsigned depth);

public:
    explicit SpatialLightMeshBuilder(bool preserveOrder = false);
    void addTriangle(const Assets::Texture* texture, const Vertex& a, const Vertex& b, const Vertex& c);
    SpatialLightMesh build();
};
}
