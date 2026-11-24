#pragma once

#include <QColor>

#include <optional>

namespace TrenchBroom::IO {

struct IconStyle {
    // An invalid color uses the source colors (the text color for glyph icons).
    QColor color;
    // An empty value uses the theme brightness or the source opacity.
    std::optional<qreal> brightness;
    std::optional<qreal> opacity;

    bool operator==(const IconStyle&) const = default;
};

} // namespace TrenchBroom::IO
