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

#include "View/Dialogs/CrashDialog.h"

#include <QDialogButtonBox>
#include <QFontInfo>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>

#include "IO/PathQt.h"
#include "IO/ResourceUtils.h"
#include "View/Dialogs/DialogHeader.h"
#include "View/Widgets/FormWithSectionsLayout.h"
#include "View/App/GetVersion.h"
#include "View/Theme/QtUtils.h"
#include "View/Theme/ViewConstants.h"

namespace TrenchBroom::View {

static const char *const EmptyPlaceholder = "-";

CrashDialog::CrashDialog(CrashReport *crashReport) : QDialog{}, m_crashReport(crashReport) {
    createGui();
}

void CrashDialog::createGui() {
    setWindowTitle(qAppName());

    auto icon = IO::loadSVGIcon("bomb.svg", 32);
    auto *header = new DialogHeader{"Crash Report", icon.pixmap(32, 32), this};

    if (!m_crashReport) {
        QMessageBox::critical(this, "TrenchBroom", "Unable to resolve crash report data.", QMessageBox::Abort);
        return;
    }

    auto *text1 = new QLabel{"TrenchBroom has crashed, but was able to save a crash report."};
    text1->setWordWrap(true);
    text1->setContentsMargins(
        LayoutConstants::WideHMargin + 2,
        LayoutConstants::MediumVMargin,
        LayoutConstants::MediumHMargin,
        LayoutConstants::MediumVMargin
    );

    setTextStyleRole(text1,"subtitle");


    auto *reasonText = new QLabel{QString::fromStdString(m_crashReport->signalType.toString())};
    makeError(makeEmphasized(reasonText));

    QString buildInfo = getBuildVersion() + " | " + getBuildIdStr() + " | " + getBuildType();
    auto *buildText = new QLabel{buildInfo};

    auto *reportPathText = new QLabel{IO::pathAsQString(m_crashReport->reportPath, EmptyPlaceholder)};
    auto *logPathText = new QLabel{IO::pathAsQString(m_crashReport->logPath, EmptyPlaceholder)};
    auto *mapPathText = new QLabel{IO::pathAsQString(m_crashReport->mapPath, EmptyPlaceholder)};

    auto *reportLayout = new FormWithSectionsLayout{};
    reportLayout->setContentsMargins(0, LayoutConstants::MediumVMargin, 0, LayoutConstants::MediumVMargin);
    reportLayout->setVerticalSpacing(0);

    reportLayout->addSection("Details");
    reportLayout->addRow("Reason", makeSmall(makeMono(reasonText)));
    reportLayout->addRow("Build", makeSmall(makeMono(buildText)));

    reportLayout->addSection("Files");
    reportLayout->addRow("Crash Report", makeSmall(makeMono(reportPathText)));
    reportLayout->addRow("Log File", makeSmall(makeMono(logPathText)));
    reportLayout->addRow("Map File", makeSmall(makeMono(mapPathText)));

    createInfoTab();

    auto *tabLayout = new QVBoxLayout{};
    tabLayout->setContentsMargins(LayoutConstants::WideMargins());
    tabLayout->addWidget(m_tabs);

    auto *buttonBox = new QDialogButtonBox{};
    buttonBox->addButton(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *outerLayout = new QVBoxLayout{};
    outerLayout->setContentsMargins(LayoutConstants::NoMargins());
    outerLayout->addWidget(header);
    outerLayout->addWidget(text1);
    outerLayout->addLayout(reportLayout);
    outerLayout->addLayout(tabLayout, 1);
    outerLayout->addLayout(wrapDialogButtonBox(buttonBox));

    setLayout(outerLayout);
}

void CrashDialog::createInfoTab() {
    m_tabs = new QTabWidget{};

    m_tabs->setMinimumWidth(900);
    m_tabs->setMinimumHeight(350);
    m_tabs->setContentsMargins(LayoutConstants::NoMargins());

    m_tabs->addTab(createStackTraceView(), "StackTrace");
    m_tabs->addTab(createLogView(), "Logfile");
}

QWidget *CrashDialog::createLogView() {
    m_logFileView = new QTextEdit{};
    m_logFileView->setReadOnly(true);
    m_logFileView->setWordWrapMode(QTextOption::NoWrap);
    m_logFileView->setObjectName("CrashLogView");
    makeMono(m_logFileView);

    m_logFileView->setText(LogMessageCache::dump());

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(LayoutConstants::NoMargins());
    layout->addWidget(m_logFileView);

    QWidget *wrapper = new QWidget{this};
    wrapper->setLayout(layout);

    return wrapper;
}

QWidget *CrashDialog::createStackTraceView() {
    m_stackTraceView = new QTextEdit{};
    m_stackTraceView->setReadOnly(true);
    m_stackTraceView->setWordWrapMode(QTextOption::NoWrap);
    makeMono(m_stackTraceView);
    m_stackTraceView->setObjectName("CrashStackTraceView");

    m_stackTraceView->setText(m_crashReport->stackTrace);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(LayoutConstants::NoMargins());
    layout->addWidget(m_stackTraceView);

    QWidget *wrapper = new QWidget{this};
    wrapper->setLayout(layout);

    return wrapper;
}
} // namespace TrenchBroom::View
