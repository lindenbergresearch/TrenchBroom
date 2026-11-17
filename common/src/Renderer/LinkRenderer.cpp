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
#include "PreferenceManager.h"
#include "Preferences.h"
#include <algorithm>
#include <chrono>
#include <cmath>

#include "Renderer/ActiveShader.h"
#include "Renderer/Camera.h"
#include "Renderer/PrimType.h"
#include "Renderer/RenderBatch.h"
#include "Renderer/RenderContext.h"
#include "Renderer/ShaderManager.h"
#include "Renderer/Shaders.h"

namespace TrenchBroom::Renderer {
LinkRenderer::LinkRenderer() = default;
void LinkRenderer::render(RenderContext&, RenderBatch& batch) {
    batch.add(this);
}
void LinkRenderer::invalidate() {
    m_valid = false;
}
void LinkRenderer::doPrepareVertices(VboManager& manager) {
    if (!m_valid) {
        validate();
        m_lines.prepare(manager);
    }
}
void LinkRenderer::doRender(RenderContext& context) {
    if (m_lines.empty()) return;
    const auto& viewport = context.camera().viewport();
    if (viewport.width <= 0 || viewport.height <= 0) return;
    const auto entity = useEntityLinkSettings();
    const auto sane = [](float value, float fallback, float low, float high) { return std::clamp(std::isfinite(value) ? value : fallback, low, high); };
    const auto width = entity ? sane(pref(Preferences::EntityLinkWidth), 2, 0.5f, 16) : 1.75f;
    const auto speed = entity ? sane(pref(Preferences::EntityLinkSpeed), 48, 0, 1000) : 0.f;
    const auto spacing = entity ? sane(pref(Preferences::EntityLinkSpacing), 72, 16, 512) : 72.f;
    const auto distance = entity ? sane(pref(Preferences::EntityLinkDistance), 2048, 0, 1000000) : 500.f;
    const auto occluded = entity ? sane(pref(Preferences::EntityLinkOccludedAlpha), 0.15f, 0, 1) : 0.25f;
    const auto dpi = context.dpiScale();
    const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    glAssert(glPushAttrib(GL_ENABLE_BIT | GL_POLYGON_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT));
    glAssert(glEnable(GL_BLEND));
    glAssert(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    glAssert(glDisable(GL_CULL_FACE));
    glAssert(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
    glAssert(glDepthMask(GL_FALSE));
    auto shader = ActiveShader{context.shaderManager(), Shaders::LinkLineShader};
    shader.set("ViewportSize", vm::vec2f{float(viewport.width), float(viewport.height)} * dpi);
    shader.set("Width", width * dpi);
    shader.set("ArrowSize", std::max(10.f, width * 3.f) * dpi);
    shader.set("Spacing", spacing * dpi);
    shader.set("Phase", float(std::fmod(seconds * double(speed), double(spacing))) * dpi);
    shader.set("CameraPosition", context.camera().position());
    shader.set("MaxDistance", distance);
    shader.set("IsOrtho", context.camera().orthographicProjection());
    glAssert(glEnable(GL_DEPTH_TEST));
    if (occluded > 0) {
        glAssert(glDepthFunc(GL_GREATER));
        shader.set("Alpha", occluded);
        m_lines.render(PrimType::Triangles);
    }
    glAssert(glDepthFunc(GL_LEQUAL));
    shader.set("Alpha", 1.f);
    m_lines.render(PrimType::Triangles);
    glAssert(glPopAttrib());
}
std::vector<LinkRenderer::RibbonVertex> LinkRenderer::buildLinkVertices(const std::vector<LineVertex>& links) {
    std::vector<RibbonVertex> vertices;
    vertices.reserve(links.size() * 3);
    for (size_t i = 0; i + 1 < links.size(); i += 2) {
        const auto a = getVertexComponent<0>(links[i]), b = getVertexComponent<0>(links[i + 1]);
        const auto delta = b - a;
        const auto length2 = vm::dot(delta, delta);
        if (!std::isfinite(length2) || length2 < 0.000001f) continue;
        for (const auto& corner : {vm::vec2f{-1, -1}, vm::vec2f{1, -1}, vm::vec2f{1, 1}, vm::vec2f{-1, -1}, vm::vec2f{1, 1}, vm::vec2f{-1, 1}})
            vertices.emplace_back(a, b, corner, getVertexComponent<1>(links[corner.x() < 0 ? i : i + 1]));
    }
    return vertices;
}
void LinkRenderer::validate() {
    m_lines = VertexArray::move(buildLinkVertices(getLinks()));
    m_valid = true;
}
} // namespace TrenchBroom::Renderer
