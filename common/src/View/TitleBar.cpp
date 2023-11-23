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

#include "TitleBar.h"

#include <QHBoxLayout>
#include <QLabel>

#include "View/ControlListBox.h"
#include "View/QtUtils.h"
#include "View/ViewConstants.h"
#include "View/BorderLine.h"

namespace TrenchBroom {
namespace View {
TitleBar::TitleBar(const QString &title, QWidget *parent, const bool boldTitle, bool subtitle) : QWidget(parent),
    m_titleText(nullptr) {

    setObjectName("TitleBar");
    m_titleText = new QLabel(title);
    m_titleText->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleText->setContentsMargins(
        LayoutConstants::MediumHMargin,
        LayoutConstants::NarrowVMargin,
        LayoutConstants::NoMargin,
        LayoutConstants::NarrowHMargin
    );

    if (!subtitle) {
        setAutoFillBackground(true);
        setBackgroundRole(QPalette::Midlight);
        setForegroundRole(QPalette::Text);
    }
    // Tell ControlListBox to not update the title label's color when the selection changes,
    // in case this widget is used inside of a ControlListBox.
    m_titleText->setProperty(ControlListBox::LabelColorShouldNotUpdateWhenSelected, true);

    if (boldTitle) {
        makePanelTitle(m_titleText, false, subtitle);
    } else {
        makePanelTitle(m_titleText, true, subtitle);
    }

    auto *layout = new QHBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_titleText, 1);
    setLayout(layout);
    setMinimumHeight(24);
}

TitleBar::TitleBar(const QString &title, const bool boldTitle, bool subtitle)
    : TitleBar(title, nullptr, boldTitle, subtitle) {
}
} // namespace View
} // namespace TrenchBroom
