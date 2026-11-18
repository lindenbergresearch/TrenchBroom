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

#pragma once

#include <QKeySequence>
#include <QToolBar>

#include "Ensure.h"
#include "Macros.h"
#include "View/Actions/ActionContext.h"
#include "View/Actions/EditorConfiguration.h"
#include "Notifier.h"
#include "NotifierConnection.h"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class QMenu;

namespace TrenchBroom {
namespace Assets {
class EntityDefinition;
}

namespace Model {
class SmartTag;
}

namespace View {
class MapDocument;

class MapFrame;

class MapViewBase;
inline constexpr auto SaveSelectionAsPrefabActionPath = "Menu/Edit/Save Selection as Prefab";

class ActionExecutionContext {
  private:
    ActionContext::Type m_actionContext;
    MapFrame *m_frame;
    MapViewBase *m_mapView;

  public:
    ActionExecutionContext(MapFrame *mapFrame, MapViewBase *mapView);

    bool hasDocument() const;

    bool hasActionContext(ActionContext::Type actionContext) const;

    MapFrame *frame();

    MapViewBase *view();

    MapDocument *document();
};

class Action {
  protected:
    QString m_label;
    std::filesystem::path m_preferencePath;
    ActionContext::Type m_actionContext;
    QKeySequence m_defaultShortcut;
    std::filesystem::path m_iconPath;
    QString m_statusTip;
    bool m_configured = false;
    bool m_enabledByConfiguration = true;
    int m_targets = MainMenuTarget | PopupMenuTarget | MainToolbarTarget | EditorToolbarTarget;
    std::optional<QKeySequence> m_configuredShortcut;
    QString m_commandId;

  public:
    Action(const std::filesystem::path &preferencePath, const QString &label, ActionContext::Type actionContext, const QKeySequence &defaultShortcut, const std::filesystem::path &iconPath, const QString &statusTip);

    virtual ~Action();

    const QString &label() const;

    const std::filesystem::path &preferencePath() const;

    ActionContext::Type actionContext() const;

    QKeySequence keySequence() const;
    QKeySequence defaultKeySequence() const { return m_defaultShortcut; }
    int targets() const { return m_targets; }
    bool configured() const { return m_configured; }
    bool enabledByConfiguration() const { return m_enabledByConfiguration; }
    QString commandId() const;

    void setKeySequence(const QKeySequence &keySequence) const;

    void resetKeySequence() const;

    virtual void execute(ActionExecutionContext &context) const = 0;

    virtual bool enabled(ActionExecutionContext &context) const = 0;

    virtual bool checkable() const = 0;

    virtual bool checked(ActionExecutionContext &context) const = 0;

    bool hasIcon() const;

    const std::filesystem::path &iconPath() const;

    const QString &statusTip() const;

  deleteCopy(Action);

    Action(Action &&other) = default; // cannot be noexcept because it will call QKeySequence's copy constructor
    Action &operator=(Action &&other) = default;
};

/**
 * ExecuteFn has type ActionExecutionContext& -> void
 * EnabledFn has type ActionExecutionContext& -> bool
 * CheckedFn has type ActionExecutionContext& -> bool
 */
template<class ExecuteFn, class EnabledFn, class CheckedFn> class LambdaAction : public Action {
  private:
    ExecuteFn m_execute;
    EnabledFn m_enabled;
    CheckedFn m_checked;
    bool m_checkable;

  public:
    LambdaAction(const std::filesystem::path &preferencePath, const QString &label, const ActionContext::Type actionContext, const QKeySequence &defaultShortcut, const ExecuteFn &execute, const EnabledFn &enabled, const CheckedFn &checked, const bool checkable, const std::filesystem::path &iconPath, const QString &statusTip)
        : Action(preferencePath, label, actionContext, defaultShortcut, iconPath, statusTip), m_execute(execute), m_enabled(enabled), m_checked(checked), m_checkable(checkable) {
    }

    void execute(ActionExecutionContext &context) const override {
        if (enabled(context)) {
            m_execute(context);
        }
    }

    bool enabled(ActionExecutionContext &context) const override {
        return context.hasActionContext(m_actionContext) && m_enabled(context);
    }

    bool checkable() const override { return m_checkable; }

    bool checked(ActionExecutionContext &context) const override {
        assert(checkable());
        return m_checked(context);
    }
};

class Menu;

QAction* addContextMenuAction(QMenu& menu, const Action& action, const std::function<void()>& execute, bool enabled = true);

class MenuSeparatorItem;

class MenuActionItem;

class MenuVisitor {
  public:
    virtual ~MenuVisitor();

    virtual void visit(const Menu &menu) = 0;

    virtual void visit(const MenuSeparatorItem &item) = 0;

    virtual void visit(const MenuActionItem &item) = 0;
};

enum class MenuEntryType {
  Menu_RecentDocuments, Menu_Undo, Menu_Redo, Menu_Cut, Menu_Copy, Menu_Paste, Menu_PasteAtOriginalPosition, Menu_None
};

class MenuEntry {
  private:
    MenuEntryType m_entryType;

  public:
    explicit MenuEntry(MenuEntryType entryType);

    virtual ~MenuEntry();

    virtual void accept(MenuVisitor &visitor) const = 0;

    MenuEntryType entryType() const;

  deleteCopyAndMove(MenuEntry);
};

class MenuSeparatorItem : public MenuEntry {
  public:
    MenuSeparatorItem();

    void accept(MenuVisitor &visitor) const override;

  deleteCopyAndMove(MenuSeparatorItem);
};

class MenuActionItem : public MenuEntry {
  private:
    const Action *m_action;

  public:
    MenuActionItem(const Action *action, MenuEntryType entryType);

    const QString &label() const;

    const Action &action() const;

    void accept(MenuVisitor &visitor) const override;

  deleteCopyAndMove(MenuActionItem);
};

class Menu : public MenuEntry {
  private:
    std::string m_name;
    std::vector<std::unique_ptr<MenuEntry>> m_entries;

  public:
    Menu(const std::string &name, MenuEntryType entryType);

    const std::string &name() const;

    Menu &addMenu(const std::string &name, MenuEntryType entryType = MenuEntryType::Menu_None);

    void addSeparator();

    MenuActionItem &addItem(const Action *action, MenuEntryType entryType = MenuEntryType::Menu_None);

    void accept(MenuVisitor &visitor) const override;

    void visitEntries(MenuVisitor &visitor) const;
    const auto& entries() const { return m_entries; }

  deleteCopyAndMove(Menu);
};

using ActionVisitor = std::function<void(const Action &)>;

class ActionManager {
  private:
    /**
     * All actions which are used either in a menu, a tool bar or as a shortcut.
     * Indexed by preference path.
     */
    std::map<std::filesystem::path, std::unique_ptr<Action>> m_actions;

    /**
     * The main menu for the map editing window.
     * These will hold pointers to the actions in m_actions.
     */
    std::vector<std::unique_ptr<Menu>> m_mainMenu;

    /**
     * The toolbars for the map editing window. Stored as a menu to allow for separators.
     * These will hold pointers to the actions in m_actions.
     */
    std::unique_ptr<Menu> m_toolBar;
    std::map<std::filesystem::path, std::unique_ptr<Action>> m_commands;
    std::vector<std::unique_ptr<Menu>> m_nativeMenus;
    EditorConfiguration m_defaults, m_configuration;
    NotifierConnection m_configurationConnection;
    bool m_configurationPending = false;
    bool m_actionsPending = false;

    void initializeConfiguration();
    void applyConfiguration(EditorConfiguration configuration);
    void rebuildConfiguredMenus();
    void queueConfigurationChanged();

  private:
    ActionManager();

  public:
  deleteCopyAndMove(ActionManager);

    static ActionManager &instance();
    Notifier<> configurationDidChangeNotifier;
    Notifier<> actionsDidChangeNotifier;
    const EditorConfiguration& configuration() const { return m_configuration; }
    const EditorConfiguration& defaultConfiguration() const { return m_defaults; }
    QStringList commandIds() const;
    const Action* command(const QString& id) const;
    const Action* action(const QString& id) const;
    bool isBuiltin(const QString& id) const;
    bool setConfiguration(const EditorConfiguration& configuration, QStringList& errors);
    bool reloadConfiguration();
    void setActionShortcut(const Action& action, const QKeySequence& shortcut);
    void resetActionShortcut(const Action& action);
    void resetActionConfiguration();
    void resetToolbarConfiguration();

    /**
     * Note, unlike createAction(), these are not registered / owned by the ActionManager.
     */
    std::vector<std::unique_ptr<Action>> createTagActions(const std::vector<Model::SmartTag> &tags) const;

    /**
     * Note, unlike createAction(), these are not registered / owned by the ActionManager.
     */
    std::vector<std::unique_ptr<Action>> createEntityDefinitionActions(const std::vector<Assets::EntityDefinition *> &entityDefinitions) const;

    void visitMainMenu(MenuVisitor &visitor) const;

    void visitToolBarActions(MenuVisitor &visitor) const;

    void createRenderViewToolbar(QToolBar *toolBar);

    /**
     * Visits actions not used in the menu or toolbar.
     */
    void visitMapViewActions(const ActionVisitor &visitor) const;

    const std::map<std::filesystem::path, std::unique_ptr<Action>> &actionsMap() const;

    class ResetMenuVisitor;

    void resetAllKeySequences() const;

  private:
    void initialize();

    void createViewActions();

    void createMenu();

    void createFileMenu();

    void createEditMenu();

    void createViewMenu();

    void createRunMenu();

    void createDebugMenu();

    void createHelpMenu();

    Menu &createMainMenu(const std::string &name);

    void createToolbar();

    const Action *existingAction(const std::filesystem::path &preferencePath) const;

    template<class ExecuteFn, class EnabledFn> static std::unique_ptr<Action> makeAction(const std::filesystem::path &preferencePath, const QString &label, const ActionContext::Type actionContext, const ExecuteFn &execute, const EnabledFn &enabled) {
        const auto checkedFn = [](ActionExecutionContext &) { return false; };
        return std::unique_ptr<Action>(new LambdaAction<ExecuteFn, EnabledFn, decltype(checkedFn)>(preferencePath, label, actionContext, QKeySequence(), execute, enabled, checkedFn, false, std::filesystem::path(), QString()));
    }

    template<class ExecuteFn, class EnabledFn> const Action *createMenuAction(const std::filesystem::path &preferencePath, const QString &label, const int key, const ExecuteFn &execute, const EnabledFn &enabled, const std::filesystem::path &iconPath = std::filesystem::path(), const QString &statusTip = QString()) {
        return createAction(preferencePath, label, ActionContext::Any, QKeySequence(key), execute, enabled, iconPath, statusTip);
    }

    template<class ExecuteFn, class EnabledFn, class CheckedFn> const Action *createMenuAction(const std::filesystem::path &preferencePath, const QString &label, const int key, const ExecuteFn &execute, const EnabledFn &enabled, const CheckedFn &checked, const std::filesystem::path &iconPath = std::filesystem::path(), const QString &statusTip = QString()) {
        return createAction(preferencePath, label, ActionContext::Any, QKeySequence(key), execute, enabled, checked, iconPath, statusTip);
    }

    template<class ExecuteFn, class EnabledFn> const Action *createMenuAction(const std::filesystem::path &preferencePath, const QString &label, const QKeySequence::StandardKey key, const ExecuteFn &execute, const EnabledFn &enabled, const std::filesystem::path &iconPath = std::filesystem::path(), const QString &statusTip = QString()) {
        return createAction(preferencePath, label, ActionContext::Any, QKeySequence(key), execute, enabled, iconPath, statusTip);
    }

    template<class ExecuteFn, class EnabledFn, class CheckedFn> const Action *createMenuAction(const std::filesystem::path &preferencePath, const QString &label, const QKeySequence::StandardKey key, const ExecuteFn &execute, const EnabledFn &enabled, const CheckedFn &checked, const std::filesystem::path &iconPath = std::filesystem::path(), const QString &statusTip = QString()) {
        return createAction(preferencePath, label, ActionContext::Any, QKeySequence(key), execute, enabled, checked, iconPath, statusTip);
    }

    template<class ExecuteFn, class EnabledFn> const Action *createAction(const std::filesystem::path &preferencePath, const QString &label, const ActionContext::Type actionContext, const QKeySequence &defaultShortcut, const ExecuteFn &execute, const EnabledFn &enabled, const std::filesystem::path &iconPath = std::filesystem::path(), const QString &statusTip = QString()) {

        const auto checkedFn = [](ActionExecutionContext &) { return false; };
        auto action = std::unique_ptr<Action>(new LambdaAction<ExecuteFn, EnabledFn, decltype(checkedFn)>(preferencePath, label, actionContext, defaultShortcut, execute, enabled, checkedFn, false, iconPath, statusTip));

        auto [it, didInsert] = m_actions.insert({preferencePath, std::move(action)});
        ensure(didInsert, "duplicate action name");
        return it->second.get();
    }

    template<class ExecuteFn, class EnabledFn, class CheckedFn> const Action *createAction(const std::filesystem::path &preferencePath, const QString &label, const ActionContext::Type actionContext, const QKeySequence &defaultShortcut, const ExecuteFn &execute, const EnabledFn &enabled, const CheckedFn &checked, const std::filesystem::path &iconPath = std::filesystem::path(), const QString &statusTip = QString()) {
        auto action = std::unique_ptr<Action>(new LambdaAction<ExecuteFn, EnabledFn, CheckedFn>(preferencePath, label, actionContext, defaultShortcut, execute, enabled, checked, true, iconPath, statusTip));

        auto [it, didInsert] = m_actions.insert({preferencePath, std::move(action)});
        ensure(didInsert, "duplicate action name");
        return it->second.get();
    }
};
} // namespace View
} // namespace TrenchBroom
