#pragma once

#include <QImage>
#include <QPointF>

class QPainterPath;

namespace TrenchBroom::FontStash {

struct GlyphMask {
    QImage pixels;
    QPoint origin;
};

GlyphMask glyphBitmap(const QPainterPath& outline, qreal scale, QPointF phase);
GlyphMask glyphSdf(const QPainterPath& outline, qreal scale);

// Convert the atlas texels to coverage at the target device's pixel centers.
GlyphMask sampleGlyphSdf(const GlyphMask& sdf, qreal scale, QPointF phase);

} // namespace TrenchBroom::FontStash
