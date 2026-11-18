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

#include "View/Widgets/TitledPanel.h"
#include <QVBoxLayout>
#include "View/Widgets/TitleBar.h"

namespace TrenchBroom::View {
TitledPanel::TitledPanel(const QString& title, QWidget* parent, const bool boldTitle, const bool subtitle, const bool fillBackground)
    : QWidget{parent}, m_titleBar{nullptr}, m_panel{nullptr} {
    m_titleBar = new TitleBar(title, boldTitle, subtitle);
    m_panel = new QWidget();

    if (fillBackground) {
        m_panel->setAttribute(Qt::WA_StyledBackground);
        m_panel->setProperty("uiSurface", "window");
    }

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_titleBar);
    layout->addWidget(m_panel, Qt::AlignmentFlag::AlignRight);
    setLayout(layout);
}

TitledPanel::TitledPanel(const QString& title, const bool boldTitle, const bool subtitle)
    : TitledPanel(title, nullptr, boldTitle, subtitle) {}

TitleBar* TitledPanel::getTitleBar() const {
    return m_titleBar;
}

QWidget* TitledPanel::getPanel() const {
    return m_panel;
}
}
