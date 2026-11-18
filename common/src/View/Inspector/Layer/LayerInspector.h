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

#include "View/Inspector/InspectorToolSection.h"
#include <QWidget>

#include <QPointer>

#include <memory>
#include <vector>

class QLineEdit;

namespace TrenchBroom::View {
class Inspector;
class MapDocument;
class MapView;
class TitledPanel;

class LayerInspector : public QWidget {
    Q_OBJECT
    QPointer<TitledPanel> m_layerPanel;
    QPointer<TitledPanel> m_mapSearchPanel;
    MapView *m_mapView;
    Inspector *m_inspector;

public:
    LayerInspector(const std::weak_ptr<MapDocument>& document, MapView *mapView, Inspector *inspector, QWidget *parent = nullptr);
    std::vector<InspectorToolSection> toolSections() const;

private:
    TitledPanel *createLayerEditor(const std::weak_ptr<MapDocument>& document);
    TitledPanel *createSearchList(const std::weak_ptr<MapDocument>& document);
    static QLineEdit *createSearchBox();
};
} // namespace TrenchBroom::View
