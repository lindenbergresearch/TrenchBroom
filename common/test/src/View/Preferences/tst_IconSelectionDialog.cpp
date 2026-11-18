#include "Error.h"
#include "IO/PathQt.h"
#include "IO/SystemPaths.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/Actions/Actions.h"
#include "View/Preferences/EditorCustomizationPreferencePane.h"
#include "View/Preferences/IconSelectionDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScopeGuard>
#include <QSet>
#include <QTimer>

#include <algorithm>

#include <catch2/catch.hpp>

namespace TrenchBroom::View {

TEST_CASE("Icon picker preserves resource paths and its current selection", "[icon-picker]") {
    const auto names = IconSelectionDialog::availableIcons();
    REQUIRE(names.contains("document.svg"));
    CHECK(QSet<QString>(names.begin(), names.end()).size() == names.size());
    CHECK(std::all_of(names.begin(), names.end(), [](const auto& name) { return !QDir::isAbsolutePath(name) && !name.startsWith("../") && name.endsWith(".svg", Qt::CaseInsensitive); }));

    IconSelectionDialog relative{"document.svg"};
    CHECK(relative.selectedIcon() == "document.svg");
    IconSelectionDialog absolute{IO::pathAsQString(IO::SystemPaths::findResourceFile("images/document.svg"))};
    CHECK(absolute.selectedIcon() == "document.svg");
    absolute.reject();
    CHECK(absolute.result() == QDialog::Rejected);
}

TEST_CASE("Icon picker searches filenames and only accepts visible matches", "[icon-picker]") {
    IconSelectionDialog dialog{"document.svg"};
    auto* filter = dialog.findChild<QLineEdit*>("EditorIconFilter");
    auto* grid = dialog.findChild<QListWidget*>("EditorIconGrid");
    auto* buttons = dialog.findChild<QDialogButtonBox*>("EditorIconButtons");
    REQUIRE(filter);
    REQUIRE(grid);
    REQUIRE(buttons);
    REQUIRE(grid->count() > 0);
    filter->setText("__no_such_icon__");
    CHECK(dialog.selectedIcon().isEmpty());
    CHECK_FALSE(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    dialog.accept();
    CHECK(dialog.result() == QDialog::Rejected);
    filter->setText("DOCUMENT.SVG");
    REQUIRE_FALSE(dialog.selectedIcon().isEmpty());
    CHECK(dialog.selectedIcon() == "document.svg");
    CHECK(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    CHECK(grid->viewMode() == QListView::IconMode);
    buttons->button(QDialogButtonBox::Ok)->click();
    CHECK(dialog.result() == QDialog::Accepted);
}

TEST_CASE("Action preferences commit the chosen icon and preserve it when the picker is cancelled", "[icon-picker][editor-preferences]") {
    auto& manager = ActionManager::instance();
    const auto oldConfiguration = manager.configuration();
    const auto oldPreference = pref(Preferences::EditorConfiguration);
    const auto restore = qScopeGuard([&] {
        QStringList errors;
        manager.setConfiguration(oldConfiguration, errors);
        PreferenceManager::instance().set(Preferences::EditorConfiguration, oldPreference);
        manager.reloadConfiguration();
    });
    const QString actionId = "Menu/View/Grid/Show Grid";
    auto configuration = oldConfiguration;
    REQUIRE(configuration.actions.contains(actionId));
    configuration.actions[actionId].icon = "document.svg";
    QStringList errors;
    REQUIRE(manager.setConfiguration(configuration, errors));

    EditorCustomizationPreferencePane pane{false};
    auto* entries = pane.findChild<QListWidget*>("EditorConfigurationList");
    auto* browse = pane.findChild<QPushButton*>("EditorActionBrowseIcon");
    auto* icon = pane.findChild<QLineEdit*>("EditorActionIcon");
    REQUIRE(entries);
    REQUIRE(browse);
    REQUIRE(icon);
    for (auto row = 0; row < entries->count(); ++row)
        if (entries->item(row)->data(Qt::UserRole).toString() == actionId) entries->setCurrentRow(row);
    REQUIRE(icon->text() == "document.svg");

    const QString chosenIcon = "ShapeTool_Cylinder.svg";
    auto opened = false, selected = false;
    QTimer::singleShot(0, &pane, [&] {
        auto* picker = pane.findChild<IconSelectionDialog*>("EditorIconSelection");
        opened = picker != nullptr;
        if (!picker) {
            if (auto* modal = qobject_cast<QDialog*>(QApplication::activeModalWidget())) modal->reject();
            return;
        }
        if (auto* filter = picker->findChild<QLineEdit*>("EditorIconFilter")) filter->setText(chosenIcon);
        selected = picker->selectedIcon() == chosenIcon;
        if (selected)
            picker->accept();
        else
            picker->reject();
    });
    browse->click();
    REQUIRE(opened);
    REQUIRE(selected);
    CHECK(manager.configuration().actions[actionId].icon == chosenIcon);
    CHECK(icon->text() == chosenIcon);

    opened = false;
    QTimer::singleShot(0, &pane, [&] {
        auto* picker = pane.findChild<IconSelectionDialog*>("EditorIconSelection");
        opened = picker != nullptr;
        if (picker) {
            if (auto* filter = picker->findChild<QLineEdit*>("EditorIconFilter")) filter->setText("document.svg");
            picker->reject();
        } else if (auto* modal = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
            modal->reject();
        }
    });
    browse->click();
    REQUIRE(opened);
    CHECK(manager.configuration().actions[actionId].icon == chosenIcon);
    CHECK(icon->text() == chosenIcon);
}

} // namespace TrenchBroom::View
