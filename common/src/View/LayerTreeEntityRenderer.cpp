#include "LayerTreeItemRenderer.h"

#include "Model/EntityNode.h"

namespace TrenchBroom::View {
LayerTreeEntityRenderer::LayerTreeEntityRenderer() = default;

LayerTreeItemContent LayerTreeEntityRenderer::content(const Model::Node &node, int, bool) const {
    const auto &entity = static_cast<const Model::EntityNode &>(node).entity();
    auto detail = QObject::tr("%1 properties").arg(entity.properties().size());
    if (node.hasChildren()) detail += QObject::tr(" · %1 geometry objects").arg(node.childCount());
    else {
        const auto &origin = entity.origin();
        detail += QObject::tr(" · at %1×%2×%3)").arg(origin.x()).arg(origin.y()).arg(origin.z());
    }
    if (const auto *targetname = entity.property("targetname")) {
        if (!targetname->empty()) detail = QString::fromStdString(*targetname) + " · " + detail;
    }
    return {QString::fromStdString(entity.classname()), detail, "explicit.svg"};
}
} // namespace TrenchBroom::View
