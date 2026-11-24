#include "View/Inspector/Layer/LayerTreeItemRenderer.h"

#include "Model/BrushNode.h"

namespace TrenchBroom::View {
LayerTreeBrushRenderer::LayerTreeBrushRenderer() = default;

LayerTreeItemContent LayerTreeBrushRenderer::content(const Model::Node &node, int ordinal, bool) const {
    const auto &brush = static_cast<const Model::BrushNode &>(node).brush();

    const auto size = node.logicalBounds().size();
    const auto detail = QObject::tr("%1 faces · %2 vertices · %3×%4×%5").arg(brush.faceCount()).arg(brush.vertexCount()).arg(size.x()).arg(size.y()).arg(size.z());
    return { QObject::tr("Brush"), detail, "widgets.svg", QObject::tr("#%1").arg(ordinal) };
}
} // namespace TrenchBroom::View
