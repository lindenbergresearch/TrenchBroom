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

#include "View/Widgets/BorderLine.h"
#include <algorithm>

#include "View/Theme/ViewConstants.h"

namespace TrenchBroom {
namespace View {
    BorderLine::BorderLine(const Direction direction, const int thickness, QWidget* parent) : QFrame(parent), m_direction(direction), m_thickness(std::clamp(thickness, 0, 32)) {
        setObjectName("borderLine");
        setProperty("orientation", direction == Direction::Horizontal ? "horizontal" : "vertical");
        setProperty("separatorWeight", thickness > 1 ? "strong" : "normal");
        setFrameShape(QFrame::NoFrame);
        setSizePolicy(direction == Direction::Horizontal ? QSizePolicy::Expanding : QSizePolicy::Fixed, direction == Direction::Horizontal ? QSizePolicy::Fixed : QSizePolicy::Expanding);
    }
    void BorderLine::setThickness(int thickness) {
        thickness = std::clamp(thickness, 0, 32);
        if (m_thickness == thickness) return;
        m_thickness = thickness;
        updateGeometry();
        update();
    }
    QSize BorderLine::sizeHint() const { return m_direction == Direction::Horizontal ? QSize(0, m_thickness) : QSize(m_thickness, 0); }
} // namespace View
} // namespace TrenchBroom
