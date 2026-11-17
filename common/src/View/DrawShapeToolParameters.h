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
#include "Model/CircleShape.h"
#include "Model/RockFormation.h"
#include "Notifier.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vm/util.h>

namespace TrenchBroom::View {
class DrawShapeToolParameters {
  public:
    enum class StairDirection { PosX, NegX, PosY, NegY };
    static constexpr uint32_t MaxRockSeed = uint32_t(std::numeric_limits<int>::max());
    Notifier<> parametersDidChangeNotifier;

  private:
    std::optional<vm::axis::type> m_axis;
    Model::CircleShape m_circle = Model::EdgeAlignedCircle{8};
    bool m_hollow = false;
    double m_thickness = 16;
    bool m_spandrel = false;
    size_t m_rings = 8;
    size_t m_accuracy = 1;
    double m_stepHeight = 16;
    StairDirection m_stairDirection = StairDirection::PosX;
    Model::RockType m_rockType = Model::RockType::Boulder;
    double m_rockDetail = 0.5;
    uint32_t m_rockSeed = 0;
    bool m_incrementSeed = false;
    template <typename T> void assign(T& field, T value) {
        if (field != value) {
            field = std::move(value);
            parametersDidChangeNotifier();
        }
    }

  public:
    auto axis() const { return m_axis; }
    void setAxis(std::optional<vm::axis::type> value) {
        if (!value || size_t(*value) < 3) assign(m_axis, value);
    }
    vm::axis::type resolvedAxis(vm::axis::type viewAxis) const { return m_axis.value_or(viewAxis); }
    const auto& circleShape() const { return m_circle; }
    void setCircleShape(Model::CircleShape value) {
        value = std::visit(
            [](auto shape) -> Model::CircleShape {
                if constexpr (requires { shape.numSides; })
                    shape.numSides = std::clamp(shape.numSides, size_t{3}, size_t{256});
                else
                    shape.precision = std::min(shape.precision, size_t{3});
                return shape;
            },
            value);
        assign(m_circle, std::move(value));
    }
    bool hollow() const { return m_hollow; }
    void setHollow(bool value) { assign(m_hollow, value); }
    double thickness() const { return m_thickness; }
    void setThickness(double value) {
        if (std::isfinite(value) && value > 0) assign(m_thickness, value);
    }
    bool createSpandrel() const { return m_spandrel; }
    void setCreateSpandrel(bool value) { assign(m_spandrel, value); }
    size_t numRings() const { return m_rings; }
    void setNumRings(size_t value) { assign(m_rings, std::clamp(value, size_t{1}, size_t{256})); }
    size_t accuracy() const { return m_accuracy; }
    void setAccuracy(size_t value) { assign(m_accuracy, std::clamp(value, size_t{1}, size_t{4})); }
    double stepHeight() const { return m_stepHeight; }
    void setStepHeight(double value) {
        if (std::isfinite(value) && value > 0) assign(m_stepHeight, value);
    }
    StairDirection stairDirection() const { return m_stairDirection; }
    void setStairDirection(StairDirection value) {
        if (size_t(value) < 4) assign(m_stairDirection, value);
    }
    Model::RockType rockType() const { return m_rockType; }
    void setRockType(Model::RockType value) {
        if (size_t(value) < Model::RockTypes.size()) assign(m_rockType, value);
    }
    double rockDetail() const { return m_rockDetail; }
    void setRockDetail(double value) {
        if (std::isfinite(value)) assign(m_rockDetail, std::clamp(value, 0.0, 1.0));
    }
    uint32_t rockSeed() const { return m_rockSeed; }
    void setRockSeed(uint32_t value) { assign(m_rockSeed, std::min(value, MaxRockSeed)); }
    bool incrementRockSeed() const { return m_incrementSeed; }
    void setIncrementRockSeed(bool value) { assign(m_incrementSeed, value); }
};
} // namespace TrenchBroom::View
