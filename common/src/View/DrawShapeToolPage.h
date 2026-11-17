/*
 Copyright (C) 2023 Kristian Duske

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
#include <QWidget>
#include <memory>
class QComboBox;
class QPushButton;
class QStackedWidget;
class QToolButton;
class QLabel;
namespace TrenchBroom::View {
class DrawShapeToolExtensionManager;
class MapDocument;
class DrawShapeToolPage : public QWidget {
    Q_OBJECT
    DrawShapeToolExtensionManager& m_manager;
    QWidget* m_quickMove = nullptr;
    QToolButton* m_shapes = nullptr;
    QToolButton* m_group = nullptr;
    QComboBox* m_axis = nullptr;
    QLabel* m_axisLabel = nullptr;
    QPushButton* m_apply = nullptr;
    QStackedWidget* m_pages = nullptr;
    NotifierConnection m_connections;

  public:
    DrawShapeToolPage(std::weak_ptr<MapDocument> document, DrawShapeToolExtensionManager& manager, QWidget* parent = nullptr);

  private:
    void refresh();
};
} // namespace TrenchBroom::View
