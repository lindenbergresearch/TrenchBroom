#include "Error.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/Actions/Actions.h"
#include "View/Actions/EditorConfiguration.h"
#include "View/Preferences/EditorCustomizationPreferencePane.h"
#include "View/Actions/EditorToolbarLayout.h"
#include "View/Preferences/KeyboardShortcutModel.h"
#include "View/Actions/MainMenuBuilder.h"
#include "View/Widgets/SearchableComboBox.h"
#include "View/Preferences/PreferenceDialog.h"
#include "View/Preferences/PreferencePane.h"

#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QComboBox>
#include <QEventLoop>
#include <QFile>
#include <QInputMethodEvent>
#include <QJsonDocument>
#include <QKeySequenceEdit>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSpinBox>
#include <QTimer>
#include <QTabWidget>
#include <QToolBar>
#include <catch2/catch.hpp>

namespace TrenchBroom::View {
namespace {
    void editorEvents() {
        QEventLoop loop;
        QTimer::singleShot(10, &loop, &QEventLoop::quit);
        loop.exec();
    }
    EditorConfiguration smallEditorConfig() {
        EditorConfiguration config;
        EditorActionDefinition a;
        a.id = "select";
        a.label = "Select";
        a.command = "select";
        config.actions.insert(a.id, a);
        EditorToolbarDefinition t;
        t.id = "tools";
        t.label = "Tools";
        t.actions << a.id;
        config.toolbars.push_back(t);
        return config;
    }
    class EditorConfigTest {
      protected:
        ActionManager& manager = ActionManager::instance();
        const EditorConfiguration oldConfiguration = manager.configuration();
        const QString oldPreference = pref(Preferences::EditorConfiguration);
        ~EditorConfigTest() {
            QStringList errors;
            manager.setConfiguration(oldConfiguration, errors);
            PreferenceManager::instance().set(Preferences::EditorConfiguration, oldPreference);
            manager.reloadConfiguration();
            editorEvents();
        }
        void apply(const EditorConfiguration& config) {
            QStringList errors;
            REQUIRE(manager.setConfiguration(config, errors));
        }
        template <typename T> T* child(QObject& parent, const char* name) {
            auto* widget = parent.findChild<T*>(name);
            REQUIRE(widget);
            return widget;
        }
        void edit(QLineEdit* control, const QString& value) {
            type(control, value);
            REQUIRE(QMetaObject::invokeMethod(control, "editingFinished", Qt::DirectConnection));
        }
        void type(QLineEdit* control, const QString& value) {
            control->selectAll();
            if (value.isEmpty()) {
                QKeyEvent press{QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier};
                QApplication::sendEvent(control, &press);
                QKeyEvent release{QEvent::KeyRelease, Qt::Key_Backspace, Qt::NoModifier};
                QApplication::sendEvent(control, &release);
                return;
            }
            QInputMethodEvent event;
            event.setCommitString(value);
            QApplication::sendEvent(control, &event);
        }
        void selectEntry(QListWidget* list, const QString& id) {
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->data(Qt::UserRole).toString() == id) {
                    list->setCurrentRow(i);
                    return;
                }
            FAIL("Missing editor configuration entry: " << id.toStdString());
        }
        EditorToolbarDefinition toolbar(const QString& id) {
            const auto& bars = manager.configuration().toolbars;
            const auto it = std::find_if(bars.begin(), bars.end(), [&](const auto& t) { return t.id == id; });
            REQUIRE(it != bars.end());
            return *it;
        }
        QStringList assignedActions(QListWidget* list) {
            QStringList result;
            for (int i = 0; i < list->count(); ++i)
                result << list->item(i)->data(Qt::UserRole).toString();
            return result;
        }
    };
    class EditorPreferenceTestDialog : public PreferenceDialog {
      public:
        using PreferenceDialog::PreferenceDialog;
      protected:
        void paintEvent(QPaintEvent* event) override {
            // The production override styles an NSWindow titlebar. Headless Qt
            // windows have no native NSWindow, but all dialog input still works.
            QDialog::paintEvent(event);
        }
    };
} // namespace

TEST_CASE("Editor configuration roundtrips overrides and keeps untouched defaults", "[editor-configuration]") {
    const auto defaults = smallEditorConfig();
    auto config = defaults;
    config.actions["select"].label = "Move and select";
    config.actions["select"].shortcut = QKeySequence{"Ctrl+K"};
    config.toolbars[0].edge = EditorToolbarEdge::Bottom;
    config.arrangements[3] = EditorToolbarArrangement::Parallel;
    const auto bytes = editorConfigurationJson(config, &defaults);
    CHECK_FALSE(bytes.contains("tooltip"));
    EditorConfiguration read;
    QStringList errors;
    REQUIRE(readEditorConfiguration(bytes, defaults, {"select"}, read, errors));
    CHECK(read == config);
    auto newerDefaults = defaults;
    newerDefaults.actions["select"].tooltip = "New release tooltip";
    REQUIRE(readEditorConfiguration(bytes, newerDefaults, {"select"}, read, errors));
    CHECK(read.actions["select"].label == "Move and select");
    CHECK(read.actions["select"].tooltip == "New release tooltip");
    CHECK(read.arrangements[3] == EditorToolbarArrangement::Parallel);
}

TEST_CASE("Invalid editor configuration is rejected without partial changes", "[editor-configuration]") {
    const auto defaults = smallEditorConfig();
    for (const auto* bytes : {"{", R"({"version":2})", R"({"version":1.5})", R"({"version":1,"unknown":1})", R"({"version":1,"actions":[{"id":"select","iconPath":"x"}]})",
                              R"({"version":1,"actions":[{"id":"select","shortcut":"Ctrl+NotAKey"}]})", R"({"version":1,"actions":[{"id":"select","command":"shell:delete"}]})",
                              R"({"version":1,"actions":[{"id":"select","targets":["unknown"]}]})", R"({"version":1,"actions":[{"id":"select","label":""}]})",
                              R"({"version":1,"actions":[{"id":"select"},{"id":"select"}]})", R"({"version":1,"actions":[{"id":"select","menuPath":"A//B"}]})",
                              R"({"version":1,"toolbars":[{"id":"t","label":"Tools","edge":"left","iconSize":12,"minimumIconSize":24,"actions":["select"]}]})",
                              R"({"version":1,"toolbars":[{"id":"t","label":"Tools","edge":"left","actions":["missing"]}]})", R"({"version":1,"sides":{"left":"unknown"}})"}) {
        CAPTURE(bytes);
        auto result = defaults;
        QStringList errors;
        CHECK_FALSE(readEditorConfiguration(bytes, defaults, {"select"}, result, errors));
        CHECK(result == defaults);
        CHECK_FALSE(errors.isEmpty());
    }
}

TEST_CASE("Editor toolbar alignment and parallel lanes respect all four edges", "[editor-toolbars]") {
    std::vector<EditorToolbarLayoutItem> items;
    for (auto edge = 0; edge < 4; ++edge) {
        EditorToolbarLayoutItem item;
        item.definition.id = QString::number(edge);
        item.definition.label = "Tools";
        item.definition.edge = EditorToolbarEdge(edge);
        item.buttons = 3;
        items.push_back(item);
    }
    const QRect area{10, 10, 800, 600};
    const auto placements = layoutEditorToolbars(area, items, {});
    REQUIRE(placements.size() == 4);
    for (const auto& p : placements) {
        REQUIRE(p.visible);
        CHECK(area.contains(p.geometry));
        CHECK(p.iconSize == 24);
    }
    CHECK(placements[0].geometry.left() == area.left());
    CHECK(placements[1].geometry.top() == area.top());
    CHECK(placements[2].geometry.right() == area.right());
    CHECK(placements[3].geometry.bottom() == area.bottom());
    for (size_t i = 0; i < placements.size(); ++i)
        for (size_t j = i + 1; j < placements.size(); ++j)
            CHECK_FALSE(placements[i].geometry.intersects(placements[j].geometry));
    auto second = items[0];
    second.definition.id = "extra";
    second.definition.alignment = EditorToolbarAlignment::End;
    items.push_back(second);
    auto arrangements = std::array<EditorToolbarArrangement, 4>{};
    arrangements[0] = EditorToolbarArrangement::Parallel;
    const auto parallel = layoutEditorToolbars(area, items, arrangements);
    CHECK(parallel[4].geometry.left() > parallel[0].geometry.right());
    CHECK(parallel[4].geometry.bottom() == area.bottom());
}

TEST_CASE("Toolbars on a side shrink uniformly and hide below their minimum", "[editor-toolbars]") {
    EditorToolbarLayoutItem a;
    a.definition.id = "one";
    a.definition.label = "One";
    a.buttons = 6;
    auto b = a;
    b.definition.id = "two";
    const auto large = layoutEditorToolbars({0, 0, 600, 600}, {a, b}, {});
    CHECK(large[0].visible);
    CHECK(large[0].iconSize == 24);
    const auto medium = layoutEditorToolbars({0, 0, 600, 390}, {a, b}, {});
    CHECK(medium[0].visible);
    CHECK(medium[1].visible);
    CHECK(medium[0].iconSize < 24);
    CHECK(medium[0].iconSize == medium[1].iconSize);
    CHECK(medium[0].iconSize >= 12);
    const auto small = layoutEditorToolbars({0, 0, 600, 100}, {a, b}, {});
    CHECK_FALSE(small[0].visible);
    CHECK_FALSE(small[1].visible);
    CHECK(layoutEditorToolbars({0, 0, 600, 600}, {a, b}, {})[0].iconSize == 24);
}

TEST_CASE("Mixed toolbar layouts never overlap or escape a shrinking editor", "[editor-toolbars]") {
    std::vector<EditorToolbarLayoutItem> items;
    for (auto i = 0; i < 8; ++i) {
        EditorToolbarLayoutItem item;
        item.definition.id = QString::number(i);
        item.definition.label = "Tools";
        item.definition.edge = EditorToolbarEdge(i % 4);
        item.definition.alignment = EditorToolbarAlignment(i % 3);
        item.definition.iconSize = i % 2 ? 32 : 24;
        item.buttons = 4 + i % 3;
        items.push_back(item);
    }
    for (const auto mode : {EditorToolbarArrangement::Sequential, EditorToolbarArrangement::Parallel}) {
        const std::array arrangements{mode, mode, mode, mode};
        for (auto width = 80; width <= 900; width += 73)
            for (auto height = 60; height <= 700; height += 89) {
                const QRect area{0, 0, width, height};
                const auto placements = layoutEditorToolbars(area, items, arrangements);
                for (size_t i = 0; i < placements.size(); ++i) {
                    const auto& p = placements[i];
                    if (!p.visible) continue;
                    REQUIRE(area.contains(p.geometry));
                    REQUIRE(p.iconSize >= 12);
                    for (size_t j = i + 1; j < placements.size(); ++j)
                        if (placements[j].visible) REQUIRE_FALSE(p.geometry.intersects(placements[j].geometry));
                }
            }
    }
}

TEST_CASE_METHOD(EditorConfigTest, "Configured actions keep stable identities and native command requirements", "[editor-configuration]") {
    auto config = manager.configuration();
    EditorActionDefinition alias;
    alias.id = "User/test-alias";
    alias.label = "My Group";
    alias.command = "Menu/Edit/Group";
    alias.shortcut = QKeySequence{"Ctrl+Alt+9"};
    alias.targets = MainMenuTarget | PopupMenuTarget | EditorToolbarTarget;
    alias.menuPath = "My Actions/Selection";
    config.actions.insert(alias.id, alias);
    apply(config);
    const auto* action = manager.action(alias.id);
    REQUIRE(action);
    CHECK(action->label() == "My Group");
    CHECK(action->keySequence() == *alias.shortcut);
    CHECK(action->actionContext() == manager.command(alias.command)->actionContext());
    ActionExecutionContext empty{nullptr, nullptr};
    CHECK_FALSE(action->enabled(empty));
    action->execute(empty);
    auto changed = manager.configuration();
    changed.actions[alias.id].label = "Group Again";
    apply(changed);
    CHECK(manager.action(alias.id) == action);
    CHECK(action->label() == "Group Again");
    action->setKeySequence(QKeySequence{"Ctrl+Alt+8"});
    CHECK(manager.action(alias.id) == action);
    CHECK(action->keySequence() == QKeySequence{"Ctrl+Alt+8"});
    REQUIRE(manager.reloadConfiguration());
    CHECK(action->keySequence() == QKeySequence{"Ctrl+Alt+8"});
    changed = manager.configuration();
    changed.actions.remove(alias.id);
    apply(changed);
    CHECK_FALSE(manager.action(alias.id));
    CHECK_FALSE(action->enabled(empty));
    CHECK(action->keySequence().isEmpty());
}

TEST_CASE_METHOD(EditorConfigTest, "Toolbar-only changes do not rebuild menus or shortcut bindings", "[editor-configuration]") {
    editorEvents();
    int all = 0, actions = 0;
    auto a = manager.configurationDidChangeNotifier.connect([&] { ++all; });
    auto b = manager.actionsDidChangeNotifier.connect([&] { ++actions; });
    auto config = manager.configuration();
    config.toolbars[0].iconSize = 32;
    apply(config);
    editorEvents();
    CHECK(all == 1);
    CHECK(actions == 0);
    config = manager.configuration();
    config.actions["Menu/Edit/Group"].tooltip = "New tooltip";
    apply(config);
    editorEvents();
    CHECK(actions == 1);
}

TEST_CASE_METHOD(EditorConfigTest, "Action and toolbar preference editors share assignments and persist edits", "[editor-configuration][editor-preferences]") {
    EditorCustomizationPreferencePane actions{false};
    child<QAction>(actions, "EditorEntryAdd")->trigger();
    auto* list = child<QListWidget>(actions, "EditorConfigurationList");
    REQUIRE(list->currentItem());
    const auto id = list->currentItem()->data(Qt::UserRole).toString();
    CHECK_FALSE(manager.isBuiltin(id));
    edit(child<QLineEdit>(actions, "EditorEntryLabel"), "Personal Select");
    REQUIRE(manager.configuration().actions.contains(id));
    CHECK(manager.action(id)->label() == "Personal Select");
    EditorCustomizationPreferencePane toolbars{true};
    child<QAction>(toolbars, "EditorEntryAdd")->trigger();
    auto* toolbarList = child<QListWidget>(toolbars, "EditorConfigurationList");
    const auto toolbarId = toolbarList->currentItem()->data(Qt::UserRole).toString();
    auto* available = child<QComboBox>(toolbars, "EditorToolbarAvailableAction");
    available->setCurrentIndex(available->findData(id));
    child<QAction>(toolbars, "EditorToolbarAssign")->trigger();
    const auto& bars = manager.configuration().toolbars;
    const auto bar = std::find_if(bars.begin(), bars.end(), [&](const auto& t) { return t.id == toolbarId; });
    REQUIRE(bar != bars.end());
    CHECK(bar->actions.contains(id));
    CHECK(manager.action(id)->targets() & EditorToolbarTarget);
    child<QSpinBox>(toolbars, "EditorToolbarSize")->setValue(36);
    REQUIRE(manager.reloadConfiguration());
    const auto& reloaded = manager.configuration().toolbars;
    CHECK(std::find_if(reloaded.begin(), reloaded.end(), [&](const auto& t) { return t.id == toolbarId; })->iconSize == 36);
    // Removing a custom action also removes all toolbar assignments, while held
    // QAction/QShortcut callbacks retain a safe, disabled action object.
    child<QAction>(actions, "EditorEntryRemove")->trigger();
    CHECK_FALSE(manager.action(id));
    for (const auto& toolbar : manager.configuration().toolbars)
        CHECK_FALSE(toolbar.actions.contains(id));
}

TEST_CASE_METHOD(EditorConfigTest, "Enter in a new toolbar name keeps the toolbar in the Preferences dialog", "[editor-configuration][editor-preferences]") {
    EditorPreferenceTestDialog dialog{nullptr};
    auto* categoryBar = child<QToolBar>(dialog, "ToolBar_PreferenceDialog");
    // Store the list rather than retaining iterators to QToolBar::actions() temporaries.
    const auto categories = categoryBar->actions();
    const auto toolbarCategory = std::find_if(categories.begin(), categories.end(), [](const auto* action) { return action->text() == "Editor Toolbar"; });
    REQUIRE(toolbarCategory != categories.end());
    (*toolbarCategory)->trigger();
    dialog.resize(1100, 800);
    dialog.show();
    editorEvents();
    auto* pane = child<EditorCustomizationPreferencePane>(dialog, "EditorToolbarPreferences");
    child<QAction>(*pane, "EditorEntryAdd")->trigger();
    auto* list = child<QListWidget>(*pane, "EditorConfigurationList");
    REQUIRE(list->currentItem());
    const auto id = list->currentItem()->data(Qt::UserRole).toString();
    const auto count = manager.configuration().toolbars.size();
    auto* label = child<QLineEdit>(*pane, "EditorEntryLabel");
    label->setFocus();
    type(label, "Navigation helpers");
    QKeyEvent enter{QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier};
    QApplication::sendEvent(label, &enter);
    QKeyEvent release{QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier};
    QApplication::sendEvent(label, &release);
    editorEvents();
    CHECK(manager.configuration().toolbars.size() == count);
    CHECK(toolbar(id).label == "Navigation helpers");
    REQUIRE(list->currentItem());
    CHECK(list->currentItem()->data(Qt::UserRole).toString() == id);
}

TEST_CASE_METHOD(EditorConfigTest, "Preferences create categories only on first use and retain their edits", "[editor-preferences]") {
    EditorPreferenceTestDialog dialog{nullptr};
    // PreferencePane has no Q_OBJECT; Qt's inherited staticMetaObject would
    // classify every QWidget as a pane. Inspect the actual C++ type instead.
    const auto panes = [&] {
        std::vector<PreferencePane*> result;
        for (auto* widget : dialog.findChildren<QWidget*>())
            if (auto* pane = dynamic_cast<PreferencePane*>(widget)) result.push_back(pane);
        return result;
    };
    CHECK(panes().size() == 1);
    auto* categories = child<QToolBar>(dialog, "ToolBar_PreferenceDialog");
    const auto visit = [&](const QString& name) {
        const auto actions = categories->actions();
        const auto it = std::find_if(actions.begin(), actions.end(), [&](const auto* action) { return action->text() == name; });
        REQUIRE(it != actions.end());
        (*it)->trigger();
    };
    visit("Editor Toolbar");
    auto* toolbarPane = child<EditorCustomizationPreferencePane>(dialog, "EditorToolbarPreferences");
    CHECK(panes().size() == 2);
    CHECK_FALSE(dialog.findChild<EditorCustomizationPreferencePane*>("EditorActionsPreferences"));
    child<QAction>(*toolbarPane, "EditorEntryAdd")->trigger();
    edit(child<QLineEdit>(*toolbarPane, "EditorEntryLabel"), "Retained toolbar");
    const auto id = child<QListWidget>(*toolbarPane, "EditorConfigurationList")->currentItem()->data(Qt::UserRole).toString();
    visit("Editor Actions");
    CHECK(panes().size() == 3);
    visit("Editor Toolbar");
    CHECK(child<EditorCustomizationPreferencePane>(dialog, "EditorToolbarPreferences") == toolbarPane);
    CHECK(child<QLineEdit>(*toolbarPane, "EditorEntryLabel")->text() == "Retained toolbar");
    CHECK(child<QListWidget>(*toolbarPane, "EditorConfigurationList")->currentItem()->data(Qt::UserRole).toString() == id);
    const auto controls = panes();
    visit("Games");
    visit("Editor Actions");
    visit("Editor Toolbar");
    CHECK(panes() == controls);
}

TEST_CASE_METHOD(EditorConfigTest, "Toolbar selection refreshes elements and commits pending edits to their original toolbar", "[editor-configuration][editor-preferences]") {
    const QString select = "Controls/Map view/Deactivate current tool", grid = "Menu/View/Grid/Show Grid", snap = "Menu/View/Grid/Snap to Grid";
    auto config = manager.configuration();
    EditorToolbarDefinition first;
    first.id = "User/first";
    first.label = "First";
    first.actions = QStringList{select, QString{}, grid};
    EditorToolbarDefinition second;
    second.id = "User/second";
    second.label = "Second";
    second.actions = QStringList{snap};
    config.toolbars = {first, second};
    apply(config);
    editorEvents();
    EditorCustomizationPreferencePane pane{true};
    auto* list = child<QListWidget>(pane, "EditorConfigurationList");
    auto* assigned = child<QListWidget>(pane, "EditorToolbarAssignedActions");
    auto* available = child<QComboBox>(pane, "EditorToolbarAvailableAction");
    auto* label = child<QLineEdit>(pane, "EditorEntryLabel");
    CHECK(assignedActions(assigned) == first.actions);
    available->setCurrentIndex(available->findData(grid));
    type(label, "First renamed");
    selectEntry(list, second.id);
    CHECK(toolbar(first.id).label == "First renamed");
    CHECK(label->text() == second.label);
    CHECK(assignedActions(assigned) == second.actions);
    CHECK(available->currentData().toString() == snap);
    const auto saved = manager.configuration();
    for (int i = 0; i < 3; ++i) {
        selectEntry(list, first.id);
        CHECK(assignedActions(assigned) == first.actions);
        CHECK(label->text() == "First renamed");
        assigned->setCurrentRow(i);
        selectEntry(list, second.id);
        editorEvents();
        CHECK(assignedActions(assigned) == second.actions);
    }
    CHECK(manager.configuration() == saved);
}

TEST_CASE_METHOD(EditorConfigTest, "Editing toolbar elements preserves chosen actions and pending toolbar settings", "[editor-configuration][editor-preferences]") {
    const QString select = "Controls/Map view/Deactivate current tool", grid = "Menu/View/Grid/Show Grid", snap = "Menu/View/Grid/Snap to Grid";
    auto config = manager.configuration();
    EditorToolbarDefinition first;
    first.id = "User/first";
    first.label = "First";
    first.actions = QStringList{select};
    EditorToolbarDefinition second;
    second.id = "User/second";
    second.label = "Second";
    second.actions = QStringList{snap};
    config.toolbars = {first, second};
    apply(config);
    editorEvents();
    EditorCustomizationPreferencePane pane{true};
    auto* assigned = child<QListWidget>(pane, "EditorToolbarAssignedActions");
    auto* available = child<QComboBox>(pane, "EditorToolbarAvailableAction");
    auto* label = child<QLineEdit>(pane, "EditorEntryLabel");
    assigned->setCurrentRow(0);
    available->setCurrentIndex(available->findData(grid));
    type(label, "First with grid");
    child<QAction>(pane, "EditorToolbarAssign")->trigger();
    CHECK(toolbar(first.id).label == "First with grid");
    CHECK(toolbar(first.id).actions == QStringList({select, grid}));
    CHECK(assignedActions(assigned) == QStringList({select, grid}));
    REQUIRE(assigned->currentItem());
    CHECK(assigned->currentItem()->data(Qt::UserRole).toString() == grid);
    type(label, "Reordered");
    child<QAction>(pane, "EditorToolbarActionUp")->trigger();
    CHECK(toolbar(first.id).label == "Reordered");
    CHECK(toolbar(first.id).actions == QStringList({grid, select}));
    type(label, "Without grid");
    child<QAction>(pane, "EditorToolbarUnassign")->trigger();
    CHECK(toolbar(first.id).label == "Without grid");
    CHECK(toolbar(first.id).actions == QStringList{select});
    CHECK(assignedActions(assigned) == QStringList{select});
    CHECK(toolbar(second.id) == second);
    const auto saved = manager.configuration();
    assigned->setCurrentRow(0);
    editorEvents();
    CHECK(manager.configuration() == saved);
    REQUIRE(manager.reloadConfiguration());
    CHECK(manager.configuration() == saved);
}

TEST_CASE_METHOD(EditorConfigTest, "Invalid toolbar edits preserve selection and empty filters clear the details", "[editor-configuration][editor-preferences]") {
    const QString select = "Controls/Map view/Deactivate current tool", grid = "Menu/View/Grid/Show Grid";
    auto config = manager.configuration();
    EditorToolbarDefinition first;
    first.id = "User/first";
    first.label = "First";
    first.actions = QStringList{select};
    auto second = first;
    second.id = "User/second";
    second.label = "Second";
    second.actions = QStringList{grid};
    config.toolbars = {first, second};
    apply(config);
    editorEvents();
    EditorCustomizationPreferencePane pane{true};
    auto* list = child<QListWidget>(pane, "EditorConfigurationList");
    auto* label = child<QLineEdit>(pane, "EditorEntryLabel");
    auto* assigned = child<QListWidget>(pane, "EditorToolbarAssignedActions");
    auto* filter = child<QLineEdit>(pane, "EditorConfigurationSearch");
    type(label, QString{});
    selectEntry(list, second.id);
    REQUIRE(list->currentItem());
    CHECK(list->currentItem()->data(Qt::UserRole).toString() == first.id);
    CHECK(label->text().isEmpty());
    CHECK(assignedActions(assigned) == first.actions);
    CHECK(manager.configuration() == config);
    CHECK_FALSE(pane.validate());
    edit(label, "Corrected first");
    selectEntry(list, second.id);
    CHECK(label->text() == second.label);
    CHECK(assignedActions(assigned) == second.actions);
    CHECK(toolbar(first.id).label == "Corrected first");
    const auto saved = manager.configuration();
    filter->setText("No toolbar matches this text");
    CHECK(list->count() == 0);
    CHECK_FALSE(list->currentItem());
    CHECK(label->text().isEmpty());
    CHECK_FALSE(label->isEnabled());
    CHECK(assigned->count() == 0);
    CHECK_FALSE(child<QAction>(pane, "EditorToolbarAssign")->isEnabled());
    CHECK_FALSE(child<QAction>(pane, "EditorToolbarUnassign")->isEnabled());
    CHECK(manager.configuration() == saved);
    filter->clear();
    REQUIRE(list->currentItem());
    CHECK(list->currentItem()->data(Qt::UserRole).toString() == first.id);
    CHECK(label->text() == "Corrected first");
    CHECK(label->isEnabled());
    CHECK(assignedActions(assigned) == first.actions);
    CHECK(manager.configuration() == saved);
}

TEST_CASE_METHOD(EditorConfigTest, "Choosing a searched action replaces the selected toolbar element", "[editor-configuration][editor-preferences]") {
    const QString select = "Controls/Map view/Deactivate current tool", grid = "Menu/View/Grid/Show Grid", snap = "Menu/View/Grid/Snap to Grid";
    auto config = manager.configuration();
    EditorToolbarDefinition bar;
    bar.id = "User/search-toolbar";
    bar.label = "Search toolbar";
    bar.actions = QStringList{select, grid};
    config.toolbars = {bar};
    apply(config);
    editorEvents();
    EditorCustomizationPreferencePane pane{true};
    pane.resize(1000, 700);
    pane.show();
    child<QTabWidget>(pane, "EditorToolbarPages")->setCurrentIndex(1);
    auto* assigned = child<QListWidget>(pane, "EditorToolbarAssignedActions");
    auto* action = child<SearchableComboBox>(pane, "EditorToolbarAvailableAction");
    editorEvents();
    CHECK(action->resolvedData().toString() == select);
    action->lineEdit()->setFocus();
    type(action->lineEdit(), "Snap to Grid");
    editorEvents();
    CHECK(toolbar(bar.id).actions == bar.actions);
    REQUIRE(action->completer()->completionModel()->rowCount() == 1);
    QKeyEvent enter{QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier};
    QApplication::sendEvent(action->lineEdit(), &enter);
    editorEvents();
    CHECK(toolbar(bar.id).actions == QStringList({snap, grid}));
    CHECK(assigned->item(0)->text() == manager.configuration().actions.value(snap).label);
    assigned->setCurrentRow(1);
    CHECK(action->resolvedData().toString() == grid);
    const auto beforeSearch = manager.configuration();
    action->lineEdit()->setFocus();
    type(action->lineEdit(), "Editor/Create/Cylinder");
    editorEvents();
    CHECK(manager.configuration() == beforeSearch);
    auto* popup = action->completer()->popup();
    REQUIRE(popup->isVisible());
    REQUIRE(popup->model()->rowCount() == 1);
    const auto point = popup->visualRect(popup->model()->index(0, 0)).center();
    QMouseEvent press{QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(popup->viewport(), &press);
    QMouseEvent release{QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier};
    QApplication::sendEvent(popup->viewport(), &release);
    editorEvents();
    CHECK(toolbar(bar.id).actions == QStringList({snap, "Editor/Create/Cylinder"}));
    CHECK(assigned->item(1)->data(Qt::UserRole).toString() == "Editor/Create/Cylinder");
    CHECK_FALSE(assigned->item(1)->icon().isNull());
    CHECK(action->resolvedData().toString() == "Editor/Create/Cylinder");
    REQUIRE(manager.reloadConfiguration());
    CHECK(toolbar(bar.id).actions == QStringList({snap, "Editor/Create/Cylinder"}));
}

TEST_CASE_METHOD(EditorConfigTest, "Searching commands does not save incomplete text as a command", "[editor-configuration][editor-preferences]") {
    EditorCustomizationPreferencePane pane{false};
    child<QAction>(pane, "EditorEntryAdd")->trigger();
    const auto id = child<QListWidget>(pane, "EditorConfigurationList")->currentItem()->data(Qt::UserRole).toString();
    auto* command = child<SearchableComboBox>(pane, "EditorActionCommand");
    const auto oldCommand = manager.configuration().actions.value(id).command;
    pane.resize(1000, 700);
    pane.show();
    command->lineEdit()->setFocus();
    type(command->lineEdit(), "not an existing command");
    CHECK(pane.validate());
    CHECK(manager.configuration().actions.value(id).command == oldCommand);
    type(command->lineEdit(), "Editor/Create/Cylinder");
    editorEvents();
    REQUIRE(command->completer()->completionModel()->rowCount() == 1);
    edit(child<QLineEdit>(pane, "EditorEntryLabel"), "Renamed while searching");
    CHECK(manager.configuration().actions.value(id).command == oldCommand);
    QKeyEvent enter{QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier};
    QApplication::sendEvent(command->lineEdit(), &enter);
    editorEvents();
    CHECK(manager.configuration().actions.value(id).command == "Editor/Create/Cylinder");
}

TEST_CASE("Export default editor action definitions", "[.][editor-defaults]") {
    QFile file{"/tmp/tb-editor-customization/EditorActions.json"};
    REQUIRE(file.open(QIODevice::WriteOnly));
    const auto bytes = editorConfigurationJson(ActionManager::instance().defaultConfiguration());
    REQUIRE(file.write(bytes) == bytes.size());
}
} // namespace TrenchBroom::View
