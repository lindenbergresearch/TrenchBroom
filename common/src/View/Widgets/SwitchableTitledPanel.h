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

#include "View/Widgets/TitleBar.h"

#include <array>

class QStackedLayout;

namespace TrenchBroom::View {
class SwitchableTitledPanel : public QWidget {
  Q_OBJECT
  private:
    TitleBar* m_titleBar;
    QStackedLayout* m_stackedLayout;
    std::array<QWidget*, 2> m_panels;

  public:
    explicit SwitchableTitledPanel(const QString &title, QWidget *parent = nullptr);

    QWidget *getPanel(size_t index) const;

    size_t currentIndex() const;
    TitleBar* getTitleBar() const;

    void setCurrentIndex(size_t index);

    QByteArray saveState() const;

    bool restoreState(const QByteArray &state);
};

} // namespace TrenchBroom::View
