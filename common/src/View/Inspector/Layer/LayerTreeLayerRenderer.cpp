#include "View/Inspector/Layer/LayerTreeItemRenderer.h"

#include "Model/LayerNode.h"

namespace TrenchBroom::View {
LayerTreeLayerRenderer::LayerTreeLayerRenderer() = default;

LayerTreeItemContent LayerTreeLayerRenderer::content(const Model::Node& node, int, bool active) const {
    const auto& layer = static_cast<const Model::LayerNode &>(node);
    auto detail = QObject::tr("%1 nested entries").arg(node.descendantCount());
    if (active) detail += QObject::tr(" · Active layer");
    if (layer.layer().omitFromExport()) detail += QObject::tr(" · Omitted from export");
    return {QString::fromStdString(node.name()), detail, "stacks.svg"};
}
} // namespace TrenchBroom::View
