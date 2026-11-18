#pragma once

#include <QFont>

#include <filesystem>

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
}
