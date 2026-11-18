/*
Copyright (C) 2010-2017 Kristian Duske

This file is part of TrenchBroom.

   TrenchBroom is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
     the Free Software Foundation, either version 3 of the License, or
                                    (at your option) any later version.

                                    TrenchBroom is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "NotifierConnection.h"
#include "View/Inspector/InspectorToolSection.h"
#include <QWidget>

#include <QPointer>

#include <memory>
#include <vector>



namespace TrenchBroom {
namespace Assets {
class Texture;
}

namespace View {

class FaceAttribsEditor;

class GLContextManager;

class MapDocument;

class SwitchableTitledPanel;
class TextureBrowser;
class TitledPanel;

class FaceInspector : public QWidget {
  Q_OBJECT
  private:
    std::weak_ptr<MapDocument> m_document;
    QPointer<TitledPanel> m_faceAttributesPanel;
    QPointer<SwitchableTitledPanel> m_textureBrowserPanel;
    QPointer<FaceAttribsEditor> m_faceAttribsEditor;
    QPointer<TextureBrowser> m_textureBrowser;
    QPointer<QWidget> m_textureBrowserInfo;

    NotifierConnection m_notifierConnection;

  public:
    FaceInspector(std::weak_ptr<MapDocument> document, GLContextManager &contextManager, QWidget *parent = nullptr);

    std::vector<InspectorToolSection> toolSections() const;

    bool cancelMouseDrag();

    void revealTexture(const Assets::Texture *texture);

  private:
    void createGui(GLContextManager &contextManager);

    TitledPanel *createFaceAttribsEditor(GLContextManager &contextManager);

    SwitchableTitledPanel *createTextureBrowser(GLContextManager &contextManager);

    QWidget *createTextureBrowserInfo();

    void textureSelected(const Assets::Texture *texture);

    void connectObservers();

    void documentWasNewedOrOpened(MapDocument *document);
};
} // namespace View
} // namespace TrenchBroom
