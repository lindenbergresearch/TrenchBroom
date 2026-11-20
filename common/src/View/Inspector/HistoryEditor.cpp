#include "View/Inspector/HistoryEditor.h"
#include "IO/ResourceUtils.h"
#include "View/Document/MapDocument.h"
#include "View/Widgets/Lists/StyledItemDelegate.h"

#include <QAction>
#include <QDateTime>
#include <QHeaderView>
#include <QLocale>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace TrenchBroom::View {
namespace {
constexpr auto FirstPositionRole = Qt::UserRole + 1;
constexpr auto StateGroupRole = Qt::UserRole + 2;
class HistoryTree : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;
    bool primaryClick = false;
protected:
    void mouseReleaseEvent(QMouseEvent* event) override {
        primaryClick = event->button() == Qt::LeftButton;
        QTreeWidget::mouseReleaseEvent(event);
        primaryClick = false;
    }
};
QDateTime entryTime(const CommandHistoryEntry& entry) {
    return QDateTime::fromMSecsSinceEpoch(std::chrono::duration_cast<std::chrono::milliseconds>(entry.timestamp.time_since_epoch()).count());
}
bool sameEntry(const CommandHistoryEntry& a, const CommandHistoryEntry& b) {
    return a.name == b.name && a.timestamp == b.timestamp && a.modifiesDocument == b.modifiesDocument;
}
}
HistoryEditor::HistoryEditor(std::weak_ptr<MapDocument> document, QWidget* parent) : QWidget{parent}, m_document{std::move(document)} {
    setObjectName("HistoryEditor");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* toolbar = new QToolBar(this);
    toolbar->setObjectName("InspectorToolToolbar");
    toolbar->setProperty("inspectorToolbar", true);
    m_undo = toolbar->addAction(IO::loadSVGIcon("Undo.svg", 24), tr("Undo"), this, [this] {
        if (auto doc = m_document.lock(); doc && doc->canUndoCommand()) doc->undoCommand();
    });
    m_redo = toolbar->addAction(IO::loadSVGIcon("Redo.svg", 24), tr("Redo"), this, [this] {
        if (auto doc = m_document.lock(); doc && doc->canRedoCommand()) doc->redoCommand();
    });
    toolbar->addSeparator();
    m_jump = toolbar->addAction(IO::loadSVGIcon("history_restore.svg", 24), tr("Return to this state"), this, &HistoryEditor::jumpToSelected);
    m_jump->setObjectName("HistoryJump");
    m_jump->setToolTip(tr("Undo or redo all steps needed to return to the selected state."));
    m_clear = new QAction(IO::loadSVGIcon("Delete.svg", 24), tr("Clear history"), this);
    m_clear->setObjectName("HistoryClear");
    m_clear->setToolTip(tr("Discard undo and redo history. The map and its unsaved changes are preserved."));
    connect(m_clear, &QAction::triggered, this, [this] {
        if (auto doc = m_document.lock()) doc->clearCommandHistory();
        refresh();
    });
    auto* tree = new HistoryTree(this);
    m_list = tree;
    tree->setObjectName("HistoryList");
    tree->setItemDelegate(new StyledItemDelegate("history", tree));
    tree->setHeaderLabels({tr("Action"), tr("Time")});
    tree->setRootIsDecorated(true);
    tree->setUniformRowHeights(true);
    tree->setAlternatingRowColors(false);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree->setContextMenuPolicy(Qt::ActionsContextMenu);
    tree->addAction(m_jump);
    auto* separator = new QAction(tree); separator->setSeparator(true); tree->addAction(separator);
    tree->addActions({m_undo, m_redo});
    separator = new QAction(tree); separator->setSeparator(true); tree->addAction(separator);
    tree->addAction(m_clear);
    connect(tree, &QTreeWidget::itemClicked, this, [this, tree](QTreeWidgetItem* item, int) {
        if (tree->primaryClick) jumpTo(size_t(item->data(0, Qt::UserRole).toULongLong()));
    });
    connect(tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) { jumpTo(size_t(item->data(0, Qt::UserRole).toULongLong())); });
    connect(tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item, int) { jumpTo(size_t(item->data(0, Qt::UserRole).toULongLong())); });
    connect(tree, &QTreeWidget::currentItemChanged, this, [this] { updateActions(); });
    layout->addWidget(toolbar);
    layout->addWidget(tree);
    if (auto doc = m_document.lock()) {
        m_connections += doc->commandHistoryDidChangeNotifier.connect(this, &HistoryEditor::scheduleUpdate);
        m_connections += doc->documentWasClearedNotifier.connect([this](auto*) { scheduleUpdate(); });
    }
    refresh();
}
void HistoryEditor::scheduleUpdate() {
    if (m_updatePending || m_navigating) return;
    m_updatePending = true;
    QTimer::singleShot(0, this, [this] { m_updatePending = false; refresh(); });
}
void HistoryEditor::updateActions() {
    const auto doc = m_document.lock();
    const auto available = doc && (doc->canUndoCommand() || doc->canRedoCommand());
    m_undo->setEnabled(doc && doc->canUndoCommand());
    m_redo->setEnabled(doc && doc->canRedoCommand());
    const auto* item = m_list->currentItem();
    m_jump->setEnabled(available && item && item->data(0, Qt::UserRole).toULongLong() != doc->commandHistoryPosition());
    m_clear->setEnabled(available);
}
void HistoryEditor::refresh() {
    const QSignalBlocker blocker(m_list);
    const auto doc = m_document.lock();
    auto history = doc ? doc->commandHistory() : CommandHistory{};
    const auto position = doc ? doc->commandHistoryPosition() : 0;
    if (!m_list->topLevelItemCount()) {
        auto* initial = new QTreeWidgetItem(m_list, {tr("Initial state"), QString{}});
        initial->setData(0, Qt::UserRole, qulonglong(0));
        initial->setData(0, StyledItemDelegate::TextStyleRole, "action");
    }
    auto* current = m_list->topLevelItem(0);
    current->setData(0, StyledItemDelegate::TextStatesRole, position == 0 ? QStringList{"current"} : QStringList{});
    int row = 1;
    for (size_t first = 0; first < history.size();) {
        const auto grouped = !history[first].modifiesDocument;
        auto end = first + 1;
        if (grouped)
            while (end < history.size() && !history[end].modifiesDocument && history[end].applied == history[first].applied) ++end;
        auto* parent = m_list->topLevelItem(row);
        if (parent && (parent->data(0, StateGroupRole).toBool() != grouped || parent->data(0, FirstPositionRole).toULongLong() != first)) {
            delete m_list->takeTopLevelItem(row);
            parent = nullptr;
        }
        if (!parent) { parent = new QTreeWidgetItem; m_list->insertTopLevelItem(row, parent); }
        parent->setData(0, FirstPositionRole, qulonglong(first));
        parent->setData(0, StateGroupRole, grouped);
        parent->setData(0, Qt::UserRole, qulonglong(end));
        if (grouped) {
            parent->setText(0, tr("Editor state (%1)").arg(end - first));
            parent->setText(1, entryTime(history[end - 1]).toString("HH:mm:ss"));
            parent->setData(0, StyledItemDelegate::TextStyleRole, "stateGroup");
            parent->setData(1, StyledItemDelegate::TextStyleRole, "time");
            parent->setIcon(0, history[first].applied ? m_undo->icon() : m_redo->icon());
            parent->setToolTip(0, tr("Selection and other editor-state changes. Expand to inspect individual steps."));
            while (size_t(parent->childCount()) > end - first) delete parent->takeChild(parent->childCount() - 1);
        }
        for (size_t i = first; i < end; ++i) {
            const auto& entry = history[i];
            auto* item = grouped ? parent->child(int(i - first)) : parent;
            const bool created = !item;
            if (!item) item = new QTreeWidgetItem(parent);
            if (created || item->data(0, Qt::UserRole).toULongLong() != i + 1 || i >= m_snapshot.size()
                || !sameEntry(entry, m_snapshot[i]) || entry.applied != m_snapshot[i].applied || item->text(0).isEmpty()) {
                const auto time = entryTime(entry);
                item->setText(0, QString::fromStdString(entry.name));
                item->setText(1, time.toString("HH:mm:ss"));
                item->setData(0, Qt::UserRole, qulonglong(i + 1));
                item->setData(0, StyledItemDelegate::TextStyleRole, "action");
                item->setData(1, StyledItemDelegate::TextStyleRole, "time");
                item->setIcon(0, entry.applied ? m_undo->icon() : m_redo->icon());
                item->setToolTip(0, tr("%1\n%2\nClick to return to this state").arg(entry.applied ? tr("Applied") : tr("Available for redo"), QLocale{}.toString(time, QLocale::LongFormat)));
            }
            const auto states = i + 1 == position ? QStringList{"current"} : entry.applied ? QStringList{} : QStringList{"redo"};
            item->setData(0, StyledItemDelegate::TextStatesRole, states);
            item->setData(1, StyledItemDelegate::TextStatesRole, states);
            if (i + 1 == position) current = grouped && !parent->isExpanded() ? parent : item;
        }
        if (grouped) parent->setData(0, StyledItemDelegate::TextStatesRole,
            current == parent ? QStringList{"current"} : history[first].applied ? QStringList{} : QStringList{"redo"});
        first = end;
        ++row;
    }
    while (m_list->topLevelItemCount() > row) delete m_list->takeTopLevelItem(m_list->topLevelItemCount() - 1);
    m_snapshot = std::move(history);
    m_list->setCurrentItem(current);
    m_list->scrollToItem(current);
    updateActions();
}
void HistoryEditor::jumpToSelected() {
    if (auto* item = m_list->currentItem()) jumpTo(size_t(item->data(0, Qt::UserRole).toULongLong()));
}
void HistoryEditor::jumpTo(size_t position) {
    const auto doc = m_document.lock();
    if (!doc || m_navigating) return;
    const auto history = doc->commandHistory();
    if (history.size() != m_snapshot.size() || !std::equal(history.begin(), history.end(), m_snapshot.begin(), sameEntry)) {
        refresh();
        return;
    }
    position = std::min(position, history.size());
    m_navigating = true;
    auto* window = this->window();
    const bool updates = window->updatesEnabled();
    window->setUpdatesEnabled(false);
    const auto restore = qScopeGuard([&] { window->setUpdatesEnabled(updates); m_navigating = false; });
    auto current = doc->commandHistoryPosition();
    while (current != position) {
        if (current > position && doc->canUndoCommand()) doc->undoCommand();
        else if (current < position && doc->canRedoCommand()) doc->redoCommand();
        else break;
        const auto next = doc->commandHistoryPosition();
        if (next == current) break;
        current = next;
    }
    refresh();
}
} // namespace TrenchBroom::View
