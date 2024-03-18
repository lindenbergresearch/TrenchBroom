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

#include "PopupButton.h"

#include <QHBoxLayout>
#include <QToolButton>
#include <QWindow>

#include "IO/ResourceUtils.h"
#include "View/PopupWindow.h"
#include "View/QtUtils.h"

namespace TrenchBroom {
namespace View {
PopupButton::PopupButton(const QString &caption, QWidget *parent, const QIcon &icon) : QWidget(parent) {
  m_button = new QToolButton();
  m_button->setText(caption);
  m_button->setCheckable(true);
  m_button->setIconSize(QSize{12, 12});
  makeSubTitle(m_button);
  m_button->setObjectName("toolButton_borderless");
  m_window = new PopupWindow(this);

  auto *sizer = new QHBoxLayout();
  sizer->setContentsMargins(
      LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin
  );
  sizer->addWidget(m_button);
  setLayout(sizer);

  connect(m_button, &QAbstractButton::clicked, this, &PopupButton::buttonClicked);
  connect(
      m_window, &PopupWindow::visibilityChanged, this, &PopupButton::popupVisibilityChanged
  );
}

QWidget *PopupButton::GetPopupWindow() const {
  return m_window;
}

void PopupButton::buttonClicked(bool checked) {
  if (checked) {
    // TODO: unfortunately it seems like we need to show the popup first, before
    // m_window->size() contains useful data, and we need the size to position the popup.
    // This show() puts the window at (0, 0) on Ubuntu, but positionTouchingWidget() is
    // able to move it without any flicker. Need to confirm on other OS'es.
    m_window->show();
    m_window->positionTouchingWidget(this);
  } else {
    m_window->close();
  }
}

void PopupButton::popupVisibilityChanged(bool visible) {
  m_button->setChecked(visible);
  m_button->setIcon(visible ? checkedIcon : unCheckedIcon);
}

void PopupButton::setIcon(const std::string &iconName) {

  checkedIcon = IO::loadSVGIcon(iconName + "_on.svg", 12);
  unCheckedIcon = IO::loadSVGIcon(iconName + "_off.svg", 12);

  if (! unCheckedIcon.isNull()) {
    m_button->setIcon(unCheckedIcon);
  }

  m_button->setToolButtonStyle(Qt::ToolButtonStyle::ToolButtonTextBesideIcon);
  m_button->setLayoutDirection(Qt::RightToLeft);
}
} // namespace View
} // namespace TrenchBroom
