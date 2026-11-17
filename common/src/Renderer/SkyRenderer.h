/*
 Copyright (C) 2026 TrenchBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include "Renderer/GL.h"
#include "Renderer/Renderable.h"
#include "Renderer/VertexArray.h"
#include <memory>
#include <string>

namespace TrenchBroom::Assets { class Texture; }
namespace TrenchBroom::View { class MapDocument; }
namespace TrenchBroom::Renderer {
class RenderBatch;

bool isSkyTexture(const Assets::Texture* texture);

// Texture unit 2 is reserved for the sky; unit 0 is the material, 1 the preview lightmap.
class SkyTextureBinding {
    bool m_bound = false;
    GLint m_previous = 0;
public:
    explicit SkyTextureBinding(GLuint texture);
    ~SkyTextureBinding();
    SkyTextureBinding(const SkyTextureBinding&) = delete;
    SkyTextureBinding& operator=(const SkyTextureBinding&) = delete;
};

class SkyRenderer : public DirectRenderable {
    struct GpuSky;
    std::weak_ptr<View::MapDocument> m_document;
    std::unique_ptr<GpuSky> m_gpu;
    VertexArray m_triangle;
    std::string m_name;
    bool m_dirty = true;
public:
    explicit SkyRenderer(std::weak_ptr<View::MapDocument> document);
    ~SkyRenderer() override;
    void invalidate();
    void render(RenderContext& context, RenderBatch& batch);
private:
    void doPrepareVertices(VboManager& manager) override;
    void doRender(RenderContext& context) override;
};
} // namespace TrenchBroom::Renderer
