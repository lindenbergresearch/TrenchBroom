/*
 Copyright (C) 2024 Kristian Duske

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
#include <cstddef>
#include <variant>

namespace TrenchBroom::Model {
struct VertexAlignedCircle;
struct ScalableCircle;
struct EdgeAlignedCircle {
    size_t numSides = 8;
    EdgeAlignedCircle() = default;
    explicit EdgeAlignedCircle(size_t n) : numSides(n) {}
    explicit EdgeAlignedCircle(const VertexAlignedCircle&);
    explicit EdgeAlignedCircle(const ScalableCircle&);
    bool operator==(const EdgeAlignedCircle&) const = default;
};
struct VertexAlignedCircle {
    size_t numSides = 8;
    VertexAlignedCircle() = default;
    explicit VertexAlignedCircle(size_t n) : numSides(n) {}
    explicit VertexAlignedCircle(const EdgeAlignedCircle&);
    explicit VertexAlignedCircle(const ScalableCircle&);
    bool operator==(const VertexAlignedCircle&) const = default;
};
struct ScalableCircle {
    size_t precision = 0;
    ScalableCircle() = default;
    explicit ScalableCircle(size_t p) : precision(p) {}
    explicit ScalableCircle(const EdgeAlignedCircle&);
    explicit ScalableCircle(const VertexAlignedCircle&);
    bool operator==(const ScalableCircle&) const = default;
};
using CircleShape = std::variant<EdgeAlignedCircle, VertexAlignedCircle, ScalableCircle>;
template <typename To> To convertCircleShape(const CircleShape& from) {
    return std::visit([](const auto& shape) { return To{shape}; }, from);
}
} // namespace TrenchBroom::Model
