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

#include <QWidget>

namespace TrenchBroom {
namespace View {

/**
 * Basic Widget for UI components.
 */
class TBWidget : public QWidget {
  Q_OBJECT

  public:
    TBWidget(QWidget *parent, const Qt::WindowFlags &f = Qt::WindowType::Widget);

    TBWidget(QWidgetPrivate &d, QWidget *parent, const Qt::WindowFlags &f);

  protected:
    /**
     * Needs to be implemented to support Qt5 stylesheets:
     * https://stackoverflow.com/questions/18344135/why-do-stylesheets-not-work-when-subclassing-qwidget-and-using-q-object
     *
     * @param event The paint-event.
     */
    void paintEvent(QPaintEvent *event) override;
};

}
}
