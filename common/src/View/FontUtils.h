#pragma once

#include <QFont>

#include <filesystem>

namespace TrenchBroom::View {
// Sizes are logical pixels. Qt applies the display's device pixel ratio.
QFont loadFont(const std::filesystem::path& path, int pixelSize);
int uiFontPixelSize(int adjustment = 0);
QFont adjustedFont(const QFont& font, qreal pixelDelta);
QFont detailFont(const QFont& font, qreal reduction = 2.0);
QFont fontForStyleSheet(QFont font);
}
