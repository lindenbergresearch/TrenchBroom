#pragma once

#include <QFont>
#include <QPointF>

#include <filesystem>
#include <optional>

class QFontMetricsF;
class QPainter;
class QRectF;

namespace TrenchBroom::View {
// Sizes are logical pixels. Qt applies the display's device pixel ratio.
// CSS/OpenType weights are 1..1000 in Qt 6. Retain old theme properties
// expressed as Qt 5 weights (0..99), mapping the named weights exactly.
QFont::Weight normalizedFontWeight(int weight);
QFont loadFont(const std::filesystem::path& path, int pixelSize);
int uiFontPixelSize(int adjustment = 0);
QFont adjustedFont(const QFont& font, qreal pixelDelta);
QFont detailFont(const QFont& font, qreal reduction = 2.0);
QFont fontForStyleSheet(QFont font);

struct FontAxisRange {
    qreal minimum, maximum, defaultValue;
};
struct FontCapabilities {
    std::optional<FontAxisRange> weight;
    std::optional<FontAxisRange> opticalSize;
};
FontCapabilities fontCapabilities(const QFont& font);

// Fine tuning applies to custom list text and its matching preferences preview.
// Keep axes off QApplication's font: inherited axes override explicit QSS weights.
struct ListFontSettings {
    int weight = 0; // 0: the selected font / stylesheet's weight
    qreal opticalSize = 0; // 0: automatic, in typographic points
    bool typoLineMetrics = false;
    bool subpixelPositioning = false;
};
ListFontSettings listFontSettings();
QFont listTextFont(QFont font, const ListFontSettings& settings, bool preserveEmphasis = false);
qreal logicalFontPixelSize(const QFont& font);
QPointF textLineBaseline(const QPainter& painter, const QRectF& rect,
    const QFontMetricsF& metrics, bool subpixelPositioning);
void paintTextLine(QPainter& painter, const QRectF& rect, const QString& text,
    const QFont& font, bool subpixelPositioning);
}
