#include "View/Tools/Brush/BrushUpstreamTest.h"
#include "Model/BrushNode.h"
#include "Model/LayerNode.h"
#include "Model/WorldNode.h"

namespace TrenchBroom::View {
TEST_CASE_METHOD(BrushUpstreamTest,"Convex merge handles the upstream crash map and can be undone","[brush-port]") {
    const auto fixture=repository()/"common/test/fixture/View/BrushUpstream/csgConvexMergeCrash.map";
    REQUIRE(document->loadDocument(Model::MapFormat::Valve,vm::bbox3{32768.0},game,fixture).is_success());
    auto* layer=document->world()->defaultLayer();
    REQUIRE(layer->childCount()==119);
    document->selectAllNodes();
    REQUIRE(document->csgConvexMerge());
    REQUIRE(layer->childCount()==1);
    auto* brush=dynamic_cast<Model::BrushNode*>(layer->children().front());
    REQUIRE(brush);
    CHECK(brush->brush().fullySpecified());
    document->undoCommand();
    CHECK(layer->childCount()==119);
    document->redoCommand();
    CHECK(layer->childCount()==1);
}
}

#include "View/Tools/Brush/DrawShapeTool.h"
#include "View/Tools/Brush/DrawShapeToolPage.h"
#include "View/Tools/Brush/DrawShapeToolExtensions.h"
#include "Model/BrushBuilder.h"
#include "Model/GroupNode.h"
#include "Model/EntityNode.h"
#include "Model/Entity.h"
#include "Model/NodeContents.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QToolButton>

namespace TrenchBroom::View {
TEST_CASE_METHOD(BrushUpstreamTest,"Shape controls share parameters without signal loops","[brush-port]") {
    newMap(); DrawShapeTool tool(document);auto& manager=tool.extensionManager();
    DrawShapeToolPage page(document,manager);
    manager.parameters().setCircleShape(Model::VertexAlignedCircle{17});
    const auto boxes=page.findChildren<QSpinBox*>("ShapeSides");
    REQUIRE(boxes.size()==4);
    for(const auto* box:boxes) CHECK(box->value()==17);
    boxes.front()->setValue(24);
    CHECK(std::get<Model::VertexAlignedCircle>(manager.parameters().circleShape()).numSides==24);
    for(const auto* box:boxes) CHECK(box->value()==24);
    auto* axis=page.findChild<QComboBox*>("ShapeToolAxis");REQUIRE(axis);axis->setCurrentIndex(2);
    CHECK(manager.parameters().resolvedAxis(vm::axis::z)==vm::axis::y);
    axis->setCurrentIndex(0);CHECK(manager.parameters().resolvedAxis(vm::axis::x)==vm::axis::x);
    manager.parameters().setAccuracy(0);CHECK(manager.parameters().accuracy()==1);
    manager.parameters().setAccuracy(99);CHECK(manager.parameters().accuracy()==4);
    manager.parameters().setCircleShape(Model::ScalableCircle{9});CHECK(std::get<Model::ScalableCircle>(manager.parameters().circleShape()).precision==3);
    CHECK_FALSE(manager.setCurrentExtensionIndex(999));
}
TEST_CASE_METHOD(BrushUpstreamTest,"Apply shape preserves the parent and supports undo and redo","[brush-port]") {
    newMap();
    const Model::BrushBuilder builder(Model::MapFormat::Standard,document->worldBounds());
    auto* entity=new Model::EntityNode(Model::Entity{document->world()->entityPropertyConfig(),{{"classname","func_detail"}}});
    auto* original=new Model::BrushNode(builder.createCube(128,"original").value());
    entity->addChild(original);document->addNodes({{document->world()->defaultLayer(),{entity}}});
    document->selectNodes({original});
    DrawShapeTool tool(document);auto& manager=tool.extensionManager();
    CHECK(manager.canApplySelection());manager.setCurrentExtension(DrawShapeKind::Cylinder);
    setPref(Preferences::GroupBrushesCreatedByShapeTool,false);
    REQUIRE(tool.applyParameters());CHECK(entity->childCount()==1);CHECK(entity->parent()==document->world()->defaultLayer());
    auto* cylinder=dynamic_cast<Model::BrushNode*>(entity->children().front());REQUIRE(cylinder);
    CHECK(cylinder->brush().faceCount()==10);
    CHECK(cylinder->brush().face(*cylinder->brush().findFace(vm::vec3{0,0,1})).attributes().textureName()=="original");
    document->undoCommand();REQUIRE(entity->childCount()==1);CHECK(entity->children().front()==original);
    document->redoCommand();REQUIRE(entity->childCount()==1);CHECK(dynamic_cast<Model::BrushNode*>(entity->children().front())->brush().faceCount()==10);
    document->deselectAll(); CHECK_FALSE(manager.canApplySelection());
}
TEST_CASE_METHOD(BrushUpstreamTest,"Compound shapes group optionally and apply is atomic","[brush-port]") {
    newMap();DrawShapeTool tool(document);auto& manager=tool.extensionManager();
    manager.setCurrentExtension(DrawShapeKind::Stairs);manager.parameters().setStepHeight(16);
    setPref(Preferences::GroupBrushesCreatedByShapeTool,true);
    tool.update(vm::bbox3{{0,0,0},{128,64,64}},vm::axis::z); REQUIRE(tool.createBrushes());
    auto* layer=document->world()->defaultLayer();REQUIRE(layer->childCount()==1);
    auto* group=dynamic_cast<Model::GroupNode*>(layer->children().front());REQUIRE(group);CHECK(group->childCount()==4);
    document->undoCommand();CHECK(layer->childCount()==0);document->redoCommand();REQUIRE(layer->childCount()==1);
    document->deselectAll();group=dynamic_cast<Model::GroupNode*>(layer->children().front());document->selectNodes(group->children());
    REQUIRE(manager.canApplySelection());
    group->setLockState(Model::LockState::Locked);
    CHECK_FALSE(tool.applyParameters());CHECK(group->childCount()==4);
    group->setLockState(Model::LockState::Inherited);
    setPref(Preferences::GroupBrushesCreatedByShapeTool,false);
    document->deselectAll();tool.update(vm::bbox3{{256,0,0},{384,64,64}},vm::axis::z);
    manager.parameters().setStepHeight(16);REQUIRE(tool.createBrushes());CHECK(layer->childCount()==5);
}
}

#include "Model/ModelUtils.h"
#include "Model/PickResult.h"
#include "View/Tools/Brush/ExtrudeTool.h"
#include "View/Tools/Vertex/VertexTool.h"
#include "View/Tools/Vertex/EdgeTool.h"
#include "View/Tools/Vertex/FaceTool.h"
#include "View/MapView/MapViewToolBox.h"
#include "View/Tools/Transform/RotateObjectsTool.h"
#include "View/Tools/Transform/ScaleObjectsTool.h"
#include "View/Tools/Transform/ShearObjectsTool.h"
#include "View/Tools/Transform/ScaleObjectsToolPage.h"
#include "Renderer/PerspectiveCamera.h"
#include "Renderer/OrthographicCamera.h"
#include <QStackedLayout>
#include <QLineEdit>
#include <QPushButton>

namespace TrenchBroom::View {
TEST_CASE_METHOD(BrushUpstreamTest,"Coplanar selection floods shared edges and respects visibility and locking","[brush-port]") {
    newMap();auto* a=addBrush({{0,0,0},{64,64,64}});auto* b=addBrush({{64,0,0},{128,64,64}});auto* c=addBrush({{128,0,0},{192,64,64}});
    const auto face=Model::BrushFaceHandle{a,*a->brush().findFace(vm::vec3{0,0,1})};
    const auto flood=[&] {return Model::collectConnectedCoplanarFaces(face,document->editorContext(),*document->world());};
    CHECK(flood().size()==3);
    b->setLockState(Model::LockState::Locked);CHECK(flood().size()==1);b->setLockState(Model::LockState::Inherited);
    b->setVisibilityState(Model::VisibilityState::Hidden);CHECK(flood().size()==1);b->setVisibilityState(Model::VisibilityState::Inherited);
    auto* corner=addBrush({{0,64,0},{64,128,64}});CHECK(flood().size()==4);
    addBrush({{-64,128,0},{0,192,64}});CHECK(flood().size()==4); // only a shared point
    addBrush({{208,0,0},{272,64,64}});CHECK(flood().size()==4); // a gap
    corner->setVisibilityState(Model::VisibilityState::Hidden);CHECK(flood().size()==3);
    (void)c;
}
TEST_CASE_METHOD(BrushUpstreamTest,"Extrude picks real seams and excludes separated opposing faces","[brush-port]") {
    newMap();auto* a=addBrush({{-16,-16,-16},{16,16,16}});auto* b=addBrush({{-16,16,-16},{16,48,16}});
    document->selectNodes({a,b});ExtrudeTool tool(document);
    const vm::ray3 ray{{0,-556,586},vm::normalize(vm::vec3{0,1,-1})};
    Renderer::PerspectiveCamera camera(90,1,8000,{0,0,1920,1080},vm::vec3f(ray.origin),vm::vec3f(ray.direction),{0,0,1});
    Model::PickResult result;document->pick(ray,result);
    const auto hit=tool.pick3D(ray,camera,result);REQUIRE(hit.isMatch());
    CHECK(hit.target<ExtrudeHitData>().face.face().normal()==vm::vec3{0,1,0});result.addHit(hit);tool.updateProposedDragHandles(result);
    REQUIRE(tool.proposedDragHandles().size()==2);
    document->deselectAll();document->selectNodes({a});result.clear();document->pick(ray,result);
    const auto single=tool.pick3D(ray,camera,result);REQUIRE(single.isMatch());CHECK(single.target<ExtrudeHitData>().face.face().normal()==vm::vec3{0,0,1});
    document->deselectAll();auto* remote=addBrush({{64,-16,16},{96,16,48}});document->selectNodes({a,remote});
    result.clear();const vm::ray3 down{{0,0,64},{0,0,-1}};document->pick(down,result);
    const auto top=tool.pick3D(down,camera,result);REQUIRE(top.isMatch());result.addHit(top);tool.updateProposedDragHandles(result);CHECK(tool.proposedDragHandles().size()==1);
}
TEST_CASE_METHOD(BrushUpstreamTest,"Stamp preserves the source brush and supports cancellation and undo","[brush-port]") {
    newMap();auto* a=addBrush({{-32,-32,-32},{32,32,32}},"source");document->selectNodes({a});ExtrudeTool tool(document);
    const auto face=Model::BrushFaceHandle{a,*a->brush().findFace(vm::vec3{0,0,1})};
    ExtrudeDragState state{{ExtrudeDragHandle{face}},{face}};
    tool.beginStamp();CHECK_FALSE(tool.stamp({0,0,-16},state));CHECK(document->world()->defaultLayer()->childCount()==1);
    REQUIRE(tool.stamp({0,0,16},state));CHECK(a->brush().bounds()==vm::bbox3{{-32,-32,-32},{32,32,32}});
    REQUIRE(tool.stamp({0,0,32},state));CHECK(document->world()->defaultLayer()->childCount()==2);
    auto* stamp=document->selectedNodes().brushes().front();CHECK(stamp->brush().bounds()==vm::bbox3{{-32,-32,32},{32,32,64}});
    tool.commit(state);document->undoCommand();CHECK(document->world()->defaultLayer()->childCount()==1);
    document->redoCommand();CHECK(document->world()->defaultLayer()->childCount()==2);
}
TEST_CASE_METHOD(BrushUpstreamTest,"Component matrices preserve unselected vertices and undo handle positions","[brush-port]") {
    newMap();auto* brush=addBrush({{-64,-64,-64},{64,64,64}});document->selectNodes({brush});VertexTool tool(document);REQUIRE(tool.activate());
    auto points=brush->brush().face(*brush->brush().findFace(vm::vec3{0,0,1})).vertexPositions();
    for(const auto& point:points) tool.handleManager().select(point);
    const auto matrix=vm::scaling_matrix(vm::vec3{0.5,0.5,1});
    REQUIRE(document->transformObjects("Scale selected vertices",matrix));
    for(const auto& point:points) CHECK(brush->brush().hasVertex(matrix*point));
    CHECK(brush->brush().hasVertex({-64,-64,-64}));CHECK(tool.handleManager().selectedHandleCount()==4);
    document->undoCommand();for(const auto& point:points) CHECK(tool.handleManager().selected(point));
    document->redoCommand();for(const auto& point:points) CHECK(tool.handleManager().selected(matrix*point));
    tool.deselectAll();CHECK_FALSE(document->rotateObjects({0,0,0},{0,0,1},0.5)); // do not rotate the entire brush when no components are selected
    REQUIRE(tool.deactivate());
}
TEST_CASE_METHOD(BrushUpstreamTest,"Scale and shear handles follow the component bounds throughout a drag","[brush-port]") {
    newMap();auto* brush=addBrush({{-64,-64,-64},{64,64,64}});document->selectNodes({brush});
    QWidget bookWidget;auto* book=new QStackedLayout(&bookWidget);MapViewToolBox tools(document,book);
    tools.toggleVertexTool();auto& vertices=tools.vertexTool();
    for(const auto& point:brush->brush().face(*brush->brush().findFace(vm::vec3{0,0,1})).vertexPositions()) vertices.handleManager().select(point);
    tools.toggleScaleObjectsTool();CHECK(tools.vertexToolActive());CHECK(tools.scaleObjectsToolActive());
    auto& scale=tools.scaleObjectsTool();
    const auto start=scale.bounds();CHECK(start==vm::bbox3{{-64,-64,-64},{64,64,64}});
    scale.startScaleWithHit(Model::Hit{ScaleObjectsTool::ScaleToolSideHitType,0,{64,0,0},BBoxSide{{1,0,0}}});
    scale.scaleByDelta({-32,0,0});CHECK(scale.bounds().max.x()==32);CHECK(brush->brush().hasVertex({32,64,64}));
    scale.scaleByDelta({-16,0,0});CHECK(scale.bounds().max.x()==16);CHECK(brush->brush().hasVertex({16,64,64}));CHECK(brush->brush().hasVertex({64,64,-64}));
    scale.commitScale();CHECK(scale.bounds().max.x()==16);
    CHECK(tools.shearObjectsTool().bounds()==scale.bounds());
    document->undoCommand();CHECK(scale.bounds()==start);CHECK(vertices.handleManager().selectedHandleCount()==4);
    tools.toggleScaleObjectsTool();CHECK(tools.vertexToolActive());CHECK_FALSE(tools.scaleObjectsToolActive());
    tools.toggleRotateObjectsTool();CHECK(tools.vertexToolActive());CHECK(tools.rotateObjectsToolActive());
    CHECK(tools.rotateObjectsTool().rotationCenter().z()==64);
    tools.deactivateAllTools();CHECK_FALSE(tools.anyToolActive());
}
}
#include <QTemporaryDir>
namespace TrenchBroom::View {
TEST_CASE_METHOD(BrushUpstreamTest,"Every new shape survives saving and reloading a map","[brush-port]") {
    QTemporaryDir folder;REQUIRE(folder.isValid());
    for(const auto kind:DrawShapeKinds) {
        CAPTURE(int(kind));newMap();DrawShapeTool tool(document);auto& manager=tool.extensionManager();manager.setCurrentExtension(kind);
        if(kind==DrawShapeKind::UVSphere) manager.parameters().setCircleShape(Model::ScalableCircle{2});
        tool.update({{-128,-96,0},{128,96,256}},vm::axis::x);REQUIRE(tool.createBrushes());
        const auto path=std::filesystem::path(folder.path().toStdString())/"shape.map";
        document->saveDocumentAs(path);
        const auto before=document->serializeSelectedNodes();CHECK_FALSE(before.empty());
        REQUIRE(document->loadDocument(Model::MapFormat::Standard,document->worldBounds(),game,path).is_success());
        document->selectAllNodes();CHECK_FALSE(document->selectedNodes().empty());
        CHECK(document->serializeSelectedNodes()==before);
    }
}
TEST_CASE_METHOD(BrushUpstreamTest,"Linked group children cannot be replaced by Apply","[brush-port]") {
    newMap();auto* brush=addBrush(vm::bbox3{64.0});document->selectNodes({brush});auto* group=document->groupSelection("source");REQUIRE(group);
    document->deselectAll();document->selectNodes({group});REQUIRE(document->createLinkedDuplicate());
    document->deselectAll();document->selectNodes({brush});DrawShapeTool tool(document);CHECK_FALSE(tool.extensionManager().canApplySelection());CHECK_FALSE(tool.applyParameters());
}
TEST_CASE_METHOD(BrushUpstreamTest,"Edge and face transformations retain selection after undo and redo","[brush-port]") {
    newMap();auto* brush=addBrush(vm::bbox3{64.0});document->selectNodes({brush});
    SECTION("edge") {
        EdgeTool tool(document);REQUIRE(tool.activate());const auto edge=brush->brush().edges().front()->segment();tool.handleManager().select(edge);
        const auto matrix=vm::translation_matrix(vm::vec3{1,2,3});REQUIRE(tool.transformSelection("Transform edge",matrix));
        CHECK(tool.handleManager().selected(edge.transform(matrix)));document->undoCommand();CHECK(tool.handleManager().selected(edge));document->redoCommand();CHECK(tool.handleManager().selected(edge.transform(matrix)));tool.deactivate();
    }
    SECTION("face") {
        FaceTool tool(document);REQUIRE(tool.activate());const auto face=brush->brush().face(*brush->brush().findFace(vm::vec3{0,0,1})).polygon();tool.handleManager().select(face);
        const auto matrix=vm::scaling_matrix(vm::vec3{0.75,0.75,1});REQUIRE(tool.transformSelection("Transform face",matrix));
        CHECK(tool.handleManager().selected(face.transform(matrix)));document->undoCommand();CHECK(tool.handleManager().selected(face));document->redoCommand();CHECK(tool.handleManager().selected(face.transform(matrix)));tool.deactivate();
    }
}
}
