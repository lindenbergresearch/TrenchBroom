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

#pragma once

#include <cstddef>

namespace TrenchBroom {
namespace Renderer {
class FontGlyph;

class FontTexture;

class FontGlyphBuilder {
  private:
    size_t m_maxAscend;
    size_t m_cellSize;
    size_t m_margin;
    size_t m_textureSize;
    char *m_textureBuffer;

    size_t m_x;
    size_t m_y;
    float m_displayScale;
    float m_displayOffset;

  public:
    FontGlyphBuilder(size_t maxAscend, size_t cellSize, size_t margin, FontTexture &texture,
                     float displayScale = 1, float displayOffset = 0);

    FontGlyph createGlyph(ptrdiff_t left, ptrdiff_t top, size_t width, size_t height, size_t advance,
                          const char *glyphBuffer, ptrdiff_t pitch);

  private:
    void drawGlyph(ptrdiff_t left, ptrdiff_t top, size_t width, size_t height, const char *glyphBuffer, ptrdiff_t pitch);
};
} // namespace Renderer
} // namespace TrenchBroom
