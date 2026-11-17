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
SplitterHandle::SplitterHandle(const Qt::Orientation orientation, QSplitter* parent) : QSplitterHandle(orientation, parent) {}

QSize SplitterHandle::sizeHint() const {
    return QSplitterHandle::sizeHint();
}

void SplitterHandle::paintEvent(QPaintEvent* event) {
    QSplitterHandle::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_gripColor.isValid() ? m_gripColor : palette().color(QPalette::Mid));
    const auto center = QPointF(rect().center());
    for (int i = -1; i <= 1; ++i) {
        const auto offset = orientation() == Qt::Horizontal ? QPointF(0, i * m_gripSpacing) : QPointF(i * m_gripSpacing, 0);
        painter.drawEllipse(center + offset, m_gripDotSize * 0.5, m_gripDotSize * 0.5);
    }
}

Splitter::Splitter(const Qt::Orientation orientation, QWidget* parent) : QSplitter(orientation, parent) {
#ifdef __APPLE__
    connect(this, &QSplitter::splitterMoved, this, &Splitter::doSplitterMoved);
#endif
}

Splitter::Splitter(QWidget* parent) : QSplitter(parent) {
#ifdef __APPLE__
    connect(this, &QSplitter::splitterMoved, this, &Splitter::doSplitterMoved);
#endif
}

static std::string getName(const Splitter::Face& face) {
    if (face == Splitter::Face::Any) return "Any";
    if (face == Splitter::Face::First) return "First";
    if (face == Splitter::Face::Second) return "Second";

    return "?";
}

void Splitter::collapse(const Face face) {
    if (count() != 2 || (face != First && face != Second) || isCollapsed()) return;
    saveSize();
    auto values = sizes();
    values[(face + 1) % 2] += values[face];
    values[face] = 0;
    setSizes(values);
}

void Splitter::expand() {
    if (isCollapsed()) restoreSize();
}

bool Splitter::isCollapsed(const Face face) const {
    if (sizes().size() < 2) return false;

    // if no index was provided, just return the information
    // if ANY of them is collapsed
    if (face == Any) {
        defaultQtLogger.trace() << "isCollapsed(Any) = " << (sizes()[0] <= 0 || sizes()[1] <= 0);
        return sizes()[0] <= 0 || sizes()[1] <= 0;
    }

    defaultQtLogger.trace() << "isCollapsed(" << getName(face) << ") = " << (sizes()[face] <= 0);

    return sizes()[face] <= 0;
}

QSplitterHandle* Splitter::createHandle() {
    return new SplitterHandle(orientation(), this);
}

void Splitter::saveSize() {
    storedSizes = sizes();
}

void Splitter::restoreSize() {
    if (storedSizes.size() == count()) setSizes(storedSizes);
}

#ifdef __APPLE__
void Splitter::doSplitterMoved() const {
    for (auto i = 0; i < count(); ++i) {
        auto* widget = this->widget(i);
        widget->repaint();
    }
}
#endif
} // namespace TrenchBroom::View
