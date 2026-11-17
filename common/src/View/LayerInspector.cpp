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

#include "LayerInspector.h"

#include <QLabel>
#include <QLineEdit>
#include <QToolBar>
#include <QVBoxLayout>

#include "IO/ResourceUtils.h"
#include "View/LayerEditor.h"
#include "View/MapSearchListBox.h"
#include "View/QtUtils.h"
#include "View/TitleBar.h"
#include "View/TitledPanel.h"

namespace TrenchBroom::View {
LayerInspector::LayerInspector(const std::weak_ptr<MapDocument>& document, MapView* mapView, Inspector* inspector, QWidget* parent)
    : QWidget(parent), m_mapView(mapView), m_inspector(inspector) {
    setObjectName("LayerInspector_Widget");

    m_layerPanel = createLayerEditor(document);
    m_mapSearchPanel = createSearchList(document);
    m_layerPanel->setMinimumHeight(150);
}

std::vector<InspectorToolSection> LayerInspector::toolSections() const {
    return {
        {QStringLiteral("layers"), tr("Layers"), m_layerPanel,
            m_layerPanel ? m_layerPanel->getTitleBar() : nullptr, 3},
        {QStringLiteral("search"), tr("Search"), m_mapSearchPanel,
            m_mapSearchPanel ? m_mapSearchPanel->getTitleBar() : nullptr, 1},
    };
}

TitledPanel* LayerInspector::createLayerEditor(const std::weak_ptr<MapDocument>& document) {
    const auto titledPanel = new TitledPanel(tr("Layers"), this);
    titledPanel->setObjectName("LayerInspector_LayersPanel");
    auto* editor = new LayerEditor(document, m_mapView);

    auto* sizer = new QVBoxLayout();
    sizer->setContentsMargins(0, 0, 0, 0);
    sizer->addWidget(editor, 1);
    titledPanel->getPanel()->setLayout(sizer);

    return titledPanel;
}

TitledPanel* LayerInspector::createSearchList(const std::weak_ptr<MapDocument>& document) {
    const auto titledPanel = new TitledPanel(tr("Search"), this);
    titledPanel->setObjectName("MapInspector_SearchBoxPanel");

    const auto label = new QLabel{};
    auto* searchList = new MapSearchListBox(document, m_mapView, m_inspector, titledPanel->getPanel(), label);
    auto* layout = new QVBoxLayout();
    auto* searchBox = createSearchBox();

    connect(searchBox, &QLineEdit::textEdited, searchList, &MapSearchListBox::triggerSearch);

    auto* toolbar = createInspectorToolBar();
    toolbar->addWidget(searchBox);
    toolbar->addWidget(label);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(toolbar);
    layout->addWidget(searchList, 1);
    titledPanel->getPanel()->setLayout(layout);

    return titledPanel;
}

QLineEdit* LayerInspector::createSearchBox() {
    auto* widget = new QLineEdit{};
    widget->setClearButtonEnabled(true);
    widget->setPlaceholderText(QLineEdit::tr("Search Entities"));

    const auto icon = IO::loadSVGIcon("Search.svg", 16);
    widget->addAction(icon, QLineEdit::LeadingPosition);
    return widget;
}
} // namespace TrenchBroom::View
