#pragma once

#include "Notifier.h"
#include "View/Commands/CommandHistory.h"
#include <QWidget>
#include <memory>

class QAction;
class QTreeWidget;
namespace TrenchBroom::View {
class MapDocument;
class HistoryEditor : public QWidget {
    Q_OBJECT
    std::weak_ptr<MapDocument> m_document;
    NotifierConnection m_connections;
    QTreeWidget* m_list = nullptr;
    QAction* m_undo = nullptr;
    QAction* m_redo = nullptr;
    QAction* m_jump = nullptr;
    QAction* m_clear = nullptr;
    CommandHistory m_snapshot;
    bool m_updatePending = false;
    bool m_navigating = false;

  public:
    explicit HistoryEditor(std::weak_ptr<MapDocument> document, QWidget* parent = nullptr);

  private:
    void scheduleUpdate();
    void refresh();
    void jumpTo(size_t position);
    void jumpToSelected();
    void updateActions();
};
} // namespace TrenchBroom::View
