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
#include "StringUtils.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <QProxyStyle>

/* ---[ Common Shorthand Definitions ]-------------------------------------------- */

#define APP View::TrenchBroomApp::instance()
#define LOG View::TrenchBroomApp::instance().logger()


class QMenu;
class QMenuBar;

class QSettings;
class QFileSystemWatcher;

namespace TrenchBroom {
class Logger;

namespace View {
//class ExecutableEvent;

class FrameManager;

class RecentDocuments;

class WelcomeWindow;

/* ------------------------------------------------------------------------------------------- */

/**
 * Signal struct holding information regarding a specific signal
 * to use with the SignalHandler.
 */
struct SignalType {
   int signal;
   std::string name;
   std::string description;

   SignalType() = default;

   SignalType(int signal, const std::string &name, const std::string &description);

   std::string toString() const;
};

/**
 * Handle common signals at program execution.
 */
struct SignalHandler {
   // @formatter:off
   std::map<int, SignalType> sigList = {
       { SIGHUP , SignalType{SIGHUP , "SIGHUP" , "hangup"} },
       { SIGINT , SignalType{SIGINT , "SIGINT" , "interrupt"} },
       { SIGQUIT, SignalType{SIGQUIT, "SIGQUIT", "quit"} },
       { SIGTRAP, SignalType{SIGTRAP, "SIGTRAP", "trace trap (not reset when caught)"} },
       { SIGSEGV, SignalType{SIGSEGV, "SIGSEGV", "segmentation violation"} },
       { SIGTERM, SignalType{SIGTERM, "SIGTERM", "software termination signal from kill"} },
   };
   // @formatter:on

   const SignalType &get(int sig) {
       return sigList[sig];
   }

   bool setupHandler(void (*func)(int));
};

static SignalHandler gSignalHandler = SignalHandler{};

/* ------------------------------------------------------------------------------------------- */

/**
 * Encapsulates all collected data regarding an application crash.
 */
struct CrashReport {
   SignalType signalType;
   std::filesystem::path reportPath;
   std::filesystem::path logPath;
   std::filesystem::path mapPath;

   QString stackTrace;
   QString logContent;

   CrashReport(const SignalType &signalType, const std::filesystem::path &reportPath, const std::filesystem::path &logPath, const std::filesystem::path &mapPath);
};

/* ------------------------------------------------------------------------------------------- */

class TrenchBroomApp : public QApplication {
  Q_OBJECT
  private:
    std::unique_ptr<FrameManager> m_frameManager;
    std::unique_ptr<RecentDocuments> m_recentDocuments;
    std::unique_ptr<QMenuBar> m_defaultMenuBar;
    std::unique_ptr<WelcomeWindow> m_welcomeWindow;

    QTimer *m_recentDocumentsReloadTimer;
    QFont m_UI_Font, m_ConsoleFont, m_RenderFont;
    QSSBuilder m_qssBuilder;
    QProxyStyle *m_proxyStyle = nullptr;
    NotifierConnection m_themeConnection;
    QTimer* m_themeRefreshTimer = nullptr;
    QFileSystemWatcher* m_themeWatcher = nullptr;
    QMap<QString, QString> m_themeFileStamps;
    std::filesystem::path m_loadedUIFontPath, m_loadedConsoleFontPath, m_loadedRenderFontPath;
    int m_loadedUIFontSize = 0, m_loadedConsoleFontSize = 0, m_loadedRenderFontSize = 0;
    MapFrame *m_currentMapFrame;
    SignalHandler m_signalHandler;

  public:
    MapFrame *getCurrentMapFrame() const;
    void setCurrentMapFrame(MapFrame *currentMapFrame);

    QProxyStyle *getQProxyStyle() const;
    void setQProxyStyle(QProxyStyle *qProxyStyle);

    Logger &logger() const;

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
    bool loadStyleSheets();
    void loadStyle();
    void refreshThemeFonts();
    void queueThemeRefresh(int delayMs = 0);
    void watchThemeFiles();

  public:
    void reloadStyle(bool reloadFonts = false, bool reloadStyleSheets = true);
    // Restart the pending refresh to combine a burst of appearance edits.
    void requestThemeRefresh(int delayMs = 0);
    QFont loadFont(const std::filesystem::path &path, int size) const;
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
    bool event(QEvent *event) override;

    void openFilesOrWelcomeFrame(const QStringList &fileNames);

  public:
    void showWelcomeWindow();
    void closeWelcomeWindow();

  private:
    static bool useSDI();
    bool m_qssLoadedSuccessful;
    std::vector<QString> m_qssErrors;

  signals:
    void recentDocumentsDidChange();
};

//[[maybe_unused]]
//void setCrashReportGUIEnabled(bool guiEnabled);

[[noreturn]]
void reportCrashAndExit(const std::string &stacktrace, const SignalType &signalType);

bool isReportingCrash();

} // namespace View
} // namespace TrenchBroom
