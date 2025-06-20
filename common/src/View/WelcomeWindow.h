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

#include <QMainWindow>

#include <filesystem>
#include "macos_utils.h"

class QPushButton;

namespace TrenchBroom {
namespace View {
class RecentDocumentListBox;

class WelcomeWindow : public QMainWindow {
  Q_OBJECT
  private:
    RecentDocumentListBox *m_recentDocumentListBox;
    QPushButton *m_createNewDocumentButton;
    QPushButton *m_openOtherDocumentButton;
    QPushButton *m_openSettingsButton;
    QPushButton *m_quitApplicationButton;

  public:
    WelcomeWindow();

  private:
    void createGui();

    QWidget *createAppPanel();

  private slots:

    void createNewDocument();

    void openOtherDocument();

    void quitApplication();

    void openSettings();

    void openDocument(const std::filesystem::path &path);

  protected:
    void paintEvent(QPaintEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

  private:
    bool m_dragging = false;
    QPoint m_dragPosition;
};
} // namespace View
} // namespace TrenchBroom
