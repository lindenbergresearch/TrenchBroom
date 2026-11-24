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

#include "IO/IconStyle.h"

#include <QIcon>
#include <QWidget>
#include <filesystem>
#include <memory>

class QRawFont;

class QImage;

namespace TrenchBroom::IO {
class VectorIcon;

// One hidden application-wide QSS proxy, shared by SVG and raster icons.
class IconAppearance : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal disabledBrightness READ disabledBrightness WRITE setDisabledBrightness)
    Q_PROPERTY(qreal disabledOpacity READ disabledOpacity WRITE setDisabledOpacity)
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness)
    Q_PROPERTY(bool normalizeBrightness READ normalizeBrightness WRITE setNormalizeBrightness)
    qreal m_brightness = 0.35, m_opacity = 1.0;
    qreal m_targetBrightness = 0.75;
    bool m_normalizeBrightness = true;
    IconAppearance();
public:
    static IconAppearance& instance();
    qreal disabledBrightness() const { return m_brightness; }
    qreal disabledOpacity() const { return m_opacity; }
    void setDisabledBrightness(qreal value);
    void setDisabledOpacity(qreal value);
    qreal brightness() const { return m_targetBrightness; }
    bool normalizeBrightness() const { return m_normalizeBrightness; }
    void setBrightness(qreal value);
    void setNormalizeBrightness(bool enabled);
};

QPixmap disabledIconPixmap(const QPixmap& source);
// Peak RGB brightness of painted pixels. Empty space and weak antialiasing
// fringes do not affect the measurement. Alpha is never normalized.
qreal measureIconBrightness(const QImage& image);
void normalizeIconBrightness(QImage& image, qreal sourceBrightness, qreal targetBrightness);

/**
 * GUI-thread cache of scalable icons, including parsed SVG documents. Entries
 * are evicted least recently used; QIcon copies held by widgets remain valid.
 * Rasterizations use Qt's separate, memory-bounded QPixmapCache.
 */
class IconCache {
public:
    static constexpr int DefaultMaxEntries = 256;
    static int size();
    static int maxEntries();
    static void setMaxEntries(int count);

    // Reload files on the next load. Existing QIcons retain their SVG snapshot;
    // their rasterizations age out of QPixmapCache normally. No dangling refs.
    static void resetCache();

private:
    friend class VectorIcon;
    static VectorIcon load(const std::filesystem::path& path);
};

/**
 * A resolution-independent SVG or outline glyph icon. A base.svg may have base_on.svg and
 * base_off.svg variants, mapped to QIcon::On and Off (not enabled/disabled).
 * Relative paths resolve in the application's images resources; absolute paths
 * and Qt resource paths are also supported. Missing/invalid files yield a null
 * icon. All operations, including painting, belong to the GUI thread.
 */
class VectorIcon {
public:
    explicit VectorIcon(const std::filesystem::path& path);
    static VectorIcon fromGlyph(const QRawFont& font, quint32 glyphIndex);
    QIcon icon() const;
    // Reuse the parsed SVG and preserve its shape/alpha, replacing the painted
    // colors with the supplied tint. An invalid color returns the original.
    QIcon icon(const QColor& tint) const;
    // An optional theme color takes precedence over the requested tint
    // and is resolved whenever the icon is painted.
    QIcon styledIcon(const IconStyle& style, const QString& themeColor = {}) const;
    // The color is resolved while painting, so an existing icon follows theme
    // changes without rebuilding every item in a large tree.
    QIcon themedIcon(const QString& colorName, const QColor& fallback = {}) const;
    // Intrinsic SVG size in logical pixels, for callers that need a bitmap.
    QSize defaultSize() const;
    // Source SVG dimensions, or the outline fitted inside a glyphExtent square.
    QSize sourceSize(int glyphExtent) const;
    bool isValid() const;

private:
    friend class IconCache;
    struct Sources;
    class Engine;
    VectorIcon(std::shared_ptr<Sources> sources, const IconStyle& style, const QString& themeColor = {});
    std::shared_ptr<Sources> m_sources;
    QIcon m_icon;
};

} // namespace TrenchBroom::IO
