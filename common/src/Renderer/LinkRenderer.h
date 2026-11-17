/*
Copyright (C) 2020 Kristian Duske

This file is part of TrenchBroom.

   TrenchBroom is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
     the Free Software Foundation, either version 3 of the License, or
                                    (at your option) any later version.

                                    TrenchBroom is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "Renderer/GLVertex.h"
#include "Renderer/GLVertexType.h"
#include "Renderer/Renderable.h"
#include "Renderer/VertexArray.h"

namespace TrenchBroom::Renderer {
class RenderContext;

class RenderBatch;

class VboManager;

class LinkRenderer : public DirectRenderable {
  public:
    using LineVertex = GLVertexTypes::P3C4::Vertex;

    using RibbonVertex = GLVertexType<GLVertexAttributeTypes::P3, GLVertexAttributeTypes::N, GLVertexAttributeTypes::T02, GLVertexAttributeTypes::C4>::Vertex;
    static std::vector<RibbonVertex> buildLinkVertices(const std::vector<LineVertex>& links);

  private:
    VertexArray m_lines;

    bool m_valid = false;

  public:
    LinkRenderer();

    void render(RenderContext& renderContext, RenderBatch& renderBatch);

    void invalidate();

  private:
    void doPrepareVertices(VboManager& vboManager) override;

    void doRender(RenderContext& renderContext) override;

    virtual bool useEntityLinkSettings() const { return false; }

    void validate();

    virtual std::vector<LinkRenderer::LineVertex> getLinks() = 0;

    deleteCopy(LinkRenderer);
};

} // namespace TrenchBroom::Renderer
