#include "Model/BezierPatch.h"
#include "Model/BrushFace.h"
#include "Model/EntityNode.h"
#include "Model/GroupNode.h"
#include "Model/NodeCollection.h"
#include "Model/PatchNode.h"
#include "View/BrushUpstreamTest.h"

#include <memory>

namespace TrenchBroom::Model {
TEST_CASE("Bulk node removal preserves order and all typed lists", "[bulk-selection]") {
    LayerNode layer{Layer{"layer"}};
    GroupNode group{Group{"group"}};
    EntityNode entity{Entity{}};
    BrushNode brush{BrushBuilder{MapFormat::Quake3, vm::bbox3{8192.0}}.createCube(64, "test").value()};
    PatchNode patch{BezierPatch{3, 3, {{0, 0, 0}, {1, 0, 1}, {2, 0, 0}, {0, 1, 1}, {1, 1, 2}, {2, 1, 1}, {0, 2, 0}, {1, 2, 1}, {2, 2, 0}}, "test"}};
    EntityNode absent{Entity{}};
    NodeCollection selection{{&layer, &group, &entity, &brush, &patch}};
    selection.removeNodes({&brush, &group, &absent, &brush});
    CHECK(selection.nodes() == std::vector<Node*>{&layer, &entity, &patch});
    CHECK(selection.layers() == std::vector<LayerNode*>{&layer});
    CHECK(selection.groups().empty());
    CHECK(selection.entities() == std::vector<EntityNode*>{&entity});
    CHECK(selection.brushes().empty());
    CHECK(selection.patches() == std::vector<PatchNode*>{&patch});
    selection.removeNodes({});
    CHECK(selection.nodeCount() == 3);
    selection.removeNodes(selection.nodes()); // The input may alias the collection.
    CHECK(selection.empty());
    CHECK(selection.layers().empty());
    CHECK(selection.entities().empty());
    CHECK(selection.patches().empty());
}

TEST_CASE("Bulk node removal handles large mixed selections", "[bulk-selection]") {
    std::vector<std::unique_ptr<Node>> owners;
    std::vector<Node*> nodes, removed, expected;
    std::vector<GroupNode*> groups;
    std::vector<EntityNode*> entities;
    for (size_t i = 0; i < 12000; ++i) {
        if (i % 2)
            owners.push_back(std::make_unique<GroupNode>(Group{"group"}));
        else
            owners.push_back(std::make_unique<EntityNode>(Entity{}));
        auto* node = owners.back().get();
        nodes.push_back(node);
        if (i % 3) {
            expected.push_back(node);
            if (auto* group = dynamic_cast<GroupNode*>(node)) groups.push_back(group);
            if (auto* entity = dynamic_cast<EntityNode*>(node)) entities.push_back(entity);
        } else
            removed.push_back(node);
    }
    NodeCollection selection{nodes};
    selection.removeNodes(removed);
    CHECK(selection.nodes() == expected);
    CHECK(selection.groups() == groups);
    CHECK(selection.entities() == entities);
    selection.removeNodes(selection.nodes());
    CHECK(selection.empty());
}
} // namespace TrenchBroom::Model

namespace TrenchBroom::View {
TEST_CASE_METHOD(BrushUpstreamTest, "Bulk deselection and undo keep node and face selection consistent", "[bulk-selection]") {
    newMap();
    auto* a = addBrush({{0, 0, 0}, {64, 64, 64}});
    auto* b = addBrush({{128, 0, 0}, {192, 64, 64}});
    auto* c = addBrush({{256, 0, 0}, {320, 64, 64}});
    document->selectNodes({a, b, c});
    document->deselectNodes({c, a, a});
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{b});
    CHECK_FALSE(a->selected());
    CHECK(b->selected());
    CHECK_FALSE(c->selected());
    document->undoCommand();
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{a, b, c});
    document->redoCommand();
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{b});
    document->deselectAll();

    const std::vector<Model::BrushFaceHandle> faces{{a, 0}, {b, 1}, {a, 2}, {c, 3}};
    document->selectBrushFaces(faces);
    document->deselectBrushFaces({faces[3], faces[0], faces[0]});
    CHECK(document->selectedBrushFaces() == std::vector<Model::BrushFaceHandle>{faces[1], faces[2]});
    CHECK_FALSE(a->brush().face(0).selected());
    CHECK(a->brush().face(2).selected());
    CHECK_FALSE(c->brush().face(3).selected());
    document->undoCommand();
    CHECK(document->selectedBrushFaces() == faces);
    document->redoCommand();
    CHECK(document->selectedBrushFaces() == std::vector<Model::BrushFaceHandle>{faces[1], faces[2]});
    document->deselectAll();
    CHECK(document->selectedBrushFaces().empty());
}
} // namespace TrenchBroom::View
