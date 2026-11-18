#include "Preferences.h"
#include "PreferenceManager.h"
#include "IO/VectorIcon.h"
#include "View/Actions/Actions.h"
#include "View/Preferences/KeyboardShortcutModel.h"
#include "View/Theme/QtUtils.h"
#include "View/Theme/TrenchBroomProxyStyle.h"
#include "View/Preferences/ViewPreferencePane.h"

#include <QAction>
#include <QCheckBox>
#include <QMenu>
#include <QScopeGuard>
#include <QStyleOption>
#include <catch2/catch.hpp>

namespace TrenchBroom::View {
TEST_CASE("Prefab creation is a configurable keyboard action", "[context-actions]") {
    const auto& action = *ActionManager::instance().actionsMap().at(SaveSelectionAsPrefabActionPath);
    CHECK(action.hasIcon()); CHECK(action.label().contains("Prefab"));
    const auto previous = action.keySequence();
    const auto restore = qScopeGuard([&] { action.setKeySequence(previous); });
    KeyboardShortcutModel model(nullptr);
    auto row = -1;
    for (int i=0;i<model.rowCount({});++i)
        if (model.data(model.index(i,2),Qt::DisplayRole).toString().contains("Save Selection as Prefab")) { row=i; break; }
    REQUIRE(row >= 0);
    const auto sequence = QKeySequence("Ctrl+Alt+Shift+P",QKeySequence::PortableText);
    REQUIRE(model.setData(model.index(row,0),QVariant::fromValue(sequence),Qt::EditRole));
    CHECK(action.keySequence() == sequence);
    const auto icons = TemporarilySetPref{Preferences::ContextMenuIcons,true};
    const auto shortcuts = TemporarilySetPref{Preferences::ContextMenuShortcuts,true};
    QMenu menu;
    int invoked = 0;
    auto* item = addContextMenuAction(menu,action,[&] { ++invoked; });
    CHECK_FALSE(item->icon().isNull()); CHECK(item->isIconVisibleInMenu());
    CHECK(item->text().contains(sequence.toString(QKeySequence::NativeText)));
    CHECK(item->shortcut().isEmpty()); // Display the binding without installing it twice.
    item->trigger(); CHECK(invoked == 1);
}

TEST_CASE("Context menu presentation preserves shared main-menu actions", "[context-actions]") {
    const auto icons = TemporarilySetPref{Preferences::ContextMenuIcons,false};
    const auto shortcuts = TemporarilySetPref{Preferences::ContextMenuShortcuts,false};
    QAction source("Shared action");
    source.setProperty("tbRegisteredAction",true);
    source.setIcon(IO::VectorIcon("Prefab.svg").icon()); source.setIconVisibleInMenu(true);
    source.setShortcut(QKeySequence("Ctrl+Alt+P",QKeySequence::PortableText));
    const auto sequence = source.shortcut();
    int invoked = 0; QObject::connect(&source,&QAction::triggered,[&] { ++invoked; });
    QMenu main; main.setProperty("tbMainMenu",true); main.addAction(&source);
    QMenu popup; popup.addAction(&source); prepareContextMenu(popup);
    REQUIRE(popup.actions().size() == 1);
    auto* copy = popup.actions().front(); REQUIRE(copy != &source);
    CHECK_FALSE(copy->isIconVisibleInMenu()); CHECK_FALSE(copy->isShortcutVisibleInContextMenu());
    CHECK_FALSE(copy->text().contains('\t')); CHECK(copy->shortcut().isEmpty());
    CHECK(source.shortcut() == sequence); CHECK(source.isIconVisibleInMenu());
    copy->trigger(); CHECK(invoked == 1);
    source.setText("Updated action"); CHECK(copy->text() == "Updated action");
    source.setEnabled(false); CHECK_FALSE(copy->isEnabled()); source.setEnabled(true);
    setPref(Preferences::ContextMenuIcons,true); setPref(Preferences::ContextMenuShortcuts,true);
    prepareContextMenu(popup);
    CHECK(popup.actions().front() == copy);
    CHECK(copy->isIconVisibleInMenu()); CHECK(copy->text().contains(sequence.toString(QKeySequence::NativeText)));
    CHECK(source.shortcut() == sequence); CHECK_FALSE(source.text().contains('\t'));
    prepareContextMenu(main); CHECK(main.actions().front() == &source);
    QAction* encoded = popup.addAction("Native action\tCtrl+N");
    setPref(Preferences::ContextMenuShortcuts,false); prepareContextMenu(popup); CHECK(encoded->text() == "Native action");
    setPref(Preferences::ContextMenuShortcuts,true); prepareContextMenu(popup); CHECK(encoded->text() == "Native action\tCtrl+N");
}

TEST_CASE("Context menu preferences can be changed independently in the view settings", "[context-actions]") {
    const auto icons = TemporarilySetPref{Preferences::ContextMenuIcons,false};
    const auto shortcuts = TemporarilySetPref{Preferences::ContextMenuShortcuts,true};
    ViewPreferencePane pane; pane.updateControls();
    auto* iconToggle = pane.findChild<QCheckBox*>("ContextMenuIcons");
    auto* keyToggle = pane.findChild<QCheckBox*>("ContextMenuShortcuts");
    REQUIRE(iconToggle); REQUIRE(keyToggle);
    CHECK_FALSE(iconToggle->isChecked()); CHECK(keyToggle->isChecked());
    iconToggle->click(); CHECK(pref(Preferences::ContextMenuIcons)); CHECK(pref(Preferences::ContextMenuShortcuts));
    keyToggle->click(); CHECK(pref(Preferences::ContextMenuIcons)); CHECK_FALSE(pref(Preferences::ContextMenuShortcuts));
    CHECK(Preferences::staticPreferencesMap().at("Editor/Context menu icons") == &Preferences::ContextMenuIcons);
    CHECK(Preferences::staticPreferencesMap().at("Editor/Context menu shortcuts") == &Preferences::ContextMenuShortcuts);
}

TEST_CASE("Disabled raster icons use the same QSS appearance as vector icons", "[context-actions]") {
    auto& appearance = IO::IconAppearance::instance();
    const auto previous = appearance.styleSheet();
    const auto oldBrightness = appearance.disabledBrightness(), oldOpacity = appearance.disabledOpacity();
    const auto restore = qScopeGuard([&] { appearance.setStyleSheet(previous); appearance.setDisabledBrightness(oldBrightness); appearance.setDisabledOpacity(oldOpacity); });
    appearance.setStyleSheet("QWidget#IconAppearance { qproperty-disabledBrightness: 0.25; qproperty-disabledOpacity: 0.6; }");
    QImage image(8,8,QImage::Format_ARGB32_Premultiplied); image.fill(QColor(160,120,80,128));
    auto pixmap = QPixmap::fromImage(image); pixmap.setDevicePixelRatio(2);
    TrenchBroomProxyStyle style("Fusion"); QStyleOption option;
    const auto result = style.generatedIconPixmap(QIcon::Disabled,pixmap,&option);
    CHECK(result.devicePixelRatio() == 2); CHECK(result.size() == pixmap.size());
    const auto color = result.toImage().pixelColor(4,4);
    CHECK(color.red() == Approx(40).margin(2)); CHECK(color.alpha() == Approx(77).margin(1));
    CHECK(result.toImage() == IO::disabledIconPixmap(pixmap).toImage());
}
} // namespace TrenchBroom::View
