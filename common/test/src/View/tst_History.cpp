#include "View/BrushUpstreamTest.h"
#include "View/HistoryEditor.h"
#include "View/Inspector.h"
#include <QApplication>
#include <QTreeWidget>
#include <QToolBar>

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
} // namespace TrenchBroom::View
