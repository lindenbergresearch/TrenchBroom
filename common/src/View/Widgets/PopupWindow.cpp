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

#include "View/Widgets/PopupWindow.h"

#include <QApplication>
#include <QDebug>
#include <QWindow>
#include <QScreen>
#include "Preferences.h"
#include "PreferenceManager.h"
#include <algorithm>

namespace TrenchBroom {
namespace View {
// PopupWindow

PopupWindow::PopupWindow(QWidget *parent) : QWidget(parent, Qt::Popup) {
}

void PopupWindow::positionTouchingWidget(QWidget *refWidget) {
    const QRect screenGeom = refWidget->screen()->availableGeometry();
    const QRect refWidgetRectOnScreen = QRect(refWidget->mapToGlobal(QPoint(0, 0)), refWidget->size());
    const QSize ourSize = size();

    const auto bounded = ourSize.boundedTo(screenGeom.size());
    const auto desiredY = refWidgetRectOnScreen.bottom() + bounded.height() <= screenGeom.bottom() ? refWidgetRectOnScreen.bottom() + 1 : refWidgetRectOnScreen.top() - bounded.height();
    const auto x = std::clamp(refWidgetRectOnScreen.right() - bounded.width() + 1, screenGeom.left(), screenGeom.right() - bounded.width() + 1);
    const auto y = std::clamp(desiredY, screenGeom.top(), screenGeom.bottom() - bounded.height() + 1);
    // Popup windows use screen coordinates, including monitors left/above the primary screen.
    setGeometry(QRect(QPoint(x, y), bounded));
}

void PopupWindow::closeEvent(QCloseEvent *) {
    emit visibilityChanged(false);
}

void PopupWindow::showEvent(QShowEvent *) {
    if (objectName() == "ViewDetailsPopup") {
        const auto available = screen()->availableGeometry().size() - QSize(32, 32);
        resize(QSize(std::clamp(pref(Preferences::ViewDetailsWindowWidth), 480, 3840), std::clamp(pref(Preferences::ViewDetailsWindowHeight), 360, 2160)).boundedTo(available));
    }
    emit visibilityChanged(true);
}
} // namespace View
} // namespace TrenchBroom
