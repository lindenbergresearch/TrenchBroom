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

#include "LinkRenderer.h"

#include "Renderer/ActiveShader.h"
#include "Renderer/Camera.h"
#include "Renderer/PrimType.h"
#include "Renderer/RenderBatch.h"
#include "Renderer/RenderContext.h"
#include "Renderer/ShaderManager.h"
#include "Renderer/Shaders.h"

namespace TrenchBroom::Renderer {

LinkRenderer::LinkRenderer() = default;

void LinkRenderer::render(RenderContext &, RenderBatch &renderBatch) {
    renderBatch.add(this);
}

void LinkRenderer::invalidate() {
    m_valid = false;
}

void LinkRenderer::doPrepareVertices(VboManager &vboManager) {
    if (!m_valid) {
        validate();

        m_lines.prepare(vboManager);
        m_arrows.prepare(vboManager);
    }
}

void LinkRenderer::doRender(RenderContext &renderContext) {
    assert(m_valid);
    renderLines(renderContext);
    renderArrows(renderContext);
}

void LinkRenderer::renderLines(RenderContext &renderContext) {
    auto shader = ActiveShader{renderContext.shaderManager(), Shaders::LinkLineShader};
    shader.set("CameraPosition", renderContext.camera().position());
    shader.set("IsOrtho", renderContext.camera().orthographicProjection());
    shader.set("MaxDistance", 500.0f); //<- todo: add to preferences

    glAssert(glLineWidth(1.75f)); //<- todo: add to preferences
    glAssert(glDisable(GL_DEPTH_TEST));
    shader.set("Alpha", 0.25f);
    m_lines.render(PrimType::Lines);

    glAssert(glEnable(GL_DEPTH_TEST));
    shader.set("Alpha", 1.0f);
    m_lines.render(PrimType::Lines);
    glAssert(glLineWidth(1.0f));
}

void LinkRenderer::renderArrows(RenderContext &renderContext) {
    auto shader = ActiveShader{renderContext.shaderManager(), Shaders::LinkArrowShader};
    shader.set("CameraPosition", renderContext.camera().position());
    shader.set("IsOrtho", renderContext.camera().orthographicProjection());
    shader.set("MaxDistance", 500.0f); //<- todo: add to preferences
    shader.set("Zoom", renderContext.camera().zoom());

    glAssert(glLineWidth(1.75f)); //<- todo: add to preferences
    glAssert(glDisable(GL_DEPTH_TEST));
    shader.set("Alpha", 0.25f);
    m_arrows.render(PrimType::Lines);

    glAssert(glEnable(GL_DEPTH_TEST));
    shader.set("Alpha", 1.0f);
    m_arrows.render(PrimType::Lines);
    glAssert(glLineWidth(1.0f));
}

static void addArrow(std::vector<LinkRenderer::ArrowVertex> &arrows, const vm::vec4f &color, const vm::vec3f &arrowPosition, const vm::vec3f &lineDir) {
    arrows.emplace_back(vm::vec3f{0, 2.25, 0}, color, arrowPosition, lineDir);
    arrows.emplace_back(vm::vec3f{9, 0, 0}, color, arrowPosition, lineDir);

    arrows.emplace_back(vm::vec3f{9, 0, 0}, color, arrowPosition, lineDir);
    arrows.emplace_back(vm::vec3f{0, -2.25, 0}, color, arrowPosition, lineDir);
}

static std::vector<LinkRenderer::ArrowVertex> getArrows(const std::vector<LinkRenderer::LineVertex> &links) {
    assert((links.size() % 2) == 0);
    auto arrows = std::vector<LinkRenderer::ArrowVertex>{};

    float arrowDistance = 100.f;

    for (size_t i = 0; i < links.size(); i += 2) {
        const auto &startVertex = links[i];
        const auto &endVertex = links[i + 1];

        const auto lineVec = (getVertexComponent<0>(endVertex) - getVertexComponent<0>(startVertex));
        const auto lineLength = length(lineVec);
        const auto lineDir = lineVec / lineLength;
        const auto color = getVertexComponent<1>(startVertex);

        size_t arrowsNum = size_t(lineLength / arrowDistance);

        for (size_t j = 0; j < arrowsNum; j++) {
            float fact = 1.f / (float) j;
            const auto arrowPosition = getVertexComponent<0>(startVertex) + (lineVec * fact);
            addArrow(arrows, color, arrowPosition, lineDir);
        }

        //    if (lineLength < 512) {
        //      const auto arrowPosition = getVertexComponent<0>(startVertex) + (lineVec *
        //      0.6f); addArrow(arrows, color, arrowPosition, lineDir);
        //    } else if (lineLength < 1024) {
        //      const auto arrowPosition1 = getVertexComponent<0>(startVertex) + (lineVec *
        //      0.2f); const auto arrowPosition2 = getVertexComponent<0>(startVertex) +
        //      (lineVec * 0.6f);
        //
        //      addArrow(arrows, color, arrowPosition1, lineDir);
        //      addArrow(arrows, color, arrowPosition2, lineDir);
        //    } else {
        //      const auto arrowPosition1 = getVertexComponent<0>(startVertex) + (lineVec *
        //      0.1f); const auto arrowPosition2 = getVertexComponent<0>(startVertex) +
        //      (lineVec * 0.4f); const auto arrowPosition3 =
        //      getVertexComponent<0>(startVertex) + (lineVec * 0.7f);
        //
        //      addArrow(arrows, color, arrowPosition1, lineDir);
        //      addArrow(arrows, color, arrowPosition2, lineDir);
        //      addArrow(arrows, color, arrowPosition3, lineDir);
        //    }
    }
    return arrows;
}

void LinkRenderer::validate() {
    auto links = getLinks();
    auto arrows = getArrows(links);

    m_lines = VertexArray::move(std::move(links));
    m_arrows = VertexArray::move(std::move(arrows));

    m_valid = true;
}

} // namespace TrenchBroom::Renderer
