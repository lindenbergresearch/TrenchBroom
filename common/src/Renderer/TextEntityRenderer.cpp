#include "TextEntityRenderer.h"
#include "RenderUtils.h"
#include "RenderContext.h"
#include <vm/mat_ext.h>
#include "AttrString.h"
#include "ActiveShader.h"
#include "Shaders.h"
#include "PrimType.h"
#include "FontManager.h"
#include "TextureFont.h"


namespace TrenchBroom::Renderer {

void TextEntityRenderer::doRender(RenderContext &renderContext) {
    if (m_topRenderCache.empty() && m_renderCache.empty()) {
        return;
    }

    const auto &viewport = renderContext.camera().viewport();
    const auto projection =
        vm::ortho_matrix(
            0.0f, 1.0f,
            static_cast<float>(viewport.x),
            static_cast<float>(viewport.height),
            static_cast<float>(viewport.width),
            static_cast<float>(viewport.y)
        );
    const auto view = vm::view_matrix(vm::vec3f{0, 0, -1}, vm::vec3f{0, 1, 0});
    auto ortho = ReplaceTransformation{renderContext.transformation(), projection, view};

    for (auto &item : m_renderCache) {
        renderVertexCache(item, renderContext);
    }

    for (auto &item : m_topRenderCache) {
        glAssert(glDisable(GL_DEPTH_TEST));
        renderVertexCache(item, renderContext);
        glAssert(glEnable(GL_DEPTH_TEST));
    }
}

void TextEntityRenderer::renderVertexCache(VertexCache &vertexCache, RenderContext &renderContext) {
    auto &fontManager = renderContext.fontManager();
    auto &font = fontManager.font(vertexCache.m_fontDescriptor);

    glAssert(glDisable(GL_TEXTURE_2D));
    ActiveShader backgroundShader(renderContext.shaderManager(), Shaders::TextBackgroundShader);
    vertexCache.m_rectVertices.render(PrimType::Triangles);
    glAssert(glEnable(GL_TEXTURE_2D));

    ActiveShader textShader(renderContext.shaderManager(), Shaders::ColoredTextShader);
    textShader.set("Texture", 0);
    font.activate();
    vertexCache.m_textVertices.render(PrimType::Quads);
    font.deactivate();
}

void TextEntityRenderer::doPrepareVertices(VboManager &vboManager) {
    prepareVertexCache(vboManager);
}

void TextEntityRenderer::addTextEntity(TextEntity textEntity, RenderContext &renderContext) {
    if (!textEntity.prepared()) {
        textEntity.setup(renderContext);
    }

    if (textEntity.valid()) {
        auto it = m_entityCache.find(textEntity.fontDescriptor());

        if (it != m_entityCache.end()) {
            it->second.push_back(textEntity);
        } else {
            m_entityCache[textEntity.fontDescriptor()].push_back(textEntity);
        }
    }
}

void TextEntityRenderer::computeEntityVertices(const TextEntity &entity, TextVertexList &textVertices, RectVertexList &rectVertices) {
    if (!entity.valid()) { return; }

    /** fill text vertices */
    for (size_t i = 0; i < entity.vertices().size() / 2; ++i) {
        const auto &position2 = entity.vertices()[2 * i];
        const auto &texCoords = entity.vertices()[2 * i + 1];
        textVertices.emplace_back(vm::vec3f(position2 + entity.offset().xy(), -entity.offset().z()), texCoords, entity.foreground());
    }
    /** fill rectangle vertices */
    const auto rect = roundedRect2D(entity.size() + 2.0f * entity.inset(), entity.rectCornerRadius(), entity.rectCornerSegments());
    for (size_t i = 0; i < rect.size(); ++i) {
        const auto &vertex = rect[i];
        rectVertices.emplace_back(vm::vec3f(vertex + entity.offset().xy() + entity.size() / 2.0f, -entity.offset().z()), entity.background());
    }
}

void TextEntityRenderer::prepareVertexCache(VboManager &vboManager) {
    for (const auto &item : m_entityCache) {
        VertexCache cache{item.first};
        VertexCache topCache{item.first};

        RectVertexList rectVertexList{};
        TextVertexList textVertexList{};
        RectVertexList topRectVertexList{};
        TextVertexList topTextVertexList{};

        for (TextEntity entity : item.second) {
            if (entity.onTop()) {
                computeEntityVertices(entity, topTextVertexList, topRectVertexList);
            } else {
                computeEntityVertices(entity, textVertexList, rectVertexList);
            }
        }

        if (topTextVertexList.size() > 0 || topRectVertexList.size() > 0) {
            topCache.m_textVertices = VertexArray::move(std::move(topTextVertexList));
            topCache.m_rectVertices = VertexArray::move(std::move(topRectVertexList));
            topCache.m_textVertices.prepare(vboManager);
            topCache.m_rectVertices.prepare(vboManager);

            m_topRenderCache.push_back(topCache);
        }

        if (textVertexList.size() > 0 || rectVertexList.size() > 0) {
            cache.m_textVertices = VertexArray::move(std::move(textVertexList));
            cache.m_rectVertices = VertexArray::move(std::move(rectVertexList));
            cache.m_textVertices.prepare(vboManager);
            cache.m_rectVertices.prepare(vboManager);

            m_renderCache.push_back(cache);
        }


    }
}

TextEntityRenderer::TextEntityRenderer() {
}


}
