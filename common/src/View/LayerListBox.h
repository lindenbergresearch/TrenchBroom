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

#include "NotifierConnection.h"
#include <QPersistentModelIndex>
#include <QTreeWidget>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace TrenchBroom::Model {
class LayerNode;
class Node;
}

namespace TrenchBroom::View {
class MapDocument;
class MapView;
class LayerTreeItemDelegate;
class SignalDelayer;
class Selection;

// The historical name is retained for the LayerEditor integration. This is now a
// scene tree, with one item per node and no per-row widgets.
class LayerListBox : public QTreeWidget {
    Q_OBJECT
    Q_PROPERTY(int attributeColumnWidth READ attributeColumnWidth WRITE setAttributeColumnWidth)
    Q_PROPERTY(int branchIndicatorSize READ branchIndicatorSize WRITE setBranchIndicatorSize)
    Q_PROPERTY(QColor branchIndicatorColor MEMBER m_branchIndicatorColor)
    Q_PROPERTY(qreal branchIndicatorLineWidth MEMBER m_branchIndicatorLineWidth)
    std::weak_ptr<MapDocument> m_document;
    MapView* m_mapView;
    LayerTreeItemDelegate* m_itemDelegate;
    std::unordered_map<Model::Node *, QTreeWidgetItem *> m_items;
    SignalDelayer* m_refresh;

    bool m_syncingSelection = false;
    bool m_structureDirty = true;
    bool m_allItemsDirty = true;
    bool m_groupByType = false;
    int m_groupingThreshold = 5;
    bool m_selectionSyncEnabled = true;
    bool m_selectionSyncPending = false;
    bool m_ignoreLeftRelease = false;
    QPersistentModelIndex m_attributePress;
    bool m_updatingAttribute = false;
    int m_attributeColumnWidth = 24;
    int m_branchIndicatorSize = 10;
    QColor m_branchIndicatorColor;
    qreal m_branchIndicatorLineWidth = 1.5;

    std::unordered_set<Model::Node *> m_changedNodes;
    NotifierConnection m_notifierConnection;

public:
    explicit LayerListBox(std::weak_ptr<MapDocument> document, MapView* mapView, QWidget* parent = nullptr);
    Model::LayerNode* selectedLayer() const;
    void setSelectedLayer(Model::LayerNode* layer);
    void setGroupByType(bool enabled);
    void setGroupingThreshold(int threshold);
    void setSelectionSyncEnabled(bool enabled);
    bool groupByType() const { return m_groupByType; }
    int groupingThreshold() const { return m_groupingThreshold; }
    bool selectionSyncEnabled() const { return m_selectionSyncEnabled; }
    int attributeColumnWidth() const { return m_attributeColumnWidth; }
    void setAttributeColumnWidth(int width);
    int branchIndicatorSize() const { return m_branchIndicatorSize; }
    void setBranchIndicatorSize(int size);
    bool hasPrefabSelection() const;
    std::vector<Model::Node*> selectedTreeNodes() const;

private:
    enum class Grouping { Type, EntityClass, None };
    void connectObservers();
    void clearTree();
    void refreshTree();
    void synchronizeChildren(QTreeWidgetItem* parent, const std::vector<Model::Node *>& nodes, Grouping grouping = Grouping::Type);
    void removeItems(const std::vector<Model::Node *>& nodes);
    void forgetItem(const QTreeWidgetItem* item);
    void updateItem(QTreeWidgetItem* item) const;
    void updateItems();
    void selectInDocument(bool force = false);
    bool selectionExceedsSyncThreshold() const;
    static bool consumeSyncBudget(const QTreeWidgetItem& item, size_t& remaining);
    void documentSelectionChanged(const Selection& change);
    void toggleAttribute(Model::Node* node, int column);
    void syncSelectionFromDocument();
    std::vector<Model::Node *> selectionNodes() const;
    void focusItem(QTreeWidgetItem* item);
    void showContextMenu(QTreeWidgetItem* item, const QPoint& globalPosition);
    std::vector<Model::Node*> removableSelection();
    void deleteSelection();
    static Model::Node* nodeForItem(const QTreeWidgetItem* item);
    static void collectItemNodes(const QTreeWidgetItem* item, std::vector<Model::Node*>& nodes);

protected:
    void changeEvent(QEvent* event) override;
    bool event(QEvent* event) override;
    void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

signals:
    void layerSetCurrent(TrenchBroom::Model::LayerNode* layer);
    void layerRightClicked(TrenchBroom::Model::LayerNode* layer);
    void nodeVisibilityToggled(TrenchBroom::Model::Node* node);
    void nodeLockToggled(TrenchBroom::Model::Node* node);
    void treeChanged();
};
} // namespace TrenchBroom::View
