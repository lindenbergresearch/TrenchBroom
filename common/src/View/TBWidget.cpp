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

#include "TBWidget.h"
#include <QStyleOption>
#include <QPainter>

namespace TrenchBroom {
namespace View {

void TBWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    QStyleOption o;
    o.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &o, &p, this);
}

TBWidget::TBWidget(QWidget *parent, const Qt::WindowFlags &f) : QWidget(parent, f) {}

TBWidget::TBWidget(QWidgetPrivate &d, QWidget *parent, const Qt::WindowFlags &f) : QWidget(d, parent, f) {}


}
}
