#pragma once

#include "fontstash/GlyphRasterizer.h"

class QFont;
class QPaintDevice;
class QRawFont;

namespace TrenchBroom::View {

struct FontStashGlyph {
    FontStash::GlyphMask mask;
    bool emptyOutline = false;
};

// The mask origin is in physical pixels, including the supplied baseline.
// Pass a null settings pointer for the bitmap comparison mode.
FontStashGlyph fontStashGlyph(const QRawFont& font, quint32 glyph, QPointF baseline,
                             qreal deviceScale, const FontStash::SdfSettings* sdf);

// Paint one shaped line into its own raster buffer. Used for the independent
// Preferences samples and for the explicit Layer comparison modes.
QImage fontStashTextLine(const QPaintDevice& fontDevice, const QString& text, const QFont& font,
                         QColor color, const QRectF& content, QSize imageSize, qreal deviceScale,
                         Qt::Alignment alignment, const FontStash::SdfSettings* sdf);

} // namespace TrenchBroom::View
