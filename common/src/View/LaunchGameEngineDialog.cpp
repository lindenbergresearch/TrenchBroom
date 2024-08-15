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

#include "LaunchGameEngineDialog.h"

#include <QCompleter>
#include <QDialogButtonBox>
#include <QDir>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>

#include "EL/EvaluationContext.h"
#include "EL/Interpolator.h"
#include "IO/PathQt.h"
#include "Model/Game.h"
#include "Model/GameConfig.h"
#include "Model/GameEngineProfile.h"
#include "Model/GameFactory.h"
#include "View/BorderLine.h"
#include "View/CompilationVariables.h"
#include "View/CurrentGameIndicator.h"
#include "View/GameEngineDialog.h"
#include "View/GameEngineProfileListBox.h"
#include "View/MapDocument.h"
#include "View/MultiCompletionLineEdit.h"
#include "View/QtUtils.h"
#include "View/VariableStoreModel.h"
#include "View/ViewConstants.h"

#include "kdl/memory_utils.h"
#include "kdl/string_utils.h"

#include <string>

namespace TrenchBroom {
namespace View {
LaunchGameEngineDialog::LaunchGameEngineDialog(std::weak_ptr<MapDocument> document, QWidget *parent) : QDialog{parent}, m_document{std::move(document)} {
    createGui();
};

void LaunchGameEngineDialog::createGui() {
    setWindowIconTB(this);
    setWindowTitle("Launch Engine");

    m_dumpToConsole = true;
    m_commandLine = new ElidedLabel{"", Qt::ElideRight, this};
    makeMono(m_commandLine, font().pointSize());

    auto document = kdl::mem_lock(m_document);
    const auto &gameName = document->game()->gameName();
    auto *gameIndicator = new CurrentGameIndicator{gameName};

    auto *midPanel = new QWidget{this};

    auto &gameFactory = Model::GameFactory::instance();
    const auto &gameConfig = gameFactory.gameConfig(gameName);
    m_config = gameConfig.gameEngineConfig;
    m_gameEngineList = new GameEngineProfileListBox{m_config};
    m_gameEngineList->setEmptyText(R"(Click the 'Configure engines...' button to create a game engine profile.)");
    m_gameEngineList->setMinimumSize(190, 200);
    m_gameEngineList->setMaximumWidth(190);
    m_gameEngineList->setContentsMargins(LayoutConstants::NarrowVMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin);

    auto *header = new QLabel{"Launch Engine"};
    makeTitle(header);

    auto *message = new QLabel{R"(Select a game engine from the list on the right and edit the commandline parameters in the text box below. You can use variables to refer to the map name and other values.)"};
    message->setWordWrap(true);

    auto *openPreferencesButton = new QPushButton{"Configure Engines"};
    openPreferencesButton->setObjectName("LaunchGameEngineDialog_smallPushButton");
    makeSmall(openPreferencesButton);
    auto *parameterLabel = new QLabel{"Command Line Parameter"};
    parameterLabel->setContentsMargins(LayoutConstants::NarrowVMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin);

    m_parameterText = new MultiCompletionLineEdit{};
    m_parameterText->setFont(Fonts::fixedWidthFont());
    m_parameterText->setMultiCompleter(new QCompleter{new VariableStoreModel{variables()}});
    m_parameterText->setWordDelimiters(QRegularExpression{"\\$"}, QRegularExpression{"\\}"});
    //makeBigger(m_parameterText, 1);

    auto *midLeftLayout = new QVBoxLayout{};
    midLeftLayout->setContentsMargins(0, 0, 0, 0);
    midLeftLayout->setSpacing(0);
    midLeftLayout->addSpacing(LayoutConstants::WideHMargin * 2);
    midLeftLayout->addWidget(header);
    midLeftLayout->addSpacing(LayoutConstants::WideHMargin);
    midLeftLayout->addWidget(message);
    midLeftLayout->addSpacing(LayoutConstants::WideHMargin * 3);
    midLeftLayout->addStretch(1);
    midLeftLayout->addWidget(parameterLabel);
    midLeftLayout->addSpacing(LayoutConstants::MediumVMargin);

    m_testButton = new QPushButton{"Test"};
    m_testButton->setObjectName("LaunchGameEngineDialog_TestButton");

    auto *lineEditLayout = new QHBoxLayout{};
    lineEditLayout->setSpacing(LayoutConstants::MediumHMargin);
    lineEditLayout->addWidget(m_parameterText);
    lineEditLayout->addWidget(m_testButton);
    m_parameterText->setMinimumSize(350, 0);


    midLeftLayout->addLayout(lineEditLayout);

    //midLeftLayout->addSpacing(LayoutConstants::MediumVMargin);

    //midLeftLayout->addWidget(m_commandLine);
    midLeftLayout->addSpacing(LayoutConstants::WideHMargin * 4);

    auto *midRightLayout = new QVBoxLayout{};
    midRightLayout->setContentsMargins(LayoutConstants::NarrowVMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowHMargin);
    midRightLayout->setSpacing(0);
    midRightLayout->addWidget(m_gameEngineList, 1);
    midRightLayout->addSpacing(LayoutConstants::NarrowVMargin);
    midRightLayout->addWidget(openPreferencesButton);
    midRightLayout->addSpacing(LayoutConstants::NarrowVMargin);

    auto *midLayout = new QHBoxLayout{};
    midLayout->setContentsMargins(0, 0, 0, 0);
    midLayout->setSpacing(0);
    midLayout->addSpacing(LayoutConstants::WideHMargin * 2);
    midLayout->addLayout(midLeftLayout, 1);
    midLayout->addSpacing(LayoutConstants::WideHMargin * 2);
    //   midLayout->addWidget(new BorderLine{BorderLine::Direction::Vertical});
    midLayout->addLayout(midRightLayout);
    // midLayout->addWidget(new BorderLine{BorderLine::Direction::Vertical});


    midPanel->setLayout(midLayout);

    auto *buttonBox = new QDialogButtonBox{};
    m_launchButton = buttonBox->addButton("Launch", QDialogButtonBox::AcceptRole);
    auto *closeButton = buttonBox->addButton("Close", QDialogButtonBox::RejectRole);

    auto *outerLayout = new QVBoxLayout{};
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(gameIndicator);
    outerLayout->addWidget(new BorderLine{BorderLine::Direction::Horizontal});
    outerLayout->addWidget(midPanel, 1);
    outerLayout->addLayout(wrapDialogButtonBox(buttonBox));

    setLayout(outerLayout);

    //m_parameterText->setEnabled(false);
    m_launchButton->setEnabled(false);

    connect(openPreferencesButton, &QPushButton::clicked, this, &LaunchGameEngineDialog::editGameEngines);
    connect(m_testButton, &QPushButton::clicked, this, &LaunchGameEngineDialog::testCommandLine);

    connect(m_parameterText, &QLineEdit::textChanged, this, &LaunchGameEngineDialog::parametersChanged);

    connect(m_launchButton, &QPushButton::clicked, this, &LaunchGameEngineDialog::launchEngine);
    connect(closeButton, &QPushButton::clicked, this, &LaunchGameEngineDialog::close);

    connect(m_gameEngineList, &GameEngineProfileListBox::currentProfileChanged, this, &LaunchGameEngineDialog::gameEngineProfileChanged);
    connect(m_gameEngineList, &GameEngineProfileListBox::profileSelected, this, &LaunchGameEngineDialog::launchEngine);

    if (m_gameEngineList->count() > 0) {
        m_gameEngineList->setCurrentRow(0);
    }

    m_parameterText->setFocus();
    // m_parameterText->deselect();
}

void LaunchGameEngineDialog::reloadConfig() {
    auto document = kdl::mem_lock(m_document);
    const auto &gameName = document->game()->gameName();

    auto &gameFactory = Model::GameFactory::instance();
    const auto &gameConfig = gameFactory.gameConfig(gameName);
    m_config = gameConfig.gameEngineConfig;

    m_gameEngineList->setConfig(m_config);
}

LaunchGameEngineVariables LaunchGameEngineDialog::variables() const {
    return LaunchGameEngineVariables{kdl::mem_lock(m_document)};
}

void LaunchGameEngineDialog::gameEngineProfileChanged() {
    m_lastProfile = m_gameEngineList->selectedProfile();
    m_parameterText->setText(m_lastProfile ? QString::fromStdString(m_lastProfile->parameterSpec) : "");
    m_parameterText->setEnabled(m_lastProfile != nullptr);
    m_launchButton->setEnabled(m_lastProfile != nullptr);
}

void LaunchGameEngineDialog::parametersChanged(const QString &text) {
    if (auto *profile = m_gameEngineList->selectedProfile()) {
        profile->parameterSpec = text.toStdString();
    }

    updateCommandLine();
}

void LaunchGameEngineDialog::editGameEngines() {
    saveConfig();

    auto dialog = GameEngineDialog{kdl::mem_lock(m_document)->game()->gameName(), this};
    dialog.exec();

    const auto previousRow = m_gameEngineList->currentRow();

    // reload m_config as it may have been changed by the GameEngineDialog
    reloadConfig();

    if (m_gameEngineList->count() > 0) {
        if (previousRow >= 0) {
            m_gameEngineList->setCurrentRow(std::min(previousRow, m_gameEngineList->count() - 1));
        } else {
            m_gameEngineList->setCurrentRow(0);
        }
    }
}

void LaunchGameEngineDialog::launchEngine() {
    try {
        const auto *profile = m_gameEngineList->selectedProfile();
        ensure(profile != nullptr, "profile is null");

        const auto parameters = EL::interpolate(profile->parameterSpec, EL::EvaluationContext{variables()});
        const auto workDir = IO::pathAsQString(profile->path.parent_path());

#ifdef __APPLE__
        // We have to launch apps via the 'open' command so that we can properly pass
        // parameters.
        const auto arguments = QStringList{IO::pathAsQString(profile->path), "--args", QString::fromStdString(parameters)};

        if (!QProcess::startDetached("/usr/bin/open", arguments, workDir)) {
            throw Exception("Unknown error");
        }
#else
        const auto commandAndArgs = QString::fromLatin1("\"%1\" %2")
                                        .arg(IO::pathAsQString(profile->path))
                                        .arg(QString::fromStdString(parameters));

        const auto oldWorkDir = QDir::currentPath();
        QDir::setCurrent(workDir);
        const auto success = QProcess::startDetached(commandAndArgs);
        QDir::setCurrent(oldWorkDir);

        if (!success) {
            throw Exception("Unknown error");
        }
#endif

        accept();
    } catch (const Exception &e) {
        const auto message = kdl::str_to_string("Could not launch game engine: ", e.what());
        auto document = kdl::mem_lock(m_document);
        document->logger().error(message);
        QMessageBox::critical(this, "TrenchBroom", QString::fromStdString(message), QMessageBox::Ok);
    }
}

void LaunchGameEngineDialog::done(const int r) {
    saveConfig();

    QDialog::done(r);
}

void LaunchGameEngineDialog::saveConfig() {
    auto document = kdl::mem_lock(m_document);
    const auto &gameName = document->game()->gameName();
    auto &gameFactory = Model::GameFactory::instance();
    gameFactory.saveGameEngineConfig(gameName, m_config, document->logger());
}
void LaunchGameEngineDialog::updateCommandLine() {
    try {
        const auto *profile = m_gameEngineList->selectedProfile();

        if (profile == nullptr) {
            makeError(m_parameterText);
            m_launchButton->setEnabled(false);
            m_testButton->setEnabled(false);
            return;
        }

        const auto parameters = EL::interpolate(profile->parameterSpec, EL::EvaluationContext{variables()});
        //  const auto workDir = IO::pathAsQString(profile->path.parent_path());

        m_commandLine->setText(QString::fromStdString(parameters));
        makeDefault(m_parameterText, false);
        m_launchButton->setEnabled(true);
        m_testButton->setEnabled(true);

    } catch (const Exception &e) {
        makeError(m_parameterText);
        m_launchButton->setEnabled(false);
        m_testButton->setEnabled(false);
        const auto message = kdl::str_to_string("Unable to resolve parameter: ", e.what());
        auto document = kdl::mem_lock(m_document);
        document->logger().error(message);
    }
}

void LaunchGameEngineDialog::testCommandLine() {
    try {
        const auto *profile = m_gameEngineList->selectedProfile();
        ensure(profile != nullptr, "profile is null");

        const auto parameters = EL::interpolate(profile->parameterSpec, EL::EvaluationContext{variables()});
        const auto workDir = IO::pathAsQString(profile->path.parent_path());

        QString cmd = "";

#ifdef __APPLE__
        // We have to launch apps via the 'open' command so that we can properly pass
        // parameters.
        const auto arguments = QStringList{IO::pathAsQString(profile->path), "--args", QString::fromStdString(parameters)};
        cmd = arguments.join(' ');

#else
        cmd = QString::fromLatin1("\"%1\" %2")
                  .arg(IO::pathAsQString(profile->path))
                  .arg(QString::fromStdString(parameters));

#endif

        QMessageBox msgBox;
        msgBox.setText(cmd);
        msgBox.setInformativeText("Workdir: " + workDir);
        msgBox.exec();

    } catch (const Exception &e) {
        const auto message = kdl::str_to_string("Error while testing command line: ", e.what());
        auto document = kdl::mem_lock(m_document);
        document->logger().error(message);
        QMessageBox::critical(this, "TrenchBroom", QString::fromStdString(message), QMessageBox::Ok);
    }
}
}// namespace View
}// namespace TrenchBroom
