#include "LayerTreeItemRenderer.h"

#include "Model/GroupNode.h"

namespace TrenchBroom::View {
LayerTreeGroupRenderer::LayerTreeGroupRenderer() {
    m_layout.margins = QMargins(4, 4, 4, 4);
}

LayerTreeItemContent LayerTreeGroupRenderer::content(const Model::Node &node, int, bool) const {
    const auto &group = static_cast<const Model::GroupNode &>(node);
    auto detail = QObject::tr("%1 nested entries").arg(node.descendantCount());
    if (group.opened()) detail += QObject::tr(" · Open group");
    return {QString::fromStdString(node.name()), detail, "Group.svg"};
}
} // namespace TrenchBroom::View
