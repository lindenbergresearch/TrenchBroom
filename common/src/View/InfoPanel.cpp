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

#include "InfoPanel.h"

#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

#include "View/BorderLine.h"
#include "View/Console.h"
#include "View/IssueBrowser.h"
#include "View/QtUtils.h"

namespace TrenchBroom {
namespace View {
InfoPanel::InfoPanel(std::weak_ptr<MapDocument> document, QWidget *parent) : QWidget(parent), m_console(nullptr), m_issueBrowser(nullptr) {
    m_console = new Console();
    m_issueBrowser = new IssueBrowser(document);

    m_tabs = new QTabWidget(this);
    m_tabs->tabBar()->setObjectName("InfoPanelTabWidget");
    makeSubTitle(m_tabs);
    m_tabs->addTab(m_console, "Console");
    m_tabs->addTab(m_issueBrowser, "Issues");
    m_tabs->setTabPosition(QTabWidget::TabPosition::North);

    auto *sizer = new QVBoxLayout();
    sizer->setContentsMargins(
        0, LayoutConstants::MediumVMargin, 0, LayoutConstants::NoMargin);
    sizer->addWidget(m_tabs);
    sizer->addWidget(new BorderLine());
    setLayout(sizer);
}

Console *InfoPanel::console() const {
    return m_console;
}
}// namespace View
}// namespace TrenchBroom
