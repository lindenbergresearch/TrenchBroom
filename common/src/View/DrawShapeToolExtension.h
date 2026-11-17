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

#pragma once
#include "Model/Brush.h"
#include "Notifier.h"
#include "Result.h"
#include "View/DrawShapeToolParameters.h"
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace TrenchBroom::View {
class MapDocument;
enum class DrawShapeKind { Cuboid, Stairs, Arch, Cylinder, Cone, UVSphere, IcoSphere, Rock };
inline constexpr auto DrawShapeKinds = std::array{DrawShapeKind::Cuboid, DrawShapeKind::Stairs,   DrawShapeKind::Arch,      DrawShapeKind::Cylinder,
                                                  DrawShapeKind::Cone,   DrawShapeKind::UVSphere, DrawShapeKind::IcoSphere, DrawShapeKind::Rock};

class DrawShapeToolExtension {
  protected:
    DrawShapeKind m_kind;
    DrawShapeToolParameters& m_parameters;

  public:
    DrawShapeToolExtension(DrawShapeKind kind, DrawShapeToolParameters& parameters);
    virtual ~DrawShapeToolExtension();
    DrawShapeKind kind() const { return m_kind; }
    virtual const std::string& name() const;
    virtual const std::filesystem::path& iconPath() const;
    virtual Result<std::vector<Model::Brush>> createBrushes(const vm::bbox3& bounds, vm::axis::type axis, const MapDocument& document) const;
    virtual void brushesWereCreated();
};

class DrawShapeToolExtensionManager {
    std::weak_ptr<MapDocument> m_document;
    DrawShapeToolParameters m_parameters;
    std::vector<std::unique_ptr<DrawShapeToolExtension>> m_extensions;
    size_t m_currentExtensionIndex = 0;

  public:
    Notifier<size_t> currentExtensionDidChangeNotifier;
    Notifier<vm::axis::type> applyParametersNotifier;
    explicit DrawShapeToolExtensionManager(std::weak_ptr<MapDocument> document);
    std::shared_ptr<MapDocument> document() const;
    DrawShapeToolParameters& parameters() { return m_parameters; }
    std::vector<DrawShapeToolExtension*> extensions() const;
    DrawShapeToolExtension& currentExtension();
    const DrawShapeToolExtension& currentExtension() const;
    bool setCurrentExtensionIndex(size_t index);
    bool setCurrentExtension(DrawShapeKind kind);
    bool canApplySelection() const;
    Result<std::vector<Model::Brush>> createBrushes(const vm::bbox3& bounds, vm::axis::type viewAxis) const;
    void brushesWereCreated();
};
} // namespace TrenchBroom::View
