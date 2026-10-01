#pragma once

#include "Renderer/GLVertexType.h"
#include "Renderer/Lighting.h"

#include <memory>
#include <vector>

namespace TrenchBroom::Assets { class Texture; }
namespace TrenchBroom::Renderer {
class ActiveShader;
class TextureRenderFunc;
class VboManager;

class PreviewLightmap {
public:
    using Vertex = GLVertexTypes::P3NT2::Vertex;
    struct Surface {
        size_t first, count;
        bool sky;
    };

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

public:
    PreviewLightmap(std::vector<Vertex> vertices, std::vector<Surface> surfaces);
    ~PreviewLightmap();
    void update(const PreviewLighting& lighting);
    void prepare(VboManager& manager);
    bool render(ActiveShader& shader, TextureRenderFunc& textures,
        const std::vector<const Assets::Texture*>& materials);
};
}
