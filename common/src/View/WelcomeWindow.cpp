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

#include "WelcomeWindow.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QPushButton>

#include "IO/PathQt.h"
#include "TrenchBroomApp.h"
#include "View/AppInfoPanel.h"
#include "View/BorderLine.h"
#include "View/QtUtils.h"
#include "View/RecentDocumentListBox.h"
#include "View/TitleBar.h"
#include "View/TitledPanel.h"
#include "View/ViewConstants.h"

namespace TrenchBroom::View {
WelcomeWindow::WelcomeWindow() :
    QMainWindow{nullptr, Qt::Window}, m_recentDocumentListBox{nullptr}, m_createNewDocumentButton{nullptr}, m_openOtherDocumentButton{nullptr} {
  createGui();
}

void WelcomeWindow::createGui() {
  setWindowIconTB(this);
  setWindowTitle("Welcome to TrenchBroom");
  setWindowFlags(Qt::WindowStaysOnTopHint);
  setWindowModality(Qt::ApplicationModal);

  m_recentDocumentListBox = new RecentDocumentListBox{};
  m_recentDocumentListBox->setToolTip("Double-click on a file to open it");
  //  m_recentDocumentListBox->setSizePolicy(QSizePolicy::Expanding,
  //  QSizePolicy::Expanding);

  connect(
      m_recentDocumentListBox, &RecentDocumentListBox::loadRecentDocument, this, &WelcomeWindow::openDocument
  );

  auto panelLayout = new QVBoxLayout;
  panelLayout->setContentsMargins(0, 0, 0, 0);
  panelLayout->setSpacing(0);
  panelLayout->addWidget(m_recentDocumentListBox);

  auto *innerLayout = new QHBoxLayout{};
  innerLayout->setContentsMargins(QMargins{});
  innerLayout->setSpacing(0);

  auto *appPanel = createAppPanel();

  innerLayout->addWidget(appPanel);
  innerLayout->addLayout(panelLayout);

  auto *container = new QWidget{};
  auto *outerLayout = new QVBoxLayout{};
  outerLayout->setContentsMargins(QMargins{});
  outerLayout->setSpacing(0);

  outerLayout->addLayout(innerLayout);
  insertTitleBarSeparator(outerLayout);

  container->setLayout(outerLayout);

  setCentralWidget(container);
  setMinimumSize(800, 500);
  setFocus();

}

QWidget *WelcomeWindow::createAppPanel() {
  auto *appPanel = new QWidget{};
  auto *infoPanel = new AppInfoPanel{appPanel};

  m_createNewDocumentButton = new QPushButton{"New..."};
  m_createNewDocumentButton->setToolTip("Create a new map document.");
  m_openOtherDocumentButton = new QPushButton{"Open..."};
  m_openOtherDocumentButton->setToolTip("Open an existing map document.");

  m_openSettingsButton = new QPushButton{"Settings"};
  m_openSettingsButton->setToolTip("Setup Trenchbroom.");
  m_quitApplicationButton = new QPushButton{"Quit"};
  m_quitApplicationButton->setToolTip("Quit Trenchbroom.");

  connect(
      m_createNewDocumentButton, &QPushButton::clicked, this, &WelcomeWindow::createNewDocument
  );
  connect(
      m_openOtherDocumentButton, &QPushButton::clicked, this, &WelcomeWindow::openOtherDocument
  );
  connect(
      m_quitApplicationButton, &QPushButton::clicked, this, &WelcomeWindow::quitApplication
  );
  connect(
      m_openSettingsButton, &QPushButton::clicked, this, &WelcomeWindow::openSettings
  );

  auto *buttonLayout = new QHBoxLayout{};
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->setSpacing(LayoutConstants::NarrowHMargin);
  buttonLayout->addStretch();
  buttonLayout->addWidget(m_createNewDocumentButton);
  buttonLayout->addWidget(m_openOtherDocumentButton);
  buttonLayout->addWidget(m_openSettingsButton);
  buttonLayout->addWidget(m_quitApplicationButton);
  buttonLayout->addStretch();

  auto *outerLayout = new QVBoxLayout{};
  outerLayout->setContentsMargins(0, 0, 0, 0);
  outerLayout->setSpacing(0);
  outerLayout->addWidget(infoPanel);
  outerLayout->addSpacing(20);
  outerLayout->addLayout(buttonLayout);
  outerLayout->addSpacing(20);
  appPanel->setLayout(outerLayout);
  appPanel->setFixedWidth(400);

  return appPanel;
}

void WelcomeWindow::createNewDocument() {
  auto &app = TrenchBroomApp::instance();
  if (! app.newDocument()) {
    show();
  }
}

void WelcomeWindow::openOtherDocument() {
  const auto pathStr = QFileDialog::getOpenFileName(
      nullptr, tr("Open Map"), fileDialogDefaultDirectory(FileDialogDir::Map), "Map files (*.map);;Any files (*.*)"
  );
  const auto path = IO::pathFromQString(pathStr);

  if (! path.empty()) {
    updateFileDialogDefaultDirectoryWithFilename(FileDialogDir::Map, pathStr);
    openDocument(path);
  }
}

void WelcomeWindow::openDocument(const std::filesystem::path &path) {
  auto &app = TrenchBroomApp::instance();
  if (! app.openDocument(path)) {
    show();
  }
}

void WelcomeWindow::quitApplication() {
  auto &app = TrenchBroomApp::instance();
  app.quit();
}

void WelcomeWindow::openSettings() {
  auto &app = TrenchBroomApp::instance();
  app.openPreferences();
}
} // namespace TrenchBroom::View
