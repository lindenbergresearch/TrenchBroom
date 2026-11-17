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

#include <QWidget>

#include <memory>

namespace TrenchBroom::View {
class FaceInspector;
class GLContextManager;
class MapDocument;
class MapView;

class Inspector : public QWidget {
    Q_OBJECT

    QWidget* m_leftPane;
    FaceInspector* m_faceInspector;
    bool m_panelsVisible = true;
    class ToolLayout;
    std::unique_ptr<ToolLayout> m_toolLayout;

public:
    Inspector(const std::weak_ptr<MapDocument>& document, GLContextManager& contextManager, MapView* mapView, QWidget* parent = nullptr);
    ~Inspector() override;

    void dockToolToSide(const QString& id, bool left, int row = -1);
    void tabifyTool(const QString& id, const QString& targetId, int index = -1);
    void floatTool(const QString& id, const QPoint& position);
    void revealTool(const QString& id);
    void resetToolLayout();
    void saveToolLayout() const;

    QWidget* leftPane() const;
    void setPanelsVisible(bool visible);
    bool panelsVisible() const;
    bool cancelMouseDrag() const;
    FaceInspector* faceInspector() const;
};
}
