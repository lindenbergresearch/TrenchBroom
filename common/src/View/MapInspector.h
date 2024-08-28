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

#include <QSplitter>

#include "FloatType.h"
#include "NotifierConnection.h"
#include "View/Inspector.h"
#include "View/LayerEditor.h"
#include "View/MapSearchListBox.h"
#include "View/MapView.h"
#include "View/TabBook.h"
#include "View/TitledPanel.h"

#include <memory>
#include <optional>
#include <vector>

class QWidget;

class QCheckBox;

class QLabel;

class QLineEdit;

class QRadioButton;

namespace TrenchBroom {
namespace Model {
class Node;
}

namespace View {
class CollapsibleTitledPanel;

class MapDocument;

class MapInspector : public TabBookPage {
  Q_OBJECT
  private:
    CollapsibleTitledPanel *m_mapPropertiesEditorPanel;
    CollapsibleTitledPanel *m_modEditorPanel;
    CollapsibleTitledPanel *m_mapSearchPanel;
    TitledPanel *m_layerEditorPanel;

    QSplitter *m_splitter;
    LayerEditor *m_layerEditor;
    MapSearchListBox *m_mapSearchListBox;
    QLineEdit *m_searchBoxEdit;
    MapView *m_mapView;
    Inspector *m_inspector;

  public:
    explicit MapInspector(std::weak_ptr<MapDocument> document, MapView *mapView, Inspector *inspector, QWidget *parent = nullptr);

    ~MapInspector();

  private:
    void createGui(std::weak_ptr<MapDocument> document);

    TitledPanel *createLayerEditor(std::weak_ptr<MapDocument> document);

    CollapsibleTitledPanel *createMapPropertiesEditor(std::weak_ptr<MapDocument> document);

    CollapsibleTitledPanel *createModEditor(std::weak_ptr<MapDocument> document);

    CollapsibleTitledPanel *createSearchList(std::weak_ptr<MapDocument> document);

    QLineEdit *createSearchBox();
};

/**
 * Currently just the soft bounds editor
 */
class MapPropertiesEditor : public QWidget {
  Q_OBJECT
  private:
    std::weak_ptr<MapDocument> m_document;
    bool m_updatingGui;

    QRadioButton *m_softBoundsDisabled;
    QRadioButton *m_softBoundsFromGame;
    QLabel *m_softBoundsFromGameMinLabel;
    QLabel *m_softBoundsFromGameMaxLabel;
    QRadioButton *m_softBoundsFromMap;
    QLineEdit *m_softBoundsFromMapMinEdit;
    QLineEdit *m_softBoundsFromMapMaxEdit;

    NotifierConnection m_notifierConnection;

  public:
    explicit MapPropertiesEditor(std::weak_ptr<MapDocument> document, QWidget *parent = nullptr);

  private:
    std::optional<vm::bbox3> parseLineEdits();

    void createGui();

  private:
    void connectObservers();

    void documentWasNewed(MapDocument *document);

    void documentWasLoaded(MapDocument *document);

    void nodesDidChange(const std::vector<Model::Node *> &nodes);

    void updateGui();
};
} // namespace View
} // namespace TrenchBroom
