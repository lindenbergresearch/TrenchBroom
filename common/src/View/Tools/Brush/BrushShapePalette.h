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

#include "NotifierConnection.h"
#include "View/Tools/Brush/DrawShapeToolExtension.h"
#include <QFrame>

class QGraphicsOpacityEffect;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

namespace TrenchBroom::View {
class MapViewToolBox;

class BrushShapePalette : public QFrame {
    Q_OBJECT
    Q_PROPERTY(int edgeMargin READ edgeMargin WRITE setEdgeMargin)
    Q_PROPERTY(int panelPadding READ panelPadding WRITE setPanelPadding)
    Q_PROPERTY(int buttonSpacing READ buttonSpacing WRITE setButtonSpacing)
    Q_PROPERTY(qreal overlayOpacity READ overlayOpacity WRITE setOverlayOpacity)

    MapViewToolBox& m_tools;
    QWidget* m_contents;
    QScrollArea* m_scroll;
    QVBoxLayout* m_layout;
    QVBoxLayout* m_buttonsLayout;
    QGraphicsOpacityEffect* m_opacity;
    std::vector<std::pair<DrawShapeKind, QToolButton*>> m_buttons;
    NotifierConnection m_connections;
    int m_edgeMargin = 12;
    bool m_placePending = false;

  public:
    BrushShapePalette(MapViewToolBox& tools, QWidget* parent);
    int edgeMargin() const;
    void setEdgeMargin(int margin);
    int panelPadding() const;
    void setPanelPadding(int padding);
    int buttonSpacing() const;
    void setButtonSpacing(int spacing);
    qreal overlayOpacity() const;
    void setOverlayOpacity(qreal opacity);

  protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void refresh();
    void schedulePlace();
    void place();
};
} // namespace TrenchBroom::View
