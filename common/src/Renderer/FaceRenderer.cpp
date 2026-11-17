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

#include "FaceRenderer.h"

#include "Assets/Texture.h"
#include "Model/EntityNode.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/ActiveShader.h"
#include "Renderer/BrushRendererArrays.h"
#include "Renderer/Camera.h"
#include "Renderer/Lighting.h"
#include "Renderer/GridRenderer.h"
#include "Renderer/SpatialLightMesh.h"
#include <limits>
#include "Renderer/PrimType.h"
#include "Renderer/RenderBatch.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderUtils.h"
#include "Renderer/ShaderManager.h"
#include "Renderer/Shaders.h"
#include "Renderer/SkyRenderer.h"
#include <algorithm>

namespace TrenchBroom::Renderer {
void configureFaceShader(ActiveShader &shader, const RenderContext &context) {
    configureLightingShader(shader, context);
    shader.set("UsePreviewLightmap", false);
    shader.set("PreviewFullbright", false);
    shader.set("PreviewLightmap", 1);
    shader.set("CameraPosition", context.camera().position());
    shader.set("Brightness", pref(Preferences::Brightness));
    shader.set("RenderGrid", context.showGrid());
    configureGridAxisColors(shader);
    shader.set("ShadeGrid", pref(Preferences::ShadeGrid));
    shader.set("GridSize", static_cast<float>(context.gridSize()));
    shader.set("GridLineWidth", std::max(1.0f, pref(Preferences::GridLineWidth)) * context.dpiScale());
    shader.set("DpiScale", context.dpiScale());
    shader.set("GridAlpha", pref(Preferences::GridAlpha));
    shader.set("MajorDivisionSize", pref(Preferences::GridMajorDivisionSize));
    shader.set("ApplyTexture", context.showTextures());
    shader.set("Texture", 0);
    shader.set("SkyTexture", 2);
    shader.set("RenderSky", false);
    shader.set("EnableMasked", false);
    shader.set("AutoBrightnessType", pref(Preferences::FaceAutoBrightness));
    shader.set("ShadeFaces", context.shadeFaces());
    shader.set("ShadeLevel", pref(Preferences::ShadeLevel));
    shader.set("ShowFog", context.showFog());
    shader.set("FogColor", pref(Preferences::FogColor));
    shader.set("FogMaxAmount", pref(Preferences::FogMaxAmount));
    shader.set("FogScale", pref(Preferences::FogScale));
    shader.set("FogBias", pref(Preferences::FogBias));
    shader.set("FogMinDistance", pref(Preferences::FogMinDistance));
    shader.set("FogType", pref(Preferences::FogType));
    shader.set("ShowSoftMapBounds", !context.softMapBounds().is_empty());
    shader.set("SoftMapBoundsMin", context.softMapBounds().min);
    shader.set("SoftMapBoundsMax", context.softMapBounds().max);
    const auto boundsColor = pref(Preferences::SoftMapBoundsColor);
    shader.set("SoftMapBoundsColor", vm::vec4f(boundsColor.r(), boundsColor.g(), boundsColor.b(), 0.1f));
}

struct FaceRenderer::RenderFunc : TextureRenderFunc {
    ActiveShader& shader;
    bool applyTexture;
    bool useSky;
    const Color& defaultColor;

    RenderFunc(ActiveShader& i_shader, const bool i_applyTexture, const bool i_useSky, const Color& i_defaultColor)
        : shader(i_shader), applyTexture(i_applyTexture), useSky(i_useSky), defaultColor(i_defaultColor) {}

    void before(const Assets::Texture* texture) override {
        shader.set("RenderSky", useSky && isSkyTexture(texture));
        if (texture != nullptr) {
            texture->activate();
            shader.set("ApplyTexture", applyTexture);
            shader.set("Color", texture->averageColor());
        }
        else {
            shader.set("ApplyTexture", false);
            shader.set("Color", defaultColor);
        }
    }

    void after(const Assets::Texture* texture) override { if (texture != nullptr) { texture->deactivate(); } }
};

struct FaceRenderer::LightingMeshCache {
    size_t vertexRevision = std::numeric_limits<size_t>::max();
    std::unordered_map<const BrushIndexArray*, size_t> indexRevisions;
    bool transparent = false;
    SpatialLightMesh mesh;

    void prepare(const BrushVertexArray& vertices, const TextureToBrushIndicesMap& sources,
        const bool preserveOrder, VboManager& manager) {
        bool changed = vertexRevision != vertices.revision() || sources.size() != indexRevisions.size()
            || transparent != preserveOrder;
        for (const auto& [texture, indices] : sources) {
            const auto it = indexRevisions.find(indices.get());
            changed |= it == indexRevisions.end() || it->second != indices->revision();
        }
        if (changed) {
            auto builder = SpatialLightMeshBuilder{preserveOrder};
            const auto& points = vertices.vertices();
            indexRevisions.clear();
            for (const auto& [texture, indices] : sources) {
                indexRevisions[indices.get()] = indices->revision();
                const auto& elements = indices->indices();
                for (size_t i = 0; i + 2 < elements.size(); i += 3) {
                    const auto a = elements[i], b = elements[i + 1], c = elements[i + 2];
                    if (a == b || b == c || a == c) continue; // freed allocation
                    ensure(a < points.size() && b < points.size() && c < points.size(), "face index out of range");
                    builder.addTriangle(texture, points[a], points[b], points[c]);
                }
            }
            mesh = builder.build();
            vertexRevision = vertices.revision();
            transparent = preserveOrder;
        }
        mesh.prepare(manager);
    }
};

FaceRenderer::FaceRenderer() : m_grayscale(false), m_tint(false), m_alpha(1.0f) {}

FaceRenderer::FaceRenderer(
    std::shared_ptr<BrushVertexArray> vertexArray, std::shared_ptr<TextureToBrushIndicesMap> indexArrayMap,
    const Color& faceColor
)
    : m_vertexArray(std::move(vertexArray)),
      m_indexArrayMap(std::move(indexArrayMap)),
      m_faceColor(faceColor),
      m_grayscale(false),
      m_tint(false),
      m_alpha(1.0f) {}

FaceRenderer::FaceRenderer(const FaceRenderer& other)
    : IndexedRenderable(other),
      m_lightingMesh(other.m_lightingMesh),
      m_vertexArray(other.m_vertexArray),
      m_indexArrayMap(other.m_indexArrayMap),
      m_faceColor(other.m_faceColor),
      m_grayscale(other.m_grayscale),
      m_tint(other.m_tint),
      m_tintColor(other.m_tintColor),
      m_alpha(other.m_alpha) {}

FaceRenderer& FaceRenderer::operator=(FaceRenderer other) {
    using std::swap;
    swap(*this, other);
    return *this;
}

void swap(FaceRenderer& left, FaceRenderer& right) {
    using std::swap;
    swap(left.m_lightingMesh, right.m_lightingMesh);
    swap(left.m_vertexArray, right.m_vertexArray);
    swap(left.m_indexArrayMap, right.m_indexArrayMap);
    swap(left.m_faceColor, right.m_faceColor);
    swap(left.m_grayscale, right.m_grayscale);
    swap(left.m_tint, right.m_tint);
    swap(left.m_tintColor, right.m_tintColor);
    swap(left.m_alpha, right.m_alpha);
}

void FaceRenderer::setGrayscale(const bool grayscale) { m_grayscale = grayscale; }

void FaceRenderer::setTint(const bool tint) { m_tint = tint; }

void FaceRenderer::setTintColor(const Color& color) { m_tintColor = color; }

void FaceRenderer::setAlpha(const float alpha) { m_alpha = alpha; }

void FaceRenderer::render(RenderBatch& renderBatch) { renderBatch.add(this); }

void FaceRenderer::prepareVerticesAndIndices(VboManager& vboManager) {
    if (pref(Preferences::EnableLightning)) {
        if (!m_lightingMesh) m_lightingMesh = std::make_shared<LightingMeshCache>();
        m_lightingMesh->prepare(*m_vertexArray, *m_indexArrayMap, m_alpha < 1.0f, vboManager);
    }
    m_vertexArray->prepare(vboManager);
    for (const auto& [texture, brushIndexHolderPtr]: *m_indexArrayMap) { brushIndexHolderPtr->prepare(vboManager); }
}

void FaceRenderer::doRender(RenderContext& context) {
    if (m_indexArrayMap->empty()) return;

    ActiveShader shader(context.shaderManager(), Shaders::FaceShader);
    configureFaceShader(shader, context);
    shader.set("ApplyTinting", m_tint);
    shader.set("TintColor", m_tintColor);
    shader.set("GrayScale", m_grayscale);
    shader.set("Alpha", m_alpha);

    glAssert(glActiveTexture(GL_TEXTURE0));
    glAssert(glEnable(GL_TEXTURE_2D));
    const auto skyBinding = SkyTextureBinding(context.skyTexture());
    RenderFunc func(shader, context.showTextures(),
        context.render3D() && context.showTextures() && context.skyTexture() != 0, m_faceColor);

    if (m_alpha < 1.0f) {
        glAssert(glPushAttrib(GL_DEPTH_BUFFER_BIT));
        glAssert(glDepthMask(GL_FALSE));
    }

    if (context.render3D() && pref(Preferences::EnableLightning) && m_lightingMesh) {
        m_lightingMesh->mesh.render(context, shader, func);
    } else if (m_vertexArray->setupVertices()) {
        for (const auto& [texture, indices] : *m_indexArrayMap) {
            if (!indices->hasValidIndices()) continue;
            shader.set("GridColor", gridColorForTexture(texture));
            shader.set("EnableMasked", texture != nullptr && texture->masked());
            func.before(texture);
            indices->setupIndices();
            indices->render(PrimType::Triangles);
            indices->cleanupIndices();
            func.after(texture);
        }

        m_vertexArray->cleanupVertices();
    }

    if (m_alpha < 1.0f) glAssert(glPopAttrib());
}
}
