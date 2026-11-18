#include "IO/PathQt.h"
#include "IO/SystemPaths.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/Actions/Actions.h"

#include <QApplication>
#include <QFile>
#include <QMenu>
#include <QTimer>
#include <algorithm>
#include <set>

namespace TrenchBroom::View {
namespace {
    class ConfiguredAction : public Action {
        const Action* m_command;
        bool m_enabled = true;

      public:
        ConfiguredAction(const EditorActionDefinition& definition, const Action& command)
            : Action(IO::pathFromQString(definition.id), definition.label, command.actionContext(),
                     definition.id == IO::pathAsQString(command.preferencePath()) ? command.defaultKeySequence() : QKeySequence{}, {}, {}),
              m_command{&command} {
            apply(definition, command);
        }
        void apply(const EditorActionDefinition& definition, const Action& command) {
            m_configured = true;
            m_label = definition.label;
            m_iconPath = IO::pathFromQString(definition.icon);
            m_statusTip = definition.tooltip;
            m_targets = definition.targets;
            m_commandId = definition.command;
            m_configuredShortcut = definition.shortcut;
            m_actionContext = command.actionContext();
            m_enabled = definition.enabled;
            m_enabledByConfiguration = definition.enabled;
            m_command = &command;
        }
        void retire() {
            m_enabled = m_enabledByConfiguration = false;
            m_targets = 0;
            m_configuredShortcut = QKeySequence{};
        }
        void execute(ActionExecutionContext& context) const override {
            if (enabled(context)) m_command->execute(context);
        }
        bool enabled(ActionExecutionContext& context) const override { return m_enabled && m_command->enabled(context); }
        bool checkable() const override { return m_command->checkable(); }
        bool checked(ActionExecutionContext& context) const override { return checkable() && m_command->checked(context); }
    };

    class DefinitionVisitor : public MenuVisitor {
        EditorConfiguration& m_config;
        bool m_toolbar;
        QStringList m_path;
        int m_group = 0, m_order = 0;

      public:
        DefinitionVisitor(EditorConfiguration& config, bool toolbar) : m_config{config}, m_toolbar{toolbar} {}
        void visit(const Menu& menu) override {
            if (!m_toolbar) m_path.push_back(QString::fromStdString(menu.name()));
            menu.visitEntries(*this);
            if (!m_toolbar) m_path.removeLast();
        }
        void visit(const MenuSeparatorItem&) override {
            if (m_toolbar) ++m_group;
        }
        void visit(const MenuActionItem& item) override {
            auto& definition = m_config.actions[IO::pathAsQString(item.action().preferencePath())];
            if (m_toolbar) {
                definition.targets |= MainToolbarTarget;
                definition.order = m_order++;
                definition.toolbarGroup = m_group;
            } else {
                definition.targets |= MainMenuTarget;
                definition.menuPath = m_path.join('/');
            }
        }
    };

    Menu* findChildMenu(Menu& parent, const QString& name) {
        for (const auto& entry : parent.entries())
            if (auto* child = dynamic_cast<Menu*>(entry.get()); child && QString::fromStdString(child->name()) == name) return child;
        return nullptr;
    }
} // namespace

QStringList ActionManager::commandIds() const {
    QStringList result;
    for (const auto& [id, action] : m_commands) {
        unused(action);
        result << IO::pathAsQString(id);
    }
    return result;
}
const Action* ActionManager::command(const QString& id) const {
    const auto it = m_commands.find(IO::pathFromQString(id));
    return it == m_commands.end() ? nullptr : it->second.get();
}
const Action* ActionManager::action(const QString& id) const {
    if (!m_configuration.actions.contains(id)) return nullptr;
    const auto it = m_actions.find(IO::pathFromQString(id));
    return it == m_actions.end() ? nullptr : it->second.get();
}
bool ActionManager::isBuiltin(const QString& id) const {
    return m_defaults.actions.contains(id);
}

void ActionManager::initializeConfiguration() {
    for (const auto& [path, action] : m_actions) {
        EditorActionDefinition definition;
        definition.id = IO::pathAsQString(path);
        definition.command = definition.id;
        definition.label = action->label();
        definition.icon = action->hasIcon() ? IO::pathAsQString(action->iconPath()) : QString{};
        definition.tooltip = action->statusTip();
        m_defaults.actions.insert(definition.id, definition);
    }
    DefinitionVisitor menus{m_defaults, false}, toolbar{m_defaults, true};
    visitMainMenu(menus);
    visitToolBarActions(toolbar);
    for (const auto* id :
         {"Menu/Edit/Group", "Menu/Edit/Ungroup", "Menu/Edit/Create Linked Duplicate", "Menu/Edit/Select Linked Groups", "Menu/Edit/Separate Linked Groups", SaveSelectionAsPrefabActionPath})
        if (m_defaults.actions.contains(id)) m_defaults.actions[id].targets |= PopupMenuTarget;
    EditorToolbarDefinition shapes;
    shapes.id = "brush-shapes";
    shapes.label = "Create";
    for (const auto* shape : {"Cuboid", "Stairs", "Arch", "Cylinder", "Cone", "UVSphere", "IcoSphere", "Rock"})
        shapes.actions << "Editor/Create/" + QString(shape);
    m_defaults.toolbars.push_back(shapes);
    m_commands = std::move(m_actions);
    m_nativeMenus = std::move(m_mainMenu);
    QFile file{IO::pathAsQString(IO::SystemPaths::findResourceFile("defaults/EditorActions.json"))};
    if (file.open(QIODevice::ReadOnly)) {
        EditorConfiguration defaults;
        QStringList errors;
        if (readEditorConfiguration(file.readAll(), m_defaults, commandIds(), defaults, errors))
            m_defaults = std::move(defaults);
        else
            qWarning().noquote() << errors.join('\n');
    }
    // No notifications during singleton construction or partially built views.
    m_configurationPending = true;
    applyConfiguration(m_defaults);
    reloadConfiguration();
    m_configurationPending = false;
    m_actionsPending = false;
    m_configurationConnection = PreferenceManager::instance().preferenceDidChangeNotifier.connect([this](const auto& path) {
        if (path == Preferences::EditorConfiguration.path()) reloadConfiguration();
    });
}

bool ActionManager::reloadConfiguration() {
    EditorConfiguration configuration;
    QStringList errors;
    if (!readEditorConfiguration(pref(Preferences::EditorConfiguration).toUtf8(), m_defaults, commandIds(), configuration, errors)) {
        qWarning().noquote() << errors.join('\n');
        return false;
    }
    applyConfiguration(std::move(configuration));
    return true;
}

bool ActionManager::setConfiguration(const EditorConfiguration& configuration, QStringList& errors) {
    EditorConfiguration checked;
    if (!readEditorConfiguration(editorConfigurationJson(configuration), m_defaults, commandIds(), checked, errors)) return false;
    const auto bytes = editorConfigurationJson(checked, &m_defaults);
    applyConfiguration(std::move(checked));
    PreferenceManager::instance().set(Preferences::EditorConfiguration, QString::fromUtf8(bytes));
    return true;
}

void ActionManager::applyConfiguration(EditorConfiguration configuration) {
    if (configuration == m_configuration && !m_actions.empty()) return;
    const auto actionsChanged = configuration.actions != m_configuration.actions;
    for (auto& [id, action] : m_actions) {
        unused(id);
        static_cast<ConfiguredAction*>(action.get())->retire();
    }
    for (const auto& definition : configuration.actions) {
        const auto* native = command(definition.command);
        if (!native) continue;
        auto& action = m_actions[IO::pathFromQString(definition.id)];
        if (!action)
            action = std::make_unique<ConfiguredAction>(definition, *native);
        else
            static_cast<ConfiguredAction*>(action.get())->apply(definition, *native);
    }
    m_configuration = std::move(configuration);
    if (actionsChanged) rebuildConfiguredMenus();
    m_actionsPending |= actionsChanged;
    queueConfigurationChanged();
}

void ActionManager::queueConfigurationChanged() {
    if (m_configurationPending) return;
    if (!qApp) {
        configurationDidChangeNotifier();
        return;
    }
    m_configurationPending = true;
    QTimer::singleShot(0, qApp, [this] {
        m_configurationPending = false;
        const auto widgets = QApplication::allWidgets();
        const auto menuOpen = std::any_of(widgets.begin(), widgets.end(), [](const auto* widget) { return widget->property("tbMenuOpen").toBool(); });
        if (QApplication::activePopupWidget() || menuOpen) {
            m_configurationPending = true;
            QTimer::singleShot(50, qApp, [this] {
                m_configurationPending = false;
                queueConfigurationChanged();
            });
            return;
        }
        const auto actions = m_actionsPending;
        m_actionsPending = false;
        if (actions) actionsDidChangeNotifier();
        configurationDidChangeNotifier();
    });
}

void ActionManager::setActionShortcut(const Action& action, const QKeySequence& shortcut) {
    auto config = m_configuration;
    const auto id = IO::pathAsQString(action.preferencePath());
    if (!config.actions.contains(id)) return;
    config.actions[id].shortcut = shortcut;
    QStringList errors;
    setConfiguration(config, errors);
}
void ActionManager::resetActionShortcut(const Action& action) {
    const auto id = IO::pathAsQString(action.preferencePath());
    const auto* native = command(id);
    setActionShortcut(action, m_defaults.actions.value(id).shortcut.value_or(native ? native->defaultKeySequence() : QKeySequence{}));
}
void ActionManager::resetActionConfiguration() {
    auto config = m_configuration;
    config.actions = m_defaults.actions;
    for (auto& definition : config.actions) {
        const auto* native = command(definition.id);
        definition.shortcut = definition.shortcut.value_or(native ? native->defaultKeySequence() : QKeySequence{});
    }
    for (auto& toolbar : config.toolbars)
        for (auto it = toolbar.actions.begin(); it != toolbar.actions.end();) {
            if (!it->isEmpty() && !config.actions.contains(*it))
                it = toolbar.actions.erase(it);
            else
                ++it;
        }
    QStringList errors;
    setConfiguration(config, errors);
}
void ActionManager::resetToolbarConfiguration() {
    auto config = m_configuration;
    config.toolbars = m_defaults.toolbars;
    config.arrangements = m_defaults.arrangements;
    for (const auto& toolbar : config.toolbars)
        for (const auto& id : toolbar.actions)
            if (config.actions.contains(id)) config.actions[id].targets |= EditorToolbarTarget;
    QStringList errors;
    setConfiguration(config, errors);
}

void ActionManager::rebuildConfiguredMenus() {
    m_mainMenu.clear();
    std::set<QString> placed;
    std::map<QString, MenuEntryType> roles;
    const auto copy = [&](auto&& self, const Menu& source, Menu& destination, QString path) -> void {
        for (const auto& entry : source.entries()) {
            if (const auto* menu = dynamic_cast<const Menu*>(entry.get()))
                self(self, *menu, destination.addMenu(menu->name(), menu->entryType()), path + '/' + QString::fromStdString(menu->name()));
            else if (dynamic_cast<const MenuSeparatorItem*>(entry.get()))
                destination.addSeparator();
            else if (const auto* item = dynamic_cast<const MenuActionItem*>(entry.get())) {
                const auto id = IO::pathAsQString(item->action().preferencePath());
                roles[id] = item->entryType();
                const auto definition = m_configuration.actions.value(id);
                if ((definition.targets & MainMenuTarget) && definition.menuPath == path) {
                    destination.addItem(action(id), item->entryType());
                    placed.insert(id);
                }
            }
        }
    };
    for (const auto& menu : m_nativeMenus) {
        auto root = std::make_unique<Menu>(menu->name(), menu->entryType());
        copy(copy, *menu, *root, QString::fromStdString(menu->name()));
        m_mainMenu.push_back(std::move(root));
    }
    auto definitions = m_configuration.actions.values();
    std::stable_sort(definitions.begin(), definitions.end(), [](const auto& a, const auto& b) { return a.order < b.order; });
    for (const auto& definition : definitions) {
        if (!(definition.targets & MainMenuTarget) || placed.contains(definition.id)) continue;
        const auto path = definition.menuPath.split('/');
        Menu* root = nullptr;
        for (auto& menu : m_mainMenu)
            if (QString::fromStdString(menu->name()) == path.front()) root = menu.get();
        if (!root) {
            m_mainMenu.push_back(std::make_unique<Menu>(path.front().toStdString(), MenuEntryType::Menu_None));
            root = m_mainMenu.back().get();
        }
        for (int i = 1; i < path.size(); ++i) {
            auto* child = findChildMenu(*root, path[i]);
            root = child ? child : &root->addMenu(path[i].toStdString());
        }
        root->addItem(action(definition.id), roles.count(definition.id) ? roles.at(definition.id) : MenuEntryType::Menu_None);
    }
    m_toolBar = std::make_unique<Menu>("Main toolbar", MenuEntryType::Menu_None);
    auto previousGroup = -1;
    for (const auto& definition : definitions)
        if (definition.targets & MainToolbarTarget) {
            if (previousGroup >= 0 && previousGroup != definition.toolbarGroup) m_toolBar->addSeparator();
            m_toolBar->addItem(action(definition.id));
            previousGroup = definition.toolbarGroup;
        }
}
} // namespace TrenchBroom::View
