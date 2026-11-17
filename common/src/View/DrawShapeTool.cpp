/*
 Copyright (C) 2023 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "DrawShapeTool.h"
#include "Error.h"
#include "Model/BrushNode.h"
#include "Model/Node.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/DrawShapeToolPage.h"
#include "View/MapDocument.h"
#include <kdl/memory_utils.h>
#include <kdl/result.h>

namespace TrenchBroom::View {
DrawShapeTool::DrawShapeTool(std::weak_ptr<MapDocument> document) : CreateBrushesToolBase(false, document), m_extensionManager(document) {
    m_connections += m_extensionManager.applyParametersNotifier.connect([this](auto axis) { applyParameters(axis); });
    const auto updatePreview = [this] {
        if (m_previewBounds) update(*m_previewBounds, m_viewAxis);
        refreshViews();
    };
    m_connections += m_extensionManager.parameters().parametersDidChangeNotifier.connect(updatePreview);
    m_connections += m_extensionManager.currentExtensionDidChangeNotifier.connect([updatePreview](size_t) { updatePreview(); });
}
void DrawShapeTool::update(const vm::bbox3& bounds, vm::axis::type viewAxis) {
    m_previewBounds = bounds;
    m_viewAxis = viewAxis;
    m_extensionManager.createBrushes(bounds, viewAxis)
        .transform([&](auto brushes) {
            auto nodes = std::vector<std::unique_ptr<Model::BrushNode>>{};
            for (auto& brush : brushes)
                nodes.push_back(std::make_unique<Model::BrushNode>(std::move(brush)));
            updateBrushes(std::move(nodes));
        })
        .transform_error([&](const auto& error) {
            clearBrushes();
            kdl::mem_lock(m_document)->error() << "Could not update brush: " << error;
        });
}
bool DrawShapeTool::createBrushes() {
    auto group = pref(Preferences::GroupBrushesCreatedByShapeTool) ? std::optional{m_extensionManager.currentExtension().name()} : std::nullopt;
    const auto result = CreateBrushesToolBase::createBrushes(std::move(group));
    m_previewBounds.reset();
    return result;
}
void DrawShapeTool::doBrushesWereCreated() {
    m_previewBounds.reset();
    m_extensionManager.brushesWereCreated();
}
bool DrawShapeTool::applyParameters(vm::axis::type viewAxis) {
    if (!m_extensionManager.canApplySelection()) return false;
    auto document = kdl::mem_lock(m_document);
    const auto original = document->selectedNodes().nodes();
    auto* parent = original.front()->parent();
    auto result = m_extensionManager.createBrushes(document->selectionBounds(), viewAxis);
    if (result.is_error()) {
        document->error() << "Could not apply shape parameters: " << std::get<Error>(result.error());
        return false;
    }
    auto brushes = std::move(result).value();
    if (brushes.empty()) return false;
    auto sources = std::vector<const Model::Brush*>{};
    for (const auto* node : document->selectedNodes().brushes())
        sources.push_back(&node->brush());
    auto owners = std::vector<std::unique_ptr<Model::BrushNode>>{};
    auto nodes = std::vector<Model::Node*>{};
    for (auto& brush : brushes) {
        brush.cloneFaceAttributesFrom(sources);
        owners.push_back(std::make_unique<Model::BrushNode>(std::move(brush)));
        nodes.push_back(owners.back().get());
    }
    auto transaction = Transaction{document, "Apply Shape Parameters"};
    // Add first: a brush entity/group must not be removed as an empty container.
    for (auto& owner : owners)
        owner.release();
    auto added = document->addNodes({{parent, std::move(nodes)}});
    if (added.empty()) {
        transaction.cancel();
        return false;
    }
    document->deleteObjects();
    document->selectNodes(added);
    if (pref(Preferences::GroupBrushesCreatedByShapeTool) && added.size() > 1 && !document->groupSelection(m_extensionManager.currentExtension().name())) {
        transaction.cancel();
        return false;
    }
    if (!transaction.commit()) return false;
    m_previewBounds.reset();
    clearBrushes();
    m_extensionManager.brushesWereCreated();
    refreshViews();
    return true;
}
bool DrawShapeTool::cancel() {
    const auto hadPreview = m_previewBounds.has_value();
    m_previewBounds.reset();
    clearBrushes();
    refreshViews();
    return hadPreview;
}
bool DrawShapeTool::doActivate() {
    m_extensionManager.setCurrentExtensionIndex(0);
    return true;
}
bool DrawShapeTool::doDeactivate() {
    cancel();
    return true;
}
QWidget* DrawShapeTool::doCreatePage(QWidget* parent) {
    return new DrawShapeToolPage(m_document, m_extensionManager, parent);
}
} // namespace TrenchBroom::View
