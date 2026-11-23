#pragma once

#include "fontstash/GlyphRasterizer.h"

class QString;

namespace TrenchBroom {
template <typename T> class Preference;

struct FontStashPreferences {
    bool enabled = true;
    FontStash::SdfSettings sdf;

    bool operator==(const FontStashPreferences&) const = default;
};

// Store the renderer and its four settings together so Apply publishes one change.
Preference<QString>& fontStashPreference();
const FontStashPreferences& fontStashPreferences();
void applyFontStashPreferences(const FontStashPreferences& settings);

} // namespace TrenchBroom
