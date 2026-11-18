#include "Error.h"
#include "View/Widgets/SearchableComboBox.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QEventLoop>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QTimer>
#include <catch2/catch.hpp>

namespace TrenchBroom::View {
namespace {
    void comboEvents() {
        QEventLoop loop;
        QTimer::singleShot(10, &loop, &QEventLoop::quit);
        loop.exec();
    }
    void comboClick(QWidget* widget, const QPoint& point) {
        QMouseEvent press{QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};
        QApplication::sendEvent(widget, &press);
        QMouseEvent release{QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier};
        QApplication::sendEvent(widget, &release);
        comboEvents();
    }
    void comboQuery(SearchableComboBox& combo, const QString& text) {
        combo.lineEdit()->selectAll();
        QInputMethodEvent event;
        event.setCommitString(text);
        QApplication::sendEvent(combo.lineEdit(), &event);
        comboEvents();
    }
    void comboKey(SearchableComboBox& combo, int key) {
        QKeyEvent press{QEvent::KeyPress, key, Qt::NoModifier};
        QApplication::sendEvent(combo.lineEdit(), &press);
        QKeyEvent release{QEvent::KeyRelease, key, Qt::NoModifier};
        QApplication::sendEvent(combo.lineEdit(), &release);
        comboEvents();
    }
    void fillCombo(SearchableComboBox& combo) {
        combo.addItem("Select", "editor/select");
        combo.addItem("Create a cylinder", "brush/cylinder");
        combo.addItem("Show grid", "view/grid");
        combo.resize(360, 32);
        combo.show();
        comboEvents();
    }
} // namespace

TEST_CASE("Search combo selects its text and filters substrings in names and IDs", "[searchable-combo]") {
    SearchableComboBox combo;
    fillCombo(combo);
    comboClick(combo.lineEdit(), combo.lineEdit()->rect().center());
    CHECK(combo.lineEdit()->selectedText() == "Select");
    const auto original = combo.currentData();
    comboQuery(combo, "CYLIN");
    CHECK(combo.currentText() == "CYLIN");
    CHECK(combo.currentData() == original);
    CHECK_FALSE(combo.resolvedData().isValid());
    const auto* matches = combo.completer()->completionModel();
    REQUIRE(matches->rowCount() == 1);
    CHECK(matches->index(0, 0).data(Qt::UserRole).toString() == "brush/cylinder");
    comboQuery(combo, "view/");
    REQUIRE(matches->rowCount() == 1);
    CHECK(matches->index(0, 0).data(Qt::UserRole).toString() == "view/grid");
    CHECK(combo.count() == 3); // Searching never destroys source entries or stable IDs.
}

TEST_CASE("Search combo confirms filtered items using their source ID by keyboard and mouse", "[searchable-combo]") {
    SearchableComboBox combo;
    fillCombo(combo);
    int confirmed = 0;
    QObject::connect(&combo, qOverload<int>(&QComboBox::activated), [&] { ++confirmed; });
    comboQuery(combo, "grid");
    CHECK(confirmed == 0);
    comboKey(combo, Qt::Key_Return);
    CHECK(combo.resolvedData().toString() == "view/grid");
    CHECK(combo.currentText() == "Show grid");
    CHECK(confirmed == 1);
    comboQuery(combo, "cylinder");
    auto* popup = combo.completer()->popup();
    REQUIRE(popup->model()->rowCount() == 1);
    REQUIRE(popup->isVisible());
    const auto index = popup->model()->index(0, 0);
    comboClick(popup->viewport(), popup->visualRect(index).center());
    CHECK(combo.resolvedData().toString() == "brush/cylinder");
    CHECK(combo.currentText() == "Create a cylinder");
    CHECK(confirmed == 2);
}

TEST_CASE("Search combo cancels unconfirmed queries without changing its command", "[searchable-combo]") {
    SearchableComboBox combo;
    fillCombo(combo);
    int confirmed = 0;
    QObject::connect(&combo, qOverload<int>(&QComboBox::activated), [&] { ++confirmed; });
    comboQuery(combo, "cylinder");
    comboKey(combo, Qt::Key_Escape);
    CHECK(combo.resolvedData().toString() == "editor/select");
    CHECK(combo.currentText() == "Select");
    comboQuery(combo, "no such command");
    CHECK(combo.completer()->completionModel()->rowCount() == 0);
    comboKey(combo, Qt::Key_Return);
    CHECK(combo.resolvedData().toString() == "editor/select");
    CHECK(confirmed == 0);
    comboQuery(combo, "view/grid");
    CHECK_FALSE(combo.resolvedData().isValid());
    comboKey(combo, Qt::Key_Escape);
    CHECK(combo.resolvedData().toString() == "editor/select");
    combo.showPopup();
    CHECK(combo.completer()->completionModel()->rowCount() == 3);
}

TEST_CASE("Search combo distinguishes identical labels by their command IDs", "[searchable-combo]") {
    SearchableComboBox combo;
    combo.addItem("Move", "entity/move");
    combo.addItem("Move", "texture/move");
    combo.resize(360, 32);
    combo.show();
    comboEvents();
    comboQuery(combo, "texture/");
    REQUIRE(combo.completer()->completionModel()->rowCount() == 1);
    comboKey(combo, Qt::Key_Return);
    CHECK(combo.resolvedData().toString() == "texture/move");
    CHECK(combo.currentIndex() == 1);
}
} // namespace TrenchBroom::View
