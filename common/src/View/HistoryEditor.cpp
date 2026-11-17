#include "View/HistoryEditor.h"
#include "IO/ResourceUtils.h"
#include "View/MapDocument.h"

#include <QDateTime>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace TrenchBroom::View {
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
    m_list = new QTreeWidget(this);
    m_list->setObjectName("HistoryList");
    m_list->setHeaderLabels({tr("Action"), tr("Time")});
    m_list->setRootIsDecorated(false);
    m_list->setAlternatingRowColors(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    connect(m_list, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) { jumpTo(size_t(item->data(0, Qt::UserRole).toULongLong())); });
    layout->addWidget(toolbar);
    layout->addWidget(m_list);
    if (auto doc = m_document.lock()) {
        m_connections += doc->commandHistoryDidChangeNotifier.connect(this, &HistoryEditor::scheduleUpdate);
        m_connections += doc->documentWasClearedNotifier.connect([this](auto*) { scheduleUpdate(); });
    }
    refresh();
}
void HistoryEditor::scheduleUpdate() {
    if (m_updatePending) return;
    m_updatePending = true;
    QTimer::singleShot(0, this, [this] {
        m_updatePending = false;
        refresh();
    });
}
void HistoryEditor::refresh() {
    const QSignalBlocker blocker(m_list);
    const auto doc = m_document.lock();
    auto history = doc ? doc->commandHistory() : CommandHistory{};
    m_undo->setEnabled(doc && doc->canUndoCommand());
    m_redo->setEnabled(doc && doc->canRedoCommand());
    if (m_list->topLevelItemCount() == 0) {
        auto* initial = new QTreeWidgetItem(m_list, {tr("Initial state"), QString{}});
        initial->setData(0, Qt::UserRole, qulonglong(0));
    }
    while (size_t(m_list->topLevelItemCount()) > history.size() + 1)
        delete m_list->takeTopLevelItem(m_list->topLevelItemCount() - 1);
    auto* current = m_list->topLevelItem(0);
    for (size_t i = 0; i < history.size(); ++i) {
        const auto& entry = history[i];
        auto* item = m_list->topLevelItem(int(i + 1));
        if (!item) item = new QTreeWidgetItem(m_list);
        if (i >= m_snapshot.size() || entry.name != m_snapshot[i].name || entry.timestamp != m_snapshot[i].timestamp || entry.applied != m_snapshot[i].applied) {
            const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(entry.timestamp.time_since_epoch()).count();
            const auto time = QDateTime::fromMSecsSinceEpoch(millis);
            item->setText(0, QString::fromStdString(entry.name));
            item->setText(1, time.toString("HH:mm:ss"));
            item->setData(0, Qt::UserRole, qulonglong(i + 1));
            item->setIcon(0, entry.applied ? m_undo->icon() : m_redo->icon());
            item->setToolTip(0, tr("%1\n%2\nDouble-click to return to this state").arg(entry.applied ? tr("Applied") : tr("Available for redo"), time.toString(Qt::DefaultLocaleLongDate)));
        }
        if (entry.applied) current = item;
    }
    m_snapshot = std::move(history);
    m_list->setCurrentItem(current);
    m_list->scrollToItem(current);
}
void HistoryEditor::jumpTo(size_t position) {
    const auto doc = m_document.lock();
    if (!doc) return;
    const auto history = doc->commandHistory();
    // Ignore a stale row if a command branched/replaced history before the queued refresh.
    if (history.size() != m_snapshot.size() ||
        !std::equal(history.begin(), history.end(), m_snapshot.begin(), [](const auto& a, const auto& b) { return a.name == b.name && a.timestamp == b.timestamp; })) {
        refresh();
        return;
    }
    position = std::min(position, history.size());
    auto current = doc->commandHistoryPosition();
    while (current != position) {
        if (current > position && doc->canUndoCommand())
            doc->undoCommand();
        else if (current < position && doc->canRedoCommand())
            doc->redoCommand();
        else
            break;
        const auto next = doc->commandHistoryPosition();
        if (next == current) break;
        current = next;
    }
    refresh();
}
} // namespace TrenchBroom::View
