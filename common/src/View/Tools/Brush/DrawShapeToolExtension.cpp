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

#include "View/Tools/Brush/DrawShapeToolExtension.h"
#include "Error.h"
#include "Model/Brush.h"
#include "Model/BrushNode.h"
#include "Model/EditorContext.h"
#include "Model/GroupNode.h"
#include "Model/LinkedGroupUtils.h"
#include "Model/WorldNode.h"
#include "View/Document/MapDocument.h"
#include "kdl/memory_utils.h"
#include <algorithm>
#include <kdl/result.h>

namespace TrenchBroom::View {
DrawShapeToolExtension::DrawShapeToolExtension(DrawShapeKind kind, DrawShapeToolParameters& parameters) : m_kind(kind), m_parameters(parameters) {}
DrawShapeToolExtension::~DrawShapeToolExtension() = default;
const std::string& DrawShapeToolExtension::name() const {
    static const std::array<std::string, 8> names{"Cuboid", "Stairs", "Arch", "Cylinder", "Cone", "Spheroid (UV)", "Spheroid (Icosahedron)", "Rock"};
    return names.at(size_t(m_kind));
}
const std::filesystem::path& DrawShapeToolExtension::iconPath() const {
    static const std::array<std::filesystem::path, 8> icons{"ShapeTool_Cuboid.svg", "ShapeTool_Stairs.svg",   "ShapeTool_Arch.svg",      "ShapeTool_Cylinder.svg",
                                                            "ShapeTool_Cone.svg",   "ShapeTool_UVSphere.svg", "ShapeTool_IcoSphere.svg", "ShapeTool_Rock.svg"};
    return icons.at(size_t(m_kind));
}
void DrawShapeToolExtension::brushesWereCreated() {
    if (m_kind == DrawShapeKind::Rock && m_parameters.incrementRockSeed())
        m_parameters.setRockSeed(m_parameters.rockSeed() == DrawShapeToolParameters::MaxRockSeed ? 0 : m_parameters.rockSeed() + 1);
}
DrawShapeToolExtensionManager::DrawShapeToolExtensionManager(std::weak_ptr<MapDocument> document) : m_document(std::move(document)) {
    for (const auto kind : DrawShapeKinds)
        m_extensions.push_back(std::make_unique<DrawShapeToolExtension>(kind, m_parameters));
}
std::shared_ptr<MapDocument> DrawShapeToolExtensionManager::document() const {
    return kdl::mem_lock(m_document);
}
std::vector<DrawShapeToolExtension*> DrawShapeToolExtensionManager::extensions() const {
    std::vector<DrawShapeToolExtension*> result;
    for (const auto& extension : m_extensions)
        result.push_back(extension.get());
    return result;
}
DrawShapeToolExtension& DrawShapeToolExtensionManager::currentExtension() {
    return *m_extensions.at(m_currentExtensionIndex);
}
const DrawShapeToolExtension& DrawShapeToolExtensionManager::currentExtension() const {
    return *m_extensions.at(m_currentExtensionIndex);
}
bool DrawShapeToolExtensionManager::setCurrentExtensionIndex(size_t index) {
    if (index >= m_extensions.size() || index == m_currentExtensionIndex) return false;
    m_currentExtensionIndex = index;
    currentExtensionDidChangeNotifier(index);
    return true;
}
bool DrawShapeToolExtensionManager::setCurrentExtension(DrawShapeKind kind) {
    const auto it = std::find(DrawShapeKinds.begin(), DrawShapeKinds.end(), kind);
    return it != DrawShapeKinds.end() && setCurrentExtensionIndex(size_t(it - DrawShapeKinds.begin()));
}
bool DrawShapeToolExtensionManager::canApplySelection() const {
    const auto doc = document();
    if (!doc->world() || !doc->hasSelectedNodes() || doc->hasSelectedBrushFaces() || !doc->selectedNodes().hasOnlyBrushes()) return false;
    const auto& brushes = doc->selectedNodes().brushes();
    auto* parent = brushes.front()->parent();
    for (const auto* brush : brushes)
        if (brush->parent() != parent || !doc->editorContext().editable(brush) || !doc->editorContext().visible(brush)) return false;
    // Replacing children of a linked prefab would change every instance. Require an
    // explicit unlink before a topology-changing replacement.
    for (auto* ancestor = parent; ancestor; ancestor = ancestor->parent()) {
        if (const auto* group = dynamic_cast<const Model::GroupNode*>(ancestor))
            if (Model::collectGroupsWithLinkId({doc->world()}, group->linkId()).size() > 1) return false;
    }
    return true;
}
Result<std::vector<Model::Brush>> DrawShapeToolExtensionManager::createBrushes(const vm::bbox3& bounds, vm::axis::type viewAxis) const {
    return currentExtension().createBrushes(bounds, m_parameters.resolvedAxis(viewAxis), *document());
}
void DrawShapeToolExtensionManager::brushesWereCreated() {
    currentExtension().brushesWereCreated();
}
} // namespace TrenchBroom::View
