/*
 Copyright (C) 2021 Kristian Duske

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

#define CATCH_CONFIG_RUNNER
#include "Error.h"
#include <kdl/result.h>
#include "Catch2.h"

#include "IO/SystemPaths.h"
#include "Model/GameFactory.h"
#include "TestPreferenceManager.h"
#include "TestSourcePath.h"

#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>

int main(int argc, char** argv) {
    using namespace TrenchBroom;
    IO::SystemPaths::setPortable();
    PreferenceManager::createInstance<TestPreferenceManager>();
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QTemporaryDir settings;
    if (!settings.isValid()) return 2;
    QCoreApplication::setOrganizationName("TrenchBroomRegression");
    QCoreApplication::setApplicationName("MigrationTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    QApplication::setStyle("Fusion");
    Model::GameFactory::instance().initialize({{testSourceRoot() / "app/resources/games"}, std::filesystem::path(settings.path().toStdString()) / "games-user"});
    return Catch::Session().run(argc, argv);
}
