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

#include <QApplication>

#include "Notifier.h"
#include "QSSBuilder.h"
#include "View/MapFrame.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class QMenu;

class QSettings;

namespace TrenchBroom {
class Logger;

namespace View {
class ExecutableEvent;

class FrameManager;

class RecentDocuments;

class WelcomeWindow;

class TrenchBroomApp : public QApplication {
  Q_OBJECT
  private:
    std::unique_ptr<FrameManager> m_frameManager;
    std::unique_ptr<RecentDocuments> m_recentDocuments;
    std::unique_ptr<WelcomeWindow> m_welcomeWindow;
    QTimer *m_recentDocumentsReloadTimer;
    QFont m_UI_Font, m_ConsoleFont, m_RenderFont;
    QSSBuilder *builder = nullptr;
    MapFrame *currentMapFrame = nullptr;

  public:
    MapFrame *getCurrentMapFrame() const;

    void setCurrentMapFrame(MapFrame *currentMapFrame);

    Logger &logger() const;

  public:
    const QFont &getMUiFont() const;

    void setMUiFont(const QFont &mUiFont);

    const QFont &getMConsoleFont() const;

    void setMConsoleFont(const QFont &mConsoleFont);

    const QFont &getMRenderFont() const;

    void setMRenderFont(const QFont &mRenderFont);

  public:
    TrenchBroomApp(int &argc, char **argv);

    ~TrenchBroomApp() override;

    const QFont &getUIFont() const;

    const QFont &getConsoleFont() const;

    const QFont &getRenderFont() const;

    static TrenchBroomApp &instance();

    void parseCommandLineAndShowFrame();

    FrameManager *frameManager();

  private:
    static QPalette darkPalette();

    bool loadStyleSheets();

    void loadStyle();

  public:
    void reloadStyle(bool reloadFonts = false, bool reloadStyleSheets = true);

    QFont loadFont(const std::filesystem::path &path, const int size);

    std::vector<std::filesystem::path> recentDocuments() const;

    void addRecentDocumentMenu(QMenu &menu);

    void removeRecentDocumentMenu(QMenu &menu);

    void updateRecentDocument(const std::filesystem::path &path);

    bool openDocument(const std::filesystem::path &path);

    void openPreferences();

    void openAbout();

    bool initializeGameFactory();

    bool newDocument();

    void openDocument();

    void showManual();

    void showPreferences();

    void showAboutDialog();

    void debugShowCrashReportDialog();

    bool notify(QObject *receiver, QEvent *event) override;

#ifdef __APPLE__

    bool event(QEvent *event) override;

#endif

    void openFilesOrWelcomeFrame(const QStringList &fileNames);

  public:
    void showWelcomeWindow();

    void closeWelcomeWindow();

  private:
    static bool useSDI();

  signals:

    void recentDocumentsDidChange();
};

void setCrashReportGUIEnbled(bool guiEnabled);

[[noreturn]] void reportCrashAndExit(const std::string &stacktrace, const std::string &reason);

bool isReportingCrash();
} // namespace View
} // namespace TrenchBroom
