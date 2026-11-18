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

#include "View/Widgets/TitleBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVariant>

namespace TrenchBroom::View {
TitleBar::TitleBar(const QString& title, QWidget* parent, const bool boldTitle, const bool subtitle)
    : TBWidget(parent) {
    setObjectName(subtitle ? "SubTitleBar" : "TitleBar");

    auto* label = new QLabel(title);
    label->setObjectName("TitleBar_m_titleText");
    label->setProperty("emphasized", !boldTitle);

    auto* layout = new QHBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(label, 1);
    setLayout(layout);
}

TitleBar::TitleBar(const QString& title, const bool boldTitle, const bool subtitle) : TitleBar(title, nullptr, boldTitle, subtitle) {}
}
