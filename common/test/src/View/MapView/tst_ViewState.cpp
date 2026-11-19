#include "PreferenceManager.h"
#include "View/Tools/Brush/BrushUpstreamTest.h"
#include "Preferences.h"
#include "View/Preferences/ViewPreferencePane.h"
#include "View/Preferences/PreferenceDialog.h"
#include "View/Widgets/PopupWindow.h"
#include "View/MapView/GLContextManager.h"
#include "View/MapView/MapViewBase.h"
#include "View/MapView/MapViewLayout.h"
#include "View/MapView/SwitchableMapViewContainer.h"
#include "View/MapView/CyclingMapView.h"
#include "View/Widgets/Splitter.h"
#include "Renderer/Camera.h"
#include <QApplication>
#include <QLabel>
#include <QScreen>
#include <QScopeGuard>
#include <QSpinBox>
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
TEST_CASE("Splitter exposes styled grips and restores collapsed sizes", "[view-state]") {
    Splitter splitter(Qt::Horizontal);
    splitter.addWidget(new QLabel("A"));
    splitter.addWidget(new QLabel("B"));
    splitter.resize(600, 300);
    splitter.show();
    QApplication::processEvents();
    splitter.setSizes({200, 400});
    const auto before = splitter.sizes();
    auto* handle = qobject_cast<SplitterHandle*>(splitter.handle(1));
    REQUIRE(handle);
    handle->setStyleSheet("TrenchBroom--View--SplitterHandle { qproperty-gripColor: #aabbcc; qproperty-gripSpacing: 7; qproperty-gripDotSize: 3; }");
    handle->ensurePolished();
    CHECK(handle->gripColor() == QColor("#aabbcc"));
    CHECK(handle->gripSpacing() == 7);
    CHECK(handle->gripDotSize() == 3);
    splitter.collapse(Splitter::First);
    CHECK(splitter.isCollapsed(Splitter::First));
    splitter.collapse(Splitter::Second);
    CHECK(splitter.isCollapsed(Splitter::First));
    splitter.expand();
    CHECK(splitter.sizes() == before);
}
TEST_CASE_METHOD(BrushUpstreamTest, "Dialog sizes are controlled by QSS and bounded to the available screen", "[view-state][theme]") {
    const auto originalStyle = qApp->styleSheet();
    const auto restore = qScopeGuard([&] { qApp->setStyleSheet(originalStyle); });
    qApp->setStyleSheet(R"(
        QDialog#PreferenceDialog { qproperty-initialWidth: 900; qproperty-initialHeight: 680; }
        QWidget#ViewDetailsPopup { qproperty-initialWidth: 640; qproperty-initialHeight: 500; }
    )");
    ViewPreferencePane pane;
    for (const auto* name : {"PreferencesWindowWidth", "PreferencesWindowHeight",
                             "ViewDetailsWindowWidth", "ViewDetailsWindowHeight"})
        CHECK_FALSE(pane.findChild<QSpinBox*>(name));
    PreferenceDialog dialog(document);
    CHECK(dialog.property("initialWidth").toInt() == 900);
    CHECK(dialog.property("initialHeight").toInt() == 680);
    CHECK(dialog.width() == std::min(900, dialog.screen()->availableGeometry().width() - 40));
    CHECK(dialog.height() == std::min(680, dialog.screen()->availableGeometry().height() - 40));
    dialog.resize(950, 700); // Initial dimensions do not impose a fixed size.
    CHECK(dialog.width() == 950);
    CHECK(dialog.height() == 700);
    PopupWindow popup;
    popup.setObjectName("ViewDetailsPopup");
    popup.show();
    QApplication::processEvents();
    CHECK(popup.size() == QSize(640, 500).boundedTo(popup.screen()->availableGeometry().size() - QSize(32, 32)));
    popup.setStyleSheet("QWidget#ViewDetailsPopup { qproperty-initialWidth: 5000; qproperty-initialHeight: 5000; }");
    popup.hide();
    popup.show();
    QApplication::processEvents();
    CHECK(popup.size() == popup.screen()->availableGeometry().size() - QSize(32, 32));
    QLabel reference;
    reference.resize(100, 30);
    reference.move(-500, -500);
    popup.positionTouchingWidget(&reference);
    CHECK(popup.screen()->availableGeometry().contains(popup.geometry()));
    popup.hide();
}

} // namespace TrenchBroom::View
