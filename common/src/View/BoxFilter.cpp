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

#include "BoxFilter.h"

namespace TrenchBroom::View {
QPointF BoxFilter::average() {
    int c = 0;
    QPointF sum = QPointF(0, 0);

    for (QPointF *item : samples) {
        if (item != nullptr) {
            sum = QPointF(sum.x() + item->x(), sum.y() + item->y());
            c++;
        }
    }

    return c == 0 ? QPoint(0, 0) : QPointF(sum.x() / c, sum.y() / c);
}

void BoxFilter::reset() {
    samples.clear();
    for (size_t i = 0; i < m_size; ++i) {
        samples.push_back(nullptr);
    }
}

void BoxFilter::add(QPointF *point) {
    if (++m_index >= m_length) {
        m_index = 0;
    }

    if (samples[m_index] != nullptr)
        delete samples[m_index];

    samples[m_index] = point;
}

}
