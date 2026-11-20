#include "View/Inspector/Layer/LayerTreeItemRenderer.h"

#include "Model/GroupNode.h"

namespace TrenchBroom::View {
LayerTreeGroupRenderer::LayerTreeGroupRenderer() = default;

LayerTreeItemContent LayerTreeGroupRenderer::content(const Model::Node &node, int, bool) const {
    const auto &group = static_cast<const Model::GroupNode &>(node);
    auto detail = QObject::tr("%1 nested entries").arg(node.descendantCount());
    return {QString::fromStdString(node.name()), detail, "widget_medium.svg", group.opened() ? QObject::tr("Open group") : QString{}};
}
} // namespace TrenchBroom::View
