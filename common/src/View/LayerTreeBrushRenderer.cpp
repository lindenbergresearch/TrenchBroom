#include "LayerTreeItemRenderer.h"

#include "Model/BrushNode.h"

namespace TrenchBroom::View {
LayerTreeBrushRenderer::LayerTreeBrushRenderer() { m_layout.margins = QMargins(4, 3, 4, 3); }

LayerTreeItemContent LayerTreeBrushRenderer::content(const Model::Node& node, int ordinal, bool) const {
    const auto& brush = static_cast<const Model::BrushNode &>(node).brush();
    const auto size = node.logicalBounds().size();
    const auto detail = QObject::tr("%1 faces · %2 vertices · %3 × %4 × %5").arg(brush.faceCount()).arg(brush.vertexCount()).arg(size.x()).arg(size.y()).arg(size.z());
    return {.title = QObject::tr("Brush %1").arg(ordinal), .detail = detail, .icon = "Brush.svg"};
}
} // namespace TrenchBroom::View
