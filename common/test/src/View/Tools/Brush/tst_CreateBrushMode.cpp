#include "Renderer/OrthographicCamera.h"
#include "Renderer/PerspectiveCamera.h"
#include "View/Tools/Brush/BrushShapePalette.h"
#include "View/Actions/ActionContext.h"
#include "View/Tools/Brush/BrushUpstreamTest.h"
#include "View/Overlay/ContextOverlayData.h"
#include "View/Tools/Brush/DrawShapeTool.h"
#include "View/Tools/Brush/DrawShapeToolController2D.h"
#include "View/Tools/Brush/DrawShapeToolController3D.h"
#include "View/Tools/Brush/DrawShapeToolPage.h"
#include "View/Input/InputState.h"
#include "View/MapView/MapViewToolBox.h"
#include "View/Tools/Transform/MoveObjectsTool.h"
#include "View/Tools/Transform/MoveObjectsToolPage.h"
#include "View/Input/PickRequest.h"
#include "View/Tools/ToolChain.h"
#include <QApplication>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QToolButton>

namespace TrenchBroom::View {
TEST_CASE_METHOD(BrushUpstreamTest, "Select is the default and Create is an exclusive explicit tool", "[create-mode]") {
    newMap();
    QWidget bookWidget;
    auto* book = new QStackedLayout(&bookWidget);
    MapViewToolBox tools(document, book);
    CHECK(actionContextMatches(ActionContext::Any,ActionContext::View3D|ActionContext::NoSelection|ActionContext::CreateTool));
    CHECK_FALSE(actionContextMatches(ActionContext::AnyView|ActionContext::AnyOrNoSelection|ActionContext::NoTool,ActionContext::View3D|ActionContext::NoSelection|ActionContext::CreateTool));
    CHECK_FALSE(tools.anyToolActive());
    CHECK_FALSE(tools.drawShapeTool().active());
    CHECK(tools.moveObjectsTool().active());
    CHECK(qobject_cast<MoveObjectsToolPage*>(book->currentWidget()));
    CHECK(contextOverlayData(*document, tools)["tool"].toMap()["id"].toString() == "select");
    tools.toggleCreateTool();
    CHECK(tools.createToolActive());
    CHECK(tools.anyToolActive());
    CHECK_FALSE(tools.moveObjectsTool().active());
    CHECK(qobject_cast<DrawShapeToolPage*>(book->currentWidget()));
    CHECK(contextOverlayData(*document, tools)["tool"].toMap()["id"].toString() == "create");
    tools.drawShapeTool().extensionManager().setCurrentExtension(DrawShapeKind::Rock);
    ToolChain chain;
    REQUIRE(tools.cancel(&chain));
    CHECK_FALSE(tools.createToolActive());
    CHECK_FALSE(tools.anyToolActive());
    CHECK(tools.moveObjectsTool().active());
    tools.toggleCreateTool();
    CHECK(tools.drawShapeTool().extensionManager().currentExtension().kind() == DrawShapeKind::Cuboid);
    tools.toggleVertexTool();
    CHECK_FALSE(tools.createToolActive());
    CHECK(tools.vertexToolActive());
    tools.toggleCreateTool();
    CHECK(tools.createToolActive());
    CHECK_FALSE(tools.vertexToolActive());
    tools.deactivateAllTools();
    CHECK(tools.moveObjectsTool().active());
}
TEST_CASE_METHOD(BrushUpstreamTest, "Dragging only creates brushes in Create mode and Escape discards the preview", "[create-mode]") {
    newMap();
    QWidget bookWidget;
    auto* book = new QStackedLayout(&bookWidget);
    MapViewToolBox tools(document, book);
    Renderer::OrthographicCamera ortho(1, 8192, {0, 0, 800, 600}, {0, 0, 512}, {0, 0, -1}, {0, 1, 0});
    Renderer::PerspectiveCamera perspective(90, 1, 8192, {0, 0, 800, 600}, {0, -512, 512}, vm::vec3f(vm::normalize(vm::vec3{0, 1, -1})), {0, 0, 1});
    for (const bool is2D : {true, false}) {
        CAPTURE(is2D);
        const Renderer::Camera& camera = is2D ? static_cast<const Renderer::Camera&>(ortho) : perspective;
        InputState input;
        input.mouseDown(MouseButtons::MBLeft);
        input.setPickRequest(PickRequest{{{0, 0, 512}, {0, 0, -1}}, camera});
        ToolChain chain;
        if (is2D)
            chain.append(std::make_unique<DrawShapeToolController2D>(tools.drawShapeTool(), document));
        else
            chain.append(std::make_unique<DrawShapeToolController3D>(tools.drawShapeTool(), document));
        tools.startMouseDrag(&chain, input);
        CHECK_FALSE(tools.dragging());
        CHECK_FALSE(tools.drawShapeTool().hasBrushes());
        tools.toggleCreateTool();
        for (int i = 0; i < 2; ++i) {
            input.setPickRequest(PickRequest{{{double(i * 128), 0, 512}, {0, 0, -1}}, camera});
            tools.startMouseDrag(&chain, input);
            REQUIRE(tools.dragging());
            input.setPickRequest(PickRequest{{{double(i * 128 + 64), 64, 512}, {0, 0, -1}}, camera});
            REQUIRE(tools.mouseDrag(input));
            REQUIRE(tools.drawShapeTool().hasBrushes());
            tools.endMouseDrag(input);
            CHECK(tools.createToolActive());
            CHECK(document->hasSelectedNodes());
        }
        CHECK(document->world()->defaultLayer()->childCount() == (is2D ? 2 : 4));
        input.setPickRequest(PickRequest{{{512, 0, 512}, {0, 0, -1}}, camera});
        tools.startMouseDrag(&chain, input);
        REQUIRE(tools.dragging());
        input.setPickRequest(PickRequest{{{576, 64, 512}, {0, 0, -1}}, camera});
        REQUIRE(tools.mouseDrag(input));
        REQUIRE(tools.cancel(&chain));
        CHECK_FALSE(tools.dragging());
        CHECK_FALSE(tools.createToolActive());
        CHECK_FALSE(tools.drawShapeTool().hasBrushes());
        CHECK(document->world()->defaultLayer()->childCount() == (is2D ? 2 : 4));
        tools.startMouseDrag(&chain, input);
        CHECK_FALSE(tools.dragging());
    }
}
TEST_CASE_METHOD(BrushUpstreamTest, "The viewport palette selects all shapes and obeys QSS and resizing", "[create-mode]") {
    newMap();
    QWidget bookWidget;
    auto* book = new QStackedLayout(&bookWidget);
    MapViewToolBox tools(document, book);
    QWidget viewport;
    viewport.resize(640, 600);
    viewport.show();
    BrushShapePalette palette(tools, &viewport);
    palette.setStyleSheet("QFrame#BrushShapePalette {qproperty-edgeMargin:17;qproperty-panelPadding:5;qproperty-buttonSpacing:3;qproperty-overlayOpacity:0.6;}"
                          "QFrame#BrushShapePalette QToolButton {qproperty-iconSize:24px 24px;padding:3px;}");
    QApplication::processEvents();
    CHECK(palette.isHidden());
    tools.toggleCreateTool();
    QApplication::processEvents();
    QApplication::processEvents();
    CHECK(palette.isVisible());
    CHECK(palette.x() == 17);
    CHECK(palette.edgeMargin() == 17);
    CHECK(palette.buttonSpacing() == 3);
    CHECK(palette.overlayOpacity() == Approx(0.6));
    CHECK(std::abs(palette.geometry().center().y() - viewport.rect().center().y()) <= 1);
    auto buttons = palette.findChildren<QToolButton*>();
    REQUIRE(buttons.size() == 8);
    for (auto* button : buttons) {
        REQUIRE_FALSE(button->icon().isNull());
        CHECK(button->iconSize() == QSize(24, 24));
        button->click();
        CHECK(tools.drawShapeTool().extensionManager().currentExtension().kind() == DrawShapeKind(button->property("shapeKind").toInt()));
        CHECK(std::count_if(buttons.begin(), buttons.end(), [](const auto* b) { return b->isChecked(); }) == 1);
    }
    viewport.resize(400, 140);
    QApplication::processEvents();
    QApplication::processEvents();
    CHECK(palette.height() <= 106);
    CHECK(palette.findChild<QScrollArea*>()->verticalScrollBar()->maximum() > 0);
    tools.deactivateAllTools();
    CHECK(palette.isHidden());
}
} // namespace TrenchBroom::View
