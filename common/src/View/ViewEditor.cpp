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

#include "ViewEditor.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>

#include "Assets/EntityDefinition.h"
#include "Assets/EntityDefinitionGroup.h"
#include "Assets/EntityDefinitionManager.h"
#include "Model/EditorContext.h"
#include "Model/Game.h"
#include "Model/Tag.h"
#include "Model/TagType.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/BorderPanel.h"
#include "View/BorderLine.h"
#include "View/MapDocument.h"
#include "View/PopupButton.h"
#include "View/QtUtils.h"
#include "View/TitledPanel.h"
#include "View/SliderWithLabel.h"
#include "View/ViewConstants.h"

#include <kdl/memory_utils.h>

#include <vector>

namespace TrenchBroom {
namespace View {
// EntityDefinitionCheckBoxList

EntityDefinitionCheckBoxList::EntityDefinitionCheckBoxList(Assets::EntityDefinitionManager &entityDefinitionManager, Model::EditorContext &editorContext,
    QWidget *parent
) : QWidget(parent), m_entityDefinitionManager(entityDefinitionManager), m_editorContext(editorContext) {
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
        groupCB->setForegroundRole(QPalette::HighlightedText);

        connect(groupCB, &QAbstractButton::clicked, this, [this, i](bool checked) {
              this->groupCheckBoxChanged(i, checked);
            }
        );

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
                }
            );

            m_defCheckBoxes.push_back(defCB);
            scrollWidgetLayout->addWidget(defCB);
        }
    }

    scrollWidgetLayout->addSpacing(1);

    auto *scrollWidget = new QWidget();
    scrollWidget->setContentsMargins(LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin);
    scrollWidget->setLayout(scrollWidgetLayout);

    auto *scrollArea = new QScrollArea();
    scrollArea->setContentsMargins(0, 0, 0, 0);
    scrollArea->setBackgroundRole(QPalette::Base);
    scrollArea->setAutoFillBackground(true);
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
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(LayoutConstants::NarrowHMargin);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(showAllButton);
    buttonLayout->addWidget(hideAllButton);
    buttonLayout->addStretch(1);

    auto *outerLayout = new QVBoxLayout();
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(LayoutConstants::MediumVMargin);
    outerLayout->addWidget(scrollArea, 1);
    outerLayout->addWidget(new BorderLine);
    outerLayout->addLayout(buttonLayout);
    setLayout(outerLayout);
}

// ViewEditor

ViewEditor::ViewEditor(std::weak_ptr<MapDocument> document, QWidget *parent) : QWidget(parent), m_document(std::move(document)),
                                                                               m_showEntityClassnamesCheckBox(nullptr), m_showGroupBoundsCheckBox(nullptr), m_showBrushEntityBoundsCheckBox(nullptr),
                                                                               m_showPointEntityBoundsCheckBox(nullptr), m_showPointEntitiesCheckBox(nullptr), m_showPointEntityModelsCheckBox(nullptr),
                                                                               m_entityDefinitionCheckBoxList(nullptr), m_showBrushesCheckBox(nullptr), m_renderModeRadioGroup(nullptr), m_shadeFacesCheckBox(nullptr),
                                                                               m_showEdgesCheckBox(nullptr),
                                                                               m_showFogCheckBox(nullptr),
                                                                               m_entityLinkRadioGroup(nullptr),
                                                                               m_showSoftBoundsCheckBox(nullptr) {
    connectObservers();
}

void ViewEditor::connectObservers() {
    auto document = kdl::mem_lock(m_document);
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
    connect(m_showBrushEntityBoundsCheckBox,
        &QAbstractButton::clicked,
        this,
        &ViewEditor::showBrushEntityBoundsChanged);
    connect(m_showPointEntityBoundsCheckBox,
        &QAbstractButton::clicked,
        this,
        &ViewEditor::showPointEntityBoundsChanged);
    connect(m_showPointEntitiesCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showPointEntitiesChanged);
    connect(m_showPointEntityModelsCheckBox,
        &QAbstractButton::clicked,
        this,
        &ViewEditor::showPointEntityModelsChanged);
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
    connect(m_selectionBoundsAlwaysShowOnSelected,&QAbstractButton::clicked, this, &ViewEditor::showAlwaysShowBoundsChanged);
    connect(m_selectionBoundsUseDashedBounds, &QAbstractButton::clicked, this, &ViewEditor::dashedBoundsChanged);

    connect(m_selectionBoundsWidth, &SliderWithLabel::valueChanged, this, &ViewEditor::selectionBoundsLineWithChanged);
    connect(m_selectionBoundsDashSize, &SliderWithLabel::valueChanged, this, &ViewEditor::dashSizeChanged);
    connect(m_selectionBoundsIntersectionSize, &SliderWithLabel::valueChanged, this, &ViewEditor::intersectionMarkerSizeChanged);

    connect(m_renderModeRadioGroup,
        static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
        this,
        &ViewEditor::faceRenderModeChanged);
    connect(m_entityLinkRadioGroup,
        static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
        this,
        &ViewEditor::entityLinkModeChanged);
    connect(m_showSoftBoundsCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showSoftMapBoundsChanged);
}

void ViewEditor::unBindEvents() {
    disconnect(m_showEntityClassnamesCheckBox,
        &QAbstractButton::clicked,
        this,
        &ViewEditor::showEntityClassnamesChanged);
    disconnect(m_showGroupBoundsCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showGroupBoundsChanged);
    disconnect(m_showBrushEntityBoundsCheckBox,
        &QAbstractButton::clicked,
        this,
        &ViewEditor::showBrushEntityBoundsChanged);
    disconnect(m_showPointEntityBoundsCheckBox,
        &QAbstractButton::clicked,
        this,
        &ViewEditor::showPointEntityBoundsChanged);
    disconnect(m_showPointEntitiesCheckBox, &QAbstractButton::clicked, this, &ViewEditor::showPointEntitiesChanged);
    disconnect(m_showPointEntityModelsCheckBox,
        &QAbstractButton::clicked,
        this,
        &ViewEditor::showPointEntityModelsChanged);
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
    disconnect(m_selectionBoundsAlwaysShowOnSelected,&QAbstractButton::clicked, this, &ViewEditor::showAlwaysShowBoundsChanged);
    disconnect(m_selectionBoundsUseDashedBounds, &QAbstractButton::clicked, this, &ViewEditor::dashedBoundsChanged);

    disconnect(m_selectionBoundsWidth, &SliderWithLabel::valueChanged, this, &ViewEditor::selectionBoundsLineWithChanged);
    disconnect(m_selectionBoundsDashSize, &SliderWithLabel::valueChanged, this, &ViewEditor::dashSizeChanged);
    disconnect(m_selectionBoundsIntersectionSize, &SliderWithLabel::valueChanged, this, &ViewEditor::intersectionMarkerSizeChanged);
    
    disconnect(m_renderModeRadioGroup,
        static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
        this,
        &ViewEditor::faceRenderModeChanged);
    disconnect(m_entityLinkRadioGroup,
        static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
        this,
        &ViewEditor::entityLinkModeChanged);
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
    // remove to avoid double loading
    // no refresh on external preferences change (who needs?)
     refreshGui();
}

void ViewEditor::createGui() {
    deleteChildWidgetsLaterAndDeleteLayout(this);

    auto *sizer = new QGridLayout();
    sizer->setContentsMargins(
        LayoutConstants::MediumHMargin,
        LayoutConstants::MediumVMargin,
        LayoutConstants::MediumHMargin,
        LayoutConstants::MediumVMargin
    );
    sizer->setHorizontalSpacing(LayoutConstants::MediumHMargin);
    sizer->setVerticalSpacing(LayoutConstants::MediumVMargin);

    sizer->addWidget(createEntityDefinitionsPanel(this), 0, 0, 3, 1);
    sizer->addWidget(createEntitiesPanel(this), 0, 1);
    sizer->addWidget(createBrushesPanel(this), 1, 1);
    sizer->addWidget(createRendererPanel(this), 2, 1);
    sizer->addWidget(createFogPanel(this), 0, 2, 3, 1);
    sizer->addWidget(createSelectionBoundsPanel(this), 2, 2, 3, 1);

    setLayout(sizer);
}

QWidget *ViewEditor::createEntityDefinitionsPanel(QWidget *parent) {
    TitledPanel *panel = new TitledPanel("Entity Definitions", parent, false);

    auto document = kdl::mem_lock(m_document);
    Assets::EntityDefinitionManager &entityDefinitionManager = document->entityDefinitionManager();

    Model::EditorContext &editorContext = document->editorContext();
    m_entityDefinitionCheckBoxList = new EntityDefinitionCheckBoxList(entityDefinitionManager, editorContext);

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(
        LayoutConstants::NoMargin,
        LayoutConstants::NoMargin,
        LayoutConstants::NoMargin,
        LayoutConstants::NoMargin
    );

    m_entityDefinitionCheckBoxList->setContentsMargins(
        LayoutConstants::NoMargin,
        LayoutConstants::NoMargin,
        LayoutConstants::NoMargin,
        LayoutConstants::NoMargin
    );

    layout->setSpacing(0);
    layout->addWidget(m_entityDefinitionCheckBoxList, 1);
    m_entityDefinitionCheckBoxList->setMinimumWidth(250);

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

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin
    );

    layout->setSpacing(0);
    layout->addWidget(m_showEntityClassnamesCheckBox);
    layout->addWidget(m_showGroupBoundsCheckBox);
    layout->addWidget(m_showBrushEntityBoundsCheckBox);
    layout->addWidget(m_showPointEntityBoundsCheckBox);
    layout->addWidget(m_showPointEntitiesCheckBox);
    layout->addWidget(m_showPointEntityModelsCheckBox);
    layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    panel->getPanel()->setLayout(layout);
    return panel;
}

QWidget *ViewEditor::createSelectionBoundsPanel(QWidget *parent) {
    TitledPanel *panel = new TitledPanel("Selection Bounds", parent, false);
    panel->setMinimumWidth(250);

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin
    );

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

    m_selectionBoundsDashSize = new SliderWithLabel(2, 10, 0.0f, "%dpx", 220, 15, this);
    m_selectionBoundsIntersectionSize = new SliderWithLabel(2, 20, 0.0f, "%dpx", 220, 15, this);
    m_selectionBoundsWidth = new SliderWithLabel(10, 300, 10e-3f, "%.1f px", 220, 15, this);

    layout->addWidget(new QLabel{"Dash size"});
    layout->addWidget(m_selectionBoundsDashSize);
    layout->addSpacing(LayoutConstants::WideVMargin);

    layout->addWidget(new QLabel{"Intersection marker size"});
    layout->addWidget(m_selectionBoundsIntersectionSize);
    layout->addSpacing(LayoutConstants::WideVMargin);

    layout->addWidget(new QLabel{"Selection bounds line-with"});
    layout->addWidget(m_selectionBoundsWidth);
    layout->addSpacing(LayoutConstants::WideVMargin);

    layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    panel->getPanel()->setLayout(layout);

    return panel;
}

QWidget *ViewEditor::createFogPanel(QWidget *parent) {
    TitledPanel *panel = new TitledPanel("Fog", parent, false);
    panel->setMinimumWidth(250);

    m_showFogCheckBox = new QCheckBox(tr("Enable fog"));

    m_showAlternateFogCheckBox = new QCheckBox(tr("Alternate Fog Algorithm"));

    m_fogMaxAmountSlider = new SliderWithLabel(1, 100, 0.0f, "%d%", 220, 15, this);
    m_fogMinDistanceSlider = new SliderWithLabel(16, 2048, 0.0f, "%d", 220, 15, this);
    m_fogScaleSlider = new SliderWithLabel(1, 100, 0.0f, "%d", 220, 15, this);
    m_fogBiasSlider = new SliderWithLabel(0, 100, 0.0f, "%d", 220, 15, this);

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin
    );
    layout->setSpacing(0);
    layout->addWidget(m_showFogCheckBox);
    layout->addSpacing(LayoutConstants::WideVMargin);

    layout->addWidget(new QLabel{"Maximum amount"});
    layout->addWidget(m_fogMaxAmountSlider);
    layout->addSpacing(LayoutConstants::WideVMargin);

    layout->addWidget(new QLabel{"Minimum distance"});
    layout->addWidget(m_fogMinDistanceSlider);
    layout->addSpacing(LayoutConstants::WideVMargin);

    layout->addWidget(new QLabel{"Scale"});
    layout->addWidget(m_fogScaleSlider);
    layout->addSpacing(LayoutConstants::WideVMargin);

    layout->addWidget(new QLabel{"Bias"});
    layout->addWidget(m_fogBiasSlider);
    layout->addSpacing(LayoutConstants::WideVMargin);

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

    auto *layout = new QHBoxLayout();
    layout->setContentsMargins(0, LayoutConstants::WideVMargin, 0, LayoutConstants::WideVMargin);
    layout->setSpacing(0);
    layout->addWidget(msg);
    layout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    parent->setLayout(layout);
}

void ViewEditor::createTagFilter(QWidget *parent, const std::vector<Model::SmartTag> &tags) {
    assert(!tags.empty());

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin
    );
    layout->setSpacing(0);

    for (const auto &tag: tags) {
        const QString label = QString::fromLatin1("Show %1").arg(QString::fromStdString(tag.name()).toLower());

        auto *checkBox = new QCheckBox(label);
        const Model::TagType::Type tagType = tag.type();

        m_tagCheckBoxes.emplace_back(tagType, checkBox);

        layout->addWidget(checkBox);
        connect(checkBox, &QAbstractButton::clicked, this, [this, tagType](const bool checked) {
              showTagChanged(checked, tagType);
            }
        );
    }

    layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    parent->setLayout(layout);
}

QWidget *ViewEditor::createRendererPanel(QWidget *parent) {
    TitledPanel *panel = new TitledPanel("Renderer", parent, false);
    QWidget * inner = panel->getPanel();

    const QList<QString> FaceRenderModes = {
        "Show textures",
        "Hide textures",
        "Hide faces"
    };

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
    m_shadeAmount = new SliderWithLabel(1, 100, 0.0f, "%d%", 220, 15, this);

    m_brightnessSlider = new SliderWithLabel{25, 300, 0, "%d%%", 220, 15};
    m_brightnessSlider->setToolTip("Sets the brightness for textures and model skins in the 3D editing view.");

    m_showEdgesCheckBox = new QCheckBox(tr("Show edges"));

    const QList<QString> EntityLinkModes = {
        "Show all entity links",
        "Show transitively selected entity links",
        "Show directly selected entity links",
        "Hide entity links"
    };

    const QList<QString> EntityLinkModesPrefValues = {
        Preferences::entityLinkModeAll(), Preferences::entityLinkModeTransitive(), Preferences::entityLinkModeDirect(), Preferences::entityLinkModeNone()
    };

    m_entityLinkRadioGroup = new QButtonGroup(this);
    for (int i = 0; i < EntityLinkModes.length(); ++i) {
        const QString &label = EntityLinkModes.at(i);
        const QString &prefValue = EntityLinkModesPrefValues.at(i);

        auto *radio = new QRadioButton(label);
        radio->setObjectName(prefValue);
        m_entityLinkRadioGroup->addButton(radio, i);
    }

    m_showSoftBoundsCheckBox = new QCheckBox(tr("Show soft bounds"));

    auto *restoreDefualtsButton = new QPushButton(tr("Restore Defaults"));
    restoreDefualtsButton->setContentsMargins(LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin
    );
    restoreDefualtsButton->setObjectName("ViewEditor_smallPushButton");
    makeSmall(restoreDefualtsButton);

    connect(restoreDefualtsButton, &QAbstractButton::clicked, this, &ViewEditor::restoreDefaultsClicked);

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin
    );
    layout->setSpacing(0);

    layout->addWidget(new QLabel{"Brightness"});
    layout->addWidget(m_brightnessSlider);

    for (auto *button: m_renderModeRadioGroup->buttons()) {
        layout->addWidget(button);
    }

    layout->addWidget(m_shadeFacesCheckBox);

    layout->addWidget(new QLabel{"Shade amount"});
    layout->addWidget(m_shadeAmount);

    layout->addWidget(m_showEdgesCheckBox);

    for (auto *button: m_entityLinkRadioGroup->buttons()) {
        layout->addWidget(button);
    }

    layout->addWidget(m_showSoftBoundsCheckBox);
    layout->addSpacing(LayoutConstants::WideVMargin);
    layout->addWidget(new BorderLine);
    layout->addWidget(restoreDefualtsButton, 0, Qt::AlignHCenter);
    layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    inner->setLayout(layout);
    return panel;
}

void ViewEditor::refreshGui() {
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
}

void ViewEditor::refreshBrushesPanel() {
    auto document = kdl::mem_lock(m_document);

    m_showBrushesCheckBox->setChecked(pref(Preferences::ShowBrushes));

    Model::EditorContext &editorContext = document->editorContext();
    const Model::TagType::Type hiddenTags = editorContext.hiddenTags();

    for (const auto &[tagType, checkBox]: m_tagCheckBoxes) {
        checkBox->setChecked((tagType & hiddenTags) == 0);
    }
}

void ViewEditor::refreshRendererPanel() {
    checkButtonInGroup(m_renderModeRadioGroup, pref(Preferences::FaceRenderMode), true);
    m_shadeFacesCheckBox->setChecked(pref(Preferences::ShadeFaces));
    m_showEdgesCheckBox->setChecked(pref(Preferences::ShowEdges));
    checkButtonInGroup(m_entityLinkRadioGroup, pref(Preferences::EntityLinkMode), true);
    m_showSoftBoundsCheckBox->setChecked(pref(Preferences::ShowSoftMapBounds));

    auto shadeAmount = pref(Preferences::ShadeLevel) * 100;
    m_shadeAmount->setValue(static_cast<int>(shadeAmount));

    auto brightness = int(vm::round(100.0f * (pref(Preferences::Brightness) - 1.0f)));
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
    m_selectionBoundsShowObjectBounds->setChecked(pref(Preferences::ShowObjectBoundsSelectionBounds));
    m_selectionBoundsIntersectionMode->setChecked(pref(Preferences::SelectionBoundsIntersectionMode));
    m_selectionBoundsUseDashedBounds->setChecked(pref(Preferences::SelectionBoundsDashedLines));
    m_selectionBoundsAlwaysShowOnSelected->setChecked(pref(Preferences::AlwaysShowSelectionBounds));

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
        case 1:
            setPref(Preferences::FaceRenderMode, Preferences::faceRenderModeFlat());
            break;
        case 2:
            setPref(Preferences::FaceRenderMode, Preferences::faceRenderModeSkip());
            break;
        default:
            setPref(Preferences::FaceRenderMode, Preferences::faceRenderModeTextured());
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
        case 0:
            setPref(Preferences::EntityLinkMode, Preferences::entityLinkModeAll());
            break;
        case 1:
            setPref(Preferences::EntityLinkMode, Preferences::entityLinkModeTransitive());
            break;
        case 2:
            setPref(Preferences::EntityLinkMode, Preferences::entityLinkModeDirect());
            break;
        default:
            setPref(Preferences::EntityLinkMode, Preferences::entityLinkModeNone());
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
    prefs.resetToDefault(Preferences::FaceRenderMode);
    prefs.resetToDefault(Preferences::ShadeFaces);
    prefs.resetToDefault(Preferences::ShadeLevel);
    prefs.resetToDefault(Preferences::ShowFog);
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
    prefs.saveChanges();
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
    prefs.set(Preferences::Brightness, (float(value) / 100.0f) + 1.0f);
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
    m_button->GetPopupWindow()->setLayout(popupSizer);

    auto *sizer = new QHBoxLayout();
    sizer->setContentsMargins(LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin,
        LayoutConstants::NarrowHMargin);
    sizer->addWidget(m_button, Qt::AlignVCenter | Qt::AlignHCenter);

    setLayout(sizer);
}
} // namespace View
} // namespace TrenchBroom
