#pragma once

#include <QVariantMap>

namespace TrenchBroom::View {
class MapDocument;
class MapViewToolBox;

// A value-only snapshot: templates never retain pointers to editable map objects.
QVariantMap contextOverlayData(const MapDocument& document, MapViewToolBox& tools);
} // namespace TrenchBroom::View
