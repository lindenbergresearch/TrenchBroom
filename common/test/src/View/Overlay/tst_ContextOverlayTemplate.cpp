#include "View/Overlay/ContextOverlayTemplate.h"

#include <catch2/catch.hpp>

namespace TrenchBroom::View {
TEST_CASE("Context overlay escapes map data and ignores missing fields", "[ContextOverlay]") {
    const ContextOverlayTemplate content("<b>{{selection.name}}</b>{{missing}} {{! comment }}");
    const QVariantMap data{{"selection", QVariantMap{{"name", "<brush>&\"test\""}}}};
    CHECK(content.render(data) == "<b>&lt;brush&gt;&amp;&quot;test&quot;</b> ");
}

TEST_CASE("Context overlay renders nested lists with parent scope", "[ContextOverlay]") {
    const ContextOverlayTemplate content("{{#textures}}{{name}}:{{tool.name}}{{#values}}[{{.}}]{{/values}};{{/textures}}");
    const QVariantMap data{
        {"tool", QVariantMap{{"name", "Clip"}}},
        {"textures", QVariantList{QVariantMap{{"name", "stone"}, {"values", QVariantList{1, 2}}},
            QVariantMap{{"name", "wood"}, {"values", QVariantList{3}}}}}};
    CHECK(content.render(data) == "stone:Clip[1][2];wood:Clip[3];");
}

TEST_CASE("Context overlay distinguishes empty selections and zero counts", "[ContextOverlay]") {
    const ContextOverlayTemplate content("{{#count}}faces{{/count}}{{^count}}empty{{/count}}{{#name}}named{{/name}}{{^list}}no list{{/list}}");
    CHECK(content.render({{"count", 0}, {"name", ""}, {"list", QVariantList{}}}) == "emptyno list");
    CHECK(content.render({{"count", 6}, {"name", "test"}, {"list", QVariantList{1}}}) == "facesnamed");
}

TEST_CASE("Context overlay validates all sections even when unused", "[ContextOverlay]") {
    CHECK_THROWS(ContextOverlayTemplate("{{name"));
    CHECK_THROWS(ContextOverlayTemplate("{{#missing}}never shown"));
    CHECK_THROWS(ContextOverlayTemplate("{{#a}}{{#b}}{{/a}}{{/b}}"));
    CHECK_THROWS(ContextOverlayTemplate("{{/a}}"));
    CHECK_THROWS(ContextOverlayTemplate("{{{unescaped}}}"));
    CHECK_THROWS(ContextOverlayTemplate("{{>partial}}"));
    CHECK_THROWS(ContextOverlayTemplate(QString("{{#a}}").repeated(34) + QString("{{/a}}").repeated(34)));
}
} // namespace TrenchBroom::View
