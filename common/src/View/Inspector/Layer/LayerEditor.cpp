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

#include "View/Inspector/Layer/LayerEditor.h"

#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include "Model/BrushNode.h"
#include "Model/EntityNode.h"
#include "Model/GroupNode.h"
#include "Model/LayerNode.h"
#include "Model/ModelUtils.h"
#include "Model/NodeQueries.h"
#include "Model/WorldNode.h"
#include "IO/ResourceUtils.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/Inspector/Layer/LayerListBox.h"
#include "View/Document/MapDocument.h"
#include "View/Theme/QtUtils.h"
#include "View/Prefabs/PrefabUtils.h"
#include "View/Actions/Actions.h"
#include "View/MapView/ViewUtils.h"

#include "kdl/memory_utils.h"
#include "kdl/string_compare.h"
#include "kdl/string_format.h"
#include "kdl/vector_utils.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace TrenchBroom {
namespace View {
LayerEditor::LayerEditor(std::weak_ptr<MapDocument> document, MapView *mapView, QWidget *parent)
    : QWidget(parent), m_document(document), m_layerList(nullptr), m_mapView(mapView), m_addLayerButton(nullptr), m_removeLayerButton(nullptr), m_moveLayerUpButton(nullptr), m_moveLayerDownButton(nullptr) {
    createGui();
    updateButtons();
    updateTreeSettings();
    m_notifierConnection += PreferenceManager::instance().preferenceDidChangeNotifier.connect([this](const auto& path) {
        if (path == Preferences::LayerTreeGroupByType.path() || path == Preferences::LayerTreeGroupingThreshold.path()
            || path == Preferences::LayerTreeSyncSelection.path()
            || path == Preferences::LayerTreeSelectionSyncThreshold.path()) updateTreeSettings();
    });
}

void LayerEditor::onSetCurrentLayer(Model::LayerNode *layer) {
    auto document = kdl::mem_lock(m_document);
    document->setCurrentLayer(layer);

    updateButtons();
}

bool LayerEditor::canSetCurrentLayer(Model::LayerNode *layer) const {
    auto document = kdl::mem_lock(m_document);
    return document->currentLayer() != layer;
}

void LayerEditor::onLayerRightClick(Model::LayerNode *layerNode) {
    auto document = kdl::mem_lock(m_document);

    QMenu popupMenu;
    auto* sourceAction = popupMenu.addAction(IO::loadSVGIcon("Source.svg", 16), tr("Show source…"), m_layerList, &LayerListBox::showSelectedSource);
    sourceAction->setEnabled(m_layerList->canShowSelectedSource());
    popupMenu.addSeparator();
    QAction *makeActiveAction = popupMenu.addAction(tr("Make active layer"), this, [this, layerNode]() { onSetCurrentLayer(layerNode); });
    QAction *moveSelectionToLayerAction = popupMenu.addAction(tr("Move selection to layer"), this, &LayerEditor::onMoveSelectionToLayer);
    QAction *selectAllInLayerAction = popupMenu.addAction(tr("Select all in layer"), this, &LayerEditor::onSelectAllInLayer);
    addContextMenuAction(popupMenu,*ActionManager::instance().actionsMap().at(SaveSelectionAsPrefabActionPath), [this, document] {
        savePrefabDialog(document, m_layerList->selectedTreeNodes(), this);
    },m_layerList->hasPrefabSelection());
    popupMenu.addSeparator();
    QAction *toggleLayerVisibleAction = popupMenu.addAction(layerNode->hidden() ? tr("Show layer") : tr("Hide layer"), this, [this, layerNode]() {
        toggleLayerVisible(layerNode);
    });
    QAction *isolateLayerAction = popupMenu.addAction(tr("Isolate layer"), this, [this, layerNode]() { isolateLayer(layerNode); });
    QAction *toggleLayerLockedAction = popupMenu.addAction(layerNode->locked() ? tr("Unlock layer") : tr("Lock layer"), this, [this, layerNode]() { toggleLayerLocked(layerNode); });
    QAction *toggleLayerOmitFromExportAction = popupMenu.addAction(tr("Omit From Export"), this, [this, layerNode]() {
        toggleOmitLayerFromExport(layerNode);
    });
    popupMenu.addSeparator();
    QAction *showAllLayersAction = popupMenu.addAction(tr("Show All Layers"), this, &LayerEditor::onShowAllLayers);
    QAction *hideAllLayersAction = popupMenu.addAction(tr("Hide All Layers"), this, &LayerEditor::onHideAllLayers);
    popupMenu.addSeparator();
    QAction *unlockAllLayersAction = popupMenu.addAction(tr("Unlock All Layers"), this, &LayerEditor::onUnlockAllLayers);
    QAction *lockAllLayersAction = popupMenu.addAction(tr("Lock All Layers"), this, &LayerEditor::onLockAllLayers);
    popupMenu.addSeparator();
    QAction *renameLayerAction = popupMenu.addAction(tr("Rename Layer"), this, &LayerEditor::onRenameLayer);
    QAction *removeLayerAction = popupMenu.addAction(tr("Remove Layer"), this, &LayerEditor::onRemoveLayer);

    makeActiveAction->setEnabled(canSetCurrentLayer(layerNode));
    moveSelectionToLayerAction->setEnabled(canMoveSelectionToLayer());
    selectAllInLayerAction->setEnabled(canSelectAllInLayer());
    toggleLayerVisibleAction->setEnabled(canToggleLayerVisible());
    isolateLayerAction->setEnabled(document->canIsolateLayers({layerNode}));
    toggleLayerOmitFromExportAction->setCheckable(true);
    toggleLayerOmitFromExportAction->setChecked(layerNode->layer().omitFromExport());

    toggleLayerLockedAction->setEnabled(canToggleLayerLocked());
    showAllLayersAction->setEnabled(canShowAllLayers());
    hideAllLayersAction->setEnabled(canHideAllLayers());
    unlockAllLayersAction->setEnabled(canUnlockAllLayers());
    lockAllLayersAction->setEnabled(canLockAllLayers());
    renameLayerAction->setEnabled(canRenameLayer());
    removeLayerAction->setEnabled(canRemoveLayer());

    popupMenu.exec(QCursor::pos());
}

bool LayerEditor::canToggleLayerVisible() const {
    auto *layer = m_layerList->selectedLayer();
    return layer != nullptr;
}

void LayerEditor::toggleLayerVisible(Model::LayerNode *layer) {
    ensure(layer != nullptr, "layer is null");
    toggleNodeVisible(layer);
}

void LayerEditor::toggleNodeVisible(Model::Node *node) {
    const auto document = kdl::mem_lock(m_document);
    if (node->visible()) document->hide({node});
    else {
        // One undoable command restores the entire subtree, including children
        // that were explicitly hidden before their parent was hidden.
        document->show(Model::collectNodesAndDescendants(std::vector<Model::Node *>{node}));
    }
}

bool LayerEditor::canToggleLayerLocked() const {
    auto *layer = m_layerList->selectedLayer();
    return layer != nullptr;
}

void LayerEditor::toggleLayerLocked(Model::LayerNode *layer) {
    ensure(layer != nullptr, "layer is null");
    auto document = kdl::mem_lock(m_document);
    if (!layer->locked()) {
        document->lock(std::vector<Model::Node *>{layer});
    } else {
        document->resetLock(std::vector<Model::Node *>{layer});
    }
}

void LayerEditor::toggleOmitLayerFromExport(Model::LayerNode *layerNode) {
    ensure(layerNode != nullptr, "layer is null");
    kdl::mem_lock(m_document)->setOmitLayerFromExport(layerNode, !layerNode->layer().omitFromExport());
}

void LayerEditor::isolateLayer(Model::LayerNode *layer) {
    auto document = kdl::mem_lock(m_document);
    document->isolateLayers(std::vector<Model::LayerNode *>{layer});
}

void LayerEditor::onMoveSelectionToLayer() {
    auto *layer = m_layerList->selectedLayer();
    ensure(layer != nullptr, "layer is null");

    auto document = kdl::mem_lock(m_document);
    document->moveSelectionToLayer(layer);
}

bool LayerEditor::canMoveSelectionToLayer() const {
    auto *layer = m_layerList->selectedLayer();
    if (layer == nullptr) {
        return false;
    }

    auto document = kdl::mem_lock(m_document);
    return document->canMoveSelectionToLayer(layer);
}

void LayerEditor::onSelectAllInLayer() {
    auto *layer = m_layerList->selectedLayer();
    ensure(layer != nullptr, "layer is null");

    kdl::mem_lock(m_document)->selectAllInLayers({layer});
}

bool LayerEditor::canSelectAllInLayer() const {
    auto *layer = m_layerList->selectedLayer();
    if (layer == nullptr) {
        return false;
    }

    return kdl::mem_lock(m_document)->canSelectAllInLayers({layer});
}

void LayerEditor::onAddLayer() {
    const std::string name = queryLayerName(this, "Unnamed");
    if (!name.empty()) {
        auto document = kdl::mem_lock(m_document);
        auto *world = document->world();

        auto layer = Model::Layer(name);

        // Sort it at the bottom of the list
        const auto customLayers = world->customLayersUserSorted();
        if (customLayers.empty()) {
            layer.setSortIndex(0);
        } else {
            layer.setSortIndex(customLayers.back()->layer().sortIndex() + 1);
        }

        auto *layerNode = new Model::LayerNode{std::move(layer)};

        auto transaction = Transaction{document, "Create Layer " + layerNode->name()};
        if (document->addNodes({{world, {layerNode}}}).empty()) {
            transaction.cancel();
            return;
        }

        document->setCurrentLayer(layerNode);
        transaction.commit();

        m_layerList->setSelectedLayer(layerNode);
    }
}

void LayerEditor::onRemoveLayer() {
    auto *layer = m_layerList->selectedLayer();
    ensure(layer != nullptr, "layer is null");

    auto document = kdl::mem_lock(m_document);
    auto *defaultLayer = document->world()->defaultLayer();

    auto transaction = Transaction{document, "Remove Layer " + layer->name()};
    document->deselectAll();
    if (layer->hasChildren()) {
        if (!document->reparentNodes({{defaultLayer, layer->children()}})) {
            transaction.cancel();
            return;
        }
    }

    if (document->currentLayer() == layer) {
        document->setCurrentLayer(defaultLayer);
    }
    document->removeNodes({layer});
    transaction.commit();
}

bool LayerEditor::canRemoveLayer() const {
    const auto *layer = m_layerList->selectedLayer();
    if (layer == nullptr) {
        return false;
    }

    if (findVisibleAndUnlockedLayer(layer) == nullptr) {
        return false;
    }

    auto document = kdl::mem_lock(m_document);
    return (layer != document->world()->defaultLayer());
}

void LayerEditor::onRenameLayer() {
    if (canRenameLayer()) {
        auto document = kdl::mem_lock(m_document);
        Model::LayerNode *layer = m_layerList->selectedLayer();

        const std::string name = queryLayerName(this, layer->name());
        if (!name.empty()) {
            document->renameLayer(layer, name);
        }
    }
}

bool LayerEditor::canRenameLayer() const {
    const auto *layer = m_layerList->selectedLayer();
    if (layer == nullptr) {
        return false;
    }

    auto document = kdl::mem_lock(m_document);
    return (layer != document->world()->defaultLayer());
}

bool LayerEditor::canMoveLayer(int direction) const {
    if (direction == 0) {
        return false;
    }

    auto *layer = m_layerList->selectedLayer();
    if (layer == nullptr) {
        return false;
    }

    auto document = kdl::mem_lock(m_document);
    return document->canMoveLayer(layer, direction);
}

void LayerEditor::moveLayer(Model::LayerNode *layer, int direction) {
    if (direction == 0) {
        return;
    }

    ensure(layer != nullptr, "layer is null");
    auto document = kdl::mem_lock(m_document);
    document->moveLayer(layer, direction);
}

void LayerEditor::onShowAllLayers() {
    auto document = kdl::mem_lock(m_document);
    auto hiddenLayers = std::vector<Model::Node *>{};
    for (auto *layer : document->world()->allLayers()) {
        if (!layer->visible()) hiddenLayers.push_back(layer);
    }
    document->show(Model::collectNodesAndDescendants(hiddenLayers));
}

bool LayerEditor::canShowAllLayers() const {
    const auto layers = kdl::mem_lock(m_document)->world()->allLayers();
    return std::any_of(std::begin(layers), std::end(layers), [](const auto *layer) {
        return !layer->visible();
    });
}

void LayerEditor::onHideAllLayers() {
    auto document = kdl::mem_lock(m_document);
    const auto layers = document->world()->allLayers();
    document->hide(std::vector<Model::Node *>(std::begin(layers), std::end(layers)));
}

bool LayerEditor::canHideAllLayers() const {
    const auto layers = kdl::mem_lock(m_document)->world()->allLayers();
    return std::any_of(std::begin(layers), std::end(layers), [](const auto *layer) {
        return layer->visible();
    });
}

void LayerEditor::onLockAllLayers() {
    auto document = kdl::mem_lock(m_document);
    const auto nodes = kdl::vec_static_cast<Model::Node *>(document->world()->allLayers());
    document->lock(nodes);
}

bool LayerEditor::canLockAllLayers() const {
    const auto layers = kdl::mem_lock(m_document)->world()->allLayers();
    return std::any_of(std::begin(layers), std::end(layers), [](const auto *layer) {
        return !layer->locked();
    });
}

void LayerEditor::onUnlockAllLayers() {
    auto document = kdl::mem_lock(m_document);
    const auto nodes = kdl::vec_static_cast<Model::Node *>(document->world()->allLayers());
    document->resetLock(nodes);
}

bool LayerEditor::canUnlockAllLayers() const {
    const auto layers = kdl::mem_lock(m_document)->world()->allLayers();
    return std::any_of(std::begin(layers), std::end(layers), [](const auto *layer) {
        return layer->locked();
    });
}

Model::LayerNode *LayerEditor::findVisibleAndUnlockedLayer(const Model::LayerNode *except) const {
    auto document = kdl::mem_lock(m_document);
    if (!document->world()->defaultLayer()->locked() && !document->world()->defaultLayer()->hidden()) {
        return document->world()->defaultLayer();
    }

    const auto &layers = document->world()->customLayers();
    for (auto *layer : layers) {
        if (layer != except && !layer->locked() && !layer->hidden()) {
            return layer;
        }
    }

    return nullptr;
}

void LayerEditor::createGui() {
    m_layerList = new LayerListBox(m_document, m_mapView, this);
    connect(m_layerList, &LayerListBox::layerSetCurrent, this, &LayerEditor::onSetCurrentLayer);
    connect(m_layerList, &LayerListBox::layerRightClicked, this, &LayerEditor::onLayerRightClick);
    connect(m_layerList, &LayerListBox::nodeVisibilityToggled, this, &LayerEditor::toggleNodeVisible);
    connect(m_layerList, &LayerListBox::nodeLockToggled, this, [this](Model::Node *node) {
        const auto document = kdl::mem_lock(m_document);
        if (node->locked()) document->unlock({node});
        else document->lock({node});
    });
    connect(m_layerList, &LayerListBox::itemSelectionChanged, this, &LayerEditor::updateButtons);
    connect(m_layerList, &LayerListBox::treeChanged, this, &LayerEditor::updateButtons);

    m_addLayerButton = createBitmapButton("Add.svg", tr("Add a new layer from the current selection"));
    m_removeLayerButton = createBitmapButton("Remove.svg", tr("Remove the selected layer and move its objects to the default layer"));
    m_moveLayerUpButton = createBitmapButton("Up.svg", "Move the selected layer up");
    m_moveLayerDownButton = createBitmapButton("Down.svg", "Move the selected layer down");

    connect(m_addLayerButton, &QAbstractButton::pressed, this, &LayerEditor::onAddLayer);
    connect(m_removeLayerButton, &QAbstractButton::pressed, this, &LayerEditor::onRemoveLayer);
    connect(m_moveLayerUpButton, &QAbstractButton::pressed, this, [=]() {
        Model::LayerNode *layer = m_layerList->selectedLayer();
        moveLayer(layer, -1);
    });
    connect(m_moveLayerDownButton, &QAbstractButton::pressed, this, [=]() {
        Model::LayerNode *layer = m_layerList->selectedLayer();
        moveLayer(layer, 1);
    });

    m_groupByTypeButton = createBitmapToggleButton("GroupByType.svg", tr("Group objects by type"));
    m_groupByTypeButton->setAccessibleName(tr("Group objects by type"));
    m_groupByTypeButton->setProperty("layerTreeControl", "groupByType");
    m_groupByTypeButton->setPopupMode(QToolButton::MenuButtonPopup);
    auto* groupingMenu = new QMenu(m_groupByTypeButton);
    auto* thresholdAction = new QWidgetAction(groupingMenu);
    m_groupingThreshold = new QSpinBox;
    m_groupingThreshold->setRange(1, 100000);
    m_groupingThreshold->setPrefix(tr("More than "));
    m_groupingThreshold->setSuffix(tr(" objects"));
    m_groupingThreshold->setKeyboardTracking(false);
    m_groupingThreshold->setAccessibleName(tr("Type grouping threshold"));
    m_groupingThreshold->setToolTip(tr("Group matching object types under the same parent when their count exceeds this value."));
    thresholdAction->setDefaultWidget(m_groupingThreshold);
    groupingMenu->addAction(thresholdAction);
    m_groupByTypeButton->setMenu(groupingMenu);

    m_syncSelectionButton = createBitmapToggleButton("SyncSelection.svg", tr("Synchronize tree and editor selection"));
    m_syncSelectionButton->setAccessibleName(tr("Synchronize tree and editor selection"));
    m_syncSelectionButton->setProperty("layerTreeControl", "syncSelection");
    m_syncSelectionButton->setPopupMode(QToolButton::MenuButtonPopup);
    auto* syncMenu = new QMenu(m_syncSelectionButton);
    auto* syncThresholdAction = new QWidgetAction(syncMenu);
    m_selectionSyncThreshold = new QSpinBox;
    m_selectionSyncThreshold->setRange(1, 100000);
    m_selectionSyncThreshold->setPrefix(tr("Automatic up to "));
    m_selectionSyncThreshold->setKeyboardTracking(false);
    m_selectionSyncThreshold->setAccessibleName(tr("Automatic selection synchronization limit"));
    m_selectionSyncThreshold->setToolTip(tr("For larger selections, double-click a row in the Layer tree to synchronize."));
    syncThresholdAction->setDefaultWidget(m_selectionSyncThreshold);
    syncMenu->addAction(syncThresholdAction);
    m_syncSelectionButton->setMenu(syncMenu);

    m_groupByTypeButton->setObjectName("layerTreeToggle");
    m_syncSelectionButton->setObjectName("layerTreeToggle");

    connect(m_groupByTypeButton, &QToolButton::toggled, this, [this](const bool enabled) {
        setPref(Preferences::LayerTreeGroupByType, enabled);
        m_layerList->setGroupByType(enabled);
        updateTreeSettings();
    });
    connect(m_groupingThreshold, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](const int value) {
        setPref(Preferences::LayerTreeGroupingThreshold, value);
        m_layerList->setGroupingThreshold(value);
        updateTreeSettings();
    });
    connect(m_syncSelectionButton, &QToolButton::toggled, this, [this](const bool enabled) {
        setPref(Preferences::LayerTreeSyncSelection, enabled);
        m_layerList->setSelectionSyncEnabled(enabled);
        updateTreeSettings();
    });
    connect(m_selectionSyncThreshold, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](const int value) {
        setPref(Preferences::LayerTreeSelectionSyncThreshold, value);
        updateTreeSettings();
    });

    auto* toolbar = createInspectorToolBar(this);
    m_sourceButton = createBitmapButton("Source.svg", tr("Show source of the selected object (MAP / FGD)"));
    m_sourceButton->setObjectName("layerShowSource");
    connect(m_sourceButton, &QAbstractButton::clicked, m_layerList, &LayerListBox::showSelectedSource);
    toolbar->addWidget(m_addLayerButton);
    toolbar->addWidget(m_removeLayerButton);
    toolbar->addWidget(m_moveLayerUpButton);
    toolbar->addWidget(m_moveLayerDownButton);
    toolbar->addWidget(m_groupByTypeButton);
    toolbar->addWidget(m_syncSelectionButton);
    toolbar->addWidget(m_sourceButton);

    auto *sizer = new QVBoxLayout();
    sizer->setContentsMargins(0, 0, 0, 0);
    sizer->setSpacing(0);
    sizer->addWidget(toolbar);
    sizer->addWidget(m_layerList, 1);
    setLayout(sizer);
}

void LayerEditor::updateButtons() {
    m_sourceButton->setEnabled(m_layerList->canShowSelectedSource());
    m_removeLayerButton->setEnabled(canRemoveLayer());
    m_moveLayerUpButton->setEnabled(canMoveLayer(-1));
    m_moveLayerDownButton->setEnabled(canMoveLayer(1));
}

void LayerEditor::updateTreeSettings() {
    const QSignalBlocker groupBlocker(m_groupByTypeButton);
    const QSignalBlocker syncBlocker(m_syncSelectionButton);
    const QSignalBlocker thresholdBlocker(m_groupingThreshold);
    const QSignalBlocker syncThresholdBlocker(m_selectionSyncThreshold);
    m_groupByTypeButton->setChecked(pref(Preferences::LayerTreeGroupByType));
    m_syncSelectionButton->setChecked(pref(Preferences::LayerTreeSyncSelection));
    m_groupingThreshold->setValue(std::clamp(pref(Preferences::LayerTreeGroupingThreshold), 1, 100000));
    m_selectionSyncThreshold->setValue(pref(Preferences::LayerTreeSelectionSyncThreshold));
    m_groupByTypeButton->setToolTip(tr("Type grouping: %1\nClick the icon to toggle. Use the arrow to change the limit (more than %2 objects per parent).")
        .arg(m_groupByTypeButton->isChecked() ? tr("On") : tr("Off")).arg(m_groupingThreshold->value()));
    m_syncSelectionButton->setToolTip(tr("Selection synchronization: %1\nClick the icon to toggle or use the arrow to change the limit.\n"
        "Selections of more than %2 objects synchronize when you double-click a row in the Layer tree.")
        .arg(m_syncSelectionButton->isChecked() ? tr("On") : tr("Off")).arg(m_selectionSyncThreshold->value()));
}
} // namespace View
} // namespace TrenchBroom
