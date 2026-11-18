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

#include "Error.h"
#include "Model/BrushBuilder.h"
#include "Model/Game.h"
#include "Model/WorldNode.h"
#include "View/Tools/Brush/DrawShapeToolExtension.h"
#include "View/Document/MapDocument.h"
#include <algorithm>
#include <cmath>
#include <kdl/result.h>

namespace TrenchBroom::View {
Result<std::vector<Model::Brush>> DrawShapeToolExtension::createBrushes(const vm::bbox3& bounds, vm::axis::type axis, const MapDocument& document) const {
    const auto builder = Model::BrushBuilder{document.world()->mapFormat(), document.worldBounds(), document.game()->defaultFaceAttribs()};
    const auto& texture = document.currentTextureName();
    const auto& p = m_parameters;
    const auto single = [](auto brush) { return std::vector<Model::Brush>{std::move(brush)}; };
    switch (m_kind) {
    case DrawShapeKind::Cuboid:
        return builder.createCuboid(bounds, texture).transform(single);
    case DrawShapeKind::Cylinder:
        return p.hollow() ? builder.createHollowCylinder(bounds, p.thickness(), p.circleShape(), axis, texture)
                          : builder.createCylinder(bounds, p.circleShape(), axis, texture).transform(single);
    case DrawShapeKind::Cone:
        return builder.createCone(bounds, p.circleShape(), axis, texture).transform(single);
    case DrawShapeKind::UVSphere:
        return builder.createUvSphere(bounds, p.circleShape(), p.numRings(), axis, texture).transform(single);
    case DrawShapeKind::IcoSphere:
        return builder.createIcoSphere(bounds, p.accuracy(), texture).transform(single);
    case DrawShapeKind::Arch:
        return builder.createArch(bounds, p.thickness(), p.circleShape(), axis, texture).and_then([&](auto arch) -> Result<std::vector<Model::Brush>> {
            if (!p.createSpandrel()) return arch;
            return builder.createSpandrelForArch(bounds, p.thickness(), p.circleShape(), axis, texture).transform([&](auto fill) {
                arch.insert(arch.end(), std::make_move_iterator(fill.begin()), std::make_move_iterator(fill.end()));
                return arch;
            });
        });
    case DrawShapeKind::Rock:
        return builder.createRockFormation(bounds, p.rockType(), p.rockDetail(), p.rockSeed(), texture);
    case DrawShapeKind::Stairs: {
        if (bounds.size().x() <= 0 || bounds.size().y() <= 0 || bounds.size().z() <= 0) return std::vector<Model::Brush>{};
        const auto height = std::max(1.0, p.stepHeight());
        const auto count = std::max(size_t{1}, size_t(std::ceil(bounds.size().z() / height)));
        if (count > 4096) return Error{"Too many stair steps; increase the step height"};
        const auto direction = p.stairDirection();
        const auto runAxis = (direction == DrawShapeToolParameters::StairDirection::PosY || direction == DrawShapeToolParameters::StairDirection::NegY) ? 1u : 0u;
        const auto positive = direction == DrawShapeToolParameters::StairDirection::PosX || direction == DrawShapeToolParameters::StairDirection::PosY;
        const auto depth = bounds.size()[runAxis] / double(count);
        auto brushes = std::vector<Model::Brush>{};
        for (size_t i = 0; i < count; ++i) {
            auto step = bounds;
            step.max[2] = std::min(bounds.min.z() + height * double(i + 1), bounds.max.z());
            step.min[runAxis] = positive ? bounds.min[runAxis] + depth * double(i) : bounds.max[runAxis] - depth * double(i + 1);
            step.max[runAxis] = positive ? bounds.min[runAxis] + depth * double(i + 1) : bounds.max[runAxis] - depth * double(i);
            auto result = builder.createCuboid(step, texture);
            if (result.is_error()) return std::get<Error>(result.error());
            brushes.push_back(std::move(result).value());
        }
        return brushes;
    }
    }
    return Error{"Unknown brush shape"};
}
} // namespace TrenchBroom::View
