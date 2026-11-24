#pragma once

#include <QImage>
#include <QPointF>

// --------------------------------------------------------------------
// uncomment to use precise trigonometric functions for glyph computing
// #define SDF_USE_PRECISE_TRIG
// --------------------------------------------------------------------

class QPainterPath;

namespace TrenchBroom::FontStash {

struct SdfSettings {
    qreal textWeight = 0.80;
    int unitsPerPixel = 32;
    qreal distancePerTexel = 16.0;
    int padding = 8;

    bool operator==(const SdfSettings&) const = default;
};

inline constexpr SdfSettings MinimumSdfSettings{0.10, 4, 4.0, 1};
inline constexpr SdfSettings MaximumSdfSettings{1.00, 64, 64.0, 24};

SdfSettings validatedSdfSettings(SdfSettings settings);
qreal sdfAtlasScale(qreal pixelSize, qreal deviceScale, const SdfSettings& settings);

struct GlyphMask {
    QImage pixels;
    QPoint origin;
};

GlyphMask glyphSdf(const QPainterPath& outline, qreal scale, const SdfSettings& settings = {});

// Convert the atlas texels to coverage at the target device's pixel centers.
GlyphMask sampleGlyphSdf(const GlyphMask& sdf, qreal scale, QPointF phase, const SdfSettings& settings = {});

} // namespace TrenchBroom::FontStash
