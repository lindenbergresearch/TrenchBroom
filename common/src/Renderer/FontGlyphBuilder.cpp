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

#include "FontGlyphBuilder.h"

#include "Ensure.h"
#include "Renderer/FontGlyph.h"
#include "Renderer/FontTexture.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>

namespace TrenchBroom {
namespace Renderer {
FontGlyphBuilder::FontGlyphBuilder(const size_t maxAscend, size_t cellSize, const size_t margin, FontTexture &texture,
                                   const float displayScale, const float displayOffset)
    : m_maxAscend(maxAscend), m_cellSize(cellSize), m_margin(margin), m_textureSize(texture.m_size), m_textureBuffer(texture.m_buffer),
      m_x(m_margin), m_y(m_margin), m_displayScale(displayScale), m_displayOffset(displayOffset) {
    ensure(m_textureBuffer != nullptr, "textureBuffer is null");
}

FontGlyph FontGlyphBuilder::createGlyph(const ptrdiff_t left, const ptrdiff_t top, const size_t width, const size_t height,
                                       const size_t advance, const char *glyphBuffer, const ptrdiff_t pitch) {

    if (m_x + m_cellSize + m_margin > m_textureSize) {
        m_x = m_margin;
        m_y += m_cellSize + m_margin;
    }

    drawGlyph(left, top, width, height, glyphBuffer, pitch);
    const FontGlyph glyph(m_x, m_y, m_cellSize, m_cellSize, advance, m_displayScale, m_displayOffset);
    m_x += m_cellSize + m_margin;
    return glyph;
}

void FontGlyphBuilder::drawGlyph(const ptrdiff_t left, const ptrdiff_t top, const size_t width, const size_t height,
                                const char *glyphBuffer, const ptrdiff_t pitch) {
    if (!glyphBuffer || width == 0 || height == 0 || std::abs(pitch) < ptrdiff_t(width)) return;
    const auto y = ptrdiff_t(m_maxAscend) - top;
    const auto clipLeft = std::max(ptrdiff_t(0), left);
    const auto clipTop = std::max(ptrdiff_t(0), y);
    const auto clipRight = std::min(ptrdiff_t(m_cellSize), left + ptrdiff_t(width));
    const auto clipBottom = std::min(ptrdiff_t(m_cellSize), y + ptrdiff_t(height));
    if (clipRight <= clipLeft) return;

    for (auto row = clipTop; row < clipBottom; ++row) {
        const auto index = (m_y + size_t(row)) * m_textureSize + m_x + size_t(clipLeft);
        assert(index + size_t(clipRight - clipLeft) <= m_textureSize * m_textureSize);
        const auto* source = glyphBuffer + (row - y) * pitch + (clipLeft - left);
        std::memcpy(m_textureBuffer + index, source, size_t(clipRight - clipLeft));
    }
}
} // namespace Renderer
} // namespace TrenchBroom
