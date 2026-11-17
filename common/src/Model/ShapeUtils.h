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
#include "FloatType.h"
#include "vm/bbox.h"
#include <algorithm>
#include <iterator>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// Small adapters for the shape generators, retaining the fork's C++20/KDL API.
namespace TrenchBroom::Model::shape_detail {
struct ToVector {
    template <std::ranges::input_range R> friend auto operator|(R&& range, ToVector) {
        auto result = std::vector<std::ranges::range_value_t<R>>{};
        if constexpr (std::ranges::sized_range<R>) result.reserve(std::ranges::size(range));
        for (auto&& value : range)
            result.emplace_back(std::forward<decltype(value)>(value));
        return result;
    }
};
inline constexpr ToVector toVector;
template <typename R> auto bounds(R&& points) {
    vm::bbox3::builder builder;
    for (const auto& point : points)
        builder.add(point);
    return builder.bounds();
}
inline auto xy(const vm::bbox3& bounds) {
    return vm::bbox2{bounds.min.xy(), bounds.max.xy()};
}
template <typename First, typename... Rest> auto concat(First&& first, Rest&&... rest) {
    using T = std::ranges::range_value_t<First>;
    auto result = std::vector<T>{};
    std::ranges::copy(first, std::back_inserter(result));
    (std::ranges::copy(rest, std::back_inserter(result)), ...);
    return result;
}
template <typename A, typename B> auto cartesianProduct(A&& a, B&& b) {
    auto result = std::vector<std::tuple<std::ranges::range_value_t<A>, std::ranges::range_value_t<B>>>{};
    for (const auto& x : a)
        for (const auto& y : b)
            result.emplace_back(x, y);
    return result;
}
template <typename F> auto unpack(F f) {
    return [f = std::move(f)](const auto& value) { return std::apply(f, value); };
}
template <typename R, typename P> std::optional<size_t> indexOf(R&& range, P predicate) {
    size_t index = 0;
    for (const auto& value : range) {
        if (predicate(value)) return index;
        ++index;
    }
    return std::nullopt;
}
} // namespace TrenchBroom::Model::shape_detail
