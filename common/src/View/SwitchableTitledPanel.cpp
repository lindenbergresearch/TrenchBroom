/*
 Copyright (C) 2024 Kristian Duske

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

#include "SwitchableTitledPanel.h"

#include <QBoxLayout>
#include <QStackedLayout>

#include "View/QtUtils.h"
#include "View/ViewConstants.h"

namespace TrenchBroom::View {

SwitchableTitledPanel::SwitchableTitledPanel(const QString& title, QWidget* parent)
    : QWidget(parent)
    , m_titleBar(new TitleBar(title))
    , m_stackedLayout(new QStackedLayout())
    , m_panels{new QWidget(), new QWidget()} {
    for (auto* panel : m_panels) m_stackedLayout->addWidget(panel);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_titleBar);
    layout->addLayout(m_stackedLayout, 1);
}

QWidget* SwitchableTitledPanel::getPanel(size_t index) const {
    return m_panels.at(index);
}

size_t SwitchableTitledPanel::currentIndex() const {
    return size_t(m_stackedLayout->currentIndex());
}

TitleBar* SwitchableTitledPanel::getTitleBar() const {
    return m_titleBar;
}

void SwitchableTitledPanel::setCurrentIndex(size_t index) {
    m_stackedLayout->setCurrentIndex(int(index));
}

QByteArray SwitchableTitledPanel::saveState() const {
    auto result = QByteArray{};
    auto stream = QDataStream{&result, QIODevice::WriteOnly};
    stream << int(currentIndex());
    return result;
}

bool SwitchableTitledPanel::restoreState(const QByteArray &state) {
    auto stream = QDataStream{state};
    int currentIndex;
    stream >> currentIndex;

    if (stream.status() == QDataStream::Ok && currentIndex >= 0 && currentIndex < 2) {
        setCurrentIndex(size_t(currentIndex));
        return true;
    }

    return false;
}

} // namespace TrenchBroom::View
