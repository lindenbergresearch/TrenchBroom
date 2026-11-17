/*
 Copyright (C) 2010-2014 Kristian Duske

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

#include "Splitter.h"

#include <QPaintEvent>
#include <QPainter>

#include "ActionContext.h"
#include "Logger.h"

namespace TrenchBroom::View {
SplitterHandle::SplitterHandle(const Qt::Orientation orientation, QSplitter *parent) : QSplitterHandle(orientation, parent) {}

QSize SplitterHandle::sizeHint() const {
    return QSize(3, 3);
}

void SplitterHandle::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setPen(Qt::SolidLine);
    painter.fillRect(event->rect(), QBrush(palette().color(QPalette::Midlight)));
}

Splitter::Splitter(const Qt::Orientation orientation, QWidget *parent)
    : QSplitter(orientation, parent) {
#ifdef __APPLE__
    connect(this, &QSplitter::splitterMoved, this, &Splitter::doSplitterMoved);
#endif
}

Splitter::Splitter(QWidget *parent)
    : QSplitter(parent) {
#ifdef __APPLE__
    connect(this, &QSplitter::splitterMoved, this, &Splitter::doSplitterMoved);
#endif
}


static std::string getName(const Splitter::Face &face) {
    if (face == Splitter::Face::Any) return "Any";
    if (face == Splitter::Face::First) return "First";
    if (face == Splitter::Face::Second) return "Second";

    return "?";
}

void Splitter::collapse(const Face face) {
    defaultQtLogger.trace() << "called: before collapse(" << getName(face) << ")" << " - isCollapsed() = " << isCollapsed();
    // if any already collapsed do return
    if (isCollapsed())
        return;

    saveSize();
    sizes()[face] = 0;
    sizes()[(face + 1) % 2] = 9999;

    defaultQtLogger.trace() << "called: after collapse() " << " size(" << getName(face) << ") is " << sizes()[face] << " size(" << getName(
        static_cast<Face>((face + 1) % 2)
    ) << ") is " << sizes()[(face + 1) % 2];
}

void Splitter::expand() const {
    defaultQtLogger.trace() << "called: expand() - isCollapsed() = " << isCollapsed();
    if (!isCollapsed()) return;
    restoreSize();
}

bool Splitter::isCollapsed(const Face face) const {
    if (sizes().size() < 2)
        return false;

    // if no index was provided, just return the information
    // if ANY of them is collapsed
    if (face == Any) {
        defaultQtLogger.trace() << "isCollapsed(Any) = " << (sizes()[0] <= 0 || sizes()[1] <= 0);
        return sizes()[0] <= 0 || sizes()[1] <= 0;
    }

    defaultQtLogger.trace() << "isCollapsed(" << getName(face) << ") = " << (sizes()[face] <= 0);

    return sizes()[face] <= 0;
}

QSplitterHandle *Splitter::createHandle() {
    return new SplitterHandle(orientation(), this);
}

void Splitter::saveSize() {
    defaultQtLogger.trace() << "save sizes: First =" << sizes()[0] << " Second =" << sizes()[1];
    storedSizes[0] = sizes()[0];
    storedSizes[1] = sizes()[1];
}

void Splitter::restoreSize() const {
    defaultQtLogger.trace() << "restore sizes: First =" << storedSizes[0] << " Second =" << storedSizes[1];
    sizes()[0] = storedSizes[0];
    sizes()[1] = storedSizes[1];
    defaultQtLogger.trace() << "restored sizes: First =" << sizes()[0] << " Second =" << sizes()[1];
}

#ifdef __APPLE__
void Splitter::doSplitterMoved() const {
    for (auto i = 0; i < count(); ++i) {
        auto *widget = this->widget(i);
        widget->repaint();
    }
}
#endif
}
