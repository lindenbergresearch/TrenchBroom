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

#include <QStyleOptionTab>
#include <QStylePainter>
#include <QTabWidget>

namespace TrenchBroom {
namespace View {

class AColoredTabWidget : public QTabBar {
public:
    AColoredTabWidget(QWidget *parent = 0);

protected:
    void paintEvent(QPaintEvent */*event*/);


private:
    QHash<QString, QColor> mColors;
};

class TabWidget : public QTabWidget {
public:
    TabWidget(QWidget *parent = 0);
};
}
}
