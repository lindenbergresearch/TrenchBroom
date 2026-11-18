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

#include "Model/CircleShape.h"

#include <algorithm>
#include <cmath>

namespace TrenchBroom::Model {
namespace {

    auto numSidesToPrecision(const size_t numSides) { return size_t(std::max(0.0, std::ceil(std::log2(double(numSides) / 12.0)))); }

    auto precisionToNumSides(const size_t precision) { return size_t(std::pow(2.0, precision) * 12); }

} // namespace

EdgeAlignedCircle::EdgeAlignedCircle(const VertexAlignedCircle& circleShape) : EdgeAlignedCircle{circleShape.numSides} {}

EdgeAlignedCircle::EdgeAlignedCircle(const ScalableCircle& circleShape) : numSides{precisionToNumSides(circleShape.precision)} {}

VertexAlignedCircle::VertexAlignedCircle(const EdgeAlignedCircle& circleShape) : VertexAlignedCircle{circleShape.numSides} {}

VertexAlignedCircle::VertexAlignedCircle(const ScalableCircle& circleShape) : numSides{precisionToNumSides(circleShape.precision)} {}

ScalableCircle::ScalableCircle(const VertexAlignedCircle& circleShape) : precision{numSidesToPrecision(circleShape.numSides)} {}

ScalableCircle::ScalableCircle(const EdgeAlignedCircle& circleShape) : precision{numSidesToPrecision(circleShape.numSides)} {}

} // namespace TrenchBroom::Model
