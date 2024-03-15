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

#include "AppInfoPanel.h"

#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QString>
#include <QStringBuilder>
#include <QVBoxLayout>

#include "IO/ResourceUtils.h"
#include "View/BorderLine.h"
#include "View/ClickableLabel.h"
#include "View/GetVersion.h"
#include "View/QtUtils.h"

namespace TrenchBroom::View {
AppInfoPanel::AppInfoPanel(QWidget *parent)
    : QWidget{parent} {
  createGui();
}

void AppInfoPanel::createGui() {
  QPixmap appIconImage = IO::loadPixmapResource("AppIcon_Glow.png");
  QLabel *appIcon = new QLabel{};
  appIcon->setPixmap(appIconImage);

  QLabel *appName = new QLabel{tr("TrenchBroom Nova")};
  makeHeader(appName);
  makeItalic(appName);
  appName->setForegroundRole(QPalette::HighlightedText);

  BorderLine *appLine = new BorderLine{BorderLine::Direction::Horizontal};
  appLine->setLineWidth(3);
  QLabel *appClaim = new QLabel{tr("Level Editor")};
  makeEmphasized(appClaim);

  QLabel *appHint = new QLabel{tr("eXperimental Version")};
  makeEmphasized(appHint);

  ClickableLabel *version = new ClickableLabel{tr("Version ")%getBuildVersion()};
  ClickableLabel *build = new ClickableLabel{tr("Build ")%getBuildIdStr()};
  ClickableLabel *qtVersion =
      new ClickableLabel{tr("Qt ")%QString::fromLocal8Bit(qVersion())};

  makeInfo(version);
  makeInfo(build);
  makeInfo(qtVersion);
  build->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

  const auto tooltip = tr("Click to copy to clipboard");
  version->setToolTip(tooltip);
  build->setToolTip(tooltip);
  qtVersion->setToolTip(tooltip);

  connect(version, &ClickableLabel::clicked, this, &AppInfoPanel::versionInfoClicked);
  connect(build, &ClickableLabel::clicked, this, &AppInfoPanel::versionInfoClicked);
  connect(qtVersion, &ClickableLabel::clicked, this, &AppInfoPanel::versionInfoClicked);

  auto *layout = new QVBoxLayout{};
  layout->setContentsMargins(20, 20, 20, 20);
  layout->setSpacing(LayoutConstants::MediumVMargin);
  layout->addStretch();
  layout->addWidget(appIcon, 0, Qt::AlignHCenter | Qt::AlignVCenter);
  layout->addWidget(appName, 0, Qt::AlignHCenter | Qt::AlignVCenter);
  layout->addWidget(appHint, 0, Qt::AlignHCenter | Qt::AlignVCenter);
  layout->addWidget(appLine);
  layout->addWidget(appClaim, 0, Qt::AlignHCenter | Qt::AlignVCenter);
  layout->addWidget(version, 0, Qt::AlignHCenter | Qt::AlignVCenter);
  layout->addWidget(build, 0, Qt::AlignHCenter | Qt::AlignVCenter);
  layout->addWidget(qtVersion, 0, Qt::AlignHCenter | Qt::AlignVCenter);
  layout->addStretch();

  setLayout(layout);
}

void AppInfoPanel::versionInfoClicked() {
  QClipboard *clipboard = QApplication::clipboard();
  const QString str =
      tr("TrenchBroom ")%getBuildVersion()%tr(" Build ")%getBuildIdStr();
  clipboard->setText(str);
}
} // namespace TrenchBroom::View
