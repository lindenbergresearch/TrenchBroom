#include "View/Tools/Brush/BrushUpstreamTest.h"
#include "View/Inspector/HistoryEditor.h"
#include "View/Inspector/Inspector.h"
#include <QApplication>
#include <QTreeWidget>
#include <QToolBar>
#include <QAction>
#include <QMenu>
#include <QTemporaryDir>
#include <QTest>

namespace TrenchBroom::View {
TEST_CASE_METHOD(BrushUpstreamTest, "History follows transactions undo redo and branches", "[history]") {
    newMap();
    HistoryEditor editor(document);
    auto* list = editor.findChild<QTreeWidget*>("HistoryList");
    REQUIRE(list);
    auto before = document->commandHistoryPosition();
    {
        Transaction transaction(document, "Two brushes");
        addBrush({{0, 0, 0}, {64, 64, 64}});
        addBrush({{128, 0, 0}, {192, 64, 64}});
        transaction.commit();
    }
    auto history = document->commandHistory();
    REQUIRE(history.size() == before + 1);
    CHECK(history.back().name == "Two brushes");
    CHECK(history.back().applied);
    CHECK(history.back().timestamp.time_since_epoch().count() > 0);
    const auto timestamp = history.back().timestamp;
    QApplication::processEvents();
    CHECK(list->topLevelItemCount() == int(history.size() + 1));
    CHECK(list->topLevelItem(int(history.size()))->text(0) == "Two brushes");
    CHECK_FALSE(list->topLevelItem(int(history.size()))->text(1).isEmpty());
    document->undoCommand();
    history = document->commandHistory();
    CHECK(history.size() == before + 1);
    CHECK_FALSE(history.back().applied);
    CHECK(history.back().timestamp == timestamp);
    CHECK(document->commandHistoryPosition() == before);
    document->redoCommand();
    CHECK(document->commandHistory().back().applied);
    CHECK(document->commandHistory().back().timestamp == timestamp);
    document->undoCommand();
    addBrush({{256, 0, 0}, {320, 64, 64}});
    CHECK_FALSE(document->canRedoCommand());
    CHECK(document->commandHistory().back().name != "Two brushes");
    QApplication::processEvents();
    CHECK(list->topLevelItemCount() == int(document->commandHistory().size() + 1));
}
TEST_CASE_METHOD(BrushUpstreamTest, "History double-click navigates actual undo and redo stacks", "[history]") {
    newMap();
    const auto initial = document->commandHistoryPosition();
    addBrush({{0, 0, 0}, {64, 64, 64}});
    addBrush({{128, 0, 0}, {192, 64, 64}});
    HistoryEditor editor(document);
    auto* list = editor.findChild<QTreeWidget*>("HistoryList");
    REQUIRE(list);
    auto* start = list->topLevelItem(int(initial));
    QMetaObject::invokeMethod(list, "itemDoubleClicked", Qt::DirectConnection, Q_ARG(QTreeWidgetItem*, start), Q_ARG(int, 0));
    CHECK(document->world()->defaultLayer()->childCount() == 0);
    CHECK(document->commandHistoryPosition() == initial);
    auto* last = list->topLevelItem(list->topLevelItemCount() - 1);
    QMetaObject::invokeMethod(list, "itemDoubleClicked", Qt::DirectConnection, Q_ARG(QTreeWidgetItem*, last), Q_ARG(int, 0));
    CHECK(document->world()->defaultLayer()->childCount() == 2);
    CHECK_FALSE(document->canRedoCommand());
}
TEST_CASE_METHOD(BrushUpstreamTest, "History collates repeated edits and omits cancelled transactions", "[history]") {
    newMap();
    auto* node = addBrush({{0, 0, 0}, {64, 64, 64}});
    document->selectNodes({node});
    const auto before = document->commandHistory().size();
    REQUIRE(document->translateObjects({16, 0, 0}));
    const auto timestamp = document->commandHistory().back().timestamp;
    REQUIRE(document->translateObjects({16, 0, 0}));
    CHECK(document->commandHistory().size() == before + 1);
    CHECK(document->commandHistory().back().timestamp >= timestamp);
    const auto position = document->commandHistoryPosition();
    {
        Transaction transaction(document, "Cancelled addition");
        addBrush({{128, 0, 0}, {192, 64, 64}});
        transaction.cancel();
    }
    CHECK(document->commandHistoryPosition() == position);
    CHECK(document->world()->defaultLayer()->childCount() == 1);
    document->undoCommand();
    CHECK(node->logicalBounds() == vm::bbox3{{0, 0, 0}, {64, 64, 64}});
}
TEST_CASE_METHOD(BrushUpstreamTest, "History groups editor-only transactions without losing undo states", "[history]") {
    newMap();
    auto* first = addBrush({{0, 0, 0}, {64, 64, 64}});
    auto* second = addBrush({{128, 0, 0}, {192, 64, 64}});
    document->clearCommandHistory();
    {
        Transaction outer(document, "Browse objects");
        Transaction inner(document, "Select first");
        document->selectNodes({first});
        inner.commit();
        outer.commit();
    }
    document->deselectAll();
    document->selectNodes({second});
    auto history = document->commandHistory();
    REQUIRE_FALSE(history.empty());
    for (const auto& entry : history) CHECK_FALSE(entry.modifiesDocument);
    const auto stateCount = history.size();
    HistoryEditor editor(document);
    auto* list = editor.findChild<QTreeWidget*>("HistoryList");
    REQUIRE(list);
    REQUIRE(list->topLevelItemCount() == 2);
    auto* group = list->topLevelItem(1);
    CHECK(group->childCount() == int(stateCount));
    CHECK_FALSE(group->isExpanded());
    CHECK(group->data(0, Qt::UserRole).toULongLong() == stateCount);
    group->setExpanded(true);
    const auto firstPosition = group->child(0)->data(0, Qt::UserRole).toULongLong();
    list->setCurrentItem(group->child(0));
    editor.findChild<QAction*>("HistoryJump")->trigger();
    CHECK(document->commandHistoryPosition() == firstPosition);
    REQUIRE(list->topLevelItemCount() == 3); // Applied and redo portions split.
    CHECK(list->topLevelItem(1)->isExpanded());
    auto* pending = list->topLevelItem(2);
    list->setCurrentItem(pending);
    editor.findChild<QAction*>("HistoryJump")->trigger();
    CHECK(document->commandHistoryPosition() == stateCount);
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{second});
    {
        Transaction outer(document, "Edit selected");
        Transaction inner(document, "Move");
        REQUIRE(document->translateObjects({0, 16, 0}));
        inner.commit();
        outer.commit();
    }
    history = document->commandHistory();
    CHECK(history.back().modifiesDocument);
    QApplication::processEvents();
    REQUIRE(list->topLevelItemCount() == 3);
    CHECK(list->topLevelItem(2)->text(0) == "Edit selected");
    CHECK(list->topLevelItem(2)->childCount() == 0);
    editor.resize(600, 350);
    editor.show();
    group = list->topLevelItem(1);
    group->setExpanded(false);
    QApplication::processEvents();
    const auto position = document->commandHistoryPosition();
    const auto rect = list->visualItemRect(group);
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
        QPoint(rect.left() - list->indentation() / 2, rect.center().y()));
    CHECK(group->isExpanded());
    CHECK(document->commandHistoryPosition() == position);
}
TEST_CASE_METHOD(BrushUpstreamTest, "History supports single-click toolbar and context navigation", "[history]") {
    newMap();
    document->clearCommandHistory();
    addBrush({{0, 0, 0}, {64, 64, 64}});
    addBrush({{128, 0, 0}, {192, 64, 64}});
    HistoryEditor editor(document);
    editor.resize(600, 350);
    editor.show();
    QApplication::processEvents();
    auto* list = editor.findChild<QTreeWidget*>("HistoryList");
    REQUIRE(list);
    auto* jump = editor.findChild<QAction*>("HistoryJump");
    auto* clear = editor.findChild<QAction*>("HistoryClear");
    REQUIRE(jump);
    REQUIRE(clear);
    CHECK_FALSE(jump->icon().isNull());
    CHECK(list->actions().contains(jump));
    CHECK(list->actions().contains(clear));
    auto* initial = list->topLevelItem(0);
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualItemRect(initial).center());
    CHECK(document->commandHistoryPosition() == 0);
    CHECK(document->world()->defaultLayer()->childCount() == 0);
    list->setCurrentItem(list->topLevelItem(2));
    CHECK(jump->isEnabled());
    jump->trigger();
    CHECK(document->world()->defaultLayer()->childCount() == 2);
    CHECK(document->commandHistoryPosition() == 2);
    // Merely selecting a context-menu target must not change the map.
    list->setContextMenuPolicy(Qt::NoContextMenu);
    QTest::mouseClick(list->viewport(), Qt::RightButton, Qt::NoModifier, list->visualItemRect(initial).center());
    CHECK(document->commandHistoryPosition() == 2);
    CHECK(jump->isEnabled());
    jump->trigger();
    CHECK(document->commandHistoryPosition() == 0);
}
TEST_CASE_METHOD(BrushUpstreamTest, "Clearing history preserves map contents and modification state", "[history]") {
    newMap();
    document->clearCommandHistory();
    auto* node = addBrush({{0, 0, 0}, {64, 64, 64}});
    document->selectNodes({node});
    REQUIRE(document->translateObjects({16, 0, 0}));
    document->undoCommand();
    REQUIRE(document->canRedoCommand());
    HistoryEditor editor(document);
    const auto modified = document->modified();
    editor.findChild<QAction*>("HistoryClear")->trigger();
    CHECK(document->modified() == modified);
    CHECK(document->world()->defaultLayer()->childCount() == 1);
    CHECK(node->logicalBounds() == vm::bbox3{{0, 0, 0}, {64, 64, 64}});
    CHECK(document->commandHistory().empty());
    CHECK_FALSE(document->canUndoCommand());
    CHECK_FALSE(document->canRedoCommand());
    CHECK_FALSE(editor.findChild<QAction*>("HistoryClear")->isEnabled());
    CHECK(editor.findChild<QTreeWidget*>("HistoryList")->topLevelItemCount() == 1);
    REQUIRE(document->translateObjects({0, 16, 0}));
    CHECK(document->canUndoCommand());
    const auto count = document->commandHistory().size();
    {
        Transaction transaction(document, "In progress");
        document->clearCommandHistory();
        CHECK(document->commandHistory().size() == count);
        transaction.commit();
    }
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    document->saveDocumentAs(std::filesystem::path(folder.path().toStdString()) / "saved.map");
    REQUIRE_FALSE(document->modified());
    document->clearCommandHistory();
    CHECK_FALSE(document->modified());
    CHECK(document->world()->defaultLayer()->childCount() == 1);
}
} // namespace TrenchBroom::View
