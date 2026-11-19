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

#include "View/Inspector/ViewSettings/ViewEditor.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QGridLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>

#include <algorithm>

#include "Assets/EntityDefinition.h"
#include "Assets/EntityDefinitionGroup.h"
#include "Assets/EntityDefinitionManager.h"
#include "Model/EditorContext.h"
#include "Model/Game.h"
#include "Model/Tag.h"
#include "Model/TagType.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/Widgets/BorderLine.h"
#include "View/Widgets/BorderPanel.h"
#include "View/Document/MapDocument.h"
#include "View/Widgets/PopupButton.h"
#include "View/Theme/QtUtils.h"
#include "View/Widgets/SliderWithLabel.h"
#include "View/Widgets/TitledPanel.h"
#include "View/Theme/ViewConstants.h"
#include "View/Document/Grid.h"
#include "View/Widgets/ColorButton.h"

#include <kdl/memory_utils.h>

#include <vector>

namespace TrenchBroom {
namespace View {
// EntityDefinitionCheckBoxList

EntityDefinitionCheckBoxList::EntityDefinitionCheckBoxList(Assets::EntityDefinitionManager &entityDefinitionManager, Model::EditorContext &editorContext, QWidget *parent)
    : QWidget(parent), m_entityDefinitionManager(entityDefinitionManager), m_editorContext(editorContext) {
    createGui();
    refresh();
}

void EntityDefinitionCheckBoxList::refresh() {
    size_t defIndex = 0;
    const std::vector<Assets::EntityDefinitionGroup> &groups = m_entityDefinitionManager.groups();
    for (size_t i = 0; i < groups.size(); ++i) {
        const Assets::EntityDefinitionGroup &group = groups[i];
        const std::vector<Assets::EntityDefinition *> &definitions = group.definitions();

        if (!definitions.empty()) {
            const bool firstHidden = m_editorContext.entityDefinitionHidden(definitions[0]);
            bool mixed = false;
            for (size_t j = 0; j < definitions.size(); ++j) {
                const bool hidden = m_editorContext.entityDefinitionHidden(definitions[j]);
                mixed |= (hidden != firstHidden);
                m_defCheckBoxes[defIndex++]->setChecked(!hidden);
            }

            if (mixed)
                m_groupCheckBoxes[i]->setCheckState(Qt::PartiallyChecked);
            else
                m_groupCheckBoxes[i]->setChecked(!firstHidden);
            m_groupCheckBoxes[i]->setEnabled(true);
        } else {
            m_groupCheckBoxes[i]->setChecked(true);
            m_groupCheckBoxes[i]->setEnabled(false);
        }
    }
}

void EntityDefinitionCheckBoxList::groupCheckBoxChanged(size_t groupIndex, bool checked) {
    const std::vector<Assets::EntityDefinitionGroup> &groups = m_entityDefinitionManager.groups();
    ensure(groupIndex < m_entityDefinitionManager.groups().size(), "index out of range");
    const Assets::EntityDefinitionGroup &group = groups[groupIndex];

    const std::vector<Assets::EntityDefinition *> &definitions = group.definitions();
    for (size_t i = 0; i < definitions.size(); ++i) {
        const Assets::EntityDefinition *definition = definitions[i];
        m_editorContext.setEntityDefinitionHidden(definition, !checked);
    }

    refresh();
}

void EntityDefinitionCheckBoxList::defCheckBoxChanged(const Assets::EntityDefinition *definition, bool checked) {
    m_editorContext.setEntityDefinitionHidden(definition, !checked);
    refresh();
}

void EntityDefinitionCheckBoxList::showAllClicked() {
    hideAll(false);
}

void EntityDefinitionCheckBoxList::hideAllClicked() {
    hideAll(true);
}

void EntityDefinitionCheckBoxList::hideAll(const bool hidden) {
    const std::vector<Assets::EntityDefinitionGroup> &groups = m_entityDefinitionManager.groups();
    for (size_t i = 0; i < groups.size(); ++i) {
        const Assets::EntityDefinitionGroup &group = groups[i];
        const std::vector<Assets::EntityDefinition *> &definitions = group.definitions();
        for (size_t j = 0; j < definitions.size(); ++j) {
            const Assets::EntityDefinition *definition = definitions[j];
            m_editorContext.setEntityDefinitionHidden(definition, hidden);
        }
    }
}

void EntityDefinitionCheckBoxList::createGui() {
    auto *scrollWidgetLayout = new QVBoxLayout();
    scrollWidgetLayout->setContentsMargins(0, 0, 0, 0);
    scrollWidgetLayout->setSpacing(1);
    scrollWidgetLayout->addSpacing(1);

    const std::vector<Assets::EntityDefinitionGroup> &groups = m_entityDefinitionManager.groups();
    for (size_t i = 0; i < groups.size(); ++i) {
        const Assets::EntityDefinitionGroup &group = groups[i];
        const std::vector<Assets::EntityDefinition *> &definitions = group.definitions();
        const std::string &groupName = group.displayName();

        // Checkbox for the prefix, e.g. "func"
        auto *groupCB = new QCheckBox(QString::fromStdString(groupName));
        groupCB->setObjectName("entityDefinition_groupCheckboxWidget");
        makeEmphasized(groupCB);
        makeSmall(groupCB);

        connect(groupCB, &QAbstractButton::clicked, this, [this, i](bool checked) {
            this->groupCheckBoxChanged(i, checked);
        });

        m_groupCheckBoxes.push_back(groupCB);
        scrollWidgetLayout->addWidget(groupCB);

        for (auto defIt = std::begin(definitions), defEnd = std::end(definitions); defIt != defEnd; ++defIt) {
            Assets::EntityDefinition *definition = *defIt;
            const std::string defName = definition->name();

            auto *defCB = new QCheckBox(QString::fromStdString(defName));
            defCB->setObjectName("entityDefinition_checkboxWidget");
            makeSmall(defCB);

            connect(defCB, &QAbstractButton::clicked, this, [this, definition](bool checked) {
                this->defCheckBoxChanged(definition, checked);
            });

            m_defCheckBoxes.push_back(defCB);
            scrollWidgetLayout->addWidget(defCB);
        }
    }

    scrollWidgetLayout->addSpacing(0);

    auto *scrollWidget = new QWidget();
    scrollWidget->setContentsMargins(LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin);
    scrollWidget->setLayout(scrollWidgetLayout);

    auto *scrollArea = new QScrollArea();
    scrollArea->setContentsMargins(0, 0, 0, 0);
    scrollArea->setAttribute(Qt::WA_StyledBackground);
    scrollArea->setProperty("uiSurface", "base");
    scrollArea->setWidget(scrollWidget);

    auto *showAllButton = new QPushButton(tr("Show all"));
    showAllButton->setObjectName("ViewEditor_smallPushButton");
    makeSmall(showAllButton);

    auto *hideAllButton = new QPushButton(tr("Hide all"));
    hideAllButton->setObjectName("ViewEditor_smallPushButton");
    makeSmall(hideAllButton);

    connect(showAllButton, &QAbstractButton::clicked, this, &EntityDefinitionCheckBoxList::showAllClicked);
    connect(hideAllButton, &QAbstractButton::clicked, this, &EntityDefinitionCheckBoxList::hideAllClicked);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin);
    buttonLayout->setSpacing(LayoutConstants::NarrowHMargin);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(showAllButton);
    buttonLayout->addWidget(hideAllButton);
    buttonLayout->addStretch(1);

    auto *outerLayout = new QVBoxLayout();
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(LayoutConstants::NarrowHMargin);
    outerLayout->addWidget(scrollArea, 1);
    outerLayout->addWidget(new BorderLine);
    outerLayout->addLayout(buttonLayout);
    setLayout(outerLayout);
}

// ViewEditor

ViewEditor::ViewEditor(std::weak_ptr<MapDocument> document, QWidget *parent)
    : QWidget(parent), m_document(std::move(document)), m_showEntityClassnamesCheckBox(nullptr), m_showGroupBoundsCheckBox(nullptr), m_showBrushEntityBoundsCheckBox(nullptr), m_showPointEntityBoundsCheckBox(nullptr), m_showPointEntitiesCheckBox(nullptr), m_showPointEntityModelsCheckBox(nullptr), m_entityDefinitionCheckBoxList(nullptr), m_showBrushesCheckBox(nullptr), m_renderModeRadioGroup(nullptr), m_shadeFacesCheckBox(nullptr), m_showEdgesCheckBox(nullptr), m_showFogCheckBox(nullptr), m_entityLinkRadioGroup(nullptr), m_showSoftBoundsCheckBox(nullptr) {
    setObjectName("ViewEditor");
    connectObservers();
    if (kdl::mem_lock(m_document)->world()) {
        createGui();
        refreshGui();
    }
}

void ViewEditor::connectObservers() {
    auto document = kdl::mem_lock(m_document);
    m_notifierConnection += document->grid().gridDidChangeNotifier.connect(this,&ViewEditor::refreshGui);
    m_notifierConnection += document->documentWasNewedNotifier.connect(this, &ViewEditor::documentWasNewedOrLoaded);
    m_notifierConnection += document->documentWasLoadedNotifier.connect(this, &ViewEditor::documentWasNewedOrLoaded);
    m_notifierConnection += document->editorContextDidChangeNotifier.connect(this, &ViewEditor::editorContextDidChange);
    m_notifierConnection += document->entityDefinitionsDidChangeNotifier.connect(this, &ViewEditor::entityDefinitionsDidChange);

    PreferenceManager &prefs = PreferenceManager::instance();
    m_notifierConnection += prefs.preferenceDidChangeNotifier.connect(this, &ViewEditor::preferenceDidChange);
}

void ViewEditor::bindEvents() {
    connect(m_showEntityClassnamesCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showEntityClassnamesChanged);
    connect(m_showGroupBoundsCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showGroupBoundsChanged);
    connect(m_showBrushEntityBoundsCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showBrushEntityBoundsChanged);
    connect(m_showPointEntityBoundsCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showPointEntityBoundsChanged);
    connect(m_showPointEntitiesCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showPointEntitiesChanged);
    connect(m_showPointEntityModelsCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showPointEntityModelsChanged);
    connect(m_showFogCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showFogChanged);
    connect(m_showAlternateFogCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showFogType);
    connect(m_fogMaxAmountSlider, &SliderWithLabel::valueChanged, this, &ViewEditor::showFogMaxAmount);
    connect(m_fogMinDistanceSlider, &SliderWithLabel::valueChanged, this, &ViewEditor::showFogMinDistance);
    connect(m_fogScaleSlider, &SliderWithLabel::valueChanged, this, &ViewEditor::showFogScale);
    connect(m_fogBiasSlider, &SliderWithLabel::valueChanged, this, &ViewEditor::showFogBias);
    connect(m_showBrushesCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showBrushesChanged);
    connect(m_shadeFacesCheckBox, &QAbstractButton::clicked, this, &ViewEditor::shadeFacesChanged);
    connect(m_showEdgesCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showEdgesChanged);
    connect(m_shadeAmount, &SliderWithLabel::valueChanged, this, &ViewEditor::shadeLevelChanged);
    connect(m_brightnessSlider, &SliderWithLabel::valueChanged, this, &ViewEditor::brightnessChanged);

    connect(m_selectionBoundsShowObjectBounds, &QAbstractButton::clicked, this, &ViewEditor::showObjectBoundsChanged);
    connect(m_selectionBoundsIntersectionMode, &QAbstractButton::clicked, this, &ViewEditor::showIntersectionModeChanged);
    connect(m_selectionBoundsAlwaysShowOnSelected, &QAbstractButton::clicked, this, &ViewEditor::showAlwaysShowBoundsChanged);
    connect(m_selectionBoundsUseDashedBounds, &QAbstractButton::clicked, this, &ViewEditor::dashedBoundsChanged);

    connect(m_selectionBoundsWidth, &SliderWithLabel::valueChanged, this, &ViewEditor::selectionBoundsLineWithChanged);
    connect(m_selectionBoundsDashSize, &SliderWithLabel::valueChanged, this, &ViewEditor::dashSizeChanged);
    connect(m_selectionBoundsIntersectionSize, &SliderWithLabel::valueChanged, this, &ViewEditor::intersectionMarkerSizeChanged);

    connect(m_renderModeRadioGroup, &QButtonGroup::idClicked, this, &ViewEditor::faceRenderModeChanged);
    connect(m_entityLinkRadioGroup, &QButtonGroup::idClicked, this, &ViewEditor::entityLinkModeChanged);
    connect(m_showSoftBoundsCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showSoftMapBoundsChanged);
}

void ViewEditor::unBindEvents() {
    disconnect(m_showEntityClassnamesCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showEntityClassnamesChanged);
    disconnect(m_showGroupBoundsCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showGroupBoundsChanged);
    disconnect(m_showBrushEntityBoundsCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showBrushEntityBoundsChanged);
    disconnect(m_showPointEntityBoundsCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showPointEntityBoundsChanged);
    disconnect(m_showPointEntitiesCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showPointEntitiesChanged);
    disconnect(m_showPointEntityModelsCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showPointEntityModelsChanged);
    disconnect(m_showFogCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showFogChanged);
    disconnect(m_showAlternateFogCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showFogType);
    disconnect(m_fogMaxAmountSlider, &SliderWithLabel::valueChanged, this, &ViewEditor::showFogMaxAmount);
    disconnect(m_fogMinDistanceSlider, &SliderWithLabel::valueChanged, this, &ViewEditor::showFogMinDistance);
    disconnect(m_fogScaleSlider, &SliderWithLabel::valueChanged, this, &ViewEditor::showFogScale);
    disconnect(m_fogBiasSlider, &SliderWithLabel::valueChanged, this, &ViewEditor::showFogBias);
    disconnect(m_showBrushesCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showBrushesChanged);
    disconnect(m_shadeFacesCheckBox, &QAbstractButton::clicked, this, &ViewEditor::shadeFacesChanged);
    disconnect(m_showEdgesCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showEdgesChanged);
    disconnect(m_shadeAmount, &SliderWithLabel::valueChanged, this, &ViewEditor::shadeLevelChanged);
    disconnect(m_brightnessSlider, &SliderWithLabel::valueChanged, this, &ViewEditor::brightnessChanged);

    disconnect(m_selectionBoundsShowObjectBounds, &QAbstractButton::clicked, this, &ViewEditor::showObjectBoundsChanged);
    disconnect(m_selectionBoundsIntersectionMode, &QAbstractButton::clicked, this, &ViewEditor::showIntersectionModeChanged);
    disconnect(m_selectionBoundsAlwaysShowOnSelected, &QAbstractButton::clicked, this, &ViewEditor::showAlwaysShowBoundsChanged);
    disconnect(m_selectionBoundsUseDashedBounds, &QAbstractButton::clicked, this, &ViewEditor::dashedBoundsChanged);

    disconnect(m_selectionBoundsWidth, &SliderWithLabel::valueChanged, this, &ViewEditor::selectionBoundsLineWithChanged);
    disconnect(m_selectionBoundsDashSize, &SliderWithLabel::valueChanged, this, &ViewEditor::dashSizeChanged);
    disconnect(m_selectionBoundsIntersectionSize, &SliderWithLabel::valueChanged, this, &ViewEditor::intersectionMarkerSizeChanged);

    disconnect(m_renderModeRadioGroup, &QButtonGroup::idClicked, this, &ViewEditor::faceRenderModeChanged);
    disconnect(m_entityLinkRadioGroup, &QButtonGroup::idClicked, this, &ViewEditor::entityLinkModeChanged);
    disconnect(m_showSoftBoundsCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showSoftMapBoundsChanged);
}

void ViewEditor::documentWasNewedOrLoaded(MapDocument *) {
    createGui();
    refreshGui();
}

void ViewEditor::editorContextDidChange() {
    refreshGui();
}

void ViewEditor::entityDefinitionsDidChange() {
    createGui();
    refreshGui();
}

void ViewEditor::preferenceDidChange(const std::filesystem::path &) {
    refreshGui();
}

void ViewEditor::createGui() {
    const auto currentTab = m_tabs ? m_tabs->currentIndex() : 0;
    deleteChildWidgetsLaterAndDeleteLayout(this);
    delete m_renderModeRadioGroup;
    m_renderModeRadioGroup = nullptr;
    delete m_entityLinkRadioGroup;
    m_entityLinkRadioGroup = nullptr;
    m_tabs = new QTabWidget;
    m_tabs->setObjectName("ViewSettingsTabs");
    const auto page = [&](const QString& title, const QString& name) {
        auto* content = new QWidget;
        content->setObjectName(name);
        auto* scroll = new QScrollArea;
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setWidget(content);
        m_tabs->addTab(scroll, title);
        return content;
    };
    auto* display = page(tr("Display"), "ViewSettingsDisplay");
    auto* displayLayout = new QHBoxLayout(display);
    displayLayout->addWidget(createRendererPanel(display), 1);
    displayLayout->addWidget(createFogPanel(display), 1, Qt::AlignTop);
    auto* geometry = page(tr("Grid && Bounds"), "ViewSettingsGeometry");
    auto* geometryLayout = new QHBoxLayout(geometry);
    geometryLayout->addWidget(createGridPanel(geometry), 1, Qt::AlignTop);
    geometryLayout->addWidget(createSelectionBoundsPanel(geometry), 1, Qt::AlignTop);
    auto* objects = page(tr("Objects"), "ViewSettingsObjects");
    auto* objectsLayout = new QHBoxLayout(objects);
    objectsLayout->addWidget(createEntityDefinitionsPanel(objects), 1);
    auto* filters = new QVBoxLayout;
    filters->addWidget(createEntitiesPanel(objects));
    filters->addWidget(createBrushesPanel(objects));
    filters->addWidget(createLinksPanel(objects));
    filters->addStretch();
    objectsLayout->addLayout(filters, 1);
    auto* reset = new QPushButton(tr("Restore View Defaults"));
    reset->setObjectName("ViewSettingsReset");
    connect(reset, &QPushButton::clicked, this, &ViewEditor::restoreDefaultsClicked);
    auto* footer = new QHBoxLayout;
    footer->addStretch();
    footer->addWidget(reset);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tabs, 1);
    layout->addLayout(footer);
    m_tabs->setCurrentIndex(std::clamp(currentTab, 0, m_tabs->count() - 1));
}

QWidget *ViewEditor::createEntityDefinitionsPanel(QWidget *parent) {
    TitledPanel *panel = new TitledPanel("Entity Definitions", parent, false);

    auto document = kdl::mem_lock(m_document);
    Assets::EntityDefinitionManager &entityDefinitionManager = document->entityDefinitionManager();

    Model::EditorContext &editorContext = document->editorContext();
    m_entityDefinitionCheckBoxList = new EntityDefinitionCheckBoxList(entityDefinitionManager, editorContext);

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(LayoutConstants::NoMargin, LayoutConstants::NoMargin, LayoutConstants::NoMargin, LayoutConstants::NoMargin);

    m_entityDefinitionCheckBoxList->setContentsMargins(LayoutConstants::NoMargin, LayoutConstants::NoMargin, LayoutConstants::NoMargin, LayoutConstants::NoMargin);

    layout->setSpacing(0);
    layout->addWidget(m_entityDefinitionCheckBoxList, 1);
    m_entityDefinitionCheckBoxList->setObjectName("ViewSettingsEntityDefinitions");

    layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    panel->getPanel()->setLayout(layout);

    return panel;
}

QWidget *ViewEditor::createEntitiesPanel(QWidget *parent) {
    TitledPanel *panel = new TitledPanel("Entities", parent, false);

    m_showEntityClassnamesCheckBox = new QCheckBox(tr("Show entity classnames"));
    m_showGroupBoundsCheckBox = new QCheckBox(tr("Show group bounds and names"));
    m_showBrushEntityBoundsCheckBox = new QCheckBox(tr("Show brush entity bounds"));
    m_showPointEntityBoundsCheckBox = new QCheckBox(tr("Show point entity bounds"));

    m_showPointEntitiesCheckBox = new QCheckBox(tr("Show point entities"));
    m_showPointEntityModelsCheckBox = new QCheckBox(tr("Show point entity models"));
    m_animateSelectedModels = new QCheckBox(tr("Animate selected models"));
    m_animateSelectedModels->setObjectName("animateSelectedEntityModels");
    m_animateSelectedModels->setToolTip(tr("Loop the model's default animation while the entity or its group is selected in a 3D view."));
    m_animateNearbyModels = new QCheckBox(tr("Animate models near the 3D camera"));
    m_animateNearbyModels->setObjectName("animateNearbyEntityModels");
    m_animateNearbyModels->setToolTip(tr("Loop the default animation within the configured distance of the model bounds. Either trigger can start playback."));
    m_modelAnimationDistance = new SliderWithLabel(16, 8192, 0, "%d units");
    m_modelAnimationDistance->setObjectName("entityModelAnimationDistance");
    m_modelAnimationSpeed = new SliderWithLabel(0, 400, 0, "%d%%");
    m_modelAnimationSpeed->setObjectName("entityModelAnimationSpeed");
    m_modelAnimationSpeed->setToolTip(tr("Playback speed. 100% uses the model timing; 0% pauses playback."));
    connect(m_animateSelectedModels, &QCheckBox::clicked, this, [](bool enabled) { setPref(Preferences::AnimateSelectedEntityModels, enabled); });
    connect(m_animateNearbyModels, &QCheckBox::clicked, this, [](bool enabled) { setPref(Preferences::AnimateNearbyEntityModels, enabled); });
    connect(m_modelAnimationDistance, &SliderWithLabel::valueChanged, this, [](int value) { setPref(Preferences::EntityModelAnimationDistance, float(value)); });
    connect(m_modelAnimationSpeed, &SliderWithLabel::valueChanged, this, [](int value) { setPref(Preferences::EntityModelAnimationSpeed, value / 100.f); });
    m_previewEntityAudio = new QCheckBox(tr("Preview audio on selection"));
    m_previewEntityAudio->setObjectName("previewEntityAudio");
    m_previewEntityAudio->setToolTip(tr("Play the audio file referenced by a single selected point entity."));
    m_loopEntityAudio = new QCheckBox(tr("Loop audio preview"));
    m_loopEntityAudio->setObjectName("loopEntityAudio");
    m_entityAudioVolume = new SliderWithLabel(0, 100, 0, "%d%%");
    m_entityAudioVolume->setObjectName("entityAudioVolume");
    connect(m_previewEntityAudio, &QCheckBox::clicked, this, [](const auto enabled) { setPref(Preferences::PreviewEntityAudio, enabled); });
    connect(m_loopEntityAudio, &QCheckBox::clicked, this, [](const auto enabled) { setPref(Preferences::LoopEntityAudio, enabled); });
    connect(m_entityAudioVolume, &SliderWithLabel::valueChanged, this, [](const auto volume) { setPref(Preferences::EntityAudioVolume, volume); });

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin);

    layout->setSpacing(0);
    layout->addWidget(m_showEntityClassnamesCheckBox);
    layout->addWidget(m_showGroupBoundsCheckBox);
    layout->addWidget(m_showBrushEntityBoundsCheckBox);
    layout->addWidget(m_showPointEntityBoundsCheckBox);
    layout->addWidget(m_showPointEntitiesCheckBox);
    layout->addWidget(m_showPointEntityModelsCheckBox);
    layout->addWidget(m_animateSelectedModels);
    layout->addWidget(m_animateNearbyModels);
    auto* animationLayout = new QFormLayout{};
    animationLayout->addRow(tr("Animation radius"), m_modelAnimationDistance);
    animationLayout->addRow(tr("Animation speed"), m_modelAnimationSpeed);
    layout->addLayout(animationLayout);
    layout->addWidget(m_previewEntityAudio);
    layout->addWidget(m_loopEntityAudio);
    auto* volumeLayout = new QFormLayout{};
    volumeLayout->addRow(tr("Audio volume"), m_entityAudioVolume);
    layout->addLayout(volumeLayout);
    layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    panel->getPanel()->setLayout(layout);
    return panel;
}

QWidget *ViewEditor::createSelectionBoundsPanel(QWidget *parent) {
    TitledPanel *panel = new TitledPanel("Selection Bounds", parent, false);

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin);

    layout->setSpacing(0);

    m_selectionBoundsAlwaysShowOnSelected = new QCheckBox(tr("Always show selection bounds"));
    m_selectionBoundsAlwaysShowOnSelected->setToolTip(tr("Always show the selected objects selection bounds (else only show on mouse-over)."));

    m_selectionBoundsIntersectionMode = new QCheckBox(tr("Show only intersections"));
    m_selectionBoundsIntersectionMode->setToolTip(tr("Only show helper lines if they hit an object."));

    m_selectionBoundsUseDashedBounds = new QCheckBox(tr("Use dashed bounds"));
    m_selectionBoundsUseDashedBounds->setToolTip(tr("Use dashed bounds to better distinguish from normal edges."));

    m_selectionBoundsShowObjectBounds = new QCheckBox(tr("Show object bounds"));
    m_selectionBoundsShowObjectBounds->setToolTip(tr("Show the objects bounding box."));
    layout->addWidget(m_selectionBoundsAlwaysShowOnSelected);
    layout->addWidget(m_selectionBoundsIntersectionMode);
    layout->addWidget(m_selectionBoundsUseDashedBounds);
    layout->addWidget(m_selectionBoundsShowObjectBounds);

    layout->addSpacing(LayoutConstants::WideVMargin);

    m_selectionBoundsDashSize = new SliderWithLabel(1, 128, 0.0f, "%d u", 0, 45, this);
    m_selectionBoundsAnimationSpeed = new SliderWithLabel(0, 200, 0.0f, "%d%%", 0, 45, this);
    m_selectionBoundsAnimationSpeed->setObjectName("BoundsAnimationSpeed");
    m_selectionBoundsAnimationSpeed->setToolTip(tr("Speed of the outward-moving dashes and solid-guide pulse. 0% disables animation; 100% moves dashes at 32 pixels per second."));
    connect(m_selectionBoundsAnimationSpeed, &SliderWithLabel::valueChanged, this,
            [](int value) { setPref(Preferences::SelectionBoundsAnimationSpeed, value / 100.f); });
    m_selectionBoundsIntersectionSize = new SliderWithLabel(2, 20, 0.0f, "%dpx", 0, 45, this);
    m_selectionBoundsWidth = new SliderWithLabel(10, 300, 10e-3f, "%.1f px", 0, 45, this);

    layout->addWidget(new QLabel{"Dash size"});
    layout->addWidget(m_selectionBoundsDashSize);
    layout->addSpacing(LayoutConstants::WideVMargin);

    layout->addWidget(new QLabel{tr("Guide animation speed")});
    layout->addWidget(m_selectionBoundsAnimationSpeed);
    layout->addSpacing(LayoutConstants::WideVMargin);

    layout->addWidget(new QLabel{"Intersection marker size"});
    layout->addWidget(m_selectionBoundsIntersectionSize);
    layout->addSpacing(LayoutConstants::WideVMargin);

    layout->addWidget(new QLabel{tr("Line width")});
    layout->addWidget(m_selectionBoundsWidth);

    const auto toggle = [&](const char* name, const QString& label, Preference<bool>& preference) {
        auto* box = new QCheckBox(label, panel);
        box->setObjectName(name);
        box->setChecked(pref(preference));
        connect(box, &QCheckBox::toggled, this, [&preference](bool value) { setPref(preference, value); });
        layout->addWidget(box);
    };
    const auto number = [&](const char* name, const QString& label, auto& preference, double low, double high, const QString& suffix) {
        using Value = std::decay_t<decltype(pref(preference))>;
        auto* field = new QDoubleSpinBox(panel);
        field->setObjectName(name);
        field->setRange(low, high);
        field->setDecimals(0);
        field->setKeyboardTracking(false);
        field->setSuffix(suffix);
        field->setValue(pref(preference));
        connect(field, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [&preference](double value) { setPref(preference, Value(value)); });
        layout->addWidget(new QLabel(label));
        layout->addWidget(field);
        return field;
    };
    toggle("BoundsCornerMarkers", tr("Show markers on selection corners"), Preferences::SelectionBoundsCornerMarkers);
    toggle("ObjectBoundsOnHover", tr("Object bounds on hover only (also unselected objects)"), Preferences::ObjectBoundsOnHover);
    number("BoundsGuideLength", tr("Maximum guide length"), Preferences::SelectionBoundsGuideLength, 1, 1000000, tr(" units"));
    auto* range = number("BoundsGuideDistance", tr("Maximum camera distance"), Preferences::SelectionBoundsGuideDistance, 0, 1000000, tr(" units"));
    range->setSpecialValueText(tr("Unlimited"));
    toggle("BoundsShowDistances", tr("Show distances to hit surfaces"), Preferences::SelectionBoundsShowDistances);
    number("BoundsDistanceFontSize", tr("Distance font size"), Preferences::SelectionBoundsDistanceFontSize, 6, 64, tr(" px"));
    number("BoundsDistanceOffset", tr("Distance label offset"), Preferences::SelectionBoundsDistanceOffset, -100, 100, tr(" px"));
    auto* position = new QComboBox(panel);
    position->setObjectName("BoundsDistancePosition");
    position->addItems({tr("Near selection"), tr("Middle of guide"), tr("Near hit surface")});
    position->setCurrentIndex(std::clamp(pref(Preferences::SelectionBoundsDistancePosition), 0, 2));
    connect(position, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int value) { setPref(Preferences::SelectionBoundsDistancePosition, value); });
    layout->addWidget(new QLabel(tr("Distance label position")));
    layout->addWidget(position);
    auto* color = new ColorButton(panel);
    color->setObjectName("BoundsDistanceColor");
    color->setColor(toQColor(pref(Preferences::SelectionBoundsDistanceColor)));
    connect(color, &ColorButton::colorChanged, this, [](const QColor& value) { setPref(Preferences::SelectionBoundsDistanceColor, Color(value)); });
    layout->addWidget(new QLabel(tr("Distance label color")));
    layout->addWidget(color);

    layout->addStretch();
    layout->addWidget(new BorderLine);
    layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    panel->getPanel()->setLayout(layout);
    return panel;
}

QWidget *ViewEditor::createFogPanel(QWidget *parent) {
    TitledPanel *panel = new TitledPanel("Fog", parent, false);

    m_showFogCheckBox = new QCheckBox(tr("Enable fog"));
    m_showAlternateFogCheckBox = new QCheckBox(tr("Alternate Fog Algorithm"));

    m_fogMaxAmountSlider = new SliderWithLabel(1, 100, 0.0f, "%d%%", 0, 45, this);
    m_fogMinDistanceSlider = new SliderWithLabel(16, 2048, 0.0f, "%d", 0, 45, this);
    m_fogScaleSlider = new SliderWithLabel(1, 100, 0.0f, "%d", 0, 45, this);
    m_fogBiasSlider = new SliderWithLabel(0, 100, 0.0f, "%d", 0, 45, this);

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin);

    layout->setSpacing(0);
    layout->addWidget(m_showFogCheckBox);
    layout->addSpacing(LayoutConstants::MediumVMargin);

    layout->addWidget(new QLabel{"Maximum amount"});
    layout->addWidget(m_fogMaxAmountSlider);
    layout->addSpacing(LayoutConstants::MediumVMargin);

    layout->addWidget(new QLabel{"Minimum distance"});
    layout->addWidget(m_fogMinDistanceSlider);
    layout->addSpacing(LayoutConstants::MediumVMargin);

    layout->addWidget(new QLabel{"Scale"});
    layout->addWidget(m_fogScaleSlider);
    layout->addSpacing(LayoutConstants::MediumVMargin);

    layout->addWidget(new QLabel{"Bias"});
    layout->addWidget(m_fogBiasSlider);
    layout->addSpacing(LayoutConstants::MediumVMargin);

    layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    panel->getPanel()->setLayout(layout);

    return panel;
}

QWidget *ViewEditor::createBrushesPanel(QWidget *parent) {
    TitledPanel *panel = new TitledPanel("Brushes", parent, false);
    auto *inner = panel->getPanel();
    createTagFilter(inner);

    m_showBrushesCheckBox = new QCheckBox(tr("Show brushes"));

    auto *innerLayout = qobject_cast<QBoxLayout *>(inner->layout());
    ensure(innerLayout != nullptr, "inner sizer is null");
    innerLayout->insertWidget(0, m_showBrushesCheckBox);

    return panel;
}

void ViewEditor::createTagFilter(QWidget *parent) {
    m_tagCheckBoxes.clear();

    auto document = kdl::mem_lock(m_document);
    const auto &tags = document->smartTags();

    if (tags.empty()) {
        createEmptyTagFilter(parent);
    } else {
        createTagFilter(parent, tags);
    }
}

void ViewEditor::createEmptyTagFilter(QWidget *parent) {
    auto *msg = new QLabel(tr("No tags found"));
    makeInfo(msg);

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(0, LayoutConstants::WideVMargin, 0, LayoutConstants::WideVMargin);
    layout->setSpacing(0);
    layout->addWidget(msg);
    layout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    parent->setLayout(layout);
}

void ViewEditor::createTagFilter(QWidget *parent, const std::vector<Model::SmartTag> &tags) {
    assert(!tags.empty());

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin);
    layout->setSpacing(0);

    for (const auto &tag : tags) {
        const QString label = QString::fromLatin1("Show %1").arg(QString::fromStdString(tag.name()).toLower());

        auto *checkBox = new QCheckBox(label);
        const Model::TagType::Type tagType = tag.type();

        m_tagCheckBoxes.emplace_back(tagType, checkBox);

        layout->addWidget(checkBox);
        connect(checkBox, &QAbstractButton::clicked, this, [this, tagType](const bool checked) {
            showTagChanged(checked, tagType);
        });
    }

    layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    parent->setLayout(layout);
}

QWidget *ViewEditor::createRendererPanel(QWidget *parent) {
    TitledPanel *panel = new TitledPanel("Renderer", parent, false);
    QWidget * inner = panel->getPanel();

    const QList<QString> FaceRenderModes = {"Show textures", "Hide textures", "Hide faces"};

    const QList<QString> FaceRenderModesPrefValues = {
        Preferences::faceRenderModeTextured(), Preferences::faceRenderModeFlat(), Preferences::faceRenderModeSkip()
    };

    m_renderModeRadioGroup = new QButtonGroup(this);
    for (int i = 0; i < FaceRenderModes.length(); ++i) {
        const QString &label = FaceRenderModes.at(i);
        const QString &prefValue = FaceRenderModesPrefValues.at(i);

        auto *radio = new QRadioButton(label);
        radio->setObjectName(prefValue);
        m_renderModeRadioGroup->addButton(radio, i);
    }

    m_shadeFacesCheckBox = new QCheckBox(tr("Shade faces"));
    m_shadeAmount = new SliderWithLabel(1, 100, 0.0f, "%d%%", 0, 45, this);
    m_brightnessSlider = new SliderWithLabel{25, 300, 0, "%d%%", 0, 45};
    m_brightnessSlider->setToolTip("Sets the brightness for textures and model skins in the 3D editing view.");

    m_showEdgesCheckBox = new QCheckBox(tr("Show edges"));
    m_showSkyCheckBox = new QCheckBox(tr("Show map skybox"));
    m_showSkyCheckBox->setObjectName("showMapSkybox");
    m_showSkyCheckBox->setToolTip(tr("Preview the map's skybox in the 3D view, using its sky / skyname property and game resources."));
    connect(m_showSkyCheckBox, &QCheckBox::clicked, this, [](const bool checked) { setPref(Preferences::ShowSky, checked); });

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin);
    layout->setSpacing(0);

    layout->addWidget(new QLabel{"Brightness"});
    layout->addWidget(m_brightnessSlider);

    for (auto *button : m_renderModeRadioGroup->buttons()) {
        layout->addWidget(button);
    }

    layout->addWidget(m_shadeFacesCheckBox);

    layout->addWidget(new QLabel{"Shade amount"});
    layout->addWidget(m_shadeAmount);
    layout->addWidget(m_showEdgesCheckBox);
    layout->addWidget(m_showSkyCheckBox);

    layout->addSpacing(LayoutConstants::MediumVMargin);
    layout->addSpacing(1);
    layout->addWidget(new BorderLine);
    layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    inner->setLayout(layout);
    return panel;
}

QWidget* ViewEditor::createLinksPanel(QWidget* parent) {
    auto* panel = new TitledPanel(tr("Links and Limits"), parent, false);
    const QList<QString> EntityLinkModes = {"Show all entity links", "Show transitively selected entity links", "Show directly selected entity links",
                                            "Hide entity links"};

    const QList<QString> EntityLinkModesPrefValues = {Preferences::entityLinkModeAll(), Preferences::entityLinkModeTransitive(),
                                                      Preferences::entityLinkModeDirect(), Preferences::entityLinkModeNone()};

    m_entityLinkRadioGroup = new QButtonGroup(this);
    for (int i = 0; i < EntityLinkModes.length(); ++i) {
        const QString& label = EntityLinkModes.at(i);
        const QString& prefValue = EntityLinkModesPrefValues.at(i);

        auto* radio = new QRadioButton(label);
        radio->setObjectName(prefValue);
        m_entityLinkRadioGroup->addButton(radio, i);
    }

    m_showSoftBoundsCheckBox = new QCheckBox(tr("Show soft bounds"));

    auto* layout = new QVBoxLayout(panel->getPanel());
    for (auto* button : m_entityLinkRadioGroup->buttons())
        layout->addWidget(button);
    auto* form = new QFormLayout;
    const auto add = [&](const QString& label, const char* name, Preference<float>& preference, double minimum, double maximum, double step, const QString& suffix, double factor = 1.0) {
        auto* spin = new QDoubleSpinBox;
        spin->setObjectName(name);
        spin->setRange(minimum, maximum);
        spin->setSingleStep(step);
        spin->setDecimals(step < 1 ? 1 : 0);
        spin->setSuffix(suffix);
        spin->setKeyboardTracking(false);
        auto* prefPtr = &preference;
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [prefPtr,factor](double value) { setPref(*prefPtr,float(value/factor)); });
        form->addRow(label,spin);
        return spin;
    };
    m_linkWidth = add(tr("Line width"),"ViewEntityLinkWidth",Preferences::EntityLinkWidth,0.5,16,0.5,tr(" px"));
    m_linkSpeed = add(tr("Animation speed"), "ViewEntityLinkSpeed", Preferences::EntityLinkSpeed, 0, 1000, 4, tr(" units/s"));
    m_linkSpeed->setSpecialValueText(tr("Disabled"));
    m_linkSpacing = add(tr("Arrow spacing"), "ViewEntityLinkSpacing", Preferences::EntityLinkSpacing, 16, 512, 4, tr(" units"));
    m_linkDistance = add(tr("Maximum camera distance"),"ViewEntityLinkDistance",Preferences::EntityLinkDistance,0,1000000,128,tr(" units"));
    m_linkDistance->setSpecialValueText(tr("Unlimited"));
    m_linkDistance->setToolTip(tr("3D view: fade links by the distance from the camera to each visible part of the connection. 0 means unlimited."));
    m_linkOccludedAlpha = add(tr("Behind objects"),"ViewEntityLinkOccludedAlpha",Preferences::EntityLinkOccludedAlpha,0,100,5,tr("%"),100);
    m_linkOccludedAlpha->setSpecialValueText(tr("Hidden"));
    layout->addLayout(form);
    layout->addWidget(m_showSoftBoundsCheckBox);
    return panel;
}

QWidget* ViewEditor::createGridPanel(QWidget* parent) {
    auto* panel = new TitledPanel(tr("Grid"), parent, false);
    auto* layout = new QFormLayout(panel->getPanel());
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_showGrid = new QCheckBox(tr("Show grid"));
    m_showGrid->setObjectName("ViewGridVisible");
    m_snapToGrid = new QCheckBox(tr("Snap to grid"));
    m_snapToGrid->setObjectName("ViewGridSnap");
    m_gridAlpha = new SliderWithLabel(0, 100, 0, "%d%%");
    m_gridAlpha->setObjectName("ViewGridOpacity");
    m_gridWidth = new SliderWithLabel(100, 300, 0.01f, "%.1f px");
    m_gridWidth->setObjectName("ViewGridWidth");
    m_gridUnits = new QComboBox;
    m_gridUnits->setObjectName("ViewGridUnits");
    m_gridUnits->addItems({tr("Map units"), tr("Meters")});
    m_shadeGridCheckBox = new QCheckBox(tr("Shade grid lines"));
    m_shadeGridCheckBox->setToolTip(tr("Apply the face shading angle and shade amount to grid lines in the 3D view."));
    connect(m_shadeGridCheckBox, &QCheckBox::clicked, this, [](bool checked) { setPref(Preferences::ShadeGrid, checked); });

    m_gridAxisColorMix = new SliderWithLabel(0, 100, 0.0f, "%d%%", 0, 45);
    m_gridAxisColorMix->setToolTip(tr("Mix the grid color with the X, Y and Z axis colors, based on each line's direction."));
    connect(m_gridAxisColorMix, &SliderWithLabel::valueChanged, this, [](int value) { setPref(Preferences::GridAxisColorMix, float(value) / 100.0f); });

    m_gridAxisColorMix->setObjectName("ViewGridAxisColorMix");
    m_shadeGridCheckBox->setObjectName("ViewGridShading");
    layout->addRow(m_showGrid);
    layout->addRow(m_snapToGrid);
    layout->addRow(tr("Opacity"), m_gridAlpha);
    layout->addRow(tr("Line width"), m_gridWidth);
    layout->addRow(tr("Axis colors"), m_gridAxisColorMix);
    layout->addRow(m_shadeGridCheckBox);
    layout->addRow(tr("Units"), m_gridUnits);
    connect(m_showGrid, &QCheckBox::clicked, this, [this](bool checked) {
        auto doc = kdl::mem_lock(m_document);
        if (doc->grid().visible() != checked) doc->grid().toggleVisible();
    });
    connect(m_snapToGrid, &QCheckBox::clicked, this, [this](bool checked) {
        auto doc = kdl::mem_lock(m_document);
        if (doc->grid().snap() != checked) doc->grid().toggleSnap();
    });
    connect(m_gridAlpha, &SliderWithLabel::valueChanged, this, [](int value) { setPref(Preferences::GridAlpha, value / 100.0f); });
    connect(m_gridWidth, &SliderWithLabel::valueChanged, this, [](int value) { setPref(Preferences::GridLineWidth, value / 100.0f); });
    connect(m_gridUnits, qOverload<int>(&QComboBox::currentIndexChanged), this, [](int value) { setPref(Preferences::GridUnitSystem, value); });
    return panel;
}

void ViewEditor::refreshGui() {
    if (!m_tabs) return;
    unBindEvents();

    refreshEntityDefinitionsPanel();
    refreshEntitiesPanel();
    refreshBrushesPanel();
    refreshRendererPanel();
    refreshFogPanel();
    refreshSelectionBoundsPanel();

    bindEvents();
}

void ViewEditor::refreshEntityDefinitionsPanel() {
    m_entityDefinitionCheckBoxList->refresh();
}

void ViewEditor::refreshEntitiesPanel() {
    auto document = kdl::mem_lock(m_document);

    m_showEntityClassnamesCheckBox->setChecked(pref(Preferences::ShowEntityClassnames));
    m_showGroupBoundsCheckBox->setChecked(pref(Preferences::ShowGroupBounds));
    m_showBrushEntityBoundsCheckBox->setChecked(pref(Preferences::ShowBrushEntityBounds));
    m_showPointEntityBoundsCheckBox->setChecked(pref(Preferences::ShowPointEntityBounds));
    m_showPointEntitiesCheckBox->setChecked(pref(Preferences::ShowPointEntities));
    m_showPointEntityModelsCheckBox->setChecked(pref(Preferences::ShowPointEntityModels));
    const QSignalBlocker distanceBlocker{m_modelAnimationDistance}, speedBlocker{m_modelAnimationSpeed};
    m_animateSelectedModels->setChecked(pref(Preferences::AnimateSelectedEntityModels));
    m_animateNearbyModels->setChecked(pref(Preferences::AnimateNearbyEntityModels));
    const auto distance = pref(Preferences::EntityModelAnimationDistance), speed = pref(Preferences::EntityModelAnimationSpeed);
    m_modelAnimationDistance->setValue(std::isfinite(distance) ? qRound(std::clamp(distance, 16.f, 8192.f)) : 512);
    m_modelAnimationSpeed->setValue(std::isfinite(speed) ? qRound(std::clamp(speed, 0.f, 4.f) * 100) : 100);
    const auto modelsVisible = pref(Preferences::ShowPointEntityModels) && pref(Preferences::ShowPointEntities);
    m_animateSelectedModels->setEnabled(modelsVisible);
    m_animateNearbyModels->setEnabled(modelsVisible);
    m_modelAnimationDistance->setEnabled(modelsVisible && m_animateNearbyModels->isChecked());
    m_modelAnimationSpeed->setEnabled(modelsVisible && (m_animateSelectedModels->isChecked() || m_animateNearbyModels->isChecked()));
    const QSignalBlocker previewBlocker{m_previewEntityAudio}, loopBlocker{m_loopEntityAudio}, volumeBlocker{m_entityAudioVolume};
    m_previewEntityAudio->setChecked(pref(Preferences::PreviewEntityAudio));
    m_loopEntityAudio->setChecked(pref(Preferences::LoopEntityAudio));
    m_entityAudioVolume->setValue(std::clamp(pref(Preferences::EntityAudioVolume), 0, 100));
    m_loopEntityAudio->setEnabled(m_previewEntityAudio->isChecked());
    m_entityAudioVolume->setEnabled(m_previewEntityAudio->isChecked());
}

void ViewEditor::refreshBrushesPanel() {
    auto document = kdl::mem_lock(m_document);

    m_showBrushesCheckBox->setChecked(pref(Preferences::ShowBrushes));

    Model::EditorContext &editorContext = document->editorContext();
    const Model::TagType::Type hiddenTags = editorContext.hiddenTags();

    for (const auto &[tagType, checkBox] : m_tagCheckBoxes) {
        checkBox->setChecked((tagType & hiddenTags) == 0);
    }
}

void ViewEditor::refreshRendererPanel() {
    checkButtonInGroup(m_renderModeRadioGroup, pref(Preferences::FaceRenderMode), true);
    m_shadeFacesCheckBox->setChecked(pref(Preferences::ShadeFaces));
    m_showEdgesCheckBox->setChecked(pref(Preferences::ShowEdges));
    m_showSkyCheckBox->setChecked(pref(Preferences::ShowSky));
    checkButtonInGroup(m_entityLinkRadioGroup, pref(Preferences::EntityLinkMode), true);
    for (const auto& pair : {std::pair{m_linkWidth,&Preferences::EntityLinkWidth},std::pair{m_linkSpeed,&Preferences::EntityLinkSpeed},std::pair{m_linkSpacing,&Preferences::EntityLinkSpacing},std::pair{m_linkDistance,&Preferences::EntityLinkDistance},std::pair{m_linkOccludedAlpha,&Preferences::EntityLinkOccludedAlpha}}) {
        const QSignalBlocker blocker{pair.first};
        pair.first->setValue(pref(*pair.second)*(pair.second==&Preferences::EntityLinkOccludedAlpha?100.0:1.0));
        pair.first->setEnabled(pref(Preferences::EntityLinkMode)!=Preferences::entityLinkModeNone());
    }
    m_showSoftBoundsCheckBox->setChecked(pref(Preferences::ShowSoftMapBounds));

    auto shadeAmount = pref(Preferences::ShadeLevel) * 100;
    m_shadeAmount->setValue(static_cast<int>(shadeAmount));
    const auto document = kdl::mem_lock(m_document);
    {
        const QSignalBlocker visible(m_showGrid), snap(m_snapToGrid), alpha(m_gridAlpha), width(m_gridWidth), units(m_gridUnits);
        m_showGrid->setChecked(document->grid().visible());
        m_snapToGrid->setChecked(document->grid().snap());
        m_gridAlpha->setValue(qRound(std::clamp(pref(Preferences::GridAlpha), 0.0f, 1.0f) * 100));
        m_gridWidth->setValue(qRound(std::clamp(pref(Preferences::GridLineWidth), 1.0f, 3.0f) * 100));
        m_gridUnits->setCurrentIndex(pref(Preferences::GridUnitSystem));
    }
    m_shadeGridCheckBox->setChecked(pref(Preferences::ShadeGrid));
    m_shadeGridCheckBox->setEnabled(pref(Preferences::ShadeFaces));
    {
        const QSignalBlocker blocker(m_gridAxisColorMix);
        m_gridAxisColorMix->setValue(int(vm::round(100.0f * std::clamp(pref(Preferences::GridAxisColorMix), 0.0f, 1.0f))));
    }

    auto brightness = int(vm::round(100.0f * (pref(Preferences::Brightness))));
    m_brightnessSlider->setValue(brightness);
}

void ViewEditor::refreshFogPanel() {
    m_showFogCheckBox->setChecked(pref(Preferences::ShowFog));
    m_showAlternateFogCheckBox->setChecked(pref(Preferences::FogType) == 1);

    auto minDist = pref(Preferences::FogMinDistance);
    m_fogMinDistanceSlider->setValue(static_cast<int>(minDist));

    auto maxAmount = pref(Preferences::FogMaxAmount) * 100;
    m_fogMaxAmountSlider->setValue(static_cast<int>(maxAmount));

    auto scale = pref(Preferences::FogScale) * 10000;
    m_fogScaleSlider->setValue(static_cast<int>(scale));

    auto bias = pref(Preferences::FogBias) * 100;
    m_fogBiasSlider->setValue(static_cast<int>(bias));
}

void ViewEditor::refreshSelectionBoundsPanel() {
    const auto check = [&](const char* name, bool value) {
        if (auto* box = findChild<QCheckBox*>(name)) {
            QSignalBlocker block(box);
            box->setChecked(value);
        }
    };
    const auto number = [&](const char* name, double value) {
        if (auto* box = findChild<QDoubleSpinBox*>(name)) {
            QSignalBlocker block(box);
            box->setValue(value);
        }
    };
    check("BoundsCornerMarkers", pref(Preferences::SelectionBoundsCornerMarkers));
    check("ObjectBoundsOnHover", pref(Preferences::ObjectBoundsOnHover));
    check("BoundsShowDistances", pref(Preferences::SelectionBoundsShowDistances));
    number("BoundsGuideLength", pref(Preferences::SelectionBoundsGuideLength));
    number("BoundsGuideDistance", pref(Preferences::SelectionBoundsGuideDistance));
    number("BoundsDistanceFontSize", pref(Preferences::SelectionBoundsDistanceFontSize));
    number("BoundsDistanceOffset", pref(Preferences::SelectionBoundsDistanceOffset));
    if (auto* box = findChild<QComboBox*>("BoundsDistancePosition")) {
        QSignalBlocker block(box);
        box->setCurrentIndex(std::clamp(pref(Preferences::SelectionBoundsDistancePosition), 0, 2));
    }
    if (auto* box = findChild<ColorButton*>("BoundsDistanceColor")) {
        QSignalBlocker block(box);
        box->setColor(toQColor(pref(Preferences::SelectionBoundsDistanceColor)));
    }
    m_selectionBoundsShowObjectBounds->setChecked(pref(Preferences::ShowObjectBoundsSelectionBounds));
    m_selectionBoundsIntersectionMode->setChecked(pref(Preferences::SelectionBoundsIntersectionMode));
    m_selectionBoundsUseDashedBounds->setChecked(pref(Preferences::SelectionBoundsDashedLines));
    m_selectionBoundsAlwaysShowOnSelected->setChecked(pref(Preferences::AlwaysShowSelectionBounds));
    {
        const QSignalBlocker blocker(m_selectionBoundsAnimationSpeed);
        const auto speed = pref(Preferences::SelectionBoundsAnimationSpeed);
        m_selectionBoundsAnimationSpeed->setValue(std::isfinite(speed)
            ? qRound(std::clamp(speed, 0.f, 2.f) * 100.f)
            : qRound(Preferences::SelectionBoundsAnimationSpeed.defaultValue() * 100.f));
    }

    auto dashSize = pref(Preferences::SelectionBoundsDashedSize);
    m_selectionBoundsDashSize->setValue(static_cast<int>(dashSize));

    auto intersectionSize = pref(Preferences::SelectionBoundsPointSize);
    m_selectionBoundsIntersectionSize->setValue(static_cast<int>(intersectionSize));

    m_selectionBoundsWidth->setValue(int(pref(Preferences::SelectionBoundsLineWidth) * 100));;
}

void ViewEditor::showEntityClassnamesChanged(const bool checked) {
    setPref(Preferences::ShowEntityClassnames, checked);
}

void ViewEditor::showGroupBoundsChanged(const bool checked) {
    setPref(Preferences::ShowGroupBounds, checked);
}

void ViewEditor::showBrushEntityBoundsChanged(const bool checked) {
    setPref(Preferences::ShowBrushEntityBounds, checked);
}

void ViewEditor::showPointEntityBoundsChanged(const bool checked) {
    setPref(Preferences::ShowPointEntityBounds, checked);
}

void ViewEditor::showPointEntitiesChanged(const bool checked) {
    setPref(Preferences::ShowPointEntities, checked);
}

void ViewEditor::showPointEntityModelsChanged(const bool checked) {
    setPref(Preferences::ShowPointEntityModels, checked);
}

void ViewEditor::showBrushesChanged(const bool checked) {
    setPref(Preferences::ShowBrushes, checked);
}

void ViewEditor::showTagChanged(const bool checked, const Model::TagType::Type tagType) {
    auto document = kdl::mem_lock(m_document);
    auto &editorContext = document->editorContext();

    Model::TagType::Type hiddenTags = editorContext.hiddenTags();
    if (checked) {
        // Unhide tagType
        hiddenTags &= ~tagType;
    } else {
        // Hide tagType
        hiddenTags |= tagType;
    }

    editorContext.setHiddenTags(hiddenTags);
}

void ViewEditor::faceRenderModeChanged(const int id) {
    switch (id) {
    case 1:setPref(Preferences::FaceRenderMode, Preferences::faceRenderModeFlat());
        break;
    case 2:setPref(Preferences::FaceRenderMode, Preferences::faceRenderModeSkip());
        break;
    default:setPref(Preferences::FaceRenderMode, Preferences::faceRenderModeTextured());
        break;
    }
}

void ViewEditor::shadeFacesChanged(const bool checked) {
    setPref(Preferences::ShadeFaces, checked);
}

void ViewEditor::showFogChanged(const bool checked) {
    setPref(Preferences::ShowFog, checked);
}

void ViewEditor::showEdgesChanged(const bool checked) {
    setPref(Preferences::ShowEdges, checked);
}

void ViewEditor::entityLinkModeChanged(const int id) {
    switch (id) {
    case 0:setPref(Preferences::EntityLinkMode, Preferences::entityLinkModeAll());
        break;
    case 1:setPref(Preferences::EntityLinkMode, Preferences::entityLinkModeTransitive());
        break;
    case 2:setPref(Preferences::EntityLinkMode, Preferences::entityLinkModeDirect());
        break;
    default:setPref(Preferences::EntityLinkMode, Preferences::entityLinkModeNone());
        break;
    }
}

void ViewEditor::showSoftMapBoundsChanged(const bool checked) {
    setPref(Preferences::ShowSoftMapBounds, checked);
}

void ViewEditor::restoreDefaultsClicked() {
    PreferenceManager &prefs = PreferenceManager::instance();
    prefs.resetToDefault(Preferences::Brightness);
    prefs.resetToDefault(Preferences::ShowEntityClassnames);
    prefs.resetToDefault(Preferences::ShowGroupBounds);
    prefs.resetToDefault(Preferences::ShowBrushEntityBounds);
    prefs.resetToDefault(Preferences::ShowPointEntityBounds);
    prefs.resetToDefault(Preferences::ShowPointEntityModels);
    prefs.resetToDefault(Preferences::AnimateSelectedEntityModels);
    prefs.resetToDefault(Preferences::AnimateNearbyEntityModels);
    prefs.resetToDefault(Preferences::EntityModelAnimationDistance);
    prefs.resetToDefault(Preferences::EntityModelAnimationSpeed);
    prefs.resetToDefault(Preferences::PreviewEntityAudio);
    prefs.resetToDefault(Preferences::LoopEntityAudio);
    prefs.resetToDefault(Preferences::EntityAudioVolume);
    prefs.resetToDefault(Preferences::FaceRenderMode);
    prefs.resetToDefault(Preferences::ShadeFaces);
    prefs.resetToDefault(Preferences::ShadeLevel);
    prefs.resetToDefault(Preferences::GridAlpha);
    prefs.resetToDefault(Preferences::GridLineWidth);
    prefs.resetToDefault(Preferences::GridUnitSystem);
    prefs.resetToDefault(Preferences::SelectionBoundsCornerMarkers);
    prefs.resetToDefault(Preferences::SelectionBoundsGuideLength);
    prefs.resetToDefault(Preferences::SelectionBoundsGuideDistance);
    prefs.resetToDefault(Preferences::SelectionBoundsShowDistances);
    prefs.resetToDefault(Preferences::SelectionBoundsDistanceColor);
    prefs.resetToDefault(Preferences::SelectionBoundsDistanceFontSize);
    prefs.resetToDefault(Preferences::SelectionBoundsDistancePosition);
    prefs.resetToDefault(Preferences::SelectionBoundsDistanceOffset);
    prefs.resetToDefault(Preferences::ObjectBoundsOnHover);
    prefs.resetToDefault(Preferences::SelectionBoundsDashedLines);
    prefs.resetToDefault(Preferences::SelectionBoundsDashedSize);
    prefs.resetToDefault(Preferences::SelectionBoundsAnimationSpeed);
    prefs.resetToDefault(Preferences::SelectionBoundsLineWidth);
    prefs.resetToDefault(Preferences::SelectionBoundsPointSize);
    prefs.resetToDefault(Preferences::ShowObjectBoundsSelectionBounds);
    prefs.resetToDefault(Preferences::AlwaysShowSelectionBounds);
    prefs.resetToDefault(Preferences::SelectionBoundsIntersectionMode);
    prefs.resetToDefault(Preferences::GridAxisColorMix);
    prefs.resetToDefault(Preferences::ShadeGrid);
    prefs.resetToDefault(Preferences::ShowFog);
    prefs.resetToDefault(Preferences::ShowSky);
    prefs.resetToDefault(Preferences::FogBias);
    prefs.resetToDefault(Preferences::FogType);
    prefs.resetToDefault(Preferences::FogScale);
    prefs.resetToDefault(Preferences::FogMaxAmount);
    prefs.resetToDefault(Preferences::FogMinDistance);
    prefs.resetToDefault(Preferences::ShowEdges);
    prefs.resetToDefault(Preferences::ShowSoftMapBounds);
    prefs.resetToDefault(Preferences::ShowPointEntities);
    prefs.resetToDefault(Preferences::ShowBrushes);
    prefs.resetToDefault(Preferences::EntityLinkMode);
    prefs.resetToDefault(Preferences::EntityLinkWidth);
    prefs.resetToDefault(Preferences::EntityLinkSpeed);
    prefs.resetToDefault(Preferences::EntityLinkSpacing);
    prefs.resetToDefault(Preferences::EntityLinkDistance);
    prefs.resetToDefault(Preferences::EntityLinkOccludedAlpha);
    prefs.saveChanges();
    const auto document = kdl::mem_lock(m_document);
    if (!document->grid().visible()) document->grid().toggleVisible();
    if (!document->grid().snap()) document->grid().toggleSnap();
    refreshGui();
}

void ViewEditor::showFogMinDistance(const int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::FogMinDistance, float(value));
}

void ViewEditor::showFogScale(const int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::FogScale, float(value) / 10000.f);
}

void ViewEditor::showFogMaxAmount(const int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::FogMaxAmount, float(value) / 100.f);
}

void ViewEditor::showFogType(const bool checked) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::FogType, checked ? 1 : 0);
}

void ViewEditor::showFogBias(const int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::FogBias, float(value) / 100.f);
}

void ViewEditor::shadeLevelChanged(const int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::ShadeLevel, float(value) / 100.f);
}

void ViewEditor::brightnessChanged(const int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::Brightness, (float(value) / 100.0f));
}

void ViewEditor::dashedBoundsChanged(bool checked) {
    setPref(Preferences::SelectionBoundsDashedLines, checked);
}

void ViewEditor::showObjectBoundsChanged(bool checked) {
    setPref(Preferences::ShowObjectBoundsSelectionBounds, checked);
}

void ViewEditor::showAlwaysShowBoundsChanged(bool checked) {
    setPref(Preferences::AlwaysShowSelectionBounds, checked);
}

void ViewEditor::showIntersectionModeChanged(bool checked) {
    setPref(Preferences::SelectionBoundsIntersectionMode, checked);
}

void ViewEditor::intersectionMarkerSizeChanged(int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::SelectionBoundsPointSize, float(value));
}

void ViewEditor::dashSizeChanged(int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::SelectionBoundsDashedSize, value);
}

void ViewEditor::selectionBoundsLineWithChanged(int value) {
    auto &prefs = PreferenceManager::instance();
    prefs.set(Preferences::SelectionBoundsLineWidth, float(value) / 100.0f);
}

ViewPopupEditor::ViewPopupEditor(std::weak_ptr<MapDocument> document, QWidget *parent) : QWidget(parent), m_button(nullptr), m_editor(nullptr) {
    m_button = new PopupButton(tr("View Settings"));
    m_button->setIcon("ArrowDown");
    m_button->setToolTip(tr("Click to edit view settings"));

    auto *editorContainer = new BorderPanel();
    m_editor = new ViewEditor(document);

    auto *containerSizer = new QVBoxLayout();
    containerSizer->setContentsMargins(0, 0, 0, 0);
    containerSizer->addWidget(m_editor);
    editorContainer->setLayout(containerSizer);

    auto *popupSizer = new QVBoxLayout();
    popupSizer->setContentsMargins(0, 0, 0, 0);
    popupSizer->addWidget(editorContainer);
    m_button->GetPopupWindow()->setObjectName("ViewDetailsPopup");
    m_button->GetPopupWindow()->setLayout(popupSizer);

    auto *sizer = new QHBoxLayout();
    sizer->setContentsMargins(LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin);
    sizer->addWidget(m_button, Qt::AlignVCenter | Qt::AlignHCenter);

    setLayout(sizer);
}
} // namespace View
} // namespace TrenchBroom
