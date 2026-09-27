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

namespace TrenchBroom::View {
struct BoxFilter {
    std::vector<QPointF *> samples;
    size_t m_size, m_length;
    size_t m_index;

    explicit BoxFilter(const size_t size, const size_t length = 0) : m_size(size) {
        samples.resize(m_size);
        m_index = 0;

        m_length = length <= 0 ? size : length;
        reset();
    }

    QPointF average() const;

    void reset();

    void add(QPointF* point);
};
}
