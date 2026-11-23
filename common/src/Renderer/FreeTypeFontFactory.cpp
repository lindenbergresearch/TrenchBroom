/*
Copyright (C) 2010-2017 Kristian Duske

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

#include "FreeTypeFontFactory.h"

#include "Error.h"
#include "Exceptions.h"
#include "FontStashPreferences.h"
#include "Logger.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "IO/DiskIO.h"
#include "IO/File.h"
#include "IO/Reader.h"
#include "IO/SystemPaths.h"
#include "Macros.h"
#include "Renderer/FontDescriptor.h"
#include "Renderer/FontGlyph.h"
#include "Renderer/FontGlyphBuilder.h"
#include "Renderer/FontTexture.h"
#include "Renderer/TextureFont.h"

#include FT_OUTLINE_H

#include <QPainterPath>

#include <algorithm>
#include <string>
#include <type_traits>

namespace TrenchBroom {
namespace Renderer {
namespace {

QPointF outlinePoint(const FT_Vector* point) {
    return {qreal(point->x) / 64, -qreal(point->y) / 64};
}

QPainterPath outlinePath(FT_Outline& outline) {
    const auto functions = FT_Outline_Funcs{
        [](const FT_Vector* point, void* data) {
            auto& path = *static_cast<QPainterPath*>(data);
            path.closeSubpath();
            path.moveTo(outlinePoint(point));
            return 0;
        },
        [](const FT_Vector* point, void* data) {
            static_cast<QPainterPath*>(data)->lineTo(outlinePoint(point));
            return 0;
        },
        [](const FT_Vector* control, const FT_Vector* point, void* data) {
            static_cast<QPainterPath*>(data)->quadTo(outlinePoint(control), outlinePoint(point));
            return 0;
        },
        [](const FT_Vector* a, const FT_Vector* b, const FT_Vector* point, void* data) {
            static_cast<QPainterPath*>(data)->cubicTo(outlinePoint(a), outlinePoint(b), outlinePoint(point));
            return 0;
        },
        0, 0};
    auto path = QPainterPath{};
    if (FT_Outline_Decompose(&outline, &functions, &path) != 0) return {};
    path.closeSubpath();
    return path;
}

}

FreeTypeFontFactory::FreeTypeFontFactory() : m_library(nullptr) {
    FT_Error error = FT_Init_FreeType(&m_library);
    if (error != 0) {
        m_library = nullptr;
        throw RenderException("Error initializing FreeType: " + std::to_string(error));
    }
}

FreeTypeFontFactory::~FreeTypeFontFactory() {
    if (m_library != nullptr) {
        FT_Done_FreeType(m_library);
        m_library = nullptr;
    }
}

std::unique_ptr<TextureFont> FreeTypeFontFactory::doCreateFont(const FontDescriptor &fontDescriptor) {
    defaultQtLogger.info() << "Create font: " << fontDescriptor.path() << " " << fontDescriptor.size();

    try {
        auto [face, bufferedReader] = loadFont(fontDescriptor);
        const auto faceGuard = std::unique_ptr<std::remove_pointer_t<FT_Face>, decltype(&FT_Done_Face)>(face, &FT_Done_Face);
        auto font = buildFont(face, fontDescriptor.minChar(), fontDescriptor.charCount(), fontDescriptor.lineHeightOffs());

        // NOTE: bufferedReader is returned from loadFont() just to keep the buffer from
        // being deallocated until after we call FT_Done_Face
        unused(bufferedReader);

        return font;
    } catch (const RenderException& exception) {
        auto fontPath = Preferences::RendererFontPath.defaultValue();
        const auto fd = FontDescriptor(fontPath, size_t(pref(Preferences::RendererFontSize)));

        defaultQtLogger.warn() << exception.what();
        defaultQtLogger.warn() << "fallback to default font: " << fd.name();

        auto [face, bufferedReader] = loadFont(fd);
        const auto faceGuard = std::unique_ptr<std::remove_pointer_t<FT_Face>, decltype(&FT_Done_Face)>(face, &FT_Done_Face);
        auto font = buildFont(face, fd.minChar(), fd.charCount(), fd.lineHeightOffs());

        // NOTE: bufferedReader is returned from loadFont() just to keep the buffer from
        // being deallocated until after we call FT_Done_Face
        unused(bufferedReader);

        return font;
    }
}

std::pair<FT_Face, IO::BufferedReader> FreeTypeFontFactory::loadFont(const FontDescriptor &fontDescriptor) {
    const auto fontPath = fontDescriptor.path().is_absolute() ? fontDescriptor.path()
                                                              : IO::SystemPaths::findResourceFile(fontDescriptor.path());

    return IO::Disk::openFile(fontPath).and_then([&](auto file) -> Result<std::pair<FT_Face, IO::BufferedReader>> {
        auto reader = file->reader().buffer();

        auto face = FT_Face{};
        const auto error = FT_New_Memory_Face(m_library, reinterpret_cast<const FT_Byte *>(reader.begin()), FT_Long(reader.size()), 0, &face);
        if (error) {
            return Error{"FT_New_Memory_Face returned " + std::to_string(error)};
        }

        FT_Set_Pixel_Sizes(face, 0, FT_UInt(fontDescriptor.size()));
        return std::pair{face, std::move(reader)};
    }).if_error([&](auto e) {
        throw RenderException{
            "Error loading font '" + fontDescriptor.name() + "': " + e.msg
        };
    }).value();
}

std::unique_ptr<TextureFont> FreeTypeFontFactory::buildFont(FT_Face face, const unsigned char firstChar, const unsigned char charCount, size_t lineHeightOffs) {
    const Metrics metrics = computeMetrics(face, firstChar, charCount, lineHeightOffs);

    if (FT_IS_SCALABLE(face) && !FT_HAS_COLOR(face) && face->size->metrics.y_ppem > 0) {
        if (auto font = buildSdfFont(face, firstChar, charCount, metrics)) return font;
    }

    constexpr auto gutter = size_t{3};
    auto texture = std::make_unique<FontTexture>(charCount, metrics.cellSize, gutter);
    FontGlyphBuilder glyphBuilder(metrics.maxAscend, metrics.cellSize, gutter, *texture);

    FT_GlyphSlot glyph = face->glyph;
    std::vector<FontGlyph> glyphs;
    for (auto c = int(firstChar); c < int(firstChar) + charCount; ++c) {
        FT_Error error = FT_Load_Char(face, static_cast<FT_ULong>(c), FT_LOAD_RENDER);
        if (error != 0) {
            glyphs.push_back(FontGlyph(0, 0, 0, 0, 0));
        } else {
            glyphs.push_back(glyphBuilder.createGlyph(glyph->bitmap_left, glyph->bitmap_top,
                glyph->bitmap.width, glyph->bitmap.rows, size_t(glyph->advance.x >> 6),
                reinterpret_cast<const char*>(glyph->bitmap.buffer), glyph->bitmap.pitch));
        }
    }

    return std::make_unique<TextureFont>(std::move(texture), glyphs, static_cast<int>(metrics.lineHeight), firstChar, charCount);
}

std::unique_ptr<TextureFont> FreeTypeFontFactory::buildSdfFont(FT_Face face, unsigned char firstChar,
                                                              unsigned char charCount, const Metrics& metrics) {
    const auto settings = fontStashPreferences().sdf;
    const auto atlasScale = std::min(4.0, 64.0 / face->size->metrics.y_ppem);
    // A tuning change may regenerate the texture while vertex buffers still
    // reference it. Reserve the same cell bounds and UVs for every padding value.
    const auto padding = FontStash::MaximumSdfSettings.padding;
    const auto cellSize = size_t(qCeil(qreal(metrics.cellSize) * atlasScale) + 2 * padding);
    const auto baseline = size_t(qRound(qreal(metrics.maxAscend) * atlasScale) + padding);
    constexpr auto gutter = size_t{2};
    auto texture = std::make_unique<FontTexture>(charCount, cellSize, gutter);
    auto builder = FontGlyphBuilder(baseline, cellSize, gutter, *texture,
        float(1 / atlasScale), float(-padding / atlasScale));
    auto glyphs = std::vector<FontGlyph>{};
    glyphs.reserve(charCount);
    for (auto c = int(firstChar); c < int(firstChar) + charCount; ++c) {
        const auto character = FT_ULong(c);
        if (FT_Load_Char(face, character, FT_LOAD_DEFAULT) != 0) return nullptr;
        const auto advance = size_t(std::max<FT_Pos>(0, face->glyph->advance.x >> 6));
        if (FT_Load_Char(face, character, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING) != 0
            || face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) return nullptr;
        auto mask = FontStash::GlyphMask{};
        if (face->glyph->outline.n_points > 0) {
            const auto outline = outlinePath(face->glyph->outline);
            if (!outline.isEmpty()) {
                mask = FontStash::glyphSdf(outline, atlasScale, settings);
                if (mask.pixels.isNull()) return nullptr;
            }
        }
        glyphs.push_back(builder.createGlyph(padding + mask.origin.x(), -mask.origin.y(),
            size_t(mask.pixels.width()), size_t(mask.pixels.height()), advance,
            reinterpret_cast<const char*>(mask.pixels.constBits()), mask.pixels.bytesPerLine()));
    }
    return std::make_unique<TextureFont>(std::move(texture), glyphs, int(metrics.lineHeight),
        firstChar, charCount, float(settings.distancePerTexel));
}

FreeTypeFontFactory::Metrics FreeTypeFontFactory::computeMetrics(FT_Face face, const unsigned char firstChar, const unsigned char charCount,size_t lineHeightOffs) const {
    FT_GlyphSlot glyph = face->glyph;

    int maxWidth = 0;
    int maxAscend = 0;
    int maxDescend = 0;
    int lineHeight = 0;

    for (auto c = int(firstChar); c < int(firstChar) + charCount; ++c) {
        FT_Error error = FT_Load_Char(face, static_cast<FT_ULong>(c), FT_LOAD_RENDER);
        if (error != 0) {
            continue;
        }

        maxWidth = std::max(maxWidth, glyph->bitmap_left + static_cast<FT_Int>(glyph->bitmap.width));
        maxAscend = std::max(maxAscend, glyph->bitmap_top);
        maxDescend = std::max(maxDescend, static_cast<FT_Int>(glyph->bitmap.rows) - glyph->bitmap_top);
        lineHeight = std::max(lineHeight, static_cast<int>(glyph->metrics.height >> 6));
    }

    const int cellSize = std::max(maxWidth, maxAscend + maxDescend);
    return {
        static_cast<size_t>(cellSize), static_cast<size_t>(maxAscend), static_cast<size_t>(lineHeight+lineHeightOffs)
    };
}
} // namespace Renderer
} // namespace TrenchBroom
