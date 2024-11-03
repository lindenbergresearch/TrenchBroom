/*
 Copyright (C) 2010-2017 Kristian Duske

 (void *)this file is part of TrenchBroom.

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

#include "TextRenderer.h"

#include "AttrString.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/ActiveShader.h"
#include "Renderer/Camera.h"
#include "Renderer/FontManager.h"
#include "Renderer/PrimType.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderUtils.h"
#include "Renderer/Shaders.h"
#include "Renderer/TextAnchor.h"
#include "Renderer/TextureFont.h"

#include <vm/forward.h>
#include <vm/mat_ext.h>
#include <vm/vec.h>

namespace TrenchBroom {
namespace Renderer {

// --- constants
const float TextRenderer::DefaultMinZoomFactor = 0.5f;
const vm::vec2f TextRenderer::DefaultInset = vm::vec2f(3.0f, 5.0f);
const size_t TextRenderer::RectCornerSegments = 8;
const float TextRenderer::RectCornerRadius = 4.0f;
const bool TextRenderer::ExactViewportCheck = true;

bool TextRenderer::Entry::valueInRange(float value, float min, float max) {
    return (value >= min) && (value <= max);
}

bool TextRenderer::Entry::overlapsWith(const TextRenderer::Entry &entry) {
    bool xOverlap = valueInRange(offset.x(), entry.offset.x(), entry.offset.x() + entry.size.x()) ||
                    valueInRange(entry.offset.x(), offset.x(), offset.x() + size.x());

    bool yOverlap = valueInRange(offset.y(), entry.offset.y(), entry.offset.y() + entry.size.y()) ||
                    valueInRange(entry.offset.y(), offset.y(), offset.y() + size.y());

    return xOverlap && yOverlap;
}

TextRenderer::Entry::Entry(const std::vector<vm::vec2f> &vertices, const vm::vec2f &size, const vm::vec3f &offset, const Color &textColor, const Color &backgroundColor, const AttrString &string)
    : vertices(vertices), size(size), offset(offset), textColor(textColor), backgroundColor(backgroundColor), string(string) {
}

TextRenderer::EntryCollection::EntryCollection(const FontDescriptor &fontDescriptor, bool onTop) :
    textVertexCount(0), rectVertexCount(0), fontDescriptor(fontDescriptor), onTop(onTop) {
}

TextRenderer::EntryCollection::EntryCollection() : fontDescriptor(Preferences::getDefaultRenderFont()) {
}

bool TextRenderer::EntryCollection::overlaps(TextRenderer::Entry &entry) {
    for (const auto &e : entries) {
        // don't compare it to self
        if (&e == &entry) {
            continue;
        }

        if (entry.overlapsWith(e)) {
            return true;
        }
    }

    return false;
}

void TextRenderer::EntryCollection::addEntry(const TextRenderer::Entry &entry) {
    entries.push_back(entry);
    updateLayout();
}

void TextRenderer::EntryCollection::updateLayout() {
    //todo: remove cheap layout with better algorithm
    auto correctionShift = vm::vec3f(3.f, 5.f, 0.f);
    Entry &last = entries.back();

    while (overlaps(last)) {
        last.offset = last.offset + correctionShift;
    }
}


TextRenderer::TextRenderer(const float maxViewDistance, const float minZoomFactor, const vm::vec2f &inset)
    : m_maxViewDistance(maxViewDistance), m_minZoomFactor(minZoomFactor), m_inset(inset) {
}

void TextRenderer::renderString(RenderContext &renderContext, const Color &textColor, const Color &backgroundColor, const AttrString &string, const TextAnchor &position) {
    renderString(renderContext, textColor, backgroundColor, string, position, false);
}

void TextRenderer::renderStringOnTop(RenderContext &renderContext, const Color &textColor, const Color &backgroundColor, const AttrString &string, const TextAnchor &position) {
    renderString(renderContext, textColor, backgroundColor, string, position, true);
}

void TextRenderer::renderString(RenderContext &renderContext, const Color &textColor, const Color &backgroundColor, AttrString string, const TextAnchor &position, const bool onTop) {
    m_maxViewDistance = pref(Preferences::TextRendererMaxDistance);
    const Camera &camera = renderContext.camera();
    const float distance = camera.perpendicularDistanceTo(position.position(camera));

    if (distance <= 0) {
        return;
    }

    FontManager &fontManager = renderContext.fontManager();
    auto renderFont = Preferences::getDefaultRenderFont();

    if (distance > m_maxViewDistance) {
        renderFont = FontDescriptor(renderFont.path(), static_cast<size_t>(pref(Preferences::RendererFontSize) * 0.75));
    }

    TextureFont &font = fontManager.font(renderFont);

    if (!isVisible(renderContext, renderFont, string, position, distance, onTop)) {
        return;
    }

    auto vertices = font.quads(string, true);
    const float alphaFactor = computeAlphaFactor(renderContext, distance, onTop);

    const vm::vec2f size = font.measure(string);
    const vm::vec3f offset = position.offset(camera, size);
    auto collection = getOrCreateCollection(renderFont, onTop);

    addEntry(
        collections[renderFont],
        Entry{
            std::move(vertices),
            size,
            floor(offset),
            Color{textColor, alphaFactor * textColor.a()},
            Color{backgroundColor, alphaFactor * backgroundColor.a()},
            string
        }
    );
}

bool TextRenderer::isVisible(RenderContext &renderContext, const FontDescriptor &descriptor, const AttrString &string, const TextAnchor &position, const float distance, const bool onTop) const {
    if (distance <= 0) { return false; };

    if (!onTop) {
        if (renderContext.render3D() && distance > m_maxViewDistance) { return false; }
        if (renderContext.render2D() && renderContext.camera().zoom() < m_minZoomFactor) { return false; }
    }

    if (!ExactViewportCheck) {
        return true;
    }

    const Camera &camera = renderContext.camera();
    const Camera::Viewport &viewport = camera.viewport();

    const vm::vec2f size = stringSize(renderContext, descriptor, string);
    const vm::vec2f offset = vm::vec2f(position.offset(camera, size)) - m_inset;
    const vm::vec2f actualSize = size + 2.0f * m_inset;

    return viewport.contains(offset.x(), offset.y(), actualSize.x(), actualSize.y());
}

float TextRenderer::computeAlphaFactor(const RenderContext &renderContext, const float distance, const bool onTop) const {
    if (onTop) { return 1.0f; }

    auto fadeoutPos = pref(Preferences::TextRendererFadeOutFactor) * m_maxViewDistance;

    if (renderContext.render3D()) {
        const float a = m_maxViewDistance - distance;
        if (a > fadeoutPos)
            return 1.0f;
        return a / fadeoutPos;
    } else {
        const float z = renderContext.camera().zoom();
        const float d = z - m_minZoomFactor;
        if (d > 0.3f)
            return 1.0f;
        return d / 0.3f;
    }
}

void TextRenderer::addEntry(EntryCollection &collection, const Entry &entry) {
    collection.addEntry(entry);

    collection.textVertexCount += entry.vertices.size();
    collection.rectVertexCount += roundedRect2DVertexCount(RectCornerSegments);
}

vm::vec2f TextRenderer::stringSize(RenderContext &renderContext, const FontDescriptor &descriptor, const AttrString &string) const {
    FontManager &fontManager = renderContext.fontManager();
    TextureFont &font = fontManager.font(descriptor);
    return round(font.measure(string));
}

void TextRenderer::doPrepareVertices(VboManager &vboManager) {
    for (auto &[descriptor, collection] : collections) {
        prepare(collection, collection.onTop, vboManager);
    }
}

void TextRenderer::prepare(EntryCollection &collection, const bool onTop, VboManager &vboManager) {
    std::vector<TextVertex> textVertices;
    textVertices.reserve(collection.textVertexCount);

    std::vector<RectVertex> rectVertices;
    rectVertices.reserve(collection.rectVertexCount);

    for (const Entry &entry : collection.entries) {
        addEntry(entry, onTop, textVertices, rectVertices);
    }

    collection.textArray = VertexArray::move(std::move(textVertices));
    collection.rectArray = VertexArray::move(std::move(rectVertices));

    collection.textArray.prepare(vboManager);
    collection.rectArray.prepare(vboManager);
}

void TextRenderer::addEntry(const Entry &entry, const bool onTop, std::vector<TextVertex> &textVertices, std::vector<RectVertex> &rectVertices) {
    const auto &stringVertices = entry.vertices;
    const auto &stringSize = entry.size;
    const auto &offset = entry.offset;

    const auto &textColor = entry.textColor;
    const auto &rectColor = entry.backgroundColor;

    for (size_t i = 0; i < stringVertices.size() / 2; ++i) {
        const auto &position2 = stringVertices[2 * i];
        const auto &texCoords = stringVertices[2 * i + 1];
        textVertices.emplace_back(vm::vec3f(position2 + offset.xy(), -offset.z()), texCoords, textColor);
    }

    const auto rect = roundedRect2D(stringSize + 2.0f * m_inset, RectCornerRadius, RectCornerSegments);

    for (size_t i = 0; i < rect.size(); ++i) {
        const auto &vertex = rect[i];
        rectVertices.emplace_back(vm::vec3f(vertex + offset.xy() + stringSize / 2.0f, -offset.z()), rectColor);
    }
}

void TextRenderer::doRender(RenderContext &renderContext) {
    const auto &viewport = renderContext.camera().viewport();
    const auto projection = vm::ortho_matrix(0.0f, 1.0f, static_cast<float>(viewport.x), static_cast<float>(viewport.height), static_cast<float>(viewport.width), static_cast<float>(viewport.y));
    const auto view = vm::view_matrix(vm::vec3f{0, 0, -1}, vm::vec3f{0, 1, 0});
    auto ortho = ReplaceTransformation{renderContext.transformation(), projection, view};

    if (collections.size() < 1) {
        return;
    }

    for (auto &[descriptor, collection] : collections) {
        if (!collection.onTop) {
            render(collection, renderContext);
        }
    }

    for (auto &[descriptor, collection] : collections) {
        if (collection.onTop) {
            glAssert(glDisable(GL_DEPTH_TEST));
            render(collection, renderContext);
            glAssert(glEnable(GL_DEPTH_TEST));
        }
    }
}

void TextRenderer::render(EntryCollection &collection, RenderContext &renderContext) {
    FontManager &fontManager = renderContext.fontManager();
    TextureFont &font = fontManager.font(collection.fontDescriptor);
    glAssert(glDisable(GL_TEXTURE_2D));

    ActiveShader backgroundShader(renderContext.shaderManager(), Shaders::TextBackgroundShader);
    collection.rectArray.render(PrimType::Triangles);

    glAssert(glEnable(GL_TEXTURE_2D));

    ActiveShader textShader(renderContext.shaderManager(), Shaders::ColoredTextShader);
    textShader.set("Texture", 0);
    font.activate();
    collection.textArray.render(PrimType::Quads);
    font.deactivate();
}

TextRenderer::EntryCollection &TextRenderer::getOrCreateCollection(const FontDescriptor &descriptor, bool onTop) {
    auto [it, inserted] = collections.try_emplace(descriptor, descriptor, onTop);
    if (inserted) {
        it->second.fontDescriptor = descriptor;
        it->second.onTop = onTop;
        return it->second;
    }

    return it->second;
}

} // namespace Renderer
} // namespace TrenchBroom
