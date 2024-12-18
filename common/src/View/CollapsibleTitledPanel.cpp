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

#include "CollapsibleTitledPanel.h"

#include <QLabel>
#include <QLayout>

#include "View/BorderLine.h"
#include "View/QtUtils.h"
#include "View/ViewConstants.h"

namespace TrenchBroom {
namespace View {
// CollapsibleTitleBar

CollapsibleTitleBar::CollapsibleTitleBar(const QString &title, QWidget *parent) : TitleBar(title, parent), m_stateText(new QLabel("")) {
    setObjectName("CollapsibleTitleBar");
    m_stateText->setObjectName("CollapsibleTitleBar_m_stateText");
    m_titleText->setObjectName("CollapsibleTitleBar_m_titleText");
    layout()->addWidget(m_stateText);
}

void CollapsibleTitleBar::setStateText(const QString &stateText) {
    m_stateText->setText(stateText);
}

void CollapsibleTitleBar::mousePressEvent(QMouseEvent * /* event */) {
    emit titleBarClicked();
}

// CollapsibleTitledPanel
CollapsibleTitledPanel::CollapsibleTitledPanel(const QString &title, const bool initiallyExpanded, QWidget *parent)
    : QWidget(parent), m_titleBar(new CollapsibleTitleBar(title)), m_panel(new QWidget()), m_expanded(initiallyExpanded) {

    m_panel->setAutoFillBackground(true);
    m_panel->setBackgroundRole(QPalette::Window);

    auto *sizer = new QVBoxLayout();
    sizer->setContentsMargins(0, 0, 0, 0);
    sizer->setSpacing(0);
    sizer->addWidget(m_titleBar, 0);
    sizer->addWidget(m_panel, Qt::AlignmentFlag::AlignRight);
    setLayout(sizer);

    connect(m_titleBar, &CollapsibleTitleBar::titleBarClicked, this, [=]() {
        setExpanded(!m_expanded);
    });

    updateExpanded();
}

QWidget *CollapsibleTitledPanel::getPanel() const {
    return m_panel;
}

void CollapsibleTitledPanel::expand() {
    setExpanded(true);
}

void CollapsibleTitledPanel::collapse() {
    setExpanded(false);
}

bool CollapsibleTitledPanel::expanded() const {
    return m_expanded;
}

void CollapsibleTitledPanel::setExpanded(const bool expanded) {
    if (expanded == m_expanded) {
        return;
    }

    m_expanded = expanded;
    updateExpanded();
}

QByteArray CollapsibleTitledPanel::saveState() const {
    auto result = QByteArray();
    auto stream = QDataStream(&result, QIODevice::WriteOnly);
    stream << m_expanded;
    return result;
}

bool CollapsibleTitledPanel::restoreState(const QByteArray &state) {
    auto stream = QDataStream(state);
    bool expanded;
    stream >> expanded;

    if (stream.status() == QDataStream::Ok) {
        setExpanded(expanded);
        return true;
    }

    return false;
}

void CollapsibleTitledPanel::updateExpanded() {
    if (m_expanded) {
        m_panel->show();
        m_titleBar->setStateText(tr("-"));
        m_titleBar->setToolTip("Hide Panel");
    } else {
        m_panel->hide();
        m_titleBar->setStateText(tr("+"));
        m_titleBar->setToolTip("Show Panel");
    }
}
} // namespace View
} // namespace TrenchBroom
