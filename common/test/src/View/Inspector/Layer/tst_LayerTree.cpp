#include "Error.h"
#include "PreferenceManager.h"
#include <kdl/result.h>
#include "TestSourcePath.h"
#include "Error.h"
#include "IO/GameConfigParser.h"
#include "Logger.h"
#include "Preferences.h"
#include "Model/BrushBuilder.h"
#include "Model/BrushFaceHandle.h"
#include "Model/BrushNode.h"
#include "Model/EntityNode.h"
#include "Model/EditorContext.h"
#include "Model/EntityProperties.h"
#include "Model/GameImpl.h"
#include "Model/GroupNode.h"
#include "Model/LayerNode.h"
#include "Model/WorldNode.h"
#include "View/Inspector/Layer/LayerDetailText.h"
#include "View/Inspector/Layer/LayerEditor.h"
#include "View/Inspector/Layer/LayerListBox.h"
#include "View/Inspector/Layer/LayerTreeItemRenderer.h"
#include "View/Document/MapDocument.h"
#include "View/Document/MapDocumentCommandFacade.h"

#include <QCoreApplication>
#include <QApplication>
#include <QKeyEvent>
#include <QHeaderView>
#include <QMenu>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QFile>
#include <QMouseEvent>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QTimer>
#include <QTreeWidgetItemIterator>
#include <QToolButton>
#include <QSpinBox>
#include <kdl/result.h>
#include <catch2/catch.hpp>

#include <algorithm>

namespace TrenchBroom::View {
namespace {
class LayerTreeTest {
protected:
    NullLogger logger;
    Model::GameConfig config = IO::GameConfigParser(R"({
        "version": 8, "name": "LayerTreeTest",
        "fileformats": [{"format": "Standard"}],
        "filesystem": {"searchpath": ".", "packageformat": {"extension": ".pak", "format": "idpak"} },
        "textures": {"root": "textures", "format": {"extensions": [".png"], "format": "image"} },
        "entities": {"definitions": ["Generic.fgd"], "defaultcolor": "0.6 0.6 0.6 1.0"}
    })", testSourceRoot()
        / "app/resources/games/Generic/GameConfig.cfg").parse();
    std::shared_ptr<Model::GameImpl> game = std::make_shared<Model::GameImpl>(config, std::filesystem::path{}, logger);
    std::shared_ptr<MapDocument> document = MapDocumentCommandFacade::newMapDocument();
    std::unique_ptr<LayerEditor> editor;
    LayerListBox *tree = nullptr;
    Model::LayerNode *layer = nullptr;
    Model::GroupNode *group = nullptr;
    Model::GroupNode *nested = nullptr;
    Model::EntityNode *entity = nullptr;
    Model::BrushNode *a = nullptr;
    Model::BrushNode *b = nullptr;
    Model::BrushNode *outside = nullptr;

    static void flush() {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }

    static void clickAt(QWidget* widget, const QPoint& position, const Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QMouseEvent press(QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton, modifiers);
        QCoreApplication::sendEvent(widget, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton, modifiers);
        QCoreApplication::sendEvent(widget, &release);
        flush();
    }

    LayerTreeTest() {
        setPref(Preferences::LayerTreeGroupByType, false);
        setPref(Preferences::LayerTreeGroupingThreshold, 5);
        setPref(Preferences::LayerTreeSyncSelection, true);
        REQUIRE(document->newDocument(Model::MapFormat::Standard, vm::bbox3(8192.0), game).is_success());
        document->selectAllNodes();
        document->deleteObjects();
        layer = document->world()->defaultLayer();
        group = new Model::GroupNode(Model::Group("Outer"));
        nested = new Model::GroupNode(Model::Group("Inner"));
        entity = new Model::EntityNode(Model::Entity({}, {{"classname", "func_detail"}}));
        Model::BrushBuilder builder(Model::MapFormat::Standard, document->worldBounds());
        a = new Model::BrushNode(builder.createCube(32, "").value());
        b = new Model::BrushNode(builder.createCube(64, "").value());
        outside = new Model::BrushNode(builder.createCube(16, "").value());
        entity->addChildren({a, b});
        nested->addChild(entity);
        group->addChild(nested);
        document->addNodes({{layer, {group, outside}}});
        editor = std::make_unique<LayerEditor>(document, nullptr);
        tree = editor->findChild<LayerListBox *>();
        REQUIRE(tree);
        flush();
    }

    QTreeWidgetItem *item(Model::Node *node) const {
        for (QTreeWidgetItemIterator it(tree); *it; ++it) {
            if ((*it)->data(0, Qt::UserRole).value<quintptr>() == reinterpret_cast<quintptr>(node)) return *it;
        }
        return nullptr;
    }

    void select(Model::Node *node, bool extend = false) {
        REQUIRE(item(node));
        tree->setCurrentItem(item(node), 0, (extend ? QItemSelectionModel::Select : QItemSelectionModel::ClearAndSelect) | QItemSelectionModel::Rows);
    }

    void showEditor() {
        editor->resize(400, 600);
        editor->show();
        flush();
    }

    QPoint cellCenter(Model::Node *node, const int column) const {
        auto* row = item(node);
        REQUIRE(row);
        const auto rect = tree->visualItemRect(row);
        REQUIRE(rect.isValid());
        const auto* header = tree->header();
        const auto pos = QPoint(header->sectionViewportPosition(column) + header->sectionSize(column) / 2,
            rect.center().y());
        REQUIRE(tree->itemAt(pos) == row);
        REQUIRE(tree->indexAt(pos).column() == column);
        return pos;
    }

    void doubleClick(Model::Node *node) {
        const auto pos = cellCenter(node, 0);
        QMouseEvent doubleClick(QEvent::MouseButtonDblClick, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(tree->viewport(), &doubleClick);
        QMouseEvent release(QEvent::MouseButtonRelease, pos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(tree->viewport(), &release);
        flush();
    }
};
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree represents nested scene nodes, with no face rows", "[LayerTree]") {
    REQUIRE(tree->topLevelItemCount() == 1);
    REQUIRE(item(group)->parent() == item(layer));
    REQUIRE(item(nested)->parent() == item(group));
    REQUIRE(item(entity)->parent() == item(nested));
    REQUIRE(item(a)->parent() == item(entity));
    CHECK(item(a)->childCount() == 0);
    CHECK(item(layer)->data(0, LayerTreeItemDelegate::DetailRole).toString().contains("6 nested entries"));
    CHECK(item(group)->data(0, LayerTreeItemDelegate::DetailRole).toString().contains("4 nested entries"));
    CHECK(tree->selectionMode() == QAbstractItemView::ExtendedSelection);
}

TEST_CASE_METHOD(LayerTreeTest, "Refreshing a layer tree icon color preserves tree and selection state", "[LayerTree]") {
    tree->setGroupingThreshold(1);
    tree->setGroupByType(true);
    select(a);
    auto* brushItem = item(a);
    auto* bucket = brushItem->parent();
    REQUIRE(bucket->data(0, LayerTreeItemDelegate::TypeGroupRole).isValid());
    bucket->setExpanded(true);
    const auto brushIcon = brushItem->icon(0).cacheKey();
    const auto bucketIcon = bucket->icon(0).cacheKey();
    const auto groupIcon = item(group)->icon(0).cacheKey();

    auto& prefs = PreferenceManager::instance();
    auto& color = Preferences::LayerTreeBrushIconColor;
    const auto previous = pref(color);
    const auto restoreColor = qScopeGuard([&] { prefs.set(color, previous); });
    REQUIRE(prefs.set(color, Color(0, 255, 0)));
    const auto* delegate = dynamic_cast<const LayerTreeItemDelegate*>(tree->itemDelegate());
    REQUIRE(delegate);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        delegate->updateIcon(**it, color.path());
    }
    flush();

    CHECK(item(a) == brushItem);
    CHECK(item(a)->parent() == bucket);
    CHECK(item(a)->isSelected());
    CHECK(bucket->isExpanded());
    CHECK(brushItem->icon(0).cacheKey() != brushIcon);
    CHECK(bucket->icon(0).cacheKey() == bucketIcon);
    CHECK(item(group)->icon(0).cacheKey() == groupIcon);
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree synchronizes nested and multiple selection in both directions", "[LayerTree]") {
    select(a);
    CHECK(a->selected());
    CHECK_FALSE(group->selected());
    select(outside, true);
    CHECK(document->selectedNodes().nodes().size() == 2);
    CHECK(outside->selected());
    document->deselectAll();
    document->selectNodes({b});
    CHECK(item(b)->isSelected());
    CHECK_FALSE(item(a)->isSelected());
    CHECK(item(group)->isExpanded());
    CHECK(item(nested)->isExpanded());
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree maps brush entities and layers to document objects", "[LayerTree]") {
    select(entity);
    CHECK(a->selected());
    CHECK(b->selected());
    CHECK_FALSE(entity->selected());
    CHECK(document->selectedNodes().nodes().size() == 2);
    select(layer);
    CHECK(group->selected());
    CHECK(outside->selected());
    CHECK_FALSE(layer->selected());
    CHECK(document->selectedNodes().nodes().size() == 2);
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree avoids selecting a parent and its descendants for editing", "[LayerTree]") {
    const auto threshold = TemporarilySetPref{Preferences::LayerTreeSelectionSyncThreshold,
        static_cast<int>(group->descendantCount() + 1u)};
    select(a);
    select(group, true);
    CHECK(group->selected());
    CHECK_FALSE(a->selected());
    CHECK(document->selectedNodes().nodes().size() == 1);
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree requires a double-click to transfer a large subtree selection", "[LayerTree]") {
    const auto threshold = TemporarilySetPref{Preferences::LayerTreeSelectionSyncThreshold, 2};
    Model::Node* ancestor = nullptr;
    SECTION("A layer") { ancestor = layer; }
    SECTION("A closed group") { ancestor = group; }
    REQUIRE(ancestor);

    showEditor();
    select(outside);
    REQUIRE(outside->selected());
    item(layer)->setExpanded(true);
    flush();
    clickAt(tree->viewport(), cellCenter(ancestor, 0));
    CHECK(tree->selectedItems() == QList<QTreeWidgetItem*>{item(ancestor)});
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{outside});

    doubleClick(ancestor);
    CHECK(tree->selectedItems() == QList<QTreeWidgetItem*>{item(ancestor)});
    CHECK(group->selected());
    CHECK(outside->selected() == (ancestor == layer));
    CHECK(document->selectedNodes().nodes().size() == (ancestor == layer ? 2u : 1u));
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree icons use undoable visibility and locking actions", "[LayerTree]") {
    select(a);
    tree->nodeLockToggled(a);
    CHECK(a->locked());
    CHECK_FALSE(a->selected());
    select(a);
    CHECK_FALSE(a->selected());
    document->undoCommand();
    CHECK_FALSE(a->locked());
    tree->nodeVisibilityToggled(group);
    CHECK_FALSE(a->visible());
    select(a);
    CHECK_FALSE(a->selected());
    tree->nodeVisibilityToggled(a);
    CHECK(a->visible());
    tree->clearSelection();
    select(a);
    CHECK(a->selected());
    tree->nodeLockToggled(group);
    CHECK(a->locked());
    tree->nodeLockToggled(a);
    CHECK_FALSE(a->locked());
    flush();
}

TEST_CASE_METHOD(LayerTreeTest, "A visibility icon leaves a selected layer row and other editor selections in place", "[LayerTree]") {
    showEditor();
    clickAt(tree->viewport(), cellCenter(layer, 0));
    REQUIRE(group->selected());
    REQUIRE(outside->selected());
    item(layer)->setExpanded(true);
    flush();

    clickAt(tree->viewport(), cellCenter(outside, 1));
    CHECK_FALSE(outside->visible());
    CHECK(tree->selectedItems() == QList<QTreeWidgetItem*>{item(layer)});
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{group});

    clickAt(tree->viewport(), cellCenter(outside, 1));
    CHECK(outside->visible());
    CHECK(tree->selectedItems() == QList<QTreeWidgetItem*>{item(layer)});
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{group});
}

TEST_CASE_METHOD(LayerTreeTest, "Hiding a child preserves its inspector selection under a selected group", "[LayerTree]") {
    showEditor();
    select(a);
    select(group, true);
    REQUIRE(item(a)->isSelected());
    REQUIRE(group->selected());
    REQUIRE_FALSE(a->selected());
    for (auto* parent = item(a)->parent(); parent; parent = parent->parent())
        parent->setExpanded(true);
    flush();

    clickAt(tree->viewport(), cellCenter(a, 1));
    CHECK_FALSE(a->visible());
    CHECK(item(a)->isSelected());
    CHECK(item(group)->isSelected());
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{group});
}

TEST_CASE_METHOD(LayerTreeTest, "Hidden and locked objects remain selectable in the inspector", "[LayerTree]") {
    SECTION("Hidden object") { document->hide({a}); }
    SECTION("Locked object") { document->lock({a}); }
    SECTION("Inherited visibility") { document->hide({group}); }
    SECTION("Inherited lock") { document->lock({group}); }
    flush();
    showEditor();
    tree->expandAll(); flush();
    clickAt(tree->viewport(), cellCenter(a, 0));
    CHECK(item(a)->isSelected());
    CHECK_FALSE(a->selected());
    CHECK(tree->selectedItems().size() == 1);
    select(outside, true);
    CHECK(item(a)->isSelected());
    CHECK(item(outside)->isSelected());
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{outside});
    document->deselectAll();
    document->selectNodes({outside}); // An explicit editor selection replaces tree browsing.
    flush();
    CHECK_FALSE(item(a)->isSelected());
}

TEST_CASE_METHOD(LayerTreeTest, "Hiding and locking a selected row preserves that row for inspection", "[LayerTree]") {
    showEditor();
    select(a);
    tree->expandAll(); flush();
    const auto column = GENERATE(1, 2);
    clickAt(tree->viewport(), cellCenter(a, column));
    CHECK_FALSE(a->selected());
    CHECK(tree->selectedItems() == QList<QTreeWidgetItem*>{item(a)});
    CHECK(tree->currentItem() == item(a));
}

TEST_CASE_METHOD(LayerTreeTest, "Moving the mouse during an attribute click cannot extend the tree selection", "[LayerTree]") {
    showEditor();
    tree->expandAll(); flush();
    clickAt(tree->viewport(), cellCenter(group, 0));
    const auto column = GENERATE(1, 2);
    const auto position = cellCenter(outside, column);
    QMouseEvent down(QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
    QMouseEvent move(QEvent::MouseMove, position + QPoint(1,1), Qt::NoButton, Qt::LeftButton, Qt::ShiftModifier);
    QMouseEvent up(QEvent::MouseButtonRelease, position + QPoint(1,1), Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier);
    QCoreApplication::sendEvent(tree->viewport(), &down);
    QCoreApplication::sendEvent(tree->viewport(), &move);
    CHECK(tree->selectedItems() == QList<QTreeWidgetItem*>{item(group)});
    QCoreApplication::sendEvent(tree->viewport(), &up); flush();
    CHECK(tree->selectedItems() == QList<QTreeWidgetItem*>{item(group)});
    CHECK(tree->currentItem() == item(group));
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{group});
    CHECK((column == 1 ? !outside->visible() : outside->locked()));
}

TEST_CASE_METHOD(LayerTreeTest, "Delete in the inspector removes its own selection and supports undo", "[LayerTree]") {
    document->selectNodes({outside});
    tree->setSelectionSyncEnabled(false);
    SECTION("Visible object") {}
    SECTION("Hidden object") { document->hide({a}); }
    SECTION("Object also selected in editor") { document->selectNodes({a}); }
    select(a);
    QKeyEvent override(QEvent::ShortcutOverride, Qt::Key_Delete, Qt::NoModifier);
    override.ignore(); QCoreApplication::sendEvent(tree, &override);
    CHECK(override.isAccepted());
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QCoreApplication::sendEvent(tree, &key); flush();
    CHECK(entity->childCount() == 1);
    CHECK(item(a) == nullptr);
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{outside});
    document->undoCommand(); flush();
    CHECK(entity->childCount() == 2);
    CHECK(item(a));
}

TEST_CASE_METHOD(LayerTreeTest, "Delete in the inspector protects locks and active editing modes", "[LayerTree]") {
    SECTION("Locked row") { document->lock({a}); select(a); }
    SECTION("Locked descendant of selected group") { document->lock({a}); select(group); }
    SECTION("Locked container which would become empty") {
        document->lock({nested}); document->unlock({entity});
        REQUIRE(entity->editable()); select(entity);
    }
    SECTION("Selection blocked by editing tool") { select(a); document->editorContext().setBlockSelection(true); }
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
    QCoreApplication::sendEvent(tree, &key); flush();
    CHECK(entity->childCount() == 2);
    CHECK(item(a));
    CHECK(item(group));
    document->editorContext().setBlockSelection(false);
}

TEST_CASE_METHOD(LayerTreeTest, "Deleting a brush also clears and restores its selected faces", "[LayerTree]") {
    tree->setSelectionSyncEnabled(false);
    document->selectBrushFaces({Model::BrushFaceHandle(a,0)});
    select(a);
    REQUIRE(document->hasSelectedBrushFaces());
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QCoreApplication::sendEvent(tree, &key); flush();
    CHECK_FALSE(document->hasSelection());
    CHECK(item(a) == nullptr);
    document->undoCommand(); flush();
    CHECK(item(a));
    CHECK(document->selectedBrushFaces() == std::vector<Model::BrushFaceHandle>{Model::BrushFaceHandle(a,0)});
}

TEST_CASE_METHOD(LayerTreeTest, "Deleting all children clears selections of automatically removed parents", "[LayerTree]") {
    select(group);
    tree->setSelectionSyncEnabled(false);
    select(a); select(b, true);
    QTimer::singleShot(0, tree, [] {
        if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            message->button(QMessageBox::Yes)->click();
    });
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QCoreApplication::sendEvent(tree, &key); flush();
    CHECK_FALSE(document->hasSelection());
    CHECK(layer->childCount() == 1);
    CHECK(item(group) == nullptr);
    document->undoCommand(); flush();
    CHECK(item(group)); CHECK(item(a)); CHECK(item(b));
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{group});
}

TEST_CASE_METHOD(LayerTreeTest, "Deleting multiple inspector objects requires confirmation", "[LayerTree]") {
    tree->setSelectionSyncEnabled(false);
    select(a); select(b, true);
    bool confirmed = false;
    const auto answer = GENERATE(QMessageBox::No, QMessageBox::Yes);
    QTimer::singleShot(0, tree, [&] {
        if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            confirmed = message->text().contains("2 objects");
            message->button(answer)->click();
        }
    });
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QCoreApplication::sendEvent(tree, &key); flush();
    REQUIRE(confirmed);
    CHECK((item(a) != nullptr) == (answer == QMessageBox::No));
    CHECK((item(b) != nullptr) == (answer == QMessageBox::No));
    if (answer == QMessageBox::Yes) {
        document->undoCommand(); flush();
        CHECK(item(a)); CHECK(item(b));
    }
}

TEST_CASE_METHOD(LayerTreeTest, "Hiding a descendant only deselects an ancestor if it has no visible contents", "[LayerTree]") {
    Model::Node* descendant = nullptr;
    SECTION("One brush remains visible") { descendant = a; }
    SECTION("All contents are hidden") { descendant = entity; }
    REQUIRE(descendant);
    showEditor();
    select(group);
    select(outside, true);
    REQUIRE(group->selected());
    REQUIRE(outside->selected());
    auto* row = item(descendant);
    REQUIRE(row);
    for (auto* parent = row->parent(); parent; parent = parent->parent())
        parent->setExpanded(true);
    flush();

    clickAt(tree->viewport(), cellCenter(descendant, 1));
    CHECK_FALSE(descendant->visible());
    CHECK_FALSE(item(descendant)->isSelected());
    CHECK(group->selected() == (descendant == a));
    CHECK(item(outside)->isSelected());
    CHECK(outside->selected());
    CHECK(document->selectedNodes().nodes().size() == (descendant == a ? 2u : 1u));

    document->undoCommand();
    flush();
    CHECK(descendant->visible());
    CHECK(group->selected());
    CHECK(outside->selected());
    CHECK(document->selectedNodes().nodes().size() == 2);
}

TEST_CASE_METHOD(LayerTreeTest, "Hiding a selected face preserves a deferred tree selection", "[LayerTree]") {
    const auto threshold = TemporarilySetPref{Preferences::LayerTreeSelectionSyncThreshold, 2};
    const auto face = Model::BrushFaceHandle{outside, 0};
    document->selectBrushFaces({face});
    REQUIRE(document->hasSelectedBrushFaces());
    showEditor();
    clickAt(tree->viewport(), cellCenter(group, 0));
    REQUIRE(tree->selectedItems() == QList<QTreeWidgetItem*>{item(group)});
    REQUIRE(document->hasSelectedBrushFaces());

    clickAt(tree->viewport(), cellCenter(outside, 1));
    CHECK_FALSE(outside->visible());
    CHECK_FALSE(document->hasSelectedBrushFaces());
    CHECK_FALSE(document->hasSelectedNodes());
    CHECK(tree->selectedItems() == QList<QTreeWidgetItem*>{item(group)});
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree shows all descendants and restores their visibility on undo", "[LayerTree]") {
    Model::Node *root = nullptr;
    SECTION("Layer") { root = layer; }
    SECTION("Group") { root = group; }
    SECTION("Entity") { root = entity; }
    REQUIRE(root);
    document->hide({a, outside});
    document->hide({root});
    const auto oldA = a->visibilityState();
    const auto oldB = b->visibilityState();
    const auto oldRoot = root->visibilityState();
    const auto oldOutside = outside->visibilityState();

    tree->nodeVisibilityToggled(root);
    CHECK(root->visible());
    CHECK(a->visible());
    CHECK(b->visible());
    CHECK(outside->visible() == (root == layer));
    if (root != entity) {
        CHECK(nested->visible());
        CHECK(entity->visible());
    }

    document->undoCommand();
    CHECK(root->visibilityState() == oldRoot);
    CHECK(a->visibilityState() == oldA);
    CHECK(b->visibilityState() == oldB);
    CHECK(outside->visibilityState() == oldOutside);
    CHECK_FALSE(root->visible());
    CHECK_FALSE(a->visible());
    document->redoCommand();
    CHECK(root->visible());
    CHECK(a->visible());
    CHECK(b->visible());
    flush();
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree removes stale nodes and follows undo and reparenting", "[LayerTree]") {
    item(group)->setExpanded(true);
    select(a);
    document->deleteObjects();
    flush();
    CHECK(item(a) == nullptr);
    CHECK(item(b));
    document->undoCommand();
    flush();
    REQUIRE(item(a));
    CHECK(item(a)->isSelected());
    REQUIRE(document->reparentNodes({{layer, {a}}}));
    flush();
    CHECK(item(a)->parent() == item(layer));
    CHECK(item(group)->isExpanded());
    document->undoCommand();
    flush();
    CHECK(item(a)->parent() == item(entity));
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree preserves expansion and container selection across changes", "[LayerTree]") {
    item(group)->setExpanded(false);
    select(layer);
    document->translateObjects(vm::vec3(16, 0, 0));
    flush();
    CHECK_FALSE(item(group)->isExpanded());
    CHECK(item(layer)->isSelected());
    CHECK_FALSE(item(group)->isSelected());
    CHECK_FALSE(item(outside)->isSelected());
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree follows layer ordering, active layer and new documents", "[LayerTree]") {
    auto *extra = new Model::LayerNode(Model::Layer("Extra"));
    document->addNodes({{document->world(), {extra}}});
    document->setCurrentLayer(extra);
    flush();
    REQUIRE(item(extra));
    CHECK(item(extra)->data(0, LayerTreeItemDelegate::CurrentLayerRole).toBool());
    CHECK_FALSE(item(layer)->font(0).bold());
    document->renameLayer(extra, "Renamed");
    flush();
    CHECK(item(extra)->text(0) == "Renamed");
    REQUIRE(document->newDocument(Model::MapFormat::Standard, vm::bbox3(8192.0), game).is_success());
    flush();
    CHECK(tree->topLevelItemCount() == 1);
    CHECK(tree->topLevelItem(0)->data(0, Qt::UserRole).value<quintptr>() == reinterpret_cast<quintptr>(document->world()->defaultLayer()));
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree groups only matching siblings above the configured threshold", "[LayerTree]") {
    tree->setGroupByType(true);
    Model::BrushBuilder builder(Model::MapFormat::Standard, document->worldBounds());
    std::vector<Model::Node*> extra;
    for (int i = 0; i < 4; ++i) extra.push_back(new Model::BrushNode(builder.createCube(16, "").value()));
    document->addNodes({{layer, extra}});
    flush();
    CHECK(item(outside)->parent() == item(layer)); // Exactly five stays flat.
    auto* sixth = new Model::BrushNode(builder.createCube(16, "").value());
    document->addNodes({{layer, {sixth}}});
    flush();
    auto* bucket = item(outside)->parent();
    REQUIRE(bucket != item(layer));
    CHECK(bucket->parent() == item(layer));
    CHECK(bucket->childCount() == 6);
    CHECK(bucket->flags() & Qt::ItemIsSelectable);
    CHECK_FALSE(bucket->isExpanded());
    CHECK(item(group)->parent() == item(layer));
    CHECK(item(a)->parent() == item(entity)); // Other levels are counted independently.
    CHECK(outside->parent() == layer); // This never changes the map hierarchy.
    tree->setGroupingThreshold(1);
    CHECK(item(a)->parent() != item(entity));
    CHECK(item(a)->parent()->parent() == item(entity));
    auto* originalItem = item(outside);
    bucket->setExpanded(true);
    select(outside);
    tree->setGroupByType(false);
    CHECK(item(outside) == originalItem);
    CHECK(item(outside)->parent() == item(layer));
    CHECK(item(outside)->isSelected());
    CHECK(outside->selected());
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree type groups survive edits and dissolve on removal and reparenting", "[LayerTree]") {
    tree->setGroupingThreshold(1);
    tree->setGroupByType(true);
    auto* bucket = item(a)->parent();
    bucket->setExpanded(true);
    select(a);
    document->translateObjects(vm::vec3(8, 0, 0));
    flush();
    CHECK(item(a)->parent() == bucket);
    CHECK(bucket->isExpanded());
    REQUIRE(document->reparentNodes({{layer, {a}}}));
    flush();
    CHECK(item(b)->parent() == item(entity));
    CHECK(item(a)->parent() == item(outside)->parent());
    CHECK(item(a)->parent()->childCount() == 2);
    document->undoCommand();
    flush();
    CHECK(item(a)->parent() == item(b)->parent());
    CHECK(item(a)->parent()->parent() == item(entity));
    CHECK(item(outside)->parent() == item(layer));
    select(a);
    document->deleteObjects();
    flush();
    CHECK(item(a) == nullptr);
    CHECK(item(b)->parent() == item(entity));
    document->undoCommand();
    flush();
    REQUIRE(item(a));
    CHECK(item(a)->parent() == item(b)->parent());
    CHECK(item(a)->isSelected());
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree selection synchronization can be disconnected in both directions", "[LayerTree]") {
    document->selectNodes({a});
    tree->setSelectionSyncEnabled(false);
    select(outside);
    CHECK(a->selected());
    CHECK_FALSE(outside->selected());
    showEditor();
    item(layer)->setExpanded(true);
    flush();
    doubleClick(outside);
    CHECK(a->selected());
    CHECK_FALSE(outside->selected());
    document->deselectAll();
    document->selectNodes({b});
    CHECK(item(outside)->isSelected());
    CHECK_FALSE(item(b)->isSelected());
    tree->setGroupingThreshold(1);
    tree->setGroupByType(true);
    flush();
    CHECK(item(outside)->isSelected());
    CHECK_FALSE(item(b)->isSelected()); // Structural changes must not reconnect selection.
    CHECK(b->selected());
    tree->setSelectionSyncEnabled(true);
    CHECK(item(b)->isSelected());
    CHECK_FALSE(item(outside)->isSelected());
    CHECK(item(b)->parent()->isExpanded());
    CHECK(b->selected());
    CHECK(document->selectedNodes().nodes().size() == 1);
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree toolbar buttons and thresholds configure the tree", "[LayerTree]") {
    const auto originalSyncThreshold = pref(Preferences::LayerTreeSelectionSyncThreshold);
    const auto restoreSyncThreshold = qScopeGuard([&] {
        setPref(Preferences::LayerTreeSelectionSyncThreshold, originalSyncThreshold);
    });
    QToolButton *groupButton = nullptr, *syncButton = nullptr;
    for (auto* button : editor->findChildren<QToolButton*>()) {
        if (button->property("layerTreeControl") == "groupByType") groupButton = button;
        if (button->property("layerTreeControl") == "syncSelection") syncButton = button;
    }
    REQUIRE(groupButton);
    REQUIRE(syncButton);
    // Exercise the real stylesheet and mouse hit areas, rather than click(),
    // which bypasses the split menu hit testing entirely.
    const auto stylesheetPath = testSourceRoot()
        / "app/resources/stylesheets/base.qss";
    QFile stylesheet(QString::fromStdString(stylesheetPath.string()));
    REQUIRE(stylesheet.open(QIODevice::ReadOnly));
    auto toolbarStyle = QString::fromUtf8(stylesheet.readAll());
    toolbarStyle = toolbarStyle.mid(toolbarStyle.indexOf("QToolButton {"));
    toolbarStyle = toolbarStyle.left(toolbarStyle.indexOf("/* ---"));
    toolbarStyle.replace(QRegularExpression("#[A-Z_]+"), "palette(mid)");
    editor->setStyleSheet(toolbarStyle);
    showEditor();
    CHECK_FALSE(groupButton->isChecked());
    CHECK(syncButton->isChecked());
    auto* threshold = groupButton->menu()->findChild<QSpinBox*>();
    REQUIRE(threshold);
    threshold->setValue(1);
    const auto before = groupButton->grab().toImage();
    clickAt(groupButton, QPoint(12, groupButton->height() / 2));
    CHECK(groupButton->isChecked());
    const auto after = groupButton->grab().toImage();
    CHECK(before != after);
    // The checked state must have a visible fill away from the icon itself.
    CHECK(before.pixelColor(5, 5) != after.pixelColor(5, 5));
    if (qEnvironmentVariableIsSet("TB_LAYER_CONTROLS_IMAGE")) editor->grab().save(qEnvironmentVariable("TB_LAYER_CONTROLS_IMAGE") + ".active.png");
    CHECK(tree->groupByType());
    CHECK(tree->groupingThreshold() == 1);
    CHECK(item(a)->parent() == item(b)->parent());
    CHECK(item(a)->parent() != item(entity));
    auto* syncMenu = syncButton->menu();
    REQUIRE(syncMenu);
    auto* syncThreshold = syncMenu->findChild<QSpinBox*>();
    REQUIRE(syncThreshold);
    CHECK(syncThreshold->value() == originalSyncThreshold);
    auto syncMenuOpened = false;
    QTimer::singleShot(0, syncMenu, [&] {
        syncMenuOpened = syncMenu->isVisible();
        syncThreshold->setValue(2);
        syncMenu->close();
    });
    clickAt(syncButton, QPoint(syncButton->width() - 5, syncButton->height() / 2));
    CHECK(syncMenuOpened);
    CHECK(pref(Preferences::LayerTreeSelectionSyncThreshold) == 2);
    CHECK(syncButton->isChecked());
    CHECK(tree->selectionSyncEnabled());
    clickAt(syncButton, QPoint(12, syncButton->height() / 2));
    CHECK_FALSE(tree->selectionSyncEnabled());
    CHECK_FALSE(pref(Preferences::LayerTreeSyncSelection));
    CHECK(pref(Preferences::LayerTreeGroupByType));
    CHECK(pref(Preferences::LayerTreeGroupingThreshold) == 1);
    clickAt(groupButton, QPoint(12, groupButton->height() / 2));
    CHECK_FALSE(groupButton->isChecked());
    CHECK_FALSE(tree->groupByType());
    CHECK(item(a)->parent() == item(entity));
    bool menuOpened = false;
    QTimer::singleShot(0, groupButton->menu(), [&] {
        menuOpened = groupButton->menu()->isVisible();
        threshold->setValue(2);
        groupButton->menu()->close();
    });
    clickAt(groupButton, QPoint(groupButton->width() - 5, groupButton->height() / 2));
    CHECK(menuOpened);
    CHECK(tree->groupingThreshold() == 2);
    CHECK_FALSE(tree->groupByType()); // Changing the limit does not toggle the option.
    if (qEnvironmentVariableIsSet("TB_LAYER_CONTROLS_IMAGE")) editor->grab().save(qEnvironmentVariable("TB_LAYER_CONTROLS_IMAGE"));
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree virtual groups select members with mouse and survive regrouping", "[LayerTree]") {
    tree->setGroupingThreshold(1);
    tree->setGroupByType(true);
    editor->resize(400, 600);
    editor->show();
    tree->expandAll();
    auto* bucket = item(a)->parent();
    bucket->setExpanded(false);
    flush();
    clickAt(tree->viewport(), tree->visualItemRect(bucket).center());
    CHECK(bucket->isSelected());
    CHECK(a->selected());
    CHECK(b->selected());
    CHECK(document->selectedNodes().nodes().size() == 2);
    select(a, true);
    CHECK(document->selectedNodes().nodes().size() == 2); // No duplicate member.
    tree->setGroupByType(false);
    CHECK(item(a)->isSelected());
    CHECK(item(b)->isSelected());
    CHECK(document->selectedNodes().nodes().size() == 2);
    tree->setGroupByType(true);
    bucket = item(a)->parent();
    CHECK(bucket->isExpanded()); // Grouping must not hide existing selection.
    tree->setCurrentItem(bucket, 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    // A structural edit elsewhere must preserve the selected folder.
    auto* extra = new Model::EntityNode(Model::Entity({}, {{"classname", "info_player_start"}}));
    document->addNodes({{layer, {extra}}});
    flush();
    CHECK(item(a)->parent()->isSelected());
    CHECK_FALSE(item(a)->isSelected());
    tree->setSelectionSyncEnabled(false);
    document->deselectAll();
    document->selectNodes({outside});
    tree->setGroupByType(false);
    CHECK(item(a)->isSelected());
    CHECK(item(b)->isSelected());
    CHECK_FALSE(a->selected());
    CHECK(outside->selected());
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree groups light families and entity classes inside nested type groups", "[LayerTree]") {
    std::vector<Model::Node*> lights, monsters;
    for (int i = 0; i < 6; ++i) {
        lights.push_back(new Model::EntityNode(Model::Entity({}, {{"classname", i % 2 ? "light" : "light_torch_small_walltorch"}})));
        monsters.push_back(new Model::EntityNode(Model::Entity({}, {{"classname", "monster_ogre"}})));
    }
    document->addNodes({{nested, lights}, {group, monsters}});
    tree->setGroupByType(true);
    flush();
    auto* lightBucket = item(lights.front())->parent();
    REQUIRE(lightBucket != item(nested));
    CHECK(lightBucket->text(0) == "Lights");
    CHECK(lightBucket->data(0, LayerTreeItemDelegate::AddendumRole).toString() == "(6)");
    CHECK(lightBucket->childCount() == 6);
    CHECK(lightBucket->parent()->text(0) == "Entities");
    CHECK(lightBucket->parent()->data(0, LayerTreeItemDelegate::AddendumRole).toString() == "(7)");
    CHECK(lightBucket->parent()->parent() == item(nested));
    CHECK(item(monsters.front())->parent()->text(0) == "monster_ogre");
    CHECK(item(monsters.front())->parent()->data(0, LayerTreeItemDelegate::AddendumRole).toString() == "(6)");
    tree->setCurrentItem(lightBucket, 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    CHECK(document->selectedNodes().nodes().size() == 6);
    for (auto* node : lights) CHECK(node->selected());
    select(lights.front());
    document->setProperty("classname", "info_player_start");
    flush();
    CHECK(item(lights.back())->parent()->text(0) == "Entities"); // Five lights remain flat.
    document->undoCommand();
    flush();
    CHECK(item(lights.front())->parent()->text(0) == "Lights");
    document->hide({lights.front()});
    document->lock({lights.back()});
    tree->setCurrentItem(item(lights[1])->parent(), 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    CHECK(document->selectedNodes().nodes().size() == 4);
    CHECK_FALSE(lights.front()->selected());
    CHECK_FALSE(lights.back()->selected());
    // Selecting an outer virtual group also traverses inner virtual folders.
    tree->setCurrentItem(item(lights[1])->parent()->parent(), 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    CHECK(document->selectedNodes().nodes().size() == 6); // Four lights and func_detail's two brushes.
    CHECK(a->selected());
    CHECK(b->selected());
    if (qEnvironmentVariableIsSet("TB_LAYER_GROUPS_IMAGE")) {
        editor->resize(440, 680);
        editor->show();
        tree->expandAll();
        item(lights[1])->parent()->setExpanded(false);
        tree->setCurrentItem(item(lights[1])->parent(), 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        flush();
        editor->grab().save(qEnvironmentVariable("TB_LAYER_GROUPS_IMAGE"));
    }
}

TEST_CASE_METHOD(LayerTreeTest, "LayerTree selection has one continuous background across every column", "[LayerTree]") {
    tree->setStyleSheet(R"(
        QWidget#LayerTreeRow { background: transparent; border: none; border-radius: 6px;
            qproperty-paddingTop: 4; qproperty-paddingBottom: 4; qproperty-rowInsetX: 2; qproperty-rowInsetY: 1; }
        QWidget#LayerTreeRow[selected="true"] { background-color: #2468ac; }
        QTreeWidget::item { background: transparent; border: none; }
    )");
    editor->resize(360, 600);
    editor->show();
    tree->expandAll();
    tree->setFocus();
    select(a);
    flush();
    const auto rect = tree->visualItemRect(item(a));
    REQUIRE(rect.isValid());
    const auto image = tree->viewport()->grab().toImage();
    if (qEnvironmentVariableIsSet("TB_LAYER_TEST_IMAGE")) image.save(qEnvironmentVariable("TB_LAYER_TEST_IMAGE"));
    const auto ratio = image.devicePixelRatio();
    const auto pixel = [&](const int x, const int y) { return image.pixelColor(qRound(x * ratio), qRound(y * ratio)); };
    const int y = rect.top() + 3; // Above the content, below the rounded outer corner.
    const auto expected = pixel(10, y);
    CHECK(expected != pixel(0, rect.top()));
    for (int column = 1; column < tree->columnCount(); ++column) {
        const int boundary = tree->columnViewportPosition(column);
        CHECK(pixel(boundary - 1, y) == expected);
        CHECK(pixel(boundary, y) == expected);
        CHECK(pixel(boundary + 1, y) == expected);
    }
    CHECK(pixel(2, rect.top() + 1) != expected); // Rounded outside corner.
}

TEST_CASE_METHOD(LayerTreeTest, "Layer row colors and type-specific layouts are controlled by QSS", "[LayerTree]") {
    tree->setStyleSheet(R"(
        QTreeWidget#LayerTree { qproperty-attributeColumnWidth: 31; qproperty-indentation: 18; qproperty-iconSize: 12px 12px; }
        QWidget#LayerTreeRow { color: #112233; font-size: 17px; qproperty-paddingLeft: 7;
            qproperty-paddingTop: 4; qproperty-paddingBottom: 4; qproperty-contentIconSize: 16;
            qproperty-iconSpacing: 9; qproperty-detailOpacity: 1; qproperty-lineSpacing: 2; }
        QWidget#LayerTreeRow[nodeType="brush"] { qproperty-paddingTop: 10; qproperty-paddingBottom: 10; }
        QWidget#LayerTreeRow[currentLayer="true"] { font-weight: bold; }
        QWidget#LayerTreeRow[hidden="true"] { color: #2288dd; }
        QWidget#LayerTreeRow[locked="true"] { color: #dd8822; }
        QWidget#LayerTreeRow[hidden="true"][locked="true"] { color: #bb44aa; }
    )");
    tree->setSelectionSyncEnabled(false);
    showEditor(); tree->expandAll(); flush();
    CHECK(tree->attributeColumnWidth() == 31); CHECK(tree->columnWidth(1) == 31);
    CHECK(tree->indentation() == 18); CHECK(tree->iconSize() == QSize(12,12));
    auto* delegate = tree->itemDelegate();
    const auto imageFor = [&] {
        const auto index = tree->indexAt(cellCenter(a,0));
        QStyleOptionViewItem option; option.initFrom(tree); option.rect = QRect(0,0,300,100);
        option.state |= QStyle::State_Selected;
        QImage image(300,100,QImage::Format_ARGB32); image.fill(Qt::transparent);
        QPainter painter(&image); delegate->paint(&painter,option,index); painter.end(); return image;
    };
    const auto contains = [&](const QImage& image, QColor color) {
        for (int y=0;y<image.height();++y) for (int x=0;x<image.width();++x) if (image.pixelColor(x,y) == color) return true;
        return false;
    };
    CHECK(contains(imageFor(), QColor("#112233")));
    document->hide({a}); flush(); CHECK(contains(imageFor(), QColor("#2288dd")));
    document->show({a}); document->lock({a}); flush(); CHECK(contains(imageFor(), QColor("#dd8822")));
    document->hide({a}); flush(); CHECK(contains(imageFor(), QColor("#bb44aa")));
    const auto styles = tree->findChildren<LayerTreeRowStyle*>(); REQUIRE(!styles.empty());
    const auto found = std::find_if(styles.begin(),styles.end(),[](const auto* style) { return style->property("nodeType").toString() == "brush"; });
    REQUIRE(found != styles.end()); auto* brushStyle = *found;
    CHECK(brushStyle->paddingLeft == 7); CHECK(brushStyle->paddingTop == 10);
    CHECK(brushStyle->contentIconSize == 16); CHECK(brushStyle->font().pixelSize() == 17);
    const auto current = std::find_if(styles.begin(),styles.end(),[](const auto* style) { return style->property("currentLayer").toBool(); });
    REQUIRE(current != styles.end()); CHECK((*current)->font().bold());
    const auto oldHeight = tree->visualItemRect(item(a)).height();
    tree->setStyleSheet(tree->styleSheet() + "\nQWidget#LayerTreeRow[nodeType=\"brush\"] { qproperty-paddingTop: 20; qproperty-paddingBottom: 20; }");
    flush();
    CHECK(tree->visualItemRect(item(a)).height() == oldHeight+20);
}

TEST_CASE_METHOD(LayerTreeTest, "Layer detail color and disclosure size can be changed independently in QSS", "[LayerTree]") {
    tree->setStyleSheet(R"(
        QTreeWidget#LayerTree { qproperty-indentation: 28; qproperty-branchIndicatorSize: 6;
            qproperty-branchIndicatorColor: #22ff44; qproperty-branchIndicatorLineWidth: 2; }
        QWidget#LayerTreeRow { color: #dd4422; font-size: 17px; qproperty-detailColor: #2266ee;
            qproperty-detailOpacity: 1; qproperty-paddingTop: 4; qproperty-paddingBottom: 4; }
    )");
    showEditor(); tree->expandAll(); flush();
    CHECK(tree->branchIndicatorSize() == 6);
    const auto index = tree->indexAt(cellCenter(a,0));
    QStyleOptionViewItem option; option.initFrom(tree); option.rect = QRect(0,0,300,100);
    QImage image(300,100,QImage::Format_ARGB32); image.fill(Qt::transparent);
    QPainter painter(&image); tree->itemDelegate()->paint(&painter,option,index); painter.end();
    bool title=false, detail=false;
    for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) {
        title |= image.pixelColor(x,y) == QColor("#dd4422"); detail |= image.pixelColor(x,y) == QColor("#2266ee");
    }
    CHECK(title); CHECK(detail);
    const auto disclosure = [&] {
        const auto shot = tree->viewport()->grab().toImage(); const auto ratio = shot.devicePixelRatio();
        const auto row = tree->visualItemRect(item(group)); QRect bounds;
        for(int y=qRound(row.top()*ratio);y<qRound((row.bottom()+1)*ratio);++y)
            for(int x=0;x<qRound(row.left()*ratio);++x) {
                const auto color = shot.pixelColor(x,y);
                if(color.green()>180 && color.red()<100 && color.blue()<130) bounds = bounds.united(QRect(x,y,1,1));
            }
        return bounds;
    };
    const auto small = disclosure(); REQUIRE(!small.isEmpty());
    tree->setStyleSheet(tree->styleSheet()+"\nQTreeWidget#LayerTree { qproperty-branchIndicatorSize: 18; }"); flush();
    const auto large = disclosure(); CHECK(large.width() > small.width());
    CHECK(tree->branchIndicatorSize() == 18);
    const auto row = tree->visualItemRect(item(group));
    clickAt(tree->viewport(),QPoint(row.left()-tree->indentation()/2,row.center().y()));
    CHECK_FALSE(item(group)->isExpanded());
}
TEST_CASE_METHOD(LayerTreeTest, "Layer types supply independent title detail and addendum content", "[LayerTree][layer-labels]") {
    CHECK(item(layer)->text(0) == "Default Layer");
    CHECK(item(layer)->data(0, LayerTreeItemDelegate::AddendumRole).toString() == "Active layer");
    CHECK_FALSE(item(layer)->data(0, LayerTreeItemDelegate::DetailRole).toString().contains("Active layer"));
    CHECK(item(a)->text(0) == "Brush");
    CHECK(item(a)->data(0, LayerTreeItemDelegate::AddendumRole).toString().startsWith('#'));
    CHECK(item(a)->data(0, LayerTreeItemDelegate::DetailRole).toString().contains("6 faces"));
    document->selectNodes({entity});
    document->setProperty("targetname", "door <main>");
    flush();
    CHECK(item(entity)->text(0) == "func_detail");
    CHECK(item(entity)->data(0, LayerTreeItemDelegate::AddendumRole).toString() == "door <main>");
    CHECK_FALSE(item(entity)->data(0, LayerTreeItemDelegate::DetailRole).toString().contains("door <main>"));
    CHECK(item(entity)->data(0, Qt::AccessibleTextRole).toString() == "func_detail door <main>");
    CHECK(item(entity)->toolTip(0).contains("door <main>"));
}

TEST_CASE("Layer labels retain independent QSS styles and align within a non-overlapping upper line", "[LayerTree][layer-labels]") {
    QTreeWidget view;
    auto* delegate = new LayerTreeItemDelegate(&view);
    view.setItemDelegate(delegate);
    const QString rules = R"(
        QWidget#LayerTreeRow { color: #123456; font-size: 17px; qproperty-contentIconSize: 0;
            qproperty-iconSpacing: 0; qproperty-lineSpacing: 3; qproperty-detailOpacity: 1; qproperty-addendumSpacing: 12; }
        QLabel#LayerTreeTitle { color: #ee2233; font-size: 21px; font-weight: bold; background: transparent; border: none; }
        QLabel#LayerTreeDetail { color: #2255ee; font-size: 12px; background: transparent; border: none; }
        QLabel#LayerTreeAddendum { color: #22cc55; font-size: 16px; background: transparent; border: none;
            qproperty-alignment: 'AlignRight | AlignVCenter'; }
    )";
    view.setStyleSheet(rules);
    auto* item = new QTreeWidgetItem(&view, {"MMMM"});
    item->setData(0, LayerTreeItemDelegate::DetailRole, "Detail text");
    item->setData(0, LayerTreeItemDelegate::AddendumRole, "Addendum");
    item->setData(0, LayerTreeItemDelegate::RendererRole, int(LayerTreeItemDelegate::Group));
    const auto index = view.model()->index(0, 0);
    QStyleOptionViewItem option;
    option.initFrom(&view);
    option.state |= QStyle::State_Enabled | QStyle::State_Active;
    option.rect = QRect(0, 0, 600, 100);
    const auto render = [&] {
        QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        delegate->paint(&painter, option, index);
        return image;
    };
    const auto bounds = [](const QImage& image, const QColor& color) {
        QRect result;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (image.pixelColor(x, y) == color) result = result.united(QRect(x, y, 1, 1));
        return result;
    };
    const auto image = render();
    const auto styles = view.findChildren<LayerTreeRowStyle*>();
    REQUIRE(styles.size() == 1);
    auto& style = *styles.front();
    CHECK(style.findChildren<QLabel*>().size() == 3);
    CHECK(style.titleLabel().font().pixelSize() == 21);
    CHECK(style.detailLabel().font().pixelSize() == 12);
    CHECK(style.addendumLabel().font().pixelSize() == 16);
    REQUIRE_FALSE(bounds(image, QColor("#ee2233")).isEmpty());
    REQUIRE_FALSE(bounds(image, QColor("#2255ee")).isEmpty());
    const auto right = bounds(image, QColor("#22cc55"));
    REQUIRE_FALSE(right.isEmpty());
    CHECK(style.titleLabel().geometry().top() == style.addendumLabel().geometry().top());
    CHECK(style.detailLabel().geometry().top() > style.titleLabel().geometry().bottom());
    CHECK(style.addendumLabel().geometry().left() - style.titleLabel().geometry().right() == 13);
    view.setStyleSheet(rules + "\nQLabel#LayerTreeAddendum { qproperty-alignment: 'AlignLeft | AlignVCenter'; }");
    const auto left = bounds(render(), QColor("#22cc55"));
    view.setStyleSheet(rules + "\nQLabel#LayerTreeAddendum { qproperty-alignment: 'AlignHCenter | AlignVCenter'; }");
    const auto center = bounds(render(), QColor("#22cc55"));
    CHECK(left.left() < center.left());
    CHECK(center.left() < right.left());
    item->setText(0, "<b>A long title that must remain plain text</b>");
    item->setData(0, LayerTreeItemDelegate::AddendumRole, "A very long addendum");
    option.rect.setWidth(90);
    render();
    CHECK(style.titleLabel().textFormat() == Qt::PlainText);
    CHECK(style.titleLabel().text() == item->text(0));
    CHECK_FALSE(style.titleLabel().geometry().intersects(style.addendumLabel().geometry()));
    CHECK(style.addendumLabel().geometry().right() < option.rect.right() + 1);
    const auto height = delegate->sizeHint(option, index).height();
    view.setStyleSheet(rules + "\nQLabel#LayerTreeAddendum { font-size: 42px; padding: 4px; }");
    CHECK(delegate->sizeHint(option, index).height() > height);
}

TEST_CASE("Layer text widgets are reused across many scene rows", "[LayerTree][layer-labels]") {
    QTreeWidget view;
    auto* delegate = new LayerTreeItemDelegate(&view);
    view.setItemDelegate(delegate);
    QStyleOptionViewItem option;
    option.initFrom(&view);
    option.rect = QRect(0, 0, 300, 60);
    option.state |= QStyle::State_Enabled;
    QImage image(option.rect.size(), QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    for (int i = 0; i < 300; ++i) {
        auto* item = new QTreeWidgetItem(&view, {QString("Brush %1").arg(i)});
        item->setData(0, LayerTreeItemDelegate::RendererRole, int(LayerTreeItemDelegate::Brush));
        item->setData(0, LayerTreeItemDelegate::AddendumRole, QString::number(i));
        delegate->paint(&painter, option, view.model()->index(i, 0));
    }
    CHECK(view.findChildren<LayerTreeRowStyle*>().size() == 1);
    CHECK(view.findChildren<QLabel*>().size() == 3);
}

TEST_CASE_METHOD(LayerTreeTest, "The Layer toolbar switches only the detail text rasterizer", "[LayerTree][layer-detail-renderer]") {
    const auto restore = TemporarilySetPref{layerDetailTextRendererPreference(), int(LayerDetailTextRenderer::Qt)};
    auto* button = editor->findChild<QToolButton*>("layerDetailTextRenderer");
    REQUIRE(button);
    REQUIRE(button->menu());
    const auto actionFor = [&](LayerDetailTextRenderer renderer) {
        for (auto* action : button->menu()->actions()) {
            if (action->isCheckable() && action->data().toInt() == int(renderer)) return action;
        }
        return static_cast<QAction*>(nullptr);
    };
    auto* qt = actionFor(LayerDetailTextRenderer::Qt);
    auto* bitmap = actionFor(LayerDetailTextRenderer::FontStashBitmap);
    auto* sdf = actionFor(LayerDetailTextRenderer::FontStashSdf);
    REQUIRE(qt);
    REQUIRE(bitmap);
    REQUIRE(sdf);
    CHECK(qt->isChecked());

    tree->setStyleSheet(R"(
        QLabel#LayerTreeDetail { font-size: 13px; color: #6688cc; padding: 1px; }
        QLabel#LayerTreeTitle { font-size: 18px; }
    )");
    auto* row = item(group);
    REQUIRE(row);
    const auto detailText = QStringLiteral("Nested brushes \u00b7 32 \u00d7 64");
    row->setData(0, LayerTreeItemDelegate::DetailRole, detailText);
    select(group);
    const auto index = tree->indexFromItem(row, 0);
    auto option = QStyleOptionViewItem{};
    option.initFrom(tree);
    option.rect = QRect(0, 0, 350, 64);
    option.state |= QStyle::State_Enabled | QStyle::State_Active;
    const auto* delegate = tree->itemDelegate();
    const auto size = delegate->sizeHint(option, index);
    const auto ratio = GENERATE(1.0, 2.0);
    const auto render = [&] {
        auto image = QImage(option.rect.size() * ratio, QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(ratio);
        image.fill(Qt::transparent);
        auto painter = QPainter(&image);
        delegate->paint(&painter, option, index);
        return image;
    };

    // Prime the native widget paint before comparing the rasterizers.
    render();
    const auto native = render();
    const auto labels = tree->findChildren<QLabel*>("LayerTreeDetail");
    const auto detailLabel = std::ranges::find_if(labels, [&](const auto* label) { return label->text() == detailText; });
    REQUIRE(detailLabel != labels.end());
    const auto* detail = *detailLabel;
    const auto detailY = qRound(detail->geometry().top() * ratio);
    CAPTURE(detailY, ratio);
    REQUIRE(detailY > 0);
    REQUIRE(detailY < native.height());
    const auto upper = QRect(0, 0, native.width(), detailY);
    bitmap->trigger();
    const auto renderedBitmap = render();
    CHECK(layerDetailTextRenderer() == LayerDetailTextRenderer::FontStashBitmap);
    CHECK(bitmap->isChecked());
    CHECK_FALSE(qt->isChecked());
    CHECK(renderedBitmap != native);
    CHECK(renderedBitmap.copy(upper) == native.copy(upper));

    sdf->trigger();
    const auto renderedSdf = render();
    CHECK(layerDetailTextRenderer() == LayerDetailTextRenderer::FontStashSdf);
    CHECK(sdf->isChecked());
    CHECK_FALSE(bitmap->isChecked());
    CHECK(renderedSdf != native);
    CHECK(renderedSdf != renderedBitmap);
    CHECK(renderedSdf.copy(upper) == native.copy(upper));
    CHECK(delegate->sizeHint(option, index) == size);
    CHECK(row->isSelected());
    CHECK(group->selected());

    // The alternative rasterizer retains the label's QSS foreground and
    // produces actual glyph pixels at the target device scale.
    for (const auto renderer : {LayerDetailTextRenderer::FontStashBitmap, LayerDetailTextRenderer::FontStashSdf}) {
        const auto text = renderLayerDetailText(*detail, renderer, ratio);
        REQUIRE_FALSE(text.isNull());
        CHECK(text.devicePixelRatio() == ratio);
        auto hasStyledColor = false;
        for (auto y = 0; y < text.height(); ++y) {
            for (auto x = 0; x < text.width(); ++x) {
                if (text.pixelColor(x, y) == QColor("#6688cc")) hasStyledColor = true;
            }
        }
        CHECK(hasStyledColor);
    }

    qt->trigger();
    CHECK(layerDetailTextRenderer() == LayerDetailTextRenderer::Qt);
    CHECK(render() == native);
}

TEST_CASE_METHOD(LayerTreeTest, "Native layer rows show all opaque QLabel texts", "[.native-layer-labels]") {
    REQUIRE(QGuiApplication::platformName() == "cocoa");
    tree->setStyleSheet(R"(
        QWidget#LayerTreeRow { color: #ee2233; background: transparent;
            border-bottom: 1px solid #223344; border-radius: 6px;
            qproperty-detailColor: #2255ee; qproperty-detailOpacity: 1; }
        QLabel#LayerTreeTitle { font-size: 18px; background: transparent; border: none; }
        QLabel#LayerTreeDetail { font-size: 13px; background: transparent; border: none; }
        QLabel#LayerTreeAddendum { font-size: 13px; background: transparent; border: none; }
    )");
    tree->setSelectionSyncEnabled(false);
    showEditor();
    tree->expandAll();
    flush();
    const auto shot = tree->viewport()->grab().toImage();
    const auto ratio = shot.devicePixelRatio();
    const auto row = tree->visualItemRect(item(a));
    const auto contains = [&](const QColor& expected, const QRect& area) {
        for (int y = qRound(area.top() * ratio); y < qRound((area.bottom() + 1) * ratio); ++y)
            for (int x = qRound(area.left() * ratio); x < qRound((area.right() + 1) * ratio); ++x)
                if (shot.pixelColor(x, y) == expected) return true;
        return false;
    };
    CHECK(contains(QColor("#ee2233"), QRect(row.topLeft(), row.size() / 2)));
    CHECK(contains(QColor("#2255ee"), row.adjusted(0, row.height() / 2, 0, 0)));
    CHECK(contains(QColor("#ee2233"), row.adjusted(row.width() / 2, 0, 0, -row.height() / 2)));
}
} // namespace TrenchBroom::View
