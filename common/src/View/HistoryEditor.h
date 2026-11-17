#pragma once

#include "Notifier.h"
#include "View/CommandHistory.h"
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
    CommandHistory m_snapshot;
    bool m_updatePending = false;

  public:
    explicit HistoryEditor(std::weak_ptr<MapDocument> document, QWidget* parent = nullptr);

  private:
    void scheduleUpdate();
    void refresh();
    void jumpTo(size_t position);
};
} // namespace TrenchBroom::View
