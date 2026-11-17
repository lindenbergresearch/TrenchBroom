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


#include "FloatType.h"
#include "NotifierConnection.h"
#include "View/InspectorToolSection.h"
#include <QWidget>

#include <QPointer>

#include <memory>
#include <optional>
#include <vector>


class QCheckBox;

class QLabel;

class QLineEdit;

class QComboBox;

namespace TrenchBroom {
namespace Model {
class Node;
}

namespace View {
class TitledPanel;

class MapDocument;

class MapInspector : public QWidget {
  Q_OBJECT
  private:
    QPointer<TitledPanel> m_mapPropertiesEditorPanel;
    QPointer<TitledPanel> m_modEditorPanel;

  public:
    explicit MapInspector(std::weak_ptr<MapDocument> document, QWidget *parent = nullptr);

    std::vector<InspectorToolSection> toolSections() const;

  private:
    void createGui(std::weak_ptr<MapDocument> document);

    TitledPanel *createMapPropertiesEditor(std::weak_ptr<MapDocument> document);

    TitledPanel *createModEditor(std::weak_ptr<MapDocument> document);

};

/**
 * Currently just the soft bounds editor
 */
class MapPropertiesEditor : public QWidget {
  Q_OBJECT
  private:
    std::weak_ptr<MapDocument> m_document;
    bool m_updatingGui;

    enum BoundsMode { Disabled, GameDefault, Custom };
    QComboBox* m_boundsMode = nullptr;
    QLabel *m_softBoundsFromGameMinLabel;
    QLabel *m_softBoundsFromGameMaxLabel;
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
