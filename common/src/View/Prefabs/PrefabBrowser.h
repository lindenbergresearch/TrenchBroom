#pragma once

#include "IO/PrefabLibrary.h"
#include "NotifierConnection.h"

#include <QWidget>
#include <memory>
#include <vector>

class QAction;
class QFileSystemWatcher;
class QLabel;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class QSpinBox;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

namespace TrenchBroom::View {
class MapDocument;
class MapView;

class PrefabBrowser : public QWidget {
    Q_OBJECT

    std::weak_ptr<MapDocument> m_document;
    MapView* m_mapView;
    IO::PrefabLibrary m_library;
    QTreeWidget* m_tree;
    QListWidget* m_tiles;
    QStackedWidget* m_views;
    QWidget* m_navigation;
    QLabel* m_location;
    QLineEdit* m_filter;
    QAction* m_linked;
    QSpinBox* m_brightness;
    QSpinBox* m_zoom;
    QLabel* m_status;
    QAction* m_add;
    QAction* m_remove;
    QAction* m_insert;
    QAction* m_treeView;
    QAction* m_tileView;
    QAction* m_up;
    QFileSystemWatcher* m_watcher;
    QTimer* m_reload;
    QTimer* m_cardRefresh = nullptr;
    NotifierConnection m_connections;
    std::vector<IO::PrefabInfo> m_entries;
    QString m_folder;
    bool m_usingTiles = false;

public:
    PrefabBrowser(std::weak_ptr<MapDocument> document, MapView* mapView, QWidget* parent = nullptr, QString libraryRoot = {});

private:
    void reload();
    void updateButtons();
    void updateSelection();
    void updateCards();
    void applyFilter();
    void rebuildTiles(const QString& selection = {});
    void setFolder(const QString& folder);
    void setTileView(bool tiles);
    void updateNavigation();
    QString selectedPath() const;
    void addPrefab();
    void removePrefab();
    void insertSelected();

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool event(QEvent* event) override;
};
}
