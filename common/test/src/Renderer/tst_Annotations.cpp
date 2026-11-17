#include "Error.h"
#include "Renderer/LinkRenderer.h"
#include "Renderer/ScreenLabelLayout.h"
#include <catch2/catch.hpp>
#include <limits>

namespace TrenchBroom::Renderer {
TEST_CASE("Small-object dimensions and names are laid out together by priority", "[annotations]") {
    ScreenLabelLayout layout{{0, 0, 400, 240}};
    const auto name = layout.add({{170, 100, 125, 24}, 0, 100, true});
    const auto x = layout.add({{175, 100, 95, 24}, 30, 100, true});
    const auto y = layout.add({{177, 100, 95, 24}, 30, 100, true});
    const auto z = layout.add({{140, 106, 95, 24}, 30, 100, true});
    CHECK(layout.placement(x).bounds == QRectF{175, 100, 95, 24});
    for (auto a : {name, x, y, z}) {
        REQUIRE(layout.placement(a).visible);
        CHECK(QRectF{0, 0, 400, 240}.contains(layout.placement(a).bounds));
        for (auto b : {name, x, y, z})
            if (a != b) CHECK_FALSE(layout.placement(a).bounds.intersects(layout.placement(b).bounds));
    }
}
TEST_CASE("Label layout respects viewport edges and fixed HUD text", "[annotations]") {
    ScreenLabelLayout layout{{20, 10, 240, 160}};
    const auto hud = layout.add({{25, 15, 200, 25}, 1000, 0, false});
    const auto edge = layout.add({{210, 12, 90, 28}, 30, 1, true});
    const auto huge = layout.add({{0, 0, 500, 30}, 0, 1, true});
    const auto invalid = layout.add({{std::numeric_limits<double>::quiet_NaN(), 0, 10, 10}, 0, 1, true});
    CHECK(layout.placement(hud).bounds == QRectF{25, 15, 200, 25});
    REQUIRE(layout.placement(edge).visible);
    CHECK(QRectF{20, 10, 240, 160}.contains(layout.placement(edge).bounds));
    CHECK_FALSE(layout.placement(edge).bounds.intersects(layout.placement(hud).bounds));
    CHECK_FALSE(layout.placement(huge).visible);
    CHECK_FALSE(layout.placement(invalid).visible);
    const auto outside = layout.add({{-500,600,90,24},30,1,true});
    REQUIRE(layout.placement(outside).visible);
    CHECK(QRectF{20,10,240,160}.contains(layout.placement(outside).bounds));
}
TEST_CASE("Dense label layouts stay bounded and deterministic", "[annotations]") {
    ScreenLabelLayout a{{0, 0, 1200, 800}}, b{{0, 0, 1200, 800}};
    for (int i = 0; i < 3000; ++i) {
        ScreenLabelLayout::Request request{{double((i * 47) % 1180), double((i * 31) % 790), 90, 20}, i < 3 ? 30 : 0, float(i), true};
        a.add(request);
        b.add(request);
    }
    std::vector<QRectF> visible;
    for (size_t i = 0; i < 3000; ++i) {
        const auto first = a.placement(i), second = b.placement(i);
        REQUIRE(first.visible == second.visible);
        if (!first.visible) continue;
        REQUIRE(first.bounds == second.bounds);
        for (const auto& bounds : visible)
            REQUIRE_FALSE(first.bounds.intersects(bounds));
        visible.push_back(first.bounds);
    }
    CHECK(visible.size() > 100);
    CHECK(visible.size() < 3000);
}
TEST_CASE("Link ribbons exclude degenerate and non-finite connections", "[annotations]") {
    const auto color = vm::vec4f::fill(1.f);
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    std::vector<LinkRenderer::LineVertex> links;
    for (const auto& position : {vm::vec3f{0, 0, 0}, vm::vec3f{64, 0, 0}, vm::vec3f{1, 2, 3}, vm::vec3f{1, 2, 3}, vm::vec3f{nan, 0, 0}, vm::vec3f{1, 2, 3}})
        links.emplace_back(position, color);
    const auto vertices = LinkRenderer::buildLinkVertices(links);
    REQUIRE(vertices.size() == 6);
    for (const auto& vertex : vertices) {
        CHECK(getVertexComponent<0>(vertex) == vm::vec3f{0, 0, 0});
        CHECK(getVertexComponent<1>(vertex) == vm::vec3f{64, 0, 0});
    }
}
} // namespace TrenchBroom::Renderer
