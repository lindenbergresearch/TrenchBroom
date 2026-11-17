/*
 Copyright (C) 2010 Kristian Duske

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

#include "Model/BrushBuilder.h"

#include "Error.h"
#include "Model/Brush.h"
#include "Model/BrushFace.h"
#include "Model/Polyhedron.h"
#include "Model/ShapeUtils.h"
#include "Renderer/RenderUtils.h"
#include "kdl/overload.h"
#include "kdl/range_to_vector.h"
#include "kdl/result.h"
#include "kdl/result_fold.h"
#include "kdl/vector_utils.h"
#include <cassert>

#include "vm/intersection.h"
#include "vm/line.h"
#include "vm/mat.h"
#include "vm/mat_ext.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <ranges>
#include <string>
#include <utility>

namespace TrenchBroom::Model {
namespace {

    // Cardinal shape axes are permutations/sign changes. Remove trigonometric
    // roundoff so otherwise coplanar grid points stay exactly coplanar.
    vm::mat4x4 axisRotation(const vm::vec3& from, const vm::vec3& to) {
        auto result = vm::rotation_matrix(from, to);
        for (size_t c = 0; c < 4; ++c)
            for (size_t r = 0; r < 4; ++r)
                result[c][r] = std::round(result[c][r]);
        return result;
    }

    bool finiteBounds(const vm::bbox3& bounds) {
        for (size_t i = 0; i < 3; ++i)
            if (!std::isfinite(bounds.min[i]) || !std::isfinite(bounds.max[i])) return false;
        return true;
    }
    bool solidBounds(const vm::bbox3& bounds) { return finiteBounds(bounds) && bounds.size().x() > 0 && bounds.size().y() > 0 && bounds.size().z() > 0; }
    bool validCircle(const CircleShape& circle) {
        return std::visit(
            [](const auto& shape) {
                if constexpr (requires { shape.numSides; })
                    return shape.numSides >= 3 && shape.numSides <= 256;
                else
                    return shape.precision <= 3;
            },
            circle);
    }
    CircleShape legacyCircle(size_t sides, RadiusMode mode) {
        if (mode == RadiusMode::ToEdge) return EdgeAlignedCircle{sides};
        return VertexAlignedCircle{sides};
    }

    auto makeEdgeAlignedCircle(const size_t numSides, const vm::bbox2d& bounds) {
        assert(numSides > 2);

        const auto transform = vm::translation_matrix(bounds.min) * vm::scaling_matrix(bounds.size()) * vm::translation_matrix(vm::vec2d{0.5, 0.5}) * vm::scaling_matrix(vm::vec2d{0.5, 0.5});

        auto vertices = std::vector<vm::vec2d>{};
        for (size_t i = 0; i < numSides; ++i) {
            const auto angle = (double(i) + 0.5) * vm::Cd::two_pi() / double(numSides) - vm::Cd::half_pi();
            const auto a = vm::Cd::pi() / double(numSides); // Half angle
            const auto ca = std::cos(a);
            const auto x = std::cos(angle) / ca;
            const auto y = std::sin(angle) / ca;
            vertices.emplace_back(x, y);
        }
        return transform * vertices;
    }

    auto makeVertexAlignedCircle(const size_t numSides, const vm::bbox2d& bounds) {
        assert(numSides > 2);

        const auto transform = vm::translation_matrix(bounds.min) * vm::scaling_matrix(bounds.size()) * vm::translation_matrix(vm::vec2d{0.5, 0.5}) * vm::scaling_matrix(vm::vec2d{0.5, 0.5});

        auto vertices = std::vector<vm::vec2d>{};
        for (size_t i = 0; i < numSides; ++i) {
            const auto angle = double(i) * vm::Cd::two_pi() / double(numSides) - vm::Cd::half_pi();
            const auto x = std::cos(angle);
            const auto y = std::sin(angle);
            vertices.emplace_back(x, y);
        }
        return transform * vertices;
    }

    auto makeScalableCircle(const size_t precision, const vm::bbox2d& bounds) {
        auto vertices = std::vector<vm::vec2d>{
            {-0.25, +1.00}, {-0.75, +0.75}, {-1.00, +0.25}, {-1.00, -0.25}, {-0.75, -0.75}, {-0.25, -1.00},
            {+0.25, -1.00}, {+0.75, -0.75}, {+1.00, -0.25}, {+1.00, +0.25}, {+0.75, +0.75}, {+0.25, +1.00},
        };

        // Clip off each corner to get a scalable unit circle with double the vertices
        for (size_t i = 0; i < precision; ++i) {

            const auto previousVertices = std::exchange(vertices, std::vector<vm::vec2d>{});
            const auto count = previousVertices.size();
            for (size_t j = 0; j < previousVertices.size(); ++j) {
                const auto prev = previousVertices[(j + count - 1) % count];
                const auto cur = previousVertices[j];
                const auto next = previousVertices[(j + 1) % count];

                vertices.push_back(prev + (cur - prev) * 0.75);
                vertices.push_back(cur + (next - cur) * 0.25);
            }
        }

        const auto size = bounds.size();
        const auto minSize = vm::min(size.x(), size.y());
        const auto squareSize = vm::vec2d::fill(minSize);

        vertices = vm::scaling_matrix(squareSize) * vm::translation_matrix(vm::vec2d{0.5, 0.5}) * vm::scaling_matrix(vm::vec2d{0.5, 0.5}) * vertices;

        // Stretch the circle to fit the bounds by moving the right half and the top half
        // instead of uniformly scaling all vertices
        const auto offset = vm::vec2d{
            vm::max(size.x() - size.y(), 0.0),
            vm::max(size.y() - size.x(), 0.0),
        };

        for (auto& v : vertices) {
            if (v.x() > minSize / 2.0) {
                v = vm::vec2d{v.x() + offset.x(), v.y()};
            }
            if (v.y() > minSize / 2.0) {
                v = vm::vec2d{v.x(), v.y() + offset.y()};
            }
        }

        return vm::translation_matrix(bounds.min) * vertices;
    }

    auto makeCircle(const CircleShape& circleShape, const vm::bbox2d& bounds) {

        return std::visit(kdl::overload([&](const EdgeAlignedCircle& edgeAligned) { return makeEdgeAlignedCircle(edgeAligned.numSides, bounds); },
                                        [&](const VertexAlignedCircle& vertexAligned) { return makeVertexAlignedCircle(vertexAligned.numSides, bounds); },
                                        [&](const ScalableCircle& scalable) { return makeScalableCircle(scalable.precision, bounds); }),
                          circleShape);
    }

    auto makeCylinder(const CircleShape& circleShape, const vm::bbox3d& boundsXY) {
        auto vertices = std::vector<vm::vec3d>{};
        for (const auto& v : makeCircle(circleShape, shape_detail::xy(boundsXY))) {
            vertices.emplace_back(v.x(), v.y(), boundsXY.min.z());
            vertices.emplace_back(v.x(), v.y(), boundsXY.max.z());
        }
        return vertices;
    }

    auto makeVerticesForWedges(const std::vector<vm::vec2d>& outerCircle, const vm::bbox2d& bounds) {
        // The bounds are too small to create an inner circle, but we can still create
        // wedges for the hollow cylinder.
        // Generate four points (here called corners) where the wedges should meet.
        // If the bounds are square, all corners coincide. If the bounds are
        // rectangular, two pairs of corners coincide.
        // Then map each vertex of the outer circle to the closest corner.
        const auto offset = vm::min(bounds.size().x(), bounds.size().y()) / 2.0;
        const auto corners = std::vector<vm::vec2d>{
            {bounds.min.x() + offset, bounds.min.y() + offset},
            {bounds.min.x() + offset, bounds.max.y() - offset},
            {bounds.max.x() - offset, bounds.min.y() + offset},
            {bounds.max.x() - offset, bounds.max.y() - offset},
        };
        return outerCircle | std::views::transform([&](const auto& v) {
                   return *std::ranges::min_element(corners, [&](const auto& a, const auto& b) { return vm::squared_distance(v, a) < vm::squared_distance(v, b); });
               }) |
               shape_detail::toVector;
    }

    auto makeHollowCylinderInnerCircle(const std::vector<vm::vec2d>& outerCircle, const double thickness, const CircleShape& circleShape, const vm::bbox2d& bounds) {
        if (bounds.size().x() <= thickness * 2.0 || bounds.size().y() <= thickness * 2.0) {
            return Result<std::vector<vm::vec2d>>{makeVerticesForWedges(outerCircle, bounds)};
        }

        return std::visit(kdl::overload(
                              [&](const ScalableCircle& scalable) -> Result<std::vector<vm::vec2d>> {
                                  const auto delta = vm::vec2d{thickness, thickness};
                                  const auto innerBounds = vm::bbox2d{bounds.min + delta, bounds.max - delta};
                                  return makeScalableCircle(scalable.precision, innerBounds);
                              },
                              [&](const auto& axisOrVertexAligned) -> Result<std::vector<vm::vec2d>> {
                                  const auto numSides = axisOrVertexAligned.numSides;
                                  auto outerLines = std::vector<vm::line2>{};
                                  outerLines.reserve(numSides);
                                  for (size_t i = 0; i < numSides; ++i) {
                                      const auto p1 = outerCircle[i];
                                      const auto p2 = outerCircle[(i + 1) % numSides];
                                      outerLines.emplace_back(p1, vm::normalize(p2 - p1));
                                  }

                                  const auto innerLines = outerLines | std::views::transform([&](const auto& l) {
                                                              const auto offsetDir = vm::vec2d{-l.direction.y(), l.direction.x()};
                                                              return vm::line2{l.point + offsetDir * thickness, l.direction};
                                                          }) |
                                                          shape_detail::toVector;

                                  auto innerCircle = std::vector<vm::vec2d>{};
                                  innerCircle.reserve(numSides);
                                  for (size_t i = 0; i < numSides; ++i) {
                                      const auto l1 = innerLines[(i + numSides - 1) % numSides];
                                      const auto l2 = innerLines[i];
                                      const auto d = vm::intersect_line_line(l1, l2);
                                      if (!d) {
                                          return Error{"Failed to intersect lines"};
                                      }

                                      innerCircle.push_back(vm::point_at_distance(l1, *d));
                                  }

                                  return innerCircle;
                              }),
                          circleShape);
    }

    auto makeHollowCylinderFragmentVertices(const std::vector<vm::vec2d>& outerCircle, const std::vector<vm::vec2d>& innerCircle, const size_t i, const vm::bbox3d& boundsXY) {
        assert(outerCircle.size() == innerCircle.size());

        const auto numSides = outerCircle.size();

        const auto po = outerCircle[(i + 0) % numSides];
        const auto pi = innerCircle[(i + 0) % numSides];
        const auto no = outerCircle[(i + 1) % numSides];
        const auto ni = innerCircle[(i + 1) % numSides];

        const auto brushVertices = std::vector<vm::vec3d>{
            {po, boundsXY.min.z()}, {po, boundsXY.max.z()}, {pi, boundsXY.min.z()}, {pi, boundsXY.max.z()},
            {no, boundsXY.min.z()}, {no, boundsXY.max.z()}, {ni, boundsXY.min.z()}, {ni, boundsXY.max.z()},
        };

        return brushVertices;
    }

    // Maps the tunnel (extrusion) axis to the span and vertical axes of the arch's
    // cross-section. Arches rise along world Z where possible so they stand upright.
    struct ArchAxes {
        size_t tunnel;
        size_t span;
        size_t vertical;
    };

    ArchAxes archAxes(const vm::axis::type axis) {
        switch (axis) {
        case vm::axis::x:
            return {vm::axis::x, vm::axis::y, vm::axis::z};
        case vm::axis::y:
            return {vm::axis::y, vm::axis::x, vm::axis::z};
        default: // vm::axis::z
            return {vm::axis::z, vm::axis::x, vm::axis::y};
        }
    }

    // Interpolates the point on segment a->b at vertical coordinate v.
    vm::vec2d crossingAtV(const vm::vec2d& a, const vm::vec2d& b, const double v) {
        const auto t = (v - a.y()) / (b.y() - a.y());
        return vm::vec2d{a.x() + (b.x() - a.x()) * t, v};
    }

    // Returns the normal of segment a->b that points away from the given center.
    vm::vec2d outwardNormal(const vm::vec2d& a, const vm::vec2d& b, const vm::vec2d& center) {
        const auto d = b - a;
        const auto normal = vm::vec2d{-d.y(), d.x()};
        return vm::dot(normal, (a + b) / 2.0 - center) < 0.0 ? -normal : normal;
    }

    // Indices of the contiguous run of circle vertices at or above the springing line.
    std::vector<size_t> archUpperRun(const std::vector<vm::vec2d>& circle, const double vMin) {
        const auto n = circle.size();
        const auto isUpper = [&](const size_t i) { return circle[i].y() >= vMin; };

        // Start of the contiguous upper run: an upper vertex whose predecessor is below.
        const auto firstUpper = shape_detail::indexOf(std::views::iota(0u, n), [&](const auto i) { return isUpper(i) && !isUpper((i + n - 1) % n); });

        if (!firstUpper) {
            return {};
        }

        return std::views::iota(0u, n) | std::views::transform([&](const auto i) { return (*firstUpper + i) % n; }) | std::views::take_while(isUpper) | shape_detail::toVector;
    }

    // Walks the given run along the given circle, capping both ends with a foot on the
    // springing line so that the arch sits flat.
    std::vector<vm::vec2d> archBoundary(const std::vector<vm::vec2d>& circle, const std::vector<size_t>& upper, const double vMin) {
        const auto n = circle.size();
        const auto first = upper.front();
        const auto last = upper.back();
        const auto beforeFirst = (first + n - 1) % n;
        const auto afterLast = (last + 1) % n;

        auto boundary = std::vector<vm::vec2d>{};
        boundary.reserve(upper.size() + 2);
        boundary.push_back(crossingAtV(circle[beforeFirst], circle[first], vMin));
        for (const auto i : upper) {
            boundary.emplace_back(circle[i].x(), std::max(circle[i].y(), vMin));
        }
        boundary.push_back(crossingAtV(circle[last], circle[afterLast], vMin));
        return boundary;
    }

    // The cross-section an arch is built from: the axes it is laid out on, the circle its
    // band follows and the boundary of its outside, running from one foot to the other.
    struct ArchCrossSection {
        ArchAxes axes;
        vm::bbox2d circleBounds;
        std::vector<vm::vec2d> circle;
        std::vector<size_t> upper;
        std::vector<vm::vec2d> outerBoundary;
    };

    std::optional<ArchCrossSection> makeArchCrossSection(const vm::bbox3d& bounds, const CircleShape& circleShape, const vm::axis::type axis) {
        const auto axes = archAxes(axis);

        const auto sMin = bounds.min[axes.span];
        const auto sMax = bounds.max[axes.span];
        const auto vMin = bounds.min[axes.vertical];
        const auto vMax = bounds.max[axes.vertical];
        const auto height = vMax - vMin;
        const auto span = sMax - sMin;

        // The bounds are often degenerate mid-drag; emit no brushes rather than erroring.
        if (height <= 0.0 || span <= 0.0) {
            return std::nullopt;
        }

        // Upper half of an ellipse whose diameter lies on the springing line (v == vMin). Build
        // the full ellipse in a bounds doubled downwards, then keep only the upper half,
        // reusing the cylinder circle-mode machinery.
        const auto circleBounds = vm::bbox2d{{sMin, vMin - height}, {sMax, vMax}};

        auto circle = makeCircle(circleShape, circleBounds);
        auto upper = archUpperRun(circle, vMin);
        if (upper.empty()) {
            return std::nullopt;
        }

        auto outerBoundary = archBoundary(circle, upper, vMin);
        return ArchCrossSection{axes, circleBounds, std::move(circle), std::move(upper), std::move(outerBoundary)};
    }

    vm::vec3d archPoint(const ArchAxes& axes, const vm::vec2d& p, const double w) {
        auto result = vm::vec3d{};
        result[axes.span] = p.x();
        result[axes.vertical] = p.y();
        result[axes.tunnel] = w;
        return result;
    }

    auto setZ(const std::vector<vm::vec2d>& vertices, const double z) {
        return vertices | std::views::transform([&](const auto& v) { return vm::vec3d{v, z}; }) | shape_detail::toVector;
    }

    /** If a scalable cone is stretched, it doesn't have one vertex as the tip. Instead,
     * the tip is an edge.
     */
    auto makeScalableConeTip(const vm::bbox3d& boundsXY) {
        const auto offset = vm::min(shape_detail::xy(boundsXY).size().x(), shape_detail::xy(boundsXY).size().y()) / 2.0;
        return kdl::vec_sort_and_remove_duplicates(std::vector<vm::vec2d>{
            {shape_detail::xy(boundsXY).min.x() + offset, shape_detail::xy(boundsXY).min.y() + offset},
            {shape_detail::xy(boundsXY).min.x() + offset, shape_detail::xy(boundsXY).max.y() - offset},
            {shape_detail::xy(boundsXY).max.x() - offset, shape_detail::xy(boundsXY).min.y() + offset},
            {shape_detail::xy(boundsXY).max.x() - offset, shape_detail::xy(boundsXY).max.y() - offset},
        });
    }

    auto makeCone(const CircleShape& circleShape, const vm::bbox3d& boundsXY) {
        return std::visit(
            kdl::overload(
                [&](const ScalableCircle& scalableCircle) {
                    return shape_detail::concat(setZ(makeScalableCircle(scalableCircle.precision, shape_detail::xy(boundsXY)), boundsXY.min.z()),
                                                setZ(makeScalableConeTip(boundsXY), boundsXY.max.z())) |
                           shape_detail::toVector;
                },
                [&](const auto&) {
                    return kdl::vec_push_back(setZ(makeCircle(circleShape, shape_detail::xy(boundsXY)), boundsXY.min.z()), vm::vec3d{shape_detail::xy(boundsXY).center(), boundsXY.max.z()});
                }),
            circleShape);
    }

    auto subDivideRatios(const std::vector<double>& ratios) {
        auto newRatios = std::vector<double>{};
        newRatios.push_back(ratios.front());
        for (size_t j = 1; j < ratios.size(); ++j) {
            const auto previousSize = ratios[j - 1];
            const auto currentSize = ratios[j];
            newRatios.push_back((previousSize + currentSize) / 2.0);
        }
        newRatios.push_back(ratios.back());
        return newRatios;
    }

    auto makeSizeRatiosPerRing(const size_t precision) {
        auto sizeRatios = std::vector<double>{0.0, 1.0 / 2.0, 7.0 / 8.0, 1.0};
        for (size_t i = 0; i < precision; ++i) {
            sizeRatios = subDivideRatios(sizeRatios);
        }

        const auto n = sizeRatios.size();
        for (size_t i = 0; i < n - 1; ++i) {
            sizeRatios.push_back(sizeRatios[n - i - 2]);
        }

        return sizeRatios;
    }

    auto makeZRatiosPerRing(const size_t precision) {
        auto zRatios = std::vector<double>{1.0, 7.0 / 8.0, 1.0 / 2.0, 0.0};
        for (size_t i = 0; i < precision; ++i) {
            zRatios = subDivideRatios(zRatios);
        }

        const auto n = zRatios.size();
        for (size_t i = 0; i < n - 1; ++i) {
            zRatios.push_back(-zRatios[n - i - 2]);
        }

        return zRatios;
    }

    auto makeScalableUvSphere(const vm::bbox3d& boundsXY, const size_t precision) {
        const auto zRatios = makeZRatiosPerRing(precision);
        const auto getZ = [&](const size_t i) {
            const auto center = boundsXY.center();
            const auto size = boundsXY.size() / 2.0;
            return center.z() + size.z() * zRatios[i];
        };

        const auto sizeRatios = makeSizeRatiosPerRing(precision);
        const auto getBounds = [&](const size_t i) {
            const auto s = vm::min(boundsXY.size().x(), boundsXY.size().y()) / 2.0;
            return shape_detail::xy(boundsXY).expand(-s * (1 - sizeRatios[i]));
        };

        // Each profile subdivision adds a sample; it does not double the ring count.
        // Derive it from the profile to avoid reading past the end at precision > 0.
        const auto numRings = zRatios.size() - 2;

        auto vertices = std::vector<vm::vec3d>{};
        vertices = kdl::vec_concat(std::move(vertices), setZ(makeScalableConeTip(boundsXY), getZ(0)));
        for (size_t i = 1; i <= numRings; ++i) {
            vertices = kdl::vec_concat(std::move(vertices), setZ(makeScalableCircle(precision, getBounds(i)), getZ(i)));
        }
        vertices = kdl::vec_concat(std::move(vertices), setZ(makeScalableConeTip(boundsXY), getZ(numRings + 1)));

        return vertices;
    }

    auto makeRing(const double angle, const CircleShape& circleShape, const vm::bbox3d& boundsXY) {
        const auto r = std::sin(angle);
        const auto z = boundsXY.center().z() + std::cos(angle) * boundsXY.size().z() / 2.0;
        const auto t = vm::translation_matrix(shape_detail::xy(boundsXY).center()) * vm::scaling_matrix(vm::vec2d{r, r}) * vm::translation_matrix(-shape_detail::xy(boundsXY).center());
        const auto circle = t * makeCircle(circleShape, shape_detail::xy(boundsXY));
        return circle | std::views::transform([&](const auto& v) { return vm::vec3d{v, z}; }) | shape_detail::toVector;
    }

    auto makeAlignedUvSphere(const vm::bbox3d& boundsXY, const CircleShape& circleShape, const size_t numRings) {
        const auto angleDelta = vm::Cd::pi() / (double(numRings) + 1.0);

        auto vertices = std::vector<vm::vec3d>{};
        vertices.emplace_back(shape_detail::xy(boundsXY).center(), boundsXY.max.z());

        for (size_t i = 0; i < numRings; ++i) {
            vertices = kdl::vec_concat(std::move(vertices), makeRing(double(i + 1) * angleDelta, circleShape, boundsXY));
        }

        vertices.emplace_back(shape_detail::xy(boundsXY).center(), boundsXY.min.z());

        // ensure that the sphere fills the bounds when number of rings is equal
        const auto centerRingRadius = std::sin(angleDelta * double(numRings / 2));
        const auto extraScale = numRings % 2 == 0 ? 1.0 / centerRingRadius : 1.0;
        const auto transform = vm::translation_matrix(boundsXY.center()) * vm::scaling_matrix(vm::vec3d{extraScale, extraScale, 1.0}) * vm::translation_matrix(-boundsXY.center());

        return transform * vertices;
    }

} // namespace

BrushBuilder::BrushBuilder(MapFormat format, const vm::bbox3& worldBounds) : m_mapFormat(format), m_worldBounds(worldBounds), m_defaultAttribs(BrushFaceAttributes::NoTextureName) {}
BrushBuilder::BrushBuilder(MapFormat format, const vm::bbox3& worldBounds, BrushFaceAttributes attributes)
    : m_mapFormat(format), m_worldBounds(worldBounds), m_defaultAttribs(std::move(attributes)) {}

Result<Brush> BrushBuilder::createCube(const double size, const std::string& materialName) const {
    return createCuboid(vm::bbox3d{size / 2.0}, materialName, materialName, materialName, materialName, materialName, materialName);
}

Result<Brush> BrushBuilder::createCube(double size, const std::string& leftMaterial, const std::string& rightMaterial, const std::string& frontMaterial, const std::string& backMaterial,
                                       const std::string& topMaterial, const std::string& bottomMaterial) const {
    return createCuboid(vm::bbox3d{size / 2.0}, leftMaterial, rightMaterial, frontMaterial, backMaterial, topMaterial, bottomMaterial);
}

Result<Brush> BrushBuilder::createCuboid(const vm::vec3d& size, const std::string& materialName) const {
    return createCuboid(vm::bbox3d{-size / 2.0, size / 2.0}, materialName, materialName, materialName, materialName, materialName, materialName);
}

Result<Brush> BrushBuilder::createCuboid(const vm::vec3d& size, const std::string& leftMaterial, const std::string& rightMaterial, const std::string& frontMaterial,
                                         const std::string& backMaterial, const std::string& topMaterial, const std::string& bottomMaterial) const {
    return createCuboid(vm::bbox3d{-size / 2.0, size / 2.0}, leftMaterial, rightMaterial, frontMaterial, backMaterial, topMaterial, bottomMaterial);
}

Result<Brush> BrushBuilder::createCuboid(const vm::bbox3d& bounds, const std::string& materialName) const {
    return createCuboid(bounds, materialName, materialName, materialName, materialName, materialName, materialName);
}

Result<Brush> BrushBuilder::createCuboid(const vm::bbox3d& bounds, const std::string& leftMaterial, const std::string& rightMaterial, const std::string& frontMaterial,
                                         const std::string& backMaterial, const std::string& topMaterial, const std::string& bottomMaterial) const {
    return std::vector{
               BrushFace::create(bounds.min, bounds.min + vm::vec3d{0, 1, 0}, bounds.min + vm::vec3d{0, 0, 1}, BrushFaceAttributes{leftMaterial, m_defaultAttribs},
                                 m_mapFormat), // left
               BrushFace::create(bounds.max, bounds.max + vm::vec3d{0, 0, 1}, bounds.max + vm::vec3d{0, 1, 0}, BrushFaceAttributes{rightMaterial, m_defaultAttribs},
                                 m_mapFormat), // right
               BrushFace::create(bounds.min, bounds.min + vm::vec3d{0, 0, 1}, bounds.min + vm::vec3d{1, 0, 0}, BrushFaceAttributes{frontMaterial, m_defaultAttribs},
                                 m_mapFormat), // front
               BrushFace::create(bounds.max, bounds.max + vm::vec3d{1, 0, 0}, bounds.max + vm::vec3d{0, 0, 1}, BrushFaceAttributes{backMaterial, m_defaultAttribs},
                                 m_mapFormat), // back
               BrushFace::create(bounds.max, bounds.max + vm::vec3d{0, 1, 0}, bounds.max + vm::vec3d{1, 0, 0}, BrushFaceAttributes{topMaterial, m_defaultAttribs},
                                 m_mapFormat), // top
               BrushFace::create(bounds.min, bounds.min + vm::vec3d{1, 0, 0}, bounds.min + vm::vec3d{0, 1, 0}, BrushFaceAttributes{bottomMaterial, m_defaultAttribs},
                                 m_mapFormat), // bottom
           } |
           kdl::fold | kdl::and_then([&](auto faces) { return Brush::create(m_worldBounds, std::move(faces)); });
}

Result<Brush> BrushBuilder::createCylinder(const vm::bbox3d& bounds, const CircleShape& circleShape, const vm::axis::type axis, const std::string& textureName) const {
    if (!solidBounds(bounds) || !validCircle(circleShape)) return Error{"Invalid shape bounds or circle parameters"};

    const auto toXY = axisRotation(vm::vec3d::axis(axis), vm::vec3d{0, 0, 1});
    const auto fromXY = axisRotation(vm::vec3d{0, 0, 1}, vm::vec3d::axis(axis));

    const auto cylinder = makeCylinder(circleShape, bounds.transform(toXY));
    return createBrush(fromXY * cylinder, textureName);
}

Result<std::vector<Brush>> BrushBuilder::createHollowCylinder(const vm::bbox3d& bounds, const double thickness, const CircleShape& circleShape, const vm::axis::type axis,
                                                              const std::string& textureName) const {
    if (!std::isfinite(thickness) || thickness <= 0) return Error{"Wall thickness must be positive and finite"};

    if (!solidBounds(bounds) || !validCircle(circleShape)) return Error{"Invalid shape bounds or circle parameters"};

    const auto toXY = axisRotation(vm::vec3d::axis(axis), vm::vec3d{0, 0, 1});
    const auto fromXY = axisRotation(vm::vec3d{0, 0, 1}, vm::vec3d::axis(axis));
    const auto boundsXY = bounds.transform(toXY);

    const auto outerCircle = makeCircle(circleShape, shape_detail::xy(boundsXY));

    return makeHollowCylinderInnerCircle(outerCircle, thickness, circleShape, shape_detail::xy(boundsXY)).and_then([&](const auto& innerCircle) {
        assert(innerCircle.size() == outerCircle.size());

        const auto numFragments = outerCircle.size();

        auto brushes = std::vector<Result<Brush>>{};
        brushes.reserve(numFragments);

        for (size_t i = 0; i < numFragments; ++i) {
            const auto fragmentVertices = makeHollowCylinderFragmentVertices(outerCircle, innerCircle, i, boundsXY);
            const auto rotatedFragmentVertices = fromXY * fragmentVertices;

            brushes.push_back(createBrush(rotatedFragmentVertices, textureName));
        }

        return brushes | shape_detail::toVector | kdl::fold;
    });
}

Result<std::vector<Brush>> BrushBuilder::createArch(const vm::bbox3d& bounds, const double thickness, const CircleShape& circleShape, const vm::axis::type axis,
                                                    const std::string& textureName) const {
    if (!finiteBounds(bounds) || !validCircle(circleShape) || !std::isfinite(thickness) || thickness <= 0) return Error{"Invalid arch bounds, wall thickness or circle parameters"};

    const auto section = makeArchCrossSection(bounds, circleShape, axis);
    if (!section) {
        return Result<std::vector<Brush>>{std::vector<Brush>{}};
    }

    const auto& axes = section->axes;
    const auto vMin = bounds.min[axes.vertical];
    const auto wMin = bounds.min[axes.tunnel];
    const auto wMax = bounds.max[axes.tunnel];

    return makeHollowCylinderInnerCircle(section->circle, thickness, circleShape, section->circleBounds) | kdl::and_then([&](const auto& inner) {
               assert(inner.size() == section->circle.size());

               const auto& outerBoundary = section->outerBoundary;
               const auto innerBoundary = archBoundary(inner, section->upper, vMin);

               // Build each voussoir independently, skipping any that are degenerate
               // mid-drag.
               return std::views::iota(0u, outerBoundary.size() - 1) | std::views::transform([&](const auto j) {
                          const auto& o0 = outerBoundary[j];
                          const auto& o1 = outerBoundary[j + 1];
                          const auto& i0 = innerBoundary[j];
                          const auto& i1 = innerBoundary[j + 1];

                          return Polyhedron3{
                              archPoint(axes, o0, wMin), archPoint(axes, o0, wMax), archPoint(axes, o1, wMin), archPoint(axes, o1, wMax),
                              archPoint(axes, i0, wMin), archPoint(axes, i0, wMax), archPoint(axes, i1, wMin), archPoint(axes, i1, wMax),
                          };
                      }) |
                      std::views::filter([](const auto& p) { return p.polyhedron(); }) | std::views::transform([&](const auto& polyhedron) { return createBrush(polyhedron, textureName); }) |
                      shape_detail::toVector | kdl::fold;
           });
}

Result<std::vector<Brush>> BrushBuilder::createSpandrelForArch(const vm::bbox3d& bounds, const double thickness, const CircleShape& circleShape, const vm::axis::type axis,
                                                               const std::string& textureName) const {
    if (!finiteBounds(bounds) || !validCircle(circleShape) || !std::isfinite(thickness) || thickness <= 0) return Error{"Invalid arch bounds, wall thickness or circle parameters"};

    const auto section = makeArchCrossSection(bounds, circleShape, axis);
    if (!section) {
        return Result<std::vector<Brush>>{std::vector<Brush>{}};
    }

    const auto& axes = section->axes;
    const auto sMin = bounds.min[axes.span];
    const auto sMax = bounds.max[axes.span];
    const auto vMin = bounds.min[axes.vertical];
    const auto vMax = bounds.max[axes.vertical];
    const auto wMin = bounds.min[axes.tunnel];
    const auto wMax = bounds.max[axes.tunnel];

    const auto& outerBoundary = section->outerBoundary;
    const auto center = vm::vec2d{(sMin + sMax) / 2.0, vMin};

    // The outer boundary is convex and touches the top of the bounds, so fanning its
    // segments to the corner they face tiles the gap exactly. Segments that lie on the
    // bounds themselves fan into a degenerate brush and drop out.
    return std::views::iota(0u, outerBoundary.size() - 1) | std::views::transform([&](const auto j) {
               const auto& o0 = outerBoundary[j];
               const auto& o1 = outerBoundary[j + 1];
               const auto corner = vm::vec2d{outwardNormal(o0, o1, center).x() < 0.0 ? sMin : sMax, vMax};

               return Polyhedron3{
                   archPoint(axes, o0, wMin), archPoint(axes, o0, wMax), archPoint(axes, o1, wMin), archPoint(axes, o1, wMax), archPoint(axes, corner, wMin), archPoint(axes, corner, wMax),
               };
           }) |
           std::views::filter([](const auto& p) { return p.polyhedron(); }) | std::views::transform([&](const auto& polyhedron) { return createBrush(polyhedron, textureName); }) |
           shape_detail::toVector | kdl::fold;
}

Result<Brush> BrushBuilder::createCone(const vm::bbox3d& bounds, const CircleShape& circleShape, const vm::axis::type axis, const std::string& textureName) const {
    if (!solidBounds(bounds) || !validCircle(circleShape)) return Error{"Invalid shape bounds or circle parameters"};

    const auto toXY = axisRotation(vm::vec3d::axis(axis), vm::vec3d{0, 0, 1});
    const auto fromXY = axisRotation(vm::vec3d{0, 0, 1}, vm::vec3d::axis(axis));
    const auto boundsXY = bounds.transform(toXY);

    const auto cone = makeCone(circleShape, boundsXY);
    return createBrush(fromXY * cone, textureName);
}

Result<Brush> BrushBuilder::createUvSphere(const vm::bbox3d& bounds, const CircleShape& circleShape, const size_t numRings, const vm::axis::type axis, const std::string& textureName) const {
    if (numRings < 1 || numRings > 256) return Error{"Ring count must be between 1 and 256"};

    if (!solidBounds(bounds) || !validCircle(circleShape)) return Error{"Invalid shape bounds or circle parameters"};

    const auto fromXY = axisRotation(vm::vec3d{0, 0, 1}, vm::vec3d::axis(axis));
    const auto toXY = axisRotation(vm::vec3d::axis(axis), vm::vec3d{0, 0, 1});
    const auto boundsXY = bounds.transform(toXY);

    const auto sphere = std::visit(kdl::overload([&](const ScalableCircle& scalable) { return makeScalableUvSphere(boundsXY, scalable.precision); },
                                                 [&](const auto& edgeOrVertexAligned) { return makeAlignedUvSphere(boundsXY, edgeOrVertexAligned, numRings); }),
                                   circleShape);

    return createBrush(fromXY * sphere, textureName);
}

Result<Brush> BrushBuilder::createIcoSphere(const vm::bbox3d& bounds, const size_t iterations, const std::string& textureName) const {
    if (!solidBounds(bounds) || iterations < 1 || iterations > 4) return Error{"Ico sphere accuracy must be between 1 and 4 and bounds must have positive volume"};

    const auto [sphereVertices_, sphereIndices] = Renderer::sphereMesh<double>(iterations);

    return sphereIndices | std::views::transform([sphereVertices = sphereVertices_, &textureName, this](const auto& face) {
               const auto& p1 = sphereVertices[face[0]];
               const auto& p2 = sphereVertices[face[1]];
               const auto& p3 = sphereVertices[face[2]];
               return BrushFace::create(p1, p2, p3, BrushFaceAttributes{textureName, m_defaultAttribs}, m_mapFormat);
           }) |
           kdl::fold | kdl::and_then([&](auto f) { return Brush::create(m_worldBounds, std::move(f)); }) | kdl::and_then([&](auto b) {
               const auto transform =
                   vm::translation_matrix(bounds.min) * vm::scaling_matrix(bounds.size()) * vm::scaling_matrix(vm::vec3d{0.5, 0.5, 0.5}) * vm::translation_matrix(vm::vec3d{1, 1, 1});
               return b.transform(m_worldBounds, transform, false) | kdl::transform([&]() { return std::move(b); });
           });
}

Result<std::vector<Brush>> BrushBuilder::createRockFormation(const vm::bbox3d& bounds, const RockType type, const double detail, const uint32_t seed, const std::string& textureName) const {
    if (!finiteBounds(bounds) || !std::isfinite(detail)) return Error{"Invalid rock bounds or detail"};

    const auto formation = makeRockFormation(bounds, type, detail, seed);
    return formation | std::views::transform([](const auto& points) { return Polyhedron3{points}; }) | std::views::filter([](const auto& p) { return p.polyhedron(); }) |
           std::views::transform([&](const auto& polyhedron) { return createBrush(polyhedron, textureName); }) | shape_detail::toVector | kdl::fold;
}

Result<Brush> BrushBuilder::createBrush(const std::vector<vm::vec3d>& points, const std::string& materialName) const {
    return createBrush(Polyhedron3{points}, materialName);
}

Result<Brush> BrushBuilder::createBrush(const Polyhedron3& polyhedron, const std::string& materialName) const {
    if (!polyhedron.polyhedron() || !polyhedron.closed()) {
        return Error{"Cannot create brush from an open or degenerate polyhedron"};
    }

    return polyhedron.faces() | std::views::transform([&](const auto* face) {
               // A face's first edge depends on hull allocation/merging order. Use a
               // deterministic, wide triangle to avoid unstable planes on fine round shapes.
               const auto vertices = face->vertexPositions();
               const auto p1 = *std::min_element(vertices.begin(), vertices.end());
               auto p2 = *std::max_element(vertices.begin(), vertices.end(), [&](const auto& a, const auto& b) {
                   const auto x = vm::squared_length(a - p1), y = vm::squared_length(b - p1);
                   return x == y ? a < b : x < y;
               });
               auto p3 = *std::max_element(vertices.begin(), vertices.end(), [&](const auto& a, const auto& b) {
                   const auto x = vm::squared_length(vm::cross(p2 - p1, a - p1)), y = vm::squared_length(vm::cross(p2 - p1, b - p1));
                   return x == y ? a < b : x < y;
               });
               if (vm::dot(vm::cross(p3 - p1, p2 - p1), face->normal()) < 0) std::swap(p2, p3);

               return BrushFace::create(p1, p2, p3, BrushFaceAttributes{materialName, m_defaultAttribs}, m_mapFormat);
           }) |
           kdl::fold | kdl::and_then([&](auto faces) { return Brush::create(m_worldBounds, std::move(faces)); });
}

Result<Brush> BrushBuilder::createCylinder(const vm::bbox3& b, size_t sides, RadiusMode mode, vm::axis::type axis, const std::string& texture) const {
    return createCylinder(b, legacyCircle(sides, mode), axis, texture);
}
Result<std::vector<Brush>> BrushBuilder::createHollowCylinder(const vm::bbox3& b, double thickness, size_t sides, RadiusMode mode, vm::axis::type axis, const std::string& texture) const {
    return createHollowCylinder(b, thickness, legacyCircle(sides, mode), axis, texture);
}
Result<Brush> BrushBuilder::createCone(const vm::bbox3& b, size_t sides, RadiusMode mode, vm::axis::type axis, const std::string& texture) const {
    return createCone(b, legacyCircle(sides, mode), axis, texture);
}
Result<Brush> BrushBuilder::createUVSphere(const vm::bbox3& b, size_t sides, size_t rings, RadiusMode mode, vm::axis::type axis, const std::string& texture) const {
    return createUvSphere(b, legacyCircle(sides, mode), rings, axis, texture);
}
} // namespace TrenchBroom::Model
