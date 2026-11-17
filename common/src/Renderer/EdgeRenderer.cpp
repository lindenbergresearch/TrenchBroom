/*
 Copyright (C) 2010-2017 Kristian Duske

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

#include "EdgeRenderer.h"

#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/ActiveShader.h"
#include "Renderer/Camera.h"
#include "Renderer/BrushRendererArrays.h"
#include "Renderer/PrimType.h"
#include "Renderer/RenderBatch.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderUtils.h"
#include "Renderer/Shaders.h"

#include <algorithm>
#include <limits>
#include <type_traits>

namespace TrenchBroom::Renderer {
namespace {
using EdgeVertex = GLVertexType<GLVertexAttributeTypes::P3, GLVertexAttributeTypes::N,
                                GLVertexAttributeTypes::T02, GLVertexAttributeTypes::C4>::Vertex;

void appendEdge(std::vector<EdgeVertex> &vertices, const vm::vec3f &start, const vm::vec3f &end,
                const vm::vec4f &startColor, const vm::vec4f &endColor) {
    if (start == end)
        return;
    for (const auto &corner : {vm::vec2f(-1, -1), vm::vec2f(1, -1), vm::vec2f(1, 1),
                               vm::vec2f(-1, -1), vm::vec2f(1, 1), vm::vec2f(-1, 1)}) {
        vertices.emplace_back(start, end, corner, corner.x() < 0 ? startColor : endColor);
    }
}

template<typename Vertex>
VertexArray edgeTriangles(const std::vector<Vertex> &source, const IndexRangeMap &ranges) {
    auto vertices = std::vector<EdgeVertex>{};
    vertices.reserve(source.size() * 6);
    const auto color = [](const Vertex &vertex) {
        if constexpr (std::is_same_v<Vertex, GLVertexTypes::P3C4::Vertex>)
            return getVertexComponent<1>(vertex);
        else
            return vm::vec4f::fill(1.0f);
    };
    const auto add = [&](const size_t a, const size_t b) {
        ensure(a < source.size() && b < source.size(), "edge index out of range");
        appendEdge(vertices, getVertexComponent<0>(source[a]), getVertexComponent<0>(source[b]),
                   color(source[a]), color(source[b]));
    };
    ranges.forEachPrimitive([&](const PrimType type, const size_t offset, const size_t count) {
        if (count < 2)
            return;
        ensure(type == PrimType::Lines || type == PrimType::LineStrip || type == PrimType::LineLoop,
               "unsupported edge primitive");
        const auto step = type == PrimType::Lines ? size_t(2) : size_t(1);
        for (size_t i = 0; i + 1 < count; i += step)
            add(offset + i, offset + i + 1);
        if (type == PrimType::LineLoop)
            add(offset + count - 1, offset);
    });
    return VertexArray::move(std::move(vertices));
}

VertexArray edgeTriangles(const VertexArray &source, const IndexRangeMap &ranges) {
    if (source.empty())
        return {};
    if (const auto *vertices = source.vertices<GLVertexTypes::P3>())
        return edgeTriangles(*vertices, ranges);
    if (const auto *vertices = source.vertices<GLVertexTypes::P3C4>())
        return edgeTriangles(*vertices, ranges);
    ensure(false, "unsupported edge vertex layout");
    return {};
}
} // namespace

struct EdgeRendererMesh {
    VertexArray vertices;
    size_t vertexRevision = std::numeric_limits<size_t>::max();
    size_t indexRevision = std::numeric_limits<size_t>::max();

    void prepare(const BrushVertexArray &source, const BrushIndexArray &indices, VboManager &vboManager) {
        if (vertexRevision != source.revision() || indexRevision != indices.revision()) {
            auto triangles = std::vector<EdgeVertex>{};
            if (indices.hasValidIndices()) {
                const auto &points = source.vertices();
                const auto &elements = indices.indices();
                triangles.reserve(elements.size() * 3);
                for (size_t i = 0; i + 1 < elements.size(); i += 2) {
                    const auto a = elements[i];
                    const auto b = elements[i + 1];
                    // Freed index ranges contain zeroes and form degenerate edges.
                    if (a != b) {
                        ensure(a < points.size() && b < points.size(), "edge index out of range");
                        appendEdge(triangles, getVertexComponent<0>(points[a]), getVertexComponent<0>(points[b]),
                                   vm::vec4f::fill(1.0f), vm::vec4f::fill(1.0f));
                    }
                }
            }
            vertices = VertexArray::move(std::move(triangles));
            vertexRevision = source.revision();
            indexRevision = indices.revision();
        }
        vertices.prepare(vboManager);
    }
};

EdgeRenderer::Params::Params(const float i_width, const double i_offset, const bool i_onTop)
    : width{i_width}, offset{i_offset}, onTop{i_onTop}, useColor{false} {}

EdgeRenderer::Params::Params(const float i_width, const double i_offset, const bool i_onTop, const Color& i_color)
    : width{i_width}, offset{i_offset}, onTop{i_onTop}, useColor{true}, color{i_color} {}

EdgeRenderer::Params::Params(
    const float i_width, const double i_offset, const bool i_onTop, const bool i_useColor, const Color& i_color
)
    : width{i_width}, offset{i_offset}, onTop{i_onTop}, useColor{i_useColor}, color{i_color} {}

EdgeRenderer::RenderBase::RenderBase(const Params& params) : m_params{params} {}

EdgeRenderer::RenderBase::~RenderBase() = default;

void EdgeRenderer::RenderBase::renderEdges(RenderContext& renderContext) {
    const auto &viewport = renderContext.camera().viewport();
    if (m_params.width <= 0.0f || viewport.width <= 0 || viewport.height <= 0)
        return;

    glAssert(glPushAttrib(GL_ENABLE_BIT | GL_POLYGON_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT));
    glAssert(glEnable(GL_BLEND));
    glAssert(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    glAssert(glDisable(GL_CULL_FACE));
    glAssert(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
    glAssert(glDepthMask(GL_FALSE));
    glSetEdgeOffset(m_params.offset != 0.0 ? m_params.offset : 0.1);
    if (m_params.onTop)
        glAssert(glDisable(GL_DEPTH_TEST));

    auto shader = ActiveShader{renderContext.shaderManager(), Shaders::EdgeShader};
    shader.set("ViewportSize", vm::vec2f(float(viewport.width), float(viewport.height)) * renderContext.dpiScale());
    shader.set("Width", m_params.width * renderContext.dpiScale());
    setOverlaySizeScaling(shader, renderContext);
    shader.set("ShowSoftMapBounds", !renderContext.softMapBounds().is_empty());
    shader.set("SoftMapBoundsMin", renderContext.softMapBounds().min);
    shader.set("SoftMapBoundsMax", renderContext.softMapBounds().max);
    shader.set("SoftMapBoundsColor", pref(Preferences::SoftMapBoundsColor));
    shader.set("UseUniformColor", m_params.useColor);
    shader.set("Color", m_params.color);
    doRenderVertices(renderContext);
    glAssert(glPopAttrib());
}

EdgeRenderer::~EdgeRenderer() = default;

void EdgeRenderer::render(RenderBatch& renderBatch, const float width, const double offset) {
    doRender(renderBatch, {width, offset, false});
}

void EdgeRenderer::render(RenderBatch& renderBatch, const Color& color, const float width, const double offset) {
    doRender(renderBatch, {width, offset, false, color});
}

void EdgeRenderer::render(
    RenderBatch& renderBatch, const bool useColor, const Color& color, const float width, const double offset
) { doRender(renderBatch, {width, offset, false, useColor, color}); }

void EdgeRenderer::renderOnTop(RenderBatch& renderBatch, const float width, const double offset) {
    doRender(renderBatch, {width, offset, true});
}

void EdgeRenderer::renderOnTop(RenderBatch& renderBatch, const Color& color, const float width, const double offset) {
    doRender(renderBatch, Params(width, offset, true, color));
}

void EdgeRenderer::renderOnTop(
    RenderBatch& renderBatch, const bool useColor, const Color& color, const float width, const double offset
) { doRender(renderBatch, {width, offset, true, useColor, color}); }

void EdgeRenderer::render(
    RenderBatch& renderBatch, const bool useColor, const Color& color, const bool onTop, const float width,
    const double offset
) { doRender(renderBatch, {width, offset, onTop, useColor, color}); }

DirectEdgeRenderer::Render::Render(
    const EdgeRenderer::Params& params, VertexArray& vertexArray, IndexRangeMap& indexRanges
)
    : RenderBase{params}, m_vertexArray{vertexArray}, m_indexRanges{indexRanges} {}

void DirectEdgeRenderer::Render::doPrepareVertices(VboManager& vboManager) { m_vertexArray.prepare(vboManager); }

void DirectEdgeRenderer::Render::doRender(RenderContext& renderContext) {
    if (m_vertexArray.vertexCount() > 0) { renderEdges(renderContext); }
}

void DirectEdgeRenderer::Render::doRenderVertices(RenderContext&) { m_indexRanges.render(m_vertexArray); }

DirectEdgeRenderer::DirectEdgeRenderer() {}

DirectEdgeRenderer::DirectEdgeRenderer(VertexArray vertexArray, IndexRangeMap indexRanges)
    : m_vertexArray{edgeTriangles(vertexArray, indexRanges)},
      m_indexRanges{PrimType::Triangles, 0, m_vertexArray.vertexCount()} {}

DirectEdgeRenderer::DirectEdgeRenderer(VertexArray vertexArray, const PrimType primType)
    : DirectEdgeRenderer(vertexArray, IndexRangeMap{primType, 0, vertexArray.vertexCount()}) {}

void DirectEdgeRenderer::doRender(RenderBatch& renderBatch, const EdgeRenderer::Params& params) {
    renderBatch.addOneShot(new Render{params, m_vertexArray, m_indexRanges});
}

// IndexedEdgeRenderer::Render

IndexedEdgeRenderer::Render::Render(
    const EdgeRenderer::Params& params, std::shared_ptr<BrushVertexArray> vertexArray,
    std::shared_ptr<BrushIndexArray> indexArray, std::shared_ptr<EdgeRendererMesh> mesh
)
    : RenderBase{params}, m_vertexArray{std::move(vertexArray)}, m_indexArray{std::move(indexArray)}, m_mesh{std::move(mesh)} {}

void IndexedEdgeRenderer::Render::prepareVerticesAndIndices(VboManager& vboManager) {
    m_mesh->prepare(*m_vertexArray, *m_indexArray, vboManager);
}

void IndexedEdgeRenderer::Render::doRender(RenderContext& renderContext) {
    if (m_indexArray->hasValidIndices()) { renderEdges(renderContext); }
}

void IndexedEdgeRenderer::Render::doRenderVertices(RenderContext&) {
    m_mesh->vertices.render(PrimType::Triangles);
}

// IndexedEdgeRenderer

IndexedEdgeRenderer::IndexedEdgeRenderer() = default;

IndexedEdgeRenderer::IndexedEdgeRenderer(
    std::shared_ptr<BrushVertexArray> vertexArray, std::shared_ptr<BrushIndexArray> indexArray
)
    : m_vertexArray{std::move(vertexArray)}, m_indexArray{std::move(indexArray)}, m_mesh{std::make_shared<EdgeRendererMesh>()} {}

void IndexedEdgeRenderer::doRender(RenderBatch& renderBatch, const EdgeRenderer::Params& params) {
    renderBatch.addOneShot(new Render{params, m_vertexArray, m_indexArray, m_mesh});
}
}
