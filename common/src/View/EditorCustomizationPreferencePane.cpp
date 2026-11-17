#include "View/EditorCustomizationPreferencePane.h"
#include "View/Splitter.h"
#include "IO/PathQt.h"
#include "IO/ResourceUtils.h"
#include "IO/SystemPaths.h"
#include "View/Actions.h"
#include "View/FormWithSectionsLayout.h"
#include "View/IconSelectionDialog.h"
#include "View/QtUtils.h"
#include "View/SearchableComboBox.h"
#include "View/ViewConstants.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QToolBar>
#include <QUuid>
#include <algorithm>

namespace TrenchBroom::View {
namespace {
    QAction* tool(QToolBar* bar, const char* icon, const QString& text, const QString& name, std::function<void()> execute) {
        auto* action = bar->addAction(IO::loadSVGIcon(icon, 16), text);
        action->setObjectName(name);
        QObject::connect(action, &QAction::triggered, bar, std::move(execute));
        return action;
    }
    QSpinBox* spin(int minimum, int maximum, const char* name) {
        auto* value = new QSpinBox;
        value->setRange(minimum, maximum);
        value->setObjectName(name);
        return value;
    }
    QIcon actionIcon(const EditorActionDefinition& action) { return action.icon.isEmpty() ? QIcon{} : IO::loadSVGIcon(IO::pathFromQString(action.icon), 24); }
    FormWithSectionsLayout* sectionForm(QWidget* widget) {
        auto* layout = new FormWithSectionsLayout{widget};
        layout->setContentsMargins(LayoutConstants::MediumHMargin, LayoutConstants::WideHMargin, LayoutConstants::WideHMargin, LayoutConstants::WideVMargin);
        layout->setVerticalSpacing(LayoutConstants::MediumVMargin);
        layout->setHorizontalSpacing(LayoutConstants::WideHMargin * 2);
        layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        layout->setFormAlignment(Qt::AlignTop);
        return layout;
    }
    void inspectorToolbar(QToolBar* toolbar) {
        toolbar->setProperty("inspectorToolbar", true);
        toolbar->setMovable(false);
        toolbar->setFloatable(false);
        toolbar->setIconSize({16, 16});
    }
} // namespace

EditorCustomizationPreferencePane::EditorCustomizationPreferencePane(bool toolbarMode, QWidget* parent)
    : PreferencePane{parent}, m_toolbarMode{toolbarMode}, m_list{new QListWidget}, m_filter{new QLineEdit}, m_label{new QLineEdit}, m_error{new QLabel},
      m_identity{new QLabel}, m_enabled{new QCheckBox}, m_listTools{new QToolBar}, m_form{new QWidget} {
    setObjectName(toolbarMode ? "EditorToolbarPreferences" : "EditorActionsPreferences");
    m_list->setObjectName("EditorConfigurationList");
    m_list->setProperty("preferencesList", true);
    m_list->setIconSize({24, 24});
    m_filter->setPlaceholderText(tr("Search…"));
    m_filter->setObjectName("EditorConfigurationSearch");
    m_list->setUniformItemSizes(true);
    m_list->setTextElideMode(Qt::ElideRight);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_label->setObjectName("EditorEntryLabel");
    m_enabled->setObjectName("EditorEntryEnabled");
    m_enabled->setAccessibleName(tr("Enabled"));
    m_identity->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_error->setObjectName("EditorConfigurationError");
    m_error->setWordWrap(true);
    m_error->hide();
    m_listTools->setObjectName("EditorConfigurationToolbar");
    inspectorToolbar(m_listTools);
    m_form->setProperty("preferenceForm", true);
    tool(m_listTools, "Add.svg", tr("Add"), "EditorEntryAdd", [this] { addEntry(false); });
    tool(m_listTools, "DuplicateObjects.svg", tr("Duplicate"), "EditorEntryDuplicate", [this] { addEntry(true); });
    tool(m_listTools, "Remove.svg", tr("Remove"), "EditorEntryRemove", [this] { removeEntry(); });
    if (toolbarMode) {
        tool(m_listTools, "Up.svg", tr("Move up"), "EditorEntryUp", [this] { moveEntry(-1); });
        tool(m_listTools, "Down.svg", tr("Move down"), "EditorEntryDown", [this] { moveEntry(1); });
    }
    m_listTools->addSeparator();
    tool(m_listTools, "folder_open_2.svg", tr("Import actions and toolbars"), "EditorConfigurationImport", [this] { importConfiguration(); });
    tool(m_listTools, "document.svg", tr("Export actions and toolbars"), "EditorConfigurationExport", [this] { exportConfiguration(); });
    auto* left = new QWidget;
    auto* leftLayout = new QVBoxLayout{left};
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(m_listTools);
    leftLayout->addWidget(m_filter);
    leftLayout->addWidget(m_list);
    left->setMaximumWidth(300);
    if (toolbarMode)
        createToolbarForm();
    else
        createActionForm();
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    scroll->setWidget(m_form);
    auto* splitter = new Splitter;
    splitter->addWidget(left);
    splitter->addWidget(scroll);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({260, 600});
    auto* layout = new QVBoxLayout{this};
    layout->addWidget(splitter);
    layout->addWidget(m_error);
    connect(m_filter, &QLineEdit::textChanged, this, [this] {
        if (commit()) reload();
    });
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current, QListWidgetItem*) {
        if (m_loading) return;
        const auto nextId = current ? current->data(Qt::UserRole).toString() : QString{};
        if (!commit()) {
            const QSignalBlocker block{m_list};
            for (auto row = 0; row < m_list->count(); ++row)
                if (m_list->item(row)->data(Qt::UserRole).toString() == m_currentId) m_list->setCurrentRow(row);
            return;
        }
        m_currentId = nextId;
        loadCurrent();
    });
    watch(m_label);
    watch(m_enabled);
    m_connection = ActionManager::instance().configurationDidChangeNotifier.connect([this] {
        if (m_displayed != ActionManager::instance().configuration() && !m_dirty) reload();
    });
    reload();
}

void EditorCustomizationPreferencePane::watch(QLineEdit* edit) {
    connect(edit, &QLineEdit::textEdited, this, [this] {
        if (!m_loading) m_dirty = true;
    });
    connect(edit, &QLineEdit::editingFinished, this, [this] {
        if (!m_loading) commit();
    });
}
void EditorCustomizationPreferencePane::watch(QSpinBox* value) {
    connect(value, qOverload<int>(&QSpinBox::valueChanged), this, [this] {
        if (!m_loading) {
            m_dirty = true;
            commit();
        }
    });
}
void EditorCustomizationPreferencePane::watch(QComboBox* combo) {
    connect(combo, qOverload<int>(&QComboBox::activated), this, [this] {
        if (!m_loading) {
            m_dirty = true;
            commit();
        }
    });
}
void EditorCustomizationPreferencePane::watch(QCheckBox* check) {
    connect(check, &QCheckBox::clicked, this, [this] {
        if (!m_loading) {
            m_dirty = true;
            commit();
        }
    });
}

void EditorCustomizationPreferencePane::createActionForm() {
    auto* form = sectionForm(m_form);
    m_command = new SearchableComboBox;
    m_command->setObjectName("EditorActionCommand");
    QMap<QString, int> commandLabels;
    for (const auto& id : ActionManager::instance().commandIds())
        ++commandLabels[ActionManager::instance().command(id)->label()];
    for (const auto& id : ActionManager::instance().commandIds()) {
        const auto label = ActionManager::instance().command(id)->label();
        m_command->addItem(commandLabels[label] > 1 ? label + " — " + id : label, id);
        m_command->setItemData(m_command->count() - 1, id, Qt::ToolTipRole);
    }
    watch(m_command);
    m_shortcut = new QKeySequenceEdit;
    m_shortcut->setObjectName("EditorActionShortcut");
    connect(m_shortcut, &QKeySequenceEdit::keySequenceChanged, this, [this] {
        if (!m_loading) {
            m_dirty = m_shortcutDirty = true;
        }
    });
    connect(m_shortcut, &QKeySequenceEdit::editingFinished, this, [this] {
        if (!m_loading) commit();
    });
    m_icon = new QLineEdit;
    m_icon->setObjectName("EditorActionIcon");
    watch(m_icon);
    auto* browse = new QPushButton{tr("Choose icon…")};
    browse->setAutoDefault(false);
    browse->setObjectName("EditorActionBrowseIcon");
    auto* iconRow = new QHBoxLayout;
    iconRow->addWidget(m_icon);
    iconRow->addWidget(browse);
    connect(browse, &QPushButton::clicked, this, [this] {
        IconSelectionDialog dialog{m_icon->text(), this};
        if (dialog.exec() == QDialog::Accepted) {
            m_icon->setText(dialog.selectedIcon());
            m_dirty = true;
            commit();
        }
    });
    m_tooltip = new QLineEdit;
    m_tooltip->setObjectName("EditorActionTooltip");
    watch(m_tooltip);
    auto* targets = new QGridLayout;
    const QStringList names{tr("Main Menu"), tr("Popup Menu"), tr("Main Toolbar"), tr("Editor Toolbars")};
    for (size_t i = 0; i < m_targets.size(); ++i) {
        m_targets[i] = new QCheckBox{names[int(i)]};
        m_targets[i]->setObjectName("EditorActionTarget" + QString::number(i));
        targets->addWidget(m_targets[i], int(i / 2), int(i % 2));
        watch(m_targets[i]);
    }
    m_menu = new QLineEdit;
    m_menu->setObjectName("EditorActionMenuPath");
    m_menu->setPlaceholderText(tr("Editor Actions/Selection"));
    watch(m_menu);
    m_menu->setToolTip(tr("Menu hierarchy, separated by /. Missing menus are created automatically."));
    m_order = spin(0, 1000000, "EditorActionOrder");
    m_group = spin(0, 1000000, "EditorActionGroup");
    watch(m_order);
    watch(m_group);
    form->addSection(tr("Action"));
    form->addRow(tr("Action ID"), m_identity);
    form->addRow(tr("Label"), m_label);
    form->addRow(tr("Enabled"), m_enabled);
    form->addRow(tr("Command"), m_command);
    form->addRow(tr("Shortcut"), m_shortcut);
    form->addRow(tr("SVG icon"), iconRow);
    form->addRow(tr("Tooltip"), m_tooltip);
    form->addSection(tr("Display targets"));
    form->addRow(tr("Show in"), targets);
    form->addRow(tr("Main menu path"), m_menu);
    form->addRow(tr("Toolbar order"), m_order);
    form->addRow(tr("Toolbar group"), m_group);
    auto* note = new QLabel{tr("Commands are existing editor operations. Their selection and tool requirements remain active. Groups add separators to the main toolbar.")};
    note->setWordWrap(true);
    makeInfo(note);
    form->addRow(note);
}

void EditorCustomizationPreferencePane::createToolbarForm() {
    auto* layout = new QVBoxLayout{m_form};
    layout->setContentsMargins(0, 0, 0, 0);
    auto* tabs = new QTabWidget;
    tabs->setObjectName("EditorToolbarPages");
    auto* settings = new QWidget;
    auto* elements = new QWidget;
    tabs->addTab(settings, tr("Toolbar settings"));
    tabs->addTab(elements, tr("Elements"));
    layout->addWidget(tabs);
    auto* form = sectionForm(settings);
    m_edge = new QComboBox;
    m_edge->setObjectName("EditorToolbarEdge");
    m_edge->addItems({tr("Left"), tr("Top"), tr("Right"), tr("Bottom")});
    m_alignment = new QComboBox;
    m_alignment->setObjectName("EditorToolbarAlignment");
    m_alignment->addItems({tr("Start"), tr("Center"), tr("End")});
    m_arrangement = new QComboBox;
    m_arrangement->setObjectName("EditorToolbarArrangement");
    m_arrangement->addItems({tr("Along the edge"), tr("Parallel rows / columns")});
    m_arrangement->setToolTip(tr("Applies to every toolbar on this side."));
    m_size = spin(8, 128, "EditorToolbarSize");
    m_size->setSuffix(tr(" px"));
    m_minimum = spin(8, 128, "EditorToolbarMinimumSize");
    m_minimum->setSuffix(tr(" px"));
    watch(m_edge);
    watch(m_alignment);
    watch(m_arrangement);
    watch(m_size);
    watch(m_minimum);
    form->addSection(tr("Toolbar"));
    form->addRow(tr("Toolbar ID"), m_identity);
    form->addRow(tr("Name"), m_label);
    form->addRow(tr("Enabled"), m_enabled);
    form->addSection(tr("Placement"));
    form->addRow(tr("Position"), m_edge);
    form->addRow(tr("Alignment"), m_alignment);
    form->addRow(tr("Arrangement on this side"), m_arrangement);
    form->addSection(tr("Size"));
    form->addRow(tr("Preferred icon size"), m_size);
    form->addRow(tr("Minimum icon size"), m_minimum);
    auto* elementForm = sectionForm(elements);
    elementForm->addSection(tr("Toolbar elements"),
                            tr("Select an element and choose its action below. Use + to add another element; the list order determines its position in the toolbar."));
    auto* tools = new QToolBar;
    tools->setObjectName("EditorAssignedActionsToolbar");
    inspectorToolbar(tools);
    tool(tools, "Add.svg", tr("Add element"), "EditorToolbarAssign", [this] { assignAction(false); });
    tool(tools, "table_rows_narrow.svg", tr("Add separator"), "EditorToolbarSeparator", [this] { assignAction(true); });
    tool(tools, "Remove.svg", tr("Remove action"), "EditorToolbarUnassign", [this] { removeAssigned(); });
    tool(tools, "Up.svg", tr("Move up"), "EditorToolbarActionUp", [this] { moveAssigned(-1); });
    tool(tools, "Down.svg", tr("Move down"), "EditorToolbarActionDown", [this] { moveAssigned(1); });
    m_available = new SearchableComboBox;
    m_available->setObjectName("EditorToolbarAvailableAction");
    m_assigned = new QListWidget;
    m_assigned->setObjectName("EditorToolbarAssignedActions");
    m_assigned->setProperty("preferencesList", true);
    m_assigned->setIconSize({24, 24});
    m_assigned->setUniformItemSizes(true);
    m_assigned->setTextElideMode(Qt::ElideRight);
    m_assigned->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_available->setToolTip(tr("Search by name or command. Choosing a result assigns it to the selected toolbar element."));
    auto* entries = new QWidget;
    auto* entryLayout = new QVBoxLayout{entries};
    entryLayout->setContentsMargins(0, 0, 0, 0);
    entryLayout->setSpacing(0);
    entryLayout->addWidget(tools);
    entryLayout->addWidget(m_assigned, 1);
    m_assigned->setMinimumHeight(180);
    elementForm->addRow(tr("Action"), m_available);
    elementForm->addRow(entries);
    connect(m_assigned, &QListWidget::currentRowChanged, this, [this] { selectAssignedAction(); });
    connect(m_available, &QComboBox::editTextChanged, this, [this] { updateAssignedTools(); });
    connect(m_available, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { updateAssignedTools(); });
    connect(m_available, &SearchableComboBox::searchStateChanged, this, [this] { updateAssignedTools(); });
    connect(m_available, qOverload<int>(&QComboBox::activated), this, [this] { replaceAssignedAction(); });
    auto* note = new QLabel{tr("Toolbars on one side shrink together. If they cannot fit at the minimum size, that side is hidden. Appearance is configured in base.qss, separately for "
                               "left, top, right and bottom.")};
    note->setWordWrap(true);
    makeInfo(note);
    form->addRow(note);
}

void EditorCustomizationPreferencePane::reload() {
    m_loading = true;
    m_dirty = m_shortcutDirty = false;
    m_list->clear();
    auto& manager = ActionManager::instance();
    m_displayed = manager.configuration();
    const auto filter = m_filter->text();
    const auto add = [&](const QString& id, const QString& label, const QString& detail, const QIcon& icon) {
        if (!filter.isEmpty() && !(label + id + detail).contains(filter, Qt::CaseInsensitive)) return;
        auto* item = new QListWidgetItem{icon, label, m_list};
        item->setData(Qt::UserRole, id);
        item->setToolTip(detail + '\n' + id);
        if (id == m_currentId) m_list->setCurrentItem(item);
    };
    if (m_toolbarMode)
        for (const auto& toolbar : m_displayed.toolbars)
            add(toolbar.id, toolbar.label, editorToolbarEdgeName(toolbar.edge), {});
    else
        for (const auto& action : m_displayed.actions)
            add(action.id, action.label, action.command, actionIcon(action));
    if (!m_list->currentItem() && m_list->count()) m_list->setCurrentRow(0);
    m_currentId = m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toString() : QString{};
    m_loading = false;
    loadCurrent();
}

void EditorCustomizationPreferencePane::loadCurrent() {
    m_loading = true;
    m_dirty = m_shortcutDirty = false;
    m_error->hide();
    m_identity->setText(m_currentId);
    m_form->setEnabled(!m_currentId.isEmpty());
    if (m_currentId.isEmpty()) {
        m_label->clear();
        m_enabled->setChecked(false);
        if (!m_toolbarMode) {
            m_command->setCurrentIndex(-1);
            m_shortcut->clear();
            m_icon->clear();
            m_tooltip->clear();
            m_menu->clear();
            for (auto* target : m_targets)
                target->setChecked(false);
        }
    }
    auto& manager = ActionManager::instance();
    if (m_toolbarMode) {
        const auto& toolbars = manager.configuration().toolbars;
        const auto it = std::find_if(toolbars.begin(), toolbars.end(), [&](const auto& t) { return t.id == m_currentId; });
        if (it != toolbars.end()) {
            m_label->setText(it->label);
            m_enabled->setChecked(it->enabled);
            m_edge->setCurrentIndex(int(it->edge));
            const auto vertical = it->edge == EditorToolbarEdge::Left || it->edge == EditorToolbarEdge::Right;
            m_alignment->setItemText(0, vertical ? tr("Top") : tr("Left"));
            m_alignment->setItemText(2, vertical ? tr("Bottom") : tr("Right"));
            m_alignment->setCurrentIndex(int(it->alignment));
            m_arrangement->setCurrentIndex(int(manager.configuration().arrangements[size_t(it->edge)]));
            m_size->setValue(it->iconSize);
            m_minimum->setMaximum(it->iconSize);
            m_minimum->setValue(it->minimumIconSize);
        }
        {
            const QSignalBlocker block{m_available};
            m_available->cancelSearch();
            m_available->clear();
            QMap<QString, int> actionLabels;
            for (const auto& action : manager.configuration().actions)
                ++actionLabels[action.label];
            for (const auto& action : manager.configuration().actions) {
                m_available->addItem(actionIcon(action), actionLabels[action.label] > 1 ? action.label + " — " + action.id : action.label, action.id);
                m_available->setItemData(m_available->count() - 1, action.tooltip + '\n' + action.id, Qt::ToolTipRole);
            }
        }
        loadAssigned();
    } else if (manager.configuration().actions.contains(m_currentId)) {
        const auto a = manager.configuration().actions[m_currentId];
        m_label->setText(a.label);
        m_enabled->setChecked(a.enabled);
        m_command->setCurrentIndex(m_command->findData(a.command));
        m_command->lineEdit()->setCursorPosition(0);
        m_shortcut->setKeySequence(manager.action(a.id)->keySequence());
        m_icon->setText(a.icon);
        m_tooltip->setText(a.tooltip);
        m_menu->setText(a.menuPath);
        for (size_t i = 0; i < m_targets.size(); ++i)
            m_targets[i]->setChecked((a.targets & (1 << i)) != 0);
        m_menu->setEnabled(a.targets & MainMenuTarget);
        m_order->setEnabled(a.targets & MainToolbarTarget);
        m_group->setEnabled(a.targets & MainToolbarTarget);
        m_order->setValue(a.order);
        m_group->setValue(a.toolbarGroup);
    }
    m_loading = false;
    updateCurrentPresentation();
}

void EditorCustomizationPreferencePane::loadAssigned() {
    const QSignalBlocker block{m_assigned};
    m_assigned->clear();
    const auto& config = ActionManager::instance().configuration();
    const auto it = std::find_if(config.toolbars.begin(), config.toolbars.end(), [&](const auto& toolbar) { return toolbar.id == m_currentId; });
    if (it != config.toolbars.end()) {
        for (const auto& id : it->actions) {
            const auto definition = config.actions.value(id);
            auto* item = new QListWidgetItem{id.isEmpty() ? tr("— separator —") : definition.label, m_assigned};
            item->setData(Qt::UserRole, id);
            item->setIcon(actionIcon(definition));
            item->setToolTip(!id.isEmpty() && !(definition.targets & EditorToolbarTarget) ? tr("Hidden: Editor Toolbars target is disabled in Editor Actions.")
                                                                                          : definition.tooltip + '\n' + id);
        }
    }
    if (m_assigned->count()) m_assigned->setCurrentRow(0);
    selectAssignedAction();
}

void EditorCustomizationPreferencePane::selectAssignedAction() {
    const auto* item = m_assigned->currentItem();
    const auto id = item ? item->data(Qt::UserRole).toString() : QString{"Controls/Map view/Deactivate current tool"};
    const QSignalBlocker block{m_available};
    m_available->setCurrentIndex(m_available->findData(id));
    m_available->cancelSearch();
    updateAssignedTools();
}

void EditorCustomizationPreferencePane::replaceAssignedAction() {
    if (m_loading || !m_available->resolvedData().isValid()) return;
    const auto id = m_available->resolvedData().toString();
    const auto row = m_assigned->currentRow();
    if (!commit()) return;
    if (row < 0) {
        assignAction(false);
        return;
    }
    auto* item = m_assigned->item(row);
    if (item->data(Qt::UserRole).toString() == id) return;
    item->setData(Qt::UserRole, id);
    saveAssigned();
}

void EditorCustomizationPreferencePane::updateCurrentPresentation() {
    auto& manager = ActionManager::instance();
    // Updating one field must not rebuild lists or destroy an ongoing selection/command choice.
    const auto wasLoading = m_loading;
    m_loading = true;
    for (auto row = 0; row < m_list->count(); ++row) {
        auto* item = m_list->item(row);
        if (item->data(Qt::UserRole).toString() != m_currentId) continue;
        item->setText(m_label->text());
        if (!m_toolbarMode) item->setIcon(actionIcon(manager.configuration().actions.value(m_currentId)));
    }
    if (m_toolbarMode) {
        const auto vertical = m_edge->currentIndex() == int(EditorToolbarEdge::Left) || m_edge->currentIndex() == int(EditorToolbarEdge::Right);
        m_alignment->setItemText(0, vertical ? tr("Top") : tr("Left"));
        m_alignment->setItemText(2, vertical ? tr("Bottom") : tr("Right"));
        m_arrangement->setCurrentIndex(int(manager.configuration().arrangements[size_t(m_edge->currentIndex())]));
        m_minimum->setMaximum(m_size->value());
        updateAssignedTools();
    } else {
        m_menu->setEnabled(m_targets[0]->isChecked());
        m_order->setEnabled(m_targets[2]->isChecked());
        m_group->setEnabled(m_targets[2]->isChecked());
    }
    for (auto* action : m_listTools->actions()) {
        if (action->objectName() == "EditorEntryRemove") action->setEnabled(!m_currentId.isEmpty() && (m_toolbarMode || !manager.isBuiltin(m_currentId)));
        if (action->objectName() == "EditorEntryDuplicate") action->setEnabled(!m_currentId.isEmpty());
        if (action->objectName() == "EditorEntryUp") action->setEnabled(m_list->currentRow() > 0);
        if (action->objectName() == "EditorEntryDown") action->setEnabled(m_list->currentRow() >= 0 && m_list->currentRow() + 1 < m_list->count());
    }
    m_loading = wasLoading;
}

void EditorCustomizationPreferencePane::updateAssignedTools() {
    if (!m_assigned || !m_available) return;
    auto* tools = m_form->findChild<QToolBar*>("EditorAssignedActionsToolbar");
    const auto row = m_assigned->currentRow();
    for (auto* action : tools->actions()) {
        const auto name = action->objectName();
        if (name == "EditorToolbarAssign") action->setEnabled(!m_currentId.isEmpty() && m_available->resolvedData().isValid());
        if (name == "EditorToolbarSeparator") action->setEnabled(!m_currentId.isEmpty());
        if (name == "EditorToolbarUnassign") action->setEnabled(row >= 0);
        if (name == "EditorToolbarActionUp") action->setEnabled(row > 0);
        if (name == "EditorToolbarActionDown") action->setEnabled(row >= 0 && row + 1 < m_assigned->count());
    }
}

bool EditorCustomizationPreferencePane::save(EditorConfiguration configuration) {
    QStringList errors;
    if (!ActionManager::instance().setConfiguration(configuration, errors)) {
        m_error->setText(errors.join('\n'));
        m_error->show();
        return false;
    }
    m_displayed = ActionManager::instance().configuration();
    m_dirty = m_shortcutDirty = false;
    m_error->hide();
    return true;
}

bool EditorCustomizationPreferencePane::commit() {
    if (m_loading || !m_dirty || m_currentId.isEmpty()) return true;
    auto config = ActionManager::instance().configuration();
    if (m_toolbarMode) {
        auto it = std::find_if(config.toolbars.begin(), config.toolbars.end(), [&](const auto& t) { return t.id == m_currentId; });
        if (it == config.toolbars.end()) return true;
        const auto previous = it->edge;
        it->label = m_label->text().trimmed();
        it->enabled = m_enabled->isChecked();
        it->edge = EditorToolbarEdge(m_edge->currentIndex());
        it->alignment = EditorToolbarAlignment(m_alignment->currentIndex());
        it->iconSize = m_size->value();
        it->minimumIconSize = std::min(m_minimum->value(), it->iconSize);
        if (previous == it->edge) config.arrangements[size_t(it->edge)] = EditorToolbarArrangement(m_arrangement->currentIndex());
    } else {
        auto& a = config.actions[m_currentId];
        const auto oldCommand = a.command;
        a.label = m_label->text().trimmed();
        a.enabled = m_enabled->isChecked();
        a.icon = m_icon->text().trimmed();
        a.tooltip = m_tooltip->text();
        a.menuPath = m_menu->text().trimmed();
        // Search queries are transient. Only confirmed model IDs change commands.
        if (const auto command = m_command->resolvedData(); command.isValid()) a.command = command.toString();
        if (m_shortcutDirty) a.shortcut = m_shortcut->keySequence();
        a.targets = 0;
        for (size_t i = 0; i < m_targets.size(); ++i)
            if (m_targets[i]->isChecked()) a.targets |= 1 << i;
        a.order = m_order->value();
        a.toolbarGroup = m_group->value();
        const auto* command = ActionManager::instance().command(a.command);
        if (command && a.enabled && (m_shortcutDirty || oldCommand != a.command)) {
            const auto key = a.shortcut.value_or(ActionManager::instance().action(a.id)->keySequence());
            for (const auto& other : config.actions) {
                if (key.isEmpty() || other.id == a.id || !other.enabled) continue;
                const auto* action = ActionManager::instance().action(other.id);
                if (action && action->keySequence() == key &&
                    (command->actionContext() == ActionContext::Any || action->actionContext() == ActionContext::Any ||
                     actionContextMatches(command->actionContext(), action->actionContext()))) {
                    m_error->setText(tr("Shortcut is already used by %1.").arg(other.label));
                    m_error->show();
                    return false;
                }
            }
        }
    }
    if (!save(std::move(config))) return false;
    updateCurrentPresentation();
    return true;
}

void EditorCustomizationPreferencePane::addEntry(bool duplicate) {
    if (!commit()) return;
    auto& manager = ActionManager::instance();
    auto config = manager.configuration();
    const auto id = "User/" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (m_toolbarMode) {
        EditorToolbarDefinition entry;
        entry.id = id;
        entry.label = tr("New Toolbar");
        entry.actions << "Controls/Map view/Deactivate current tool";
        if (duplicate)
            for (const auto& toolbar : config.toolbars)
                if (toolbar.id == m_currentId) {
                    entry = toolbar;
                    entry.id = id;
                    entry.label += tr(" Copy");
                    break;
                }
        config.toolbars.push_back(entry);
        for (const auto& action : entry.actions)
            if (config.actions.contains(action)) config.actions[action].targets |= EditorToolbarTarget;
    } else {
        EditorActionDefinition entry;
        if (duplicate && config.actions.contains(m_currentId)) {
            entry = config.actions[m_currentId];
            entry.label += tr(" Copy");
        } else {
            entry.label = tr("New Action");
            entry.command = "Controls/Map view/Deactivate current tool";
            entry.icon = "document.svg";
        }
        entry.id = id;
        entry.shortcut = QKeySequence{};
        config.actions.insert(id, entry);
    }
    if (save(config)) {
        m_currentId = id;
        m_filter->clear();
        reload();
    }
}
void EditorCustomizationPreferencePane::removeEntry() {
    if (!commit() || m_currentId.isEmpty()) return;
    auto config = ActionManager::instance().configuration();
    if (m_toolbarMode)
        std::erase_if(config.toolbars, [&](const auto& t) { return t.id == m_currentId; });
    else {
        if (ActionManager::instance().isBuiltin(m_currentId)) return;
        config.actions.remove(m_currentId);
        for (auto& toolbar : config.toolbars)
            toolbar.actions.removeAll(m_currentId);
    }
    if (save(config)) reload();
}
void EditorCustomizationPreferencePane::moveEntry(int delta) {
    if (!commit()) return;
    auto config = ActionManager::instance().configuration();
    const auto it = std::find_if(config.toolbars.begin(), config.toolbars.end(), [&](const auto& t) { return t.id == m_currentId; });
    if (it == config.toolbars.end()) return;
    const auto index = int(it - config.toolbars.begin()), destination = index + delta;
    if (destination < 0 || destination >= int(config.toolbars.size())) return;
    std::swap(config.toolbars[size_t(index)], config.toolbars[size_t(destination)]);
    if (save(config)) reload();
}
void EditorCustomizationPreferencePane::assignAction(bool separator) {
    const auto choice = m_available->resolvedData();
    if (!separator && !choice.isValid()) return;
    const auto id = separator ? QString{} : choice.toString();
    const auto row = m_assigned->currentRow();
    if (!commit()) return;
    auto* item = new QListWidgetItem{separator ? tr("— separator —") : ActionManager::instance().configuration().actions.value(id).label};
    item->setData(Qt::UserRole, id);
    m_assigned->insertItem(row >= 0 ? row + 1 : m_assigned->count(), item);
    m_assigned->setCurrentItem(item);
    saveAssigned();
}
void EditorCustomizationPreferencePane::removeAssigned() {
    if (!commit()) return;
    delete m_assigned->takeItem(m_assigned->currentRow());
    saveAssigned();
}
void EditorCustomizationPreferencePane::moveAssigned(int delta) {
    if (!commit()) return;
    const auto row = m_assigned->currentRow(), next = row + delta;
    if (row < 0 || next < 0 || next >= m_assigned->count()) return;
    auto* item = m_assigned->takeItem(row);
    m_assigned->insertItem(next, item);
    m_assigned->setCurrentItem(item);
    saveAssigned();
}
void EditorCustomizationPreferencePane::saveAssigned() {
    auto config = ActionManager::instance().configuration();
    for (auto& toolbar : config.toolbars)
        if (toolbar.id == m_currentId) {
            toolbar.actions.clear();
            for (auto i = 0; i < m_assigned->count(); ++i) {
                const auto id = m_assigned->item(i)->data(Qt::UserRole).toString();
                toolbar.actions << id;
                if (config.actions.contains(id)) config.actions[id].targets |= EditorToolbarTarget;
            }
        }
    const auto row = m_assigned->currentRow();
    if (save(config)) {
        loadAssigned();
        m_assigned->setCurrentRow(std::min(row, m_assigned->count() - 1));
        updateAssignedTools();
    }
}
void EditorCustomizationPreferencePane::importConfiguration() {
    if (!commit()) return;
    const auto path = QFileDialog::getOpenFileName(this, tr("Import editor configuration"), {}, tr("JSON (*.json)"));
    if (path.isEmpty()) return;
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Import"), file.errorString());
        return;
    }
    EditorConfiguration configuration;
    QStringList errors;
    auto& manager = ActionManager::instance();
    if (!readEditorConfiguration(file.readAll(), manager.defaultConfiguration(), manager.commandIds(), configuration, errors)) {
        QMessageBox::warning(this, tr("Import"), errors.join('\n'));
        return;
    }
    if (save(configuration)) reload();
}
void EditorCustomizationPreferencePane::exportConfiguration() {
    if (!commit()) return;
    const auto path = QFileDialog::getSaveFileName(this, tr("Export editor configuration"), "editor-actions.json", tr("JSON (*.json)"));
    if (path.isEmpty()) return;
    auto configuration = ActionManager::instance().configuration();
    for (auto& definition : configuration.actions)
        definition.shortcut = ActionManager::instance().action(definition.id)->keySequence();
    QSaveFile file{path};
    const auto data = editorConfigurationJson(configuration);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) QMessageBox::warning(this, tr("Export"), file.errorString());
}
bool EditorCustomizationPreferencePane::doCanResetToDefaults() {
    return true;
}
void EditorCustomizationPreferencePane::doResetToDefaults() {
    if (m_toolbarMode)
        ActionManager::instance().resetToolbarConfiguration();
    else
        ActionManager::instance().resetActionConfiguration();
    reload();
}
void EditorCustomizationPreferencePane::doUpdateControls() {
    ActionManager::instance().reloadConfiguration();
    reload();
}
bool EditorCustomizationPreferencePane::doValidate() {
    return commit();
}
} // namespace TrenchBroom::View
