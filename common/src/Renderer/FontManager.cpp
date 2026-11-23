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

#include "FontManager.h"

#include "FontStashPreferences.h"
#include "Renderer/FontDescriptor.h"
#include "Renderer/FreeTypeFontFactory.h"
#include "Renderer/TextureFont.h"

#include <QOpenGLContext>

#include <string>
#include <memory>
#include <algorithm>

namespace TrenchBroom {
namespace Renderer {
FontManager::FontManager() : m_factory(std::make_unique<FreeTypeFontFactory>()) {}

FontManager::~FontManager() = default;

void FontManager::clearCache() {
    // A preferences change can arrive without the rendering context current.
    m_clearPending = true;
}

TextureFont &FontManager::font(const FontDescriptor &fontDescriptor) {
    const auto& sdf = fontStashPreferences().sdf;
    const auto atlasSettings = std::tuple{sdf.unitsPerPixel, double(sdf.distancePerTexel), sdf.padding};
    if (m_cache.empty() || ((m_clearPending || m_atlasSettings != atlasSettings) && QOpenGLContext::currentContext())) {
        m_cache.clear();
        m_atlasSettings = atlasSettings;
        m_clearPending = false;
    }
    auto it = m_cache.find(fontDescriptor);
    if (it == m_cache.end()) it = m_cache.emplace(fontDescriptor, m_factory->createFont(fontDescriptor)).first;
    return *it->second;
}

FontDescriptor FontManager::selectFontSize(const FontDescriptor &fontDescriptor, const std::string &text, float maxWidth, size_t minFontSize) {
    FontDescriptor actualDescriptor = fontDescriptor;
    auto actualBounds = font(actualDescriptor).measure(text);

    while (actualBounds.x() > maxWidth && actualDescriptor.size() > minFontSize) {
        actualDescriptor = FontDescriptor(actualDescriptor.path(), actualDescriptor.size() - 1);
        actualBounds = font(actualDescriptor).measure(text);
    }
    return actualDescriptor;
}

FontDescriptor FontManager::selectFontSize(const FontDescriptor &fontDescriptor, float distance, float maxDistance, int min, int max) {
    float m = static_cast<float>(min - max) / maxDistance;
    int size = std::clamp(static_cast<int>(m * distance + max), min, max);

    return FontDescriptor(fontDescriptor.path(), static_cast<size_t>(size));
}
} // namespace Renderer
} // namespace TrenchBroom
