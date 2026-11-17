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
#include "View/DrawShapeToolExtension.h"
#include <QWidget>
#include <functional>

namespace TrenchBroom::View {
class DrawShapeToolExtensionPage : public QWidget {
    Q_OBJECT
    NotifierConnection m_connections;
    std::function<void()> m_refresh;

  public:
    DrawShapeToolExtensionPage(DrawShapeKind kind, DrawShapeToolParameters& parameters, QWidget* parent = nullptr);
};
} // namespace TrenchBroom::View
