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

#include "LayerListBox.h"

#include "IO/ResourceUtils.h"
#include "Model/EditorContext.h"
#include "Model/BrushNode.h"
#include "Model/EntityNode.h"
#include "Model/GroupNode.h"
#include "Model/LayerNode.h"
#include "Model/ModelUtils.h"
#include "Model/WorldNode.h"
#include "Preferences.h"
#include "ThemeManager.h"
#include "PreferenceManager.h"
#include "View/MapDocument.h"
#include "View/NodeSourceDialog.h"
#include "View/LayerTreeItemRenderer.h"
#include "View/MapFrame.h"
#include "View/MapView.h"
#include "View/MapViewBase.h"
#include "View/MapView3D.h"
#include "View/QtUtils.h"
#include "View/PrefabUtils.h"
#include "View/Actions.h"
#include "View/Selection.h"
#include "View/SignalDelayer.h"

#include <QContextMenuEvent>
#include <QAction>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>
#include <QTreeWidgetItemIterator>

#include <kdl/memory_utils.h>
#include <kdl/set_temp.h>

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <ranges>
#include <set>
#include <tuple>
#include <unordered_set>
#include <utility>

namespace TrenchBroom::View {
/**
 *Local Namespace
 */
namespace {
constexpr auto NameColumn = 0;
constexpr auto VisibilityColumn = 1;
constexpr auto LockColumn = 2;


QString entityGroupClass(const Model::Node *node) {
    const auto *entity = dynamic_cast<const Model::EntityNode *>(node);

    if (!entity) return {};

    const auto &name = entity->entity().classname();
    // Quake light variants (torches, globes, etc.) belong to the same family.
    return QString::fromStdString(name == "light" || name.starts_with("light_") ? "light" : name);
}

struct TypeGroupState {
   bool expanded;
   bool selected;
   bool current;
};

Model::Node *groupParentNode(const QTreeWidgetItem *item) {
    for (auto *parent = item->parent(); parent; parent = parent->parent()) {
        if (const auto value = parent->data(NameColumn, Qt::UserRole).value<quintptr>())
            return reinterpret_cast<Model::Node *>(value);
    }

    return nullptr;
}

using TypeGroupKey = std::tuple<quintptr, int, QString>;

TypeGroupKey typeGroupKey(const QTreeWidgetItem *item) {
    return {reinterpret_cast<quintptr>(groupParentNode(item)),
            item->data(NameColumn, LayerTreeItemDelegate::TypeGroupRole).toInt(),
            item->data(NameColumn, LayerTreeItemDelegate::EntityClassRole).toString()};
}

// Containers map to editable scene objects, never to a selectable LayerNode or
// a brush-entity parent. Closed groups can be inspected down to individual nodes.
void collectSelection(Model::Node *node, const Model::EditorContext &context, std::vector<Model::Node *> &result) {
    if (!node) return;
    if (dynamic_cast<Model::LayerNode *>(node) || (dynamic_cast<Model::EntityNode *>(node) && node->hasChildren()) || (
        dynamic_cast<Model::GroupNode *>(node) &&
        static_cast<Model::GroupNode *>(node)->opened())) {
        for (auto *child : node->children())
            collectSelection(child, context, result);
    } else if (node->visible() && node->editable() && context.visible(node)) {
        result.push_back(node);
    }
}

std::unordered_set<Model::Node*> removedSubtrees(const std::vector<Model::Node*>& roots) {
    auto removed = std::unordered_set<Model::Node*>{};
    const auto collect = [&](auto&& self, Model::Node* node) -> void {
        if (!removed.insert(node).second) return;
        for (auto* child : node->children()) self(self, child);
    };
    for (auto* root : roots) collect(collect, root);
    // MapDocument also removes containers which become empty. Account for them
    // when checking locks and clearing selections before the undoable removal.
    for (auto* root : roots) for (auto* parent = root->parent(); parent; parent = parent->parent()) {
        if (!parent->removeIfEmpty() || !std::ranges::all_of(parent->children(), [&](auto* child) { return removed.contains(child); })) break;
        removed.insert(parent);
    }
    return removed;
}
}

LayerListBox::LayerListBox(std::weak_ptr<MapDocument> document, MapView *mapView, QWidget *parent)
    : QTreeWidget(parent), m_document(std::move(document)), m_mapView(mapView), m_itemDelegate(new LayerTreeItemDelegate(this)), m_refresh(new SignalDelayer(this)) {
    setObjectName("LayerTree");
    setAccessibleName(tr("Layers and objects"));
    setColumnCount(3);
    setHeaderLabels({tr("Object"), tr("Visibility"), tr("Lock")});

    setHeaderHidden(true);
    setSelectionMode(ExtendedSelection);
    setSelectionBehavior(SelectRows);
    setEditTriggers(NoEditTriggers);
    setExpandsOnDoubleClick(false);
    setUniformRowHeights(false);
    setItemDelegate(m_itemDelegate);

    setVerticalScrollMode(ScrollPerPixel);


    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    header()->setSectionResizeMode(VisibilityColumn, QHeaderView::Fixed);
    header()->setSectionResizeMode(LockColumn, QHeaderView::Fixed);

    setAttributeColumnWidth(m_attributeColumnWidth);

    m_groupByType = pref(Preferences::LayerTreeGroupByType);
    m_groupingThreshold = std::clamp(pref(Preferences::LayerTreeGroupingThreshold), 1, 100000);
    m_selectionSyncEnabled = pref(Preferences::LayerTreeSyncSelection);

    connect(this, &QTreeWidget::itemSelectionChanged, this, [this] { selectInDocument(); });
    connect(this, &QTreeWidget::itemClicked, this, [this](auto*, int column) {
        if (column == NameColumn) selectInDocument();
    });
    connect(m_refresh, &SignalDelayer::processSignal, this, &LayerListBox::refreshTree);
    connectObservers();

    refreshTree();
    syncSelectionFromDocument();
}

Model::LayerNode *LayerListBox::selectedLayer() const {
    return selectedItems().size() == 1 ?
           dynamic_cast<Model::LayerNode *>(nodeForItem(currentItem())) :
           nullptr;
}

void LayerListBox::setSelectedLayer(Model::LayerNode *layer) {
    refreshTree();
    const auto it = m_items.find(layer);
    if (it != m_items.end())
        setCurrentItem(it->second, NameColumn, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void LayerListBox::connectObservers() {
    const auto document = kdl::mem_lock(m_document);

    const auto structureChanged = [this](const auto &) {
        m_structureDirty = true;
        m_refresh->queueSignal();
    };

    m_notifierConnection += document->documentWillBeClearedNotifier.connect([this](auto *) { clearTree(); });
    m_notifierConnection += document->documentWasNewedNotifier.connect(structureChanged);
    m_notifierConnection += document->documentWasLoadedNotifier.connect(structureChanged);
    m_notifierConnection += document->documentWasClearedNotifier.connect(structureChanged);
    m_notifierConnection += document->nodesWillBeRemovedNotifier.connect(this, &LayerListBox::removeItems);
    m_notifierConnection += document->nodesWereAddedNotifier.connect(structureChanged);
    m_notifierConnection += document->nodesWereRemovedNotifier.connect(structureChanged);
    m_notifierConnection += document->nodesDidChangeNotifier.connect(
        [this](const auto &nodes) {
            for (auto *node : nodes) {
                m_changedNodes.insert(node);
                const auto it = m_items.find(node);
                if (m_groupByType && it != m_items.end()
                    && it->second->data(NameColumn, LayerTreeItemDelegate::EntityClassRole).toString() !=
                       entityGroupClass(node))
                    m_structureDirty = true;
            }
            // Layer sorting can change without adding/removing nodes. Ordinary edits
            // only update the affected rows, even when their ancestors are notified.
            const auto doc = kdl::mem_lock(m_document);
            if (doc->world()) {
                const auto layers = doc->world()->allLayersUserSorted();
                if (int(layers.size()) != topLevelItemCount()) m_structureDirty = true;
                else
                    for (auto i = 0; i < topLevelItemCount(); ++i) {
                        if (nodeForItem(topLevelItem(i)) != layers[size_t(i)])m_structureDirty = true;
                    }
            }

            m_refresh->queueSignal();
        }
    );

    const auto statesChanged = [this] {
        m_allItemsDirty = true;
        m_refresh->queueSignal();
    };

    m_notifierConnection += document->currentLayerDidChangeNotifier.connect([statesChanged](const auto *) { statesChanged(); });
    m_notifierConnection += document->nodeVisibilityDidChangeNotifier.connect([statesChanged](const auto &) { statesChanged(); });
    m_notifierConnection += document->nodeLockingDidChangeNotifier.connect([statesChanged](const auto &) { statesChanged(); });
    m_notifierConnection += document->editorContextDidChangeNotifier.connect(statesChanged);
    m_notifierConnection += document->selectionDidChangeNotifier.connect(this, &LayerListBox::documentSelectionChanged);
    m_notifierConnection += PreferenceManager::instance().preferenceDidChangeNotifier.connect([this](const auto &path) {
        if (path == Preferences::LayerTreeGroupByType.path()) setGroupByType(pref(Preferences::LayerTreeGroupByType));
        else if (path == Preferences::LayerTreeGroupingThreshold.path())
            setGroupingThreshold(pref(Preferences::LayerTreeGroupingThreshold));
        else if (path == Preferences::LayerTreeSyncSelection.path())
            setSelectionSyncEnabled(pref(Preferences::LayerTreeSyncSelection));
        else if (path == Preferences::UIFontMinimumSize.path()) {
            scheduleDelayedItemsLayout();
            viewport()->update();
        }
        else if (path == Preferences::UIDetailTextOpacity.path()) viewport()->update();
        else if (LayerTreeItemDelegate::isIconColorPreference(path)) {
            if (!ThemeManager::instance().current().values.isEmpty()) return;
            // Change decorations only: keep the selection, expansion and scroll
            // state, including rows synthesized by grouping.
            const auto blocker = QSignalBlocker(this);
            for (QTreeWidgetItemIterator item(this); *item; ++item) {
                m_itemDelegate->updateIcon(**item, path);
            }
        }
    });
    m_notifierConnection += ThemeManager::instance().themeDidChangeNotifier.connect([this](const QStringList& names) {
        if (std::any_of(names.begin(), names.end(), [](const auto& name) { return name.startsWith("color.icon."); }))
            viewport()->update();
    });
}

void LayerListBox::setGroupByType(const bool enabled) {
    if (m_groupByType == enabled) return;
    m_groupByType = enabled;
    m_structureDirty = true;
    refreshTree();
}

void LayerListBox::setGroupingThreshold(const int threshold) {
    const auto value = std::clamp(threshold, 1, 100000);
    if (m_groupingThreshold == value) return;
    m_groupingThreshold = value;
    m_structureDirty = true;
    refreshTree();
}

void LayerListBox::setSelectionSyncEnabled(const bool enabled) {
    if (m_selectionSyncEnabled == enabled) return;
    m_selectionSyncEnabled = enabled;
    m_selectionSyncPending = false;
    // On reconnect the editor is authoritative; browsing never overwrites it.
    if (enabled) syncSelectionFromDocument();
    emit treeChanged();
}

void LayerListBox::setAttributeColumnWidth(int width) {
    m_attributeColumnWidth = std::max(1,width);
    setColumnWidth(VisibilityColumn,m_attributeColumnWidth);
    setColumnWidth(LockColumn,m_attributeColumnWidth);
}

void LayerListBox::setBranchIndicatorSize(int size) { m_branchIndicatorSize = std::clamp(size,1,64); viewport()->update(); }

void LayerListBox::drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const {
    if (rect.isEmpty() || !model()->hasChildren(index)) return;
    const auto x = layoutDirection() == Qt::RightToLeft ? rect.left()+indentation()/2.0 : rect.right()-indentation()/2.0+1;
    const auto center = QPointF(x, rect.center().y()+0.5);
    const auto half = m_branchIndicatorSize/2.0;
    QPolygonF chevron;
    if (isExpanded(index)) chevron << center+QPointF(-half,-half/2) << center+QPointF(0,half/2) << center+QPointF(half,-half/2);
    else {
        const auto direction = layoutDirection() == Qt::RightToLeft ? -1.0 : 1.0;
        chevron << center+QPointF(-direction*half/2,-half) << center+QPointF(direction*half/2,0) << center+QPointF(-direction*half/2,half);
    }
    const auto color = m_branchIndicatorColor.isValid() ? m_branchIndicatorColor : palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled,QPalette::Text);
    painter->save(); painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(color,std::clamp(m_branchIndicatorLineWidth,qreal(0.5),qreal(8)),Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    painter->drawPolyline(chevron); painter->restore();
}

void LayerListBox::changeEvent(QEvent* event) {
    QTreeWidget::changeEvent(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange) {
        scheduleDelayedItemsLayout(); viewport()->update();
    }
}

void LayerListBox::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    auto row = option;
    row.rect.setLeft(0); row.rect.setWidth(viewport()->width());
    if (selectionModel()->isRowSelected(index.row(), index.parent())) row.state |= QStyle::State_Selected;
    m_itemDelegate->paintRowBackground(*painter, row, index);

    auto content = option;
    content.state &= ~QStyle::State_HasFocus;
    // Keep the selected state for the delegates, but suppress Qt's second
    // selection background because the complete row has already been painted.
    content.palette.setBrush(QPalette::Highlight, Qt::NoBrush);
    QTreeWidget::drawRow(painter, content, index);
}

void LayerListBox::clearTree() {
    const auto blocker = QSignalBlocker(this);
    m_items.clear();
    m_changedNodes.clear();
    clear();
    m_structureDirty = true;
    m_selectionSyncPending = false;
}

void LayerListBox::refreshTree() {
    const auto document = m_document.lock();
    if (!document || !document->world()) {
        clearTree();
        return;
    }

    auto blocker = QSignalBlocker(this);
    if (m_structureDirty) {
        auto expanded = std::unordered_set<Model::Node *>{};
        auto selected = std::unordered_set<Model::Node *>{};
        auto *current = nodeForItem(currentItem());
        auto typeGroups = std::map<TypeGroupKey, TypeGroupState>{};
        const auto scroll = verticalScrollBar()->value();

        for (const auto &[node, item] : m_items) {
            if (item->isExpanded()) expanded.insert(node);
            if (item->isSelected()) selected.insert(node);
        }

        for (QTreeWidgetItemIterator it(this); *it; ++it) {
            const auto *item = *it;
            if (nodeForItem(item)) continue;
            typeGroups.emplace(typeGroupKey(item), TypeGroupState{item->isExpanded(), item->isSelected(),
                                                                  item == currentItem()});
            if (item->isSelected()) {
                auto members = std::vector<Model::Node *>{};
                collectItemNodes(item, members);
                selected.insert(members.begin(), members.end());
                if (item == currentItem() && !members.empty()) current = members.front();
            }
        }

        const auto layers = document->world()->allLayersUserSorted();
        synchronizeChildren(invisibleRootItem(), std::vector<Model::Node *>(layers.begin(), layers.end()));
        for (const auto &[node, item] : m_items) {
            if (expanded.contains(node)) item->setExpanded(true);
            auto representedByParent = false;
            for (auto *parent = node->parent(); parent &&
                                                !representedByParent; parent = parent->parent()) { representedByParent = selected.contains(parent); }
            const bool addedSelection =
                m_selectionSyncEnabled && !m_selectionSyncPending
                && node->selected() && !representedByParent && !selected.contains(node);
            item->setSelected(selected.contains(node) || addedSelection);
            if (addedSelection) for (auto *parent = item->parent(); parent; parent = parent->parent()) parent->setExpanded(true);
        }

        const auto it = m_items.find(current);
        if (it != m_items.end()) setCurrentItem(it->second, NameColumn, QItemSelectionModel::NoUpdate);

        // Keep selected display folders where possible. If a folder dissolves,
        // its real members retain the selection instead (also with sync disabled).
        for (QTreeWidgetItemIterator itemIt(this); *itemIt; ++itemIt) {
            auto *item = *itemIt;
            if (nodeForItem(item)) continue;
            const auto state = typeGroups.find(typeGroupKey(item));
            if (state == typeGroups.end()) continue;
            item->setExpanded(state->second.expanded);
            auto members = std::vector<Model::Node *>{};
            collectItemNodes(item, members);
            const auto keepSelected = state->second.selected && !members.empty()
                                      &&
                                      std::ranges::all_of(members, [&](auto *node) { return selected.contains(node); });
            item->setSelected(keepSelected);
            if (keepSelected) for (auto *node : members) m_items.at(node)->setSelected(false);
            if (state->second.current) setCurrentItem(item, NameColumn, QItemSelectionModel::NoUpdate);
        }
        for (auto *item : selectedItems())
            for (auto *parent = item->parent(); parent; parent = parent->parent()) parent->setExpanded(true);

        verticalScrollBar()->setValue(scroll);

        m_structureDirty = false;
        m_allItemsDirty = true;
    }
    if (m_allItemsDirty) updateItems();
    else
        for (auto *node : m_changedNodes) {
            const auto it = m_items.find(node);
            if (it != m_items.end()) updateItem(it->second);
        }
    m_changedNodes.clear();
    m_allItemsDirty = false;
    blocker.unblock();
    emit treeChanged();
}

void LayerListBox::synchronizeChildren(QTreeWidgetItem *parent, const std::vector<Model::Node *> &nodes, const Grouping grouping) {
    using Delegate = LayerTreeItemDelegate;
    const auto keyForNode = [&](const auto *node) {
        return grouping == Grouping::EntityClass ? entityGroupClass(node)
                                                 : QString::number(Delegate::typeForNode(*node));
    };
    std::map<QString, std::vector<Model::Node *>> byType;
    if (grouping != Grouping::None && m_groupByType && parent != invisibleRootItem())
        for (auto *node : nodes) byType[keyForNode(node)].push_back(node);
    std::set<QString> added;
    int row = 0;
    const auto moveItem = [this](QTreeWidgetItem *item, QTreeWidgetItem *target, const int position) {
        if (target->child(position) == item) return;
        if (auto *oldParent = item->parent()) oldParent->takeChild(oldParent->indexOfChild(item));
        else if (item->treeWidget()) takeTopLevelItem(indexOfTopLevelItem(item));
        target->insertChild(position, item);
    };
    for (auto *node : nodes) {
        const auto type = Delegate::typeForNode(*node);
        const auto key = keyForNode(node);
        if (type != Delegate::Layer && !key.isEmpty() && byType[key].size() > size_t(m_groupingThreshold)) {
            if (!added.insert(key).second) continue;
            const auto entityClass = grouping == Grouping::EntityClass ? key : QString{};
            QTreeWidgetItem *bucket = nullptr;
            for (int i = 0; i < parent->childCount(); ++i) {
                auto *candidate = parent->child(i);
                if (candidate->data(NameColumn, Delegate::TypeGroupRole).isValid()
                    && candidate->data(NameColumn, Delegate::TypeGroupRole).toInt() == type
                    && candidate->data(NameColumn, Delegate::EntityClassRole).toString() == entityClass) {
                    bucket = candidate;
                    break;
                }
            }
            if (!bucket) {
                bucket = new QTreeWidgetItem;
                bucket->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            }
            moveItem(bucket, parent, row++);
            m_itemDelegate->updateTypeGroup(*bucket, type, byType[key].size(), entityClass);
            // Entity folders can contain class/family folders. Real members always
            // restart grouping for their own children, at every depth in the map.
            synchronizeChildren(bucket, byType[key], grouping == Grouping::Type && type == Delegate::Entity
                                                     ? Grouping::EntityClass : Grouping::None);
            continue;
        }
        auto it = m_items.find(node);
        auto *item = it != m_items.end() ? it->second : new QTreeWidgetItem();

        const bool created = it == m_items.end();
        if (created) {
            item->setData(NameColumn, Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(node)));
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            m_items.emplace(node, item);
        }

        moveItem(item, parent, row++);
        item->setData(NameColumn, Delegate::EntityClassRole, entityGroupClass(node));
        synchronizeChildren(item, node->children());
        if (created && dynamic_cast<Model::LayerNode *>(node)) item->setExpanded(true);
    }
    while (parent->childCount() > row) {
        const auto *item = parent->takeChild(parent->childCount() - 1);
        forgetItem(item);
        delete item;
    }
}

void LayerListBox::removeItems(const std::vector<Model::Node *> &nodes) {
    // Remove raw node references before a command can destroy their targets.
    const auto blocker = QSignalBlocker(this);
    for (auto *node : nodes) {
        const auto it = m_items.find(node);
        if (it != m_items.end()) {
            const auto *item = it->second;
            forgetItem(item);
            delete item;
        }
    }

    m_structureDirty = true;
}

void LayerListBox::forgetItem(const QTreeWidgetItem *item) {
    for (auto i = 0; i < item->childCount(); ++i) forgetItem(item->child(i));
    m_items.erase(nodeForItem(item));
}

void LayerListBox::updateItem(QTreeWidgetItem *item) const {
    auto *node = nodeForItem(item);
    const auto *layer = dynamic_cast<Model::LayerNode *>(node);
    const auto document = kdl::mem_lock(m_document);
    const bool active = layer && document->currentLayer() == layer;
    const auto *parent = item->parent();
    const auto ordinal = parent ? parent->indexOfChild(item) + 1 : indexOfTopLevelItem(item) + 1;
    m_itemDelegate->updateItem(*item, *node, ordinal, active);

    item->setIcon(VisibilityColumn, IO::loadSVGIcon(node->visible() ? "Hidden_off.svg" : "Hidden_on.svg", iconSize().width()));
    item->setIcon(LockColumn, IO::loadSVGIcon(node->locked() ? "Lock_on.svg" : "Lock_off.svg", iconSize().width()));

    const auto visibility = node->visible() ? tr("Hide this object") : tr("Show this object");
    const auto locking = node->locked() ? tr("Unlock this object") : tr("Lock this object");

    item->setToolTip(VisibilityColumn, visibility);
    item->setToolTip(LockColumn, locking);
    item->setData(VisibilityColumn, Qt::AccessibleTextRole, visibility);
    item->setData(LockColumn, Qt::AccessibleTextRole, locking);
}

void LayerListBox::updateItems() { for (const auto &item : m_items | std::views::values) updateItem(item); }

bool LayerListBox::consumeSyncBudget(const QTreeWidgetItem& item, size_t& remaining) {
    if (auto* node = nodeForItem(&item)) {
        // Selecting a closed group still affects the rendering of its entire
        // subtree. The cached count avoids visiting any of those objects.
        const auto cost = dynamic_cast<Model::LayerNode*>(node) ? node->descendantCount() : node->familySize();
        if (cost > remaining) return false;
        remaining -= cost;
        return true;
    }

    for (auto i = 0; i < item.childCount(); ++i) {
        if (!consumeSyncBudget(*item.child(i), remaining)) return false;
    }
    return true;
}

bool LayerListBox::selectionExceedsSyncThreshold() const {
    auto remaining = size_t(std::clamp(pref(Preferences::LayerTreeSelectionSyncThreshold), 1, 100000));
    for (const auto* item : selectedItems()) {
        auto* parent = item->parent();
        while (parent && !parent->isSelected()) parent = parent->parent();
        if (!parent && !consumeSyncBudget(*item, remaining)) return true;
    }
    return false;
}

void LayerListBox::documentSelectionChanged(const Selection& change) {
    if (m_syncingSelection || !m_selectionSyncEnabled || m_updatingAttribute) return;
    const auto document = m_document.lock();
    if (!document) return;
    const auto unavailable = [&](const auto* node) {
        return !node->visible() || !node->editable() || !document->editorContext().visible(node);
    };
    // Hiding/locking can remove objects from the map selection. They remain
    // valid inspector selections, including when the change came from elsewhere.
    if (change.selectedNodes().empty() && change.selectedBrushFaces().empty()
        && std::ranges::all_of(change.deselectedNodes(), unavailable)
        && std::ranges::all_of(change.deselectedBrushFaces(), [&](const auto& face) { return unavailable(face.node()); }))
        return;
    syncSelectionFromDocument();
}

void LayerListBox::toggleAttribute(Model::Node* node, const int column) {
    if (!node) return;
    const auto guard = kdl::set_temp(m_updatingAttribute, true);
    if (column == VisibilityColumn) emit nodeVisibilityToggled(node);
    else if (column == LockColumn) emit nodeLockToggled(node);
}

void LayerListBox::selectInDocument(const bool force) {
    if (m_syncingSelection || (!m_selectionSyncEnabled && !force)) return;
    const auto document = m_document.lock();
    if (!document || !document->world()) return;
    if (!document->editorContext().canChangeSelection()) {
        m_selectionSyncPending = true;
        return;
    }
    m_selectionSyncPending = !force && selectionExceedsSyncThreshold();
    if (m_selectionSyncPending) return;
    const auto nodes = selectionNodes();
    const auto &current = document->selectedNodes().nodes();
    if (!document->hasSelectedBrushFaces() && nodes.size() == current.size() &&
        std::ranges::all_of(nodes, [](const auto *node) { return node->selected(); }))
        return;
    const auto guard = kdl::set_temp(m_syncingSelection, true);
    auto transaction = Transaction{document, "Select Objects in Layer Tree"};
    document->deselectAll();
    if (!nodes.empty()) document->selectNodes(nodes);
    transaction.commit();
}

void LayerListBox::syncSelectionFromDocument() {
    if (m_syncingSelection || !m_selectionSyncEnabled) return;
    const auto document = m_document.lock();
    if (!document || !document->world()) return;
    m_selectionSyncPending = false;
    const auto guard = kdl::set_temp(m_syncingSelection, true);
    auto blocker = QSignalBlocker(this);
    clearSelection();
    QTreeWidgetItem *first = nullptr;
    auto selectedNodes = document->selectedNodes().nodes();
    for (const auto &face : document->selectedBrushFaces()) selectedNodes.push_back(face.node());
    for (auto *node : selectedNodes) {
        const auto it = m_items.find(node);
        if (it == m_items.end()) continue; // An addition will be synchronized at the end of the command.
        it->second->setSelected(true);
        for (auto *parent = it->second->parent(); parent; parent = parent->parent()) parent->setExpanded(true);
        if (!first) first = it->second;
    }
    if (first) {
        setCurrentItem(first, NameColumn, QItemSelectionModel::NoUpdate);
        scrollToItem(first);
    }

    blocker.unblock();
    emit treeChanged();
}

std::vector<Model::Node *> LayerListBox::selectionNodes() const {
    const auto document = kdl::mem_lock(m_document);
    auto members = std::vector<Model::Node *>{};
    for (const auto *item : selectedItems()) collectItemNodes(item, members);
    const auto selected = std::unordered_set<Model::Node *>(members.begin(), members.end());
    auto result = std::vector<Model::Node *>{};

    for (auto *node : selected) {
        auto covered = false;
        for (auto *ancestor = node->parent(); ancestor && !covered; ancestor = ancestor->parent())
            covered = selected.contains(ancestor);
        if (!covered) collectSelection(node, document->editorContext(), result);
    }

    return result;
}

std::vector<Model::Node*> LayerListBox::selectedTreeNodes() const {
    std::vector<Model::Node*> nodes;
    for (const auto* item : selectedItems()) collectItemNodes(item, nodes);
    return nodes;
}

Model::Node* LayerListBox::singleSelectedTreeNode() const {
    // Do not expand virtual groups with thousands of members just to enable an
    // inspection command or check whether a single audio entity is selected.
    const auto items = selectedItems();
    return items.size() == 1 ? nodeForItem(items.front()) : nullptr;
}

bool LayerListBox::hasPrefabSelection() const {
    for (const auto* item : selectedItems()) {
        const auto* node = nodeForItem(item);
        if (!node ? item->childCount() > 0 : !dynamic_cast<const Model::LayerNode*>(node) || node->hasChildren()) return true;
    }
    return false;
}

void LayerListBox::focusItem(QTreeWidgetItem *item) {
    const auto document = m_document.lock();
    if (!item || !document || !document->world()) return;
    // Focus the double-clicked item, not unrelated rows from a previous multi-selection.
    {
        const auto blocker = QSignalBlocker(this);
        setCurrentItem(item, NameColumn, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    if (m_selectionSyncEnabled) {
        selectInDocument(true);
        if (m_mapView && document->editorContext().canChangeSelection() && document->hasSelectedNodes())
            m_mapView->focusCameraOnSelection(true);
    }
    if (m_mapView && (!m_selectionSyncEnabled || selectionNodes().empty())) {
        // Camera navigation must not replace an unrelated editor selection.
        const auto nodes = selectedTreeNodes();
        if (!nodes.empty()) {
            if (auto *view = dynamic_cast<MapView3D *>(m_mapView->firstMapViewBase()))
                view->focusCameraOnObjects(nodes, true);
            else m_mapView->moveCameraToPosition(vm::vec3f(Model::computeLogicalBounds(nodes).center()), true);
        }
    }
    emit treeChanged();
}

bool LayerListBox::canShowSelectedSource() const {
    return singleSelectedTreeNode() != nullptr;
}

void LayerListBox::showSelectedSource() {
    const auto document = m_document.lock();
    const auto* node = singleSelectedTreeNode();
    if (!document || !node) return;
    auto* frame = findMapFrame(this);
    auto* dialog = new NodeSourceDialog{*document, *node, frame ? static_cast<QWidget*>(frame) : window()};
    dialog->show();
}

void LayerListBox::showContextMenu(QTreeWidgetItem *item, const QPoint &globalPosition) {
    if (!item) return;
    if (!item->isSelected())
        setCurrentItem(item, NameColumn, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    else setCurrentItem(item, NameColumn, QItemSelectionModel::NoUpdate);
    if (auto *layer = dynamic_cast<Model::LayerNode *>(nodeForItem(item))) { emit layerRightClicked(layer); }
    else if (!m_selectionSyncEnabled || m_selectionSyncPending
        || std::ranges::any_of(selectedTreeNodes(), [](const auto* node) { return !node->visible() || !node->editable(); })) {
        // Editor actions operate on the document selection. Offer an explicit
        // transfer instead of silently modifying the selection while browsing.
        QMenu menu;
        auto* sourceAction = menu.addAction(IO::loadSVGIcon("Source.svg", 16), tr("Show source…"), this, &LayerListBox::showSelectedSource);
        sourceAction->setEnabled(canShowSelectedSource());
        menu.addAction(tr("Select in editor"), this, [this] { selectInDocument(true); })->setEnabled(!selectionNodes().empty());
        menu.addAction(tr("Focus in editor"), this, [this, item] { focusItem(item); });
        addContextMenuAction(menu,*ActionManager::instance().actionsMap().at(SaveSelectionAsPrefabActionPath), [this] {
            savePrefabDialog(m_document.lock(), selectedTreeNodes(), this);
        },hasPrefabSelection());
        menu.addSeparator();
        menu.addAction(tr("Delete"), this, &LayerListBox::deleteSelection)->setEnabled(!removableSelection().empty());
        menu.exec(globalPosition);
    } else if (auto *frame = findMapFrame(this)) {
        if (auto *view = frame->currentMapViewBase()) {
            QAction deleteAction(tr("Delete"), this);
            deleteAction.setEnabled(!removableSelection().empty());
            connect(&deleteAction, &QAction::triggered, this, &LayerListBox::deleteSelection);
            QAction sourceAction(IO::loadSVGIcon("Source.svg", 16), tr("Show source…"), this);
            sourceAction.setEnabled(canShowSelectedSource());
            connect(&sourceAction, &QAction::triggered, this, &LayerListBox::showSelectedSource);
            view->showPopupMenuForSelection(globalPosition, &deleteAction, &sourceAction);
        }
    }
}

Model::Node *LayerListBox::nodeForItem(const QTreeWidgetItem *item) {
    return item ? reinterpret_cast<Model::Node *>(item->data(NameColumn, Qt::UserRole).value<quintptr>()) : nullptr;
}

void LayerListBox::collectItemNodes(const QTreeWidgetItem *item, std::vector<Model::Node *> &nodes) {
    if (auto *node = nodeForItem(item)) nodes.push_back(node);
    else if (item) for (auto i = 0; i < item->childCount(); ++i) collectItemNodes(item->child(i), nodes);
}

void LayerListBox::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        const auto index = indexAt(event->pos());
        m_ignoreLeftRelease = index.isValid() && index.column() != NameColumn;
        m_attributePress = m_ignoreLeftRelease ? QPersistentModelIndex(index) : QPersistentModelIndex{};
        if (m_ignoreLeftRelease) {
            event->accept();
            return;
        }
    }

    QTreeWidget::mousePressEvent(event);
}

void LayerListBox::mouseMoveEvent(QMouseEvent* event) {
    // The base view still remembers the last name-column press. Forwarding an
    // icon gesture's moves would extend the selection from that stale anchor.
    if (m_ignoreLeftRelease) { event->accept(); return; }
    QTreeWidget::mouseMoveEvent(event);
}

void LayerListBox::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && std::exchange(m_ignoreLeftRelease, false)) {
        const auto pressed = std::exchange(m_attributePress, QPersistentModelIndex{});
        if (pressed.isValid() && pressed == indexAt(event->pos()))
            toggleAttribute(nodeForItem(itemFromIndex(pressed)), pressed.column());
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton) {
        showContextMenu(itemAt(event->pos()), event->globalPos());
        event->accept();
        return;
    }

    QTreeWidget::mouseReleaseEvent(event);
}

void LayerListBox::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        const auto index = indexAt(event->pos());
        if (index.isValid() && index.column() == NameColumn) focusItem(itemAt(event->pos()));
        // The base view did not receive this press, so it must not handle its release.
        m_ignoreLeftRelease = true;
        m_attributePress = QPersistentModelIndex{};
        event->accept();
        return;
    }

    QTreeWidget::mouseDoubleClickEvent(event);
}

void LayerListBox::contextMenuEvent(QContextMenuEvent *event) {
    // Handle mouse menus on release consistently on all platforms. Native context
    // events may arrive on press (macOS) or release (Windows).
    if (event->reason() == QContextMenuEvent::Keyboard) showContextMenu(currentItem(), event->globalPos());
    event->accept();
}

void LayerListBox::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (!event->isAutoRepeat()) deleteSelection();
        event->accept();
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && currentItem()) {
        focusItem(currentItem());
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Space && currentItem() && currentColumn() != NameColumn) {
        toggleAttribute(nodeForItem(currentItem()), currentColumn());
        event->accept();
        return;
    }

    QTreeWidget::keyPressEvent(event);
}

bool LayerListBox::event(QEvent* event) {
    if (event->type() == QEvent::ShortcutOverride) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace) {
            // The editor's Delete shortcut must not act on an unrelated map
            // selection while the inspector owns the keyboard focus.
            event->accept();
            return true;
        }
    }
    return QTreeWidget::event(event);
}

std::vector<Model::Node*> LayerListBox::removableSelection() {
    const auto document = m_document.lock();
    if (!document || !document->world() || !document->editorContext().canChangeSelection()) return {};
    if (const auto* frame = findMapFrame(this); frame && frame->anyToolActive()) return {};
    auto nodes = std::vector<Model::Node*>{};
    for (auto* node : selectedTreeNodes()) {
        // Layer management retains its own Remove action. Delete removes the
        // objects in a selected layer, preserving the layer itself.
        if (dynamic_cast<Model::LayerNode*>(node)) nodes.insert(nodes.end(), node->children().begin(), node->children().end());
        else nodes.push_back(node);
    }
    const auto selected = std::unordered_set<Model::Node*>(nodes.begin(), nodes.end());
    nodes.clear();
    for (auto* node : selected) {
        auto covered = false;
        for (auto* parent = node->parent(); parent && !covered; parent = parent->parent()) covered = selected.contains(parent);
        if (!covered) nodes.push_back(node);
    }
    // Deleting an unlocked container must not bypass a locked descendant.
    if (!std::ranges::all_of(removedSubtrees(nodes), [](const auto* node) { return node->editable(); })) return {};
    return nodes;
}

void LayerListBox::deleteSelection() {
    const auto document = m_document.lock();
    const auto nodes = removableSelection();
    if (!document || nodes.empty()) return;
    auto count = size_t{0};
    for (const auto* node : nodes) count += node->familySize();
    if (count > 1 && QMessageBox::question(this, tr("Delete Objects"),
        tr("Delete %1 objects, including contained objects?").arg(qulonglong(count)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    const auto removed = removedSubtrees(nodes);
    auto deselected = std::vector<Model::Node*>{};
    auto deselectedFaces = std::vector<Model::BrushFaceHandle>{};
    for (auto* node : document->selectedNodes().nodes()) if (removed.contains(node)) deselected.push_back(node);
    for (const auto& face : document->selectedBrushFaces()) if (removed.contains(face.node())) deselectedFaces.push_back(face);
    const auto guard = kdl::set_temp(m_syncingSelection, true);
    auto transaction = Transaction{document, "Delete Objects in Layer Tree"};
    if (!deselected.empty()) document->deselectNodes(deselected);
    if (!deselectedFaces.empty()) document->deselectBrushFaces(deselectedFaces);
    document->removeNodes(nodes);
    transaction.commit();
}
} // namespace TrenchBroom::View
