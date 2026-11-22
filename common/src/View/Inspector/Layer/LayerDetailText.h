#pragma once

#include <QImage>

class QLabel;

namespace TrenchBroom {
template <typename T> class Preference;

namespace View {

enum class LayerDetailTextRenderer { Qt = 0, FontStashBitmap = 1, FontStashSdf = 2 };

Preference<int>& layerDetailTextRendererPreference();
LayerDetailTextRenderer layerDetailTextRenderer();

// Use the label's polished font, palette and content rectangle. A null image
// leaves text painting to the label, for example for a bitmap-only glyph.
QImage renderLayerDetailText(const QLabel& label, LayerDetailTextRenderer renderer, qreal devicePixelRatio);

}
}
