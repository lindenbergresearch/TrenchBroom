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
#include "AColoredTabWidget.h"
#include "Preferences.h"
#include "PreferenceManager.h"
#include "QtUtils.h"

void TrenchBroom::View::AColoredTabWidget::paintEvent(QPaintEvent *) {

    QStylePainter painter(this);
    QStyleOptionTab opt;

    for (int i = 0; i < count(); i++) {
        initStyleOption(&opt, i);

        auto index = this->currentIndex();
        auto pal = QPalette{};
        auto buttonColor = pal.color(QPalette::Button);
        auto windowText = pal.color(QPalette::WindowText);

        if (index == i) {
            if (pref(Preferences::Theme) == Preferences::darkTheme()) {
                opt.palette.setColor(QPalette::Button, buttonColor.lighter(100));
                opt.palette.setColor(QPalette::WindowText, windowText.lighter(130));
            }
            else {
                opt.palette.setColor(QPalette::Button, buttonColor.darker(130));
                opt.palette.setColor(QPalette::WindowText, windowText.darker(130));
            }
        }
        else {
            if (pref(Preferences::Theme) == Preferences::darkTheme()) {
                opt.palette.setColor(QPalette::Button, buttonColor.darker(120));
                opt.palette.setColor(QPalette::WindowText, windowText.darker(120));
            }
            else {
                opt.palette.setColor(QPalette::Button, buttonColor.lighter(140));
                opt.palette.setColor(QPalette::WindowText, windowText.lighter(170));
            }
        }

        painter.drawControl(QStyle::CE_TabBarTabShape, opt);
        painter.drawControl(QStyle::CE_TabBarTab, opt);
        painter.drawControl(QStyle::CE_TabBarTabLabel, opt);
    }
}

TrenchBroom::View::AColoredTabWidget::AColoredTabWidget(QWidget *parent) : QTabBar(parent) {
}

TrenchBroom::View::TabWidget::TabWidget(QWidget *parent) : QTabWidget(parent) {

    // text - color
    QHash<QString, QColor> dict;

    dict["All"] = QColor("yellow");
    dict["purchase"] = QColor("#87ceeb");
    dict["POS Sales"] = QColor("#90EE90");
    dict["Cash Sales"] = QColor("pink");
    dict["invoice"] = QColor("#800080");

    setTabBar(new AColoredTabWidget());
}
