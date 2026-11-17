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

#include "View/InspectorToolSection.h"
#include <QWidget>

#include <QPointer>

#include <memory>
#include <vector>


namespace TrenchBroom {
namespace View {

class TitledPanel;

class GLContextManager;

class MapDocument;

class EntityInspector : public QWidget {
  Q_OBJECT
  private:
    QPointer<TitledPanel> m_entityBrowserPanel;
    QPointer<TitledPanel> m_entityDefinitionFileChooser;

  public:
    EntityInspector(std::weak_ptr<MapDocument> document, GLContextManager &contextManager, QWidget *parent = nullptr);

    std::vector<InspectorToolSection> toolSections() const;

  private:
    void createGui(std::weak_ptr<MapDocument> document, GLContextManager &contextManager);

    TitledPanel *createEntityBrowser(QWidget *parent, std::weak_ptr<MapDocument> document, GLContextManager &contextManager);

    TitledPanel *createEntityDefinitionFileChooser(QWidget *parent, std::weak_ptr<MapDocument> document);
};
} // namespace View
} // namespace TrenchBroom
