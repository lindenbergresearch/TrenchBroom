#include "View/BrushUpstreamTest.h"
#include "Preferences.h"
#include "View/GLContextManager.h"
#include "View/MapViewBase.h"
#include "View/MapViewLayout.h"
#include "View/SwitchableMapViewContainer.h"
#include "View/CyclingMapView.h"
#include "Renderer/Camera.h"
#include <QApplication>
#include <map>
#include <tuple>

namespace TrenchBroom::View {
TEST_CASE_METHOD(BrushUpstreamTest, "Changing viewport layouts preserves camera position direction and zoom", "[view-state]") {
    newMap();
    const auto linked = TemporarilySetPref{Preferences::Link2DCameras, GENERATE(false, true)};
    GLContextManager context;
    SwitchableMapViewContainer views(&logger, document, context);
    const auto key = [](Renderer::Camera& camera) -> size_t { return camera.orthographicProjection() ? 1 + vm::find_abs_max_component(camera.direction()) : 0; };
    std::map<size_t, std::tuple<vm::vec3f, vm::vec3f, float>> expected;
    for (auto* view : views.findChildren<MapViewBase*>()) {
        auto& camera = view->camera();
        const auto i = key(camera);
        if (!camera.orthographicProjection()) camera.setDirection(vm::normalize(vm::vec3f{1, 2, -1}), vm::vec3f::pos_z());
        camera.moveTo(vm::vec3f{float(100 + i * 100), float(-400 + i * 30), float(500 + i * 40)});
        camera.setZoom(0.5f + float(i) * 0.25f);
    }
    for (auto* view : views.findChildren<MapViewBase*>()) {
        auto& camera = view->camera();
        const auto i = key(camera);
        expected[i] = {camera.position(), camera.direction(), camera.zoom()};
    }
    for (auto layout : {MapViewLayout::FourPanes, MapViewLayout::TwoPanes, MapViewLayout::ThreePanes, MapViewLayout::OnePane}) {
        views.switchToMapView(layout);
        for (auto* view : views.findChildren<MapViewBase*>()) {
            auto& camera = view->camera();
            const auto found = expected.find(key(camera));
            if (found != expected.end()) {
                CHECK(camera.position() == std::get<0>(found->second));
                CHECK(camera.direction() == std::get<1>(found->second));
                CHECK(camera.zoom() == std::get<2>(found->second));
            }
        }
    }
}
TEST_CASE_METHOD(BrushUpstreamTest, "Cycling 2D and 3D views does not refocus their cameras", "[view-state]") {
    newMap();
    const auto linked = TemporarilySetPref{Preferences::Link2DCameras, false};
    GLContextManager context;
    SwitchableMapViewContainer views(&logger, document, context);
    views.switchToMapView(MapViewLayout::OnePane);
    auto* cycling = views.findChild<CyclingMapView*>();
    REQUIRE(cycling);
    const auto children = cycling->findChildren<MapViewBase*>();
    REQUIRE(children.size() == 4);
    std::vector<vm::vec3f> positions;
    for (auto* view : children) {
        auto& camera = view->camera();
        camera.moveTo(camera.position() + vm::vec3f{123, 456, 789});
        positions.push_back(camera.position());
    }
    for (int cycle = 0; cycle < 2; ++cycle)
        for (auto* view : children)
            cycling->cycleChildMapView(view);
    for (int i = 0; i < children.size(); ++i)
        CHECK(children[i]->camera().position() == positions[size_t(i)]);
}
} // namespace TrenchBroom::View
