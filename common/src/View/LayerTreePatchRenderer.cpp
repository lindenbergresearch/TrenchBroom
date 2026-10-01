#include "LayerTreeItemRenderer.h"

#include "Model/PatchNode.h"

namespace TrenchBroom::View {
LayerTreePatchRenderer::LayerTreePatchRenderer() {
    m_layout.margins = QMargins(4, 3, 4, 3);
}

LayerTreeItemContent LayerTreePatchRenderer::content(const Model::Node &node, int ordinal, bool) const {
    const auto &patch = static_cast<const Model::PatchNode &>(node).patch();
    auto detail = QObject::tr("%1 × %2 control points").arg(patch.pointRowCount()).arg(patch.pointColumnCount());
    if (!patch.textureName().empty()) detail += " · " + QString::fromStdString(patch.textureName());
    return {QObject::tr("Patch %1").arg(ordinal), detail, "Brush.svg"};
}
} // namespace TrenchBroom::View
