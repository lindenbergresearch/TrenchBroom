/*
 Copyright (C) 2010-2017 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "View/Actions/MainMenuBuilder.h"
#include <algorithm>

#include <QMenuBar>

#include "IO/ResourceUtils.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/App/MapFrame.h"

namespace TrenchBroom {
namespace View {
MenuBuilderBase::MenuBuilderBase(MenuBuilderBase::ActionMap &actions, const TriggerFn &triggerFn, QObject* owner) : m_actions(actions), m_triggerFn(triggerFn), m_owner{owner} {
}

MenuBuilderBase::~MenuBuilderBase() = default;

void MenuBuilderBase::updateActionKeySeqeunce(QAction *qAction, const Action *tAction) {
    qAction->setText(tAction->label()); qAction->setCheckable(tAction->checkable()); qAction->setStatusTip(tAction->statusTip());
    qAction->setIcon(tAction->hasIcon() ? IO::loadIcon(tAction->iconPath(), pref(Preferences::ToolBarIconsSize)) : QIcon{});
    qAction->setProperty("tbActionId", QString::fromStdString(tAction->preferencePath().generic_string()));
    if (!tAction->keySequence().isEmpty()) {
        const QString tooltip = QObject::tr("%1 (%2)").arg(tAction->label(), tAction->keySequence().toString(QKeySequence::NativeText));
        qAction->setToolTip(tooltip);
    } else {
        qAction->setToolTip(tAction->label());
    }

    qAction->setShortcut((tAction->targets() & (MainMenuTarget | MainToolbarTarget)) ? tAction->keySequence() : QKeySequence{});
    if (!tAction->statusTip().isEmpty()) qAction->setToolTip(qAction->toolTip() + '\n' + tAction->statusTip());
}

QAction *MenuBuilderBase::findOrCreateQAction(const Action *tAction) {
    // Check if it already exists
    {
        auto it = m_actions.find(tAction);
        if (it != m_actions.end()) {
            return it->second;
        }
    }

    auto *qAction = new QAction(tAction->label(), m_owner);
    qAction->setProperty("tbRegisteredAction",true);
    updateActionKeySeqeunce(qAction, tAction);

    const auto &triggerFn = m_triggerFn;
    QObject::connect(qAction, &QAction::triggered, [=]() { triggerFn(*tAction); });

    m_actions[tAction] = qAction;
    return qAction;
}

MainMenuBuilder::MainMenuBuilder(QMenuBar &menuBar, ActionMap &actions, const TriggerFn &triggerFn)
    : MenuBuilderBase(actions, triggerFn, &menuBar), m_menuBar(menuBar), m_currentMenu(nullptr), recentDocumentsMenu(nullptr), undoAction(nullptr), redoAction(nullptr), pasteAction(nullptr), pasteAtOriginalPositionAction(nullptr) {
}

void MainMenuBuilder::ensureAllActions() {
    for (const auto& [path, action] : ActionManager::instance().actionsMap()) { unused(path); findOrCreateQAction(action.get()); }
}

void MainMenuBuilder::visit(const Menu &menu) {
    auto *parentMenu = m_currentMenu;
    if (m_currentMenu == nullptr) {
        // top level menu
        m_currentMenu = m_menuBar.addMenu(QString::fromStdString(menu.name()));
    } else {
        m_currentMenu = m_currentMenu->addMenu(QString::fromStdString(menu.name()));
    }

    if (menu.entryType() == MenuEntryType::Menu_RecentDocuments) {
        recentDocumentsMenu = m_currentMenu;
    }

    m_currentMenu->setProperty("tbMainMenu",true);
    auto* tracked = m_currentMenu;
    QObject::connect(tracked, &QMenu::aboutToShow, tracked, [tracked] { tracked->setProperty("tbMenuOpen", true); });
    QObject::connect(tracked, &QMenu::aboutToHide, tracked, [tracked] { tracked->setProperty("tbMenuOpen", false); });

    menu.visitEntries(*this);
    const auto entries = m_currentMenu->actions();
    const auto hasItems = std::any_of(entries.begin(), entries.end(), [](const auto* action) { return action->isVisible() && !action->isSeparator(); });
    m_currentMenu->menuAction()->setVisible(hasItems || menu.entryType() == MenuEntryType::Menu_RecentDocuments);
    m_currentMenu = parentMenu;
}

void MainMenuBuilder::visit(const MenuSeparatorItem &) {
    assert(m_currentMenu != nullptr);
    m_currentMenu->addSeparator();
}

void MainMenuBuilder::visit(const MenuActionItem &item) {
    assert(m_currentMenu != nullptr);
    const auto &tAction = item.action();
    QAction *qAction = findOrCreateQAction(&tAction);
    m_currentMenu->addAction(qAction);

    if (item.entryType() == MenuEntryType::Menu_Undo) {
        undoAction = qAction;
    } else if (item.entryType() == MenuEntryType::Menu_Redo) {
        redoAction = qAction;
    } else if (item.entryType() == MenuEntryType::Menu_Paste) {
        pasteAction = qAction;
    } else if (item.entryType() == MenuEntryType::Menu_PasteAtOriginalPosition) {
        pasteAtOriginalPositionAction = qAction;
    }
}
} // namespace View
} // namespace TrenchBroom
