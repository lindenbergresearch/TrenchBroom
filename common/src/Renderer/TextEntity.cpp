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
#include "TextEntity.h"
#include "AttrString.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/Camera.h"
#include "Renderer/FontManager.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderUtils.h"
#include "Renderer/Shaders.h"
#include "Renderer/TextAnchor.h"

#include <vm/forward.h>
#include <vm/vec.h>


namespace TrenchBroom::Renderer {

TextEntity::TextEntity(const AttrString &text, const TextAnchor *anchor, const vm::vec2f &inset, bool onTop,
                       const FontDescriptor &fontDescriptor, const Color &foreground, const Color &background,
                       const float minZoomFactor, const bool exactViewportMeasure, const size_t rectCornerSegments, const float rectCornerRadius)
    : m_text(text), m_anchor(anchor), m_inset(inset), m_onTop(onTop), m_fontDescriptor(fontDescriptor),
      m_foreground(foreground), m_background(background), m_prepared(false),
      m_exactViewportMeasure(exactViewportMeasure), m_minZoomFactor(minZoomFactor),
      m_rectCornerSegments(rectCornerSegments), m_rectCornerRadius(rectCornerRadius) {

}

bool TextEntity::setup(RenderContext &renderContext) {
    const auto &camera = renderContext.camera();
    m_distance = camera.perpendicularDistanceTo(m_anchor->position(camera));
    auto &font = renderContext.getTextureFont(m_fontDescriptor);
    m_prepared = true;

    // test for negative distance
    if (m_distance < 0.0f) {
        return false;
    }

    // if not on top (show always) test for valid bounds
    if (!m_onTop) {
        if (renderContext.render3D() && m_distance > pref(Preferences::TextRendererMaxDistance)) {
            return false;
        }

        if (renderContext.render2D() && renderContext.camera().zoom() < m_minZoomFactor) {
            return false;
        }
    }

    // check text size
    if (m_text.empty()) {
        return false;
    }

    // check trimmed length
    if (QString::fromStdString(m_text.raw()).trimmed().length() <= 0) {
        return false;
    }

    // get dimensions of the string
    m_size = font.measure(m_text);

    // test for invalid dimensions
    if (m_size.x() <= 0 || m_size.y() <= 0) {
        return false;
    }

    // use floor(...) to avoid odd pixel positions which results in blurry font rendering
    m_offset = floor(m_anchor->offset(camera, m_size));

    // if turned on, only draw texts fitting completely to the viewport
    if (m_exactViewportMeasure) {
        const auto &viewport = camera.viewport();
        const auto offset = m_offset.xy() - m_inset;
        const auto actualSize = m_size + 2.0f * m_inset;
        m_visible = viewport.contains(offset.x(), offset.y(), actualSize.x(), actualSize.y());
    } else {
        m_visible = true;
    }

    if (!m_visible) {
        return false;
    }

    m_vertices = font.quads(m_text);

    return m_valid = true;
}

const vm::vec2f &TextEntity::size() const {
    return m_size;
}

const vm::vec3f &TextEntity::offset() const {
    return m_offset;
}

float TextEntity::distance() const {
    return m_distance;
}

bool TextEntity::valid() const {
    return m_valid;
}

const Color &TextEntity::background() const {
    return m_background;
}

const Color &TextEntity::foreground() const {
    return m_foreground;
}

const FontDescriptor &TextEntity::fontDescriptor() const {
    return m_fontDescriptor;
}

bool TextEntity::onTop() const {
    return m_onTop;
}

const vm::vec2f &TextEntity::inset() const {
    return m_inset;
}

const AttrString &TextEntity::text() const {
    return m_text;
}


const std::__1::vector<vm::vec2f> &TextEntity::vertices() const {
    return m_vertices;
}

float TextEntity::minZoomFactor() const {
    return m_minZoomFactor;
}

bool TextEntity::exactViewportMeasure() const {
    return m_exactViewportMeasure;
}

size_t TextEntity::rectCornerSegments() const {
    return m_rectCornerSegments;
}

float TextEntity::rectCornerRadius() const {
    return m_rectCornerRadius;
}

size_t TextEntity::textVertexSize() {
    return vertices().size();
}

size_t TextEntity::rectVertexSize() {
    return roundedRect2DVertexCount(rectCornerSegments());
}

void TextEntity::setValid(bool valid) {
    m_valid = valid;
}

void TextEntity::setAnchor(TextAnchor *anchor) {
    m_anchor = anchor;
}

void TextEntity::setInset(const vm::vec2f &inset) {
    m_inset = inset;
}

void TextEntity::setOnTop(bool onTop) {
    m_onTop = onTop;
}

void TextEntity::setBackground(const Color &background) {
    m_background = background;
}

void TextEntity::setForeground(const Color &foreground) {
    m_foreground = foreground;
}

void TextEntity::setFontDescriptor(const FontDescriptor &fontDescriptor) {
    m_fontDescriptor = fontDescriptor;
}

bool TextEntity::visible() const {
    return m_visible;
}

void TextEntity::setVisible(bool visible) {
    m_visible = visible;
}

void TextEntity::setSize(const vm::vec2f &size) {
    m_size = size;
}

void TextEntity::setOffset(const vm::vec3f &offset) {
    m_offset = offset;
}

bool TextEntity::prepared() const {
    return m_prepared;
}

const TextAnchor *TextEntity::anchor() const {
    return m_anchor;
}
}
