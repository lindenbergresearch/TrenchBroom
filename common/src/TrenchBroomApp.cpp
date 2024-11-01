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

#include "TrenchBroomApp.h"

#include <QStyleFactory.h>

#include "Error.h"
#include "Exceptions.h"
#include "IO/DiskIO.h"
#include "IO/PathInfo.h"
#include "IO/PathQt.h"
#include "IO/SystemPaths.h"
#include "Model/GameFactory.h"
#include "Model/MapFormat.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "QSSBuilder.h"
#include "Logger.h"
#include "Result.h"
#include "Version.h"
#include "TrenchBroomStackWalker.h"
#include "View/AboutDialog.h"
#include "View/Actions.h"
#include "View/Console.h"
#include "View/CrashDialog.h"
#include "View/FrameManager.h"
#include "View/GLContextManager.h"
#include "View/GameDialog.h"
#include "View/GetVersion.h"
#include "View/MapDocument.h"
#include "View/MapFrame.h"
#include "View/MapViewBase.h"
#include "View/PreferenceDialog.h"
#include "View/QtUtils.h"
#include "View/RecentDocuments.h"
#include "View/WelcomeWindow.h"

#ifdef __APPLE__

#include "View/MainMenuBuilder.h"

#endif

#include <QColor>
#include <QCommandLineParser>
#include <QDebug>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QProxyStyle>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>

#include <kdl/path_utils.h>
#include <kdl/set_temp.h>
#include <kdl/string_utils.h>

#include <fmt/format.h>

#include <chrono>
#include <clocale>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace TrenchBroom {
namespace View {

namespace {
// returns the topmost MapDocument as a shared pointer, or the empty shared pointer
std::shared_ptr<MapDocument> topDocument() {
    if (const auto *frameManager = TrenchBroomApp::instance().frameManager()) {
        if (const auto *frame = frameManager->topFrame()) {
            return frame->document();
        }
    }
    return {};
}
} // namespace


TrenchBroomApp &TrenchBroomApp::instance() {
    return *dynamic_cast<TrenchBroomApp *>(qApp);
}

#if defined(_WIN32) && defined(_MSC_VER)
LONG WINAPI TrenchBroomUnhandledExceptionFilter(PEXCEPTION_POINTERS pExceptionPtrs);
#else

[[noreturn]] static void CrashHandler(int signum);

#endif

TrenchBroomApp::TrenchBroomApp(int &argc, char **argv) : QApplication{argc, argv} {
    using namespace std::chrono_literals;

    // When this flag is enabled, font and palette changes propagate as though the user had
    // manually called the corresponding QWidget methods.
    setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles);

    // Don't show icons in menus, they are scaled down and don't look very good.
    setAttribute(Qt::AA_DontShowIconsInMenus);

#if defined(_WIN32) && defined(_MSC_VER)
    // with MSVC, set our own handler for segfaults so we can access the context
    // pointer, to allow StackWalker to read the backtrace.
    // see also: http://crashrpt.sourceforge.net/docs/html/exception_handling.html
    SetUnhandledExceptionFilter(TrenchBroomUnhandledExceptionFilter);
#else
    signal(SIGSEGV, CrashHandler);
#endif

    // always set this locale so that we can properly parse floats from text files
    // regardless of the platforms locale
    std::setlocale(LC_NUMERIC, "C");

    setApplicationName("TrenchBroom Nova");


    const LogLevel logLevel = pref(Preferences::AppLogLevel);
    const QString label = levelAttributes[logLevel].label;
    defaultQtLogger.setLogLevel(logLevel);

    logger().info() << "TrenchBroom Nova " << getBuildVersion();
    logger().info() << "Build : " << getBuildIdStr();
    logger().info() << "OS    : " << QSysInfo::prettyProductName();
    logger().info() << "Qt    : v" << qVersion();


    logger().info() << "Current log-level is set to: " << label;

    // Needs to be "" otherwise Qt adds this to the paths returned by QStandardPaths
    // which would cause preferences to move from where they were with wx
    setOrganizationName("");
    setOrganizationDomain("io.github.trenchbroom");

    if (!initializeGameFactory()) {
        QCoreApplication::exit(1);
        return;
    }

    loadStyle();
    loadStyleSheets();

    m_UI_Font = loadFont(pref(Preferences::UIFontPath), pref(Preferences::UIFontSize));
    m_ConsoleFont = loadFont(pref(Preferences::ConsoleFontPath), pref(Preferences::ConsoleFontSize));
    m_RenderFont = loadFont(pref(Preferences::RendererFontPath), pref(Preferences::RendererFontSize));

    QApplication::setFont(m_UI_Font);

    // these must be initialized here and not earlier
    m_frameManager = std::make_unique<FrameManager>(useSDI());

    m_recentDocuments = std::make_unique<RecentDocuments>(10, [](const auto &path) { return std::filesystem::exists(path); });
    connect(m_recentDocuments.get(), &RecentDocuments::loadDocument, this, [this](const std::filesystem::path &path) { openDocument(path); });
    connect(m_recentDocuments.get(), &RecentDocuments::didChange, this, &TrenchBroomApp::recentDocumentsDidChange);
    m_recentDocuments->reload();
    m_recentDocumentsReloadTimer = new QTimer{};
    connect(m_recentDocumentsReloadTimer, &QTimer::timeout, m_recentDocuments.get(), &RecentDocuments::reload);
    m_recentDocumentsReloadTimer->start(1s);

#ifdef __APPLE__
    setQuitOnLastWindowClosed(false);

    auto *menuBar = new QMenuBar{};
    auto actionMap = std::map<const Action *, QAction *>{};

    auto menuBuilder = MainMenuBuilder{
        *menuBar, actionMap, [](const Action &action) {
            auto context = ActionExecutionContext{nullptr, nullptr};
            action.execute(context);
        }};

    const auto &actionManager = ActionManager::instance();
    actionManager.visitMainMenu(menuBuilder);

    addRecentDocumentMenu(*menuBuilder.recentDocumentsMenu);

    auto context = ActionExecutionContext{nullptr, nullptr};
    for (auto [tbAction, qtAction] : actionMap) {
        qtAction->setEnabled(tbAction->enabled(context));
        if (qtAction->isCheckable()) {
            qtAction->setChecked(tbAction->checked(context));
        }
    }

#endif
}

TrenchBroomApp::~TrenchBroomApp() {
    PreferenceManager::destroyInstance();
}

void TrenchBroomApp::parseCommandLineAndShowFrame() {
    auto parser = QCommandLineParser{};
    parser.process(*this);
    openFilesOrWelcomeFrame(parser.positionalArguments());
}

FrameManager *TrenchBroomApp::frameManager() {
    return m_frameManager.get();
}

QPalette TrenchBroomApp::darkPalette() {
    const auto brightness = pref(Preferences::UIBrightness);
    const auto contrast = 1.0f; // internal

    const auto hl_color = pref(Preferences::UIHighlightColor);
    const auto tint_color = pref(Preferences::UIWindowTintColor);
    const auto text_color = pref(Preferences::UITextColor);

    const auto windowColor = toQColor(tint_color, brightness);
    const auto textColor = toQColor(text_color, brightness);
    const auto highlightColor = toQColor(hl_color, brightness);

    // Build an initial palette based on the button color
    auto palette = QPalette{windowColor};

    // Window colors
    palette.setColor(QPalette::Active, QPalette::Window, windowColor.lighter(int(130.f * contrast)));
    palette.setColor(QPalette::Inactive, QPalette::Window, windowColor.lighter(int(130.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::Window, windowColor.darker(int(260.f * contrast)));

    // List box backgrounds, text entry backgrounds, menu backgrounds
    palette.setColor(QPalette::Base, windowColor.darker(int(125.f * contrast)));
    palette.setColor(QPalette::AlternateBase, windowColor.darker(int(160.f * contrast)));

    // Placeholder text color
    palette.setColor(QPalette::Active, QPalette::PlaceholderText, textColor.darker(int(150.f * contrast)));
    palette.setColor(QPalette::Inactive, QPalette::PlaceholderText, textColor.darker(int(170.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, textColor.darker(int(210.f * contrast)));

    // Button text
    palette.setColor(QPalette::Active, QPalette::ButtonText, textColor.lighter(int(100.f * contrast)));
    palette.setColor(QPalette::Inactive, QPalette::ButtonText, textColor.lighter(int(100.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, textColor.darker(int(200.f * contrast)));

    // Button
    palette.setColor(QPalette::Active, QPalette::Button, windowColor.lighter(int(190.f * contrast)));
    palette.setColor(QPalette::Inactive, QPalette::Button, windowColor.lighter(int(190.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::Button, windowColor.darker(int(100.f * contrast)));

    // WindowText is supposed to be against QPalette::Window
    palette.setColor(QPalette::Active, QPalette::WindowText, textColor);
    palette.setColor(QPalette::Inactive, QPalette::WindowText, textColor);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, textColor.darker(int(200.f * contrast)));

    // Menu text, text edit text, table cell text
    palette.setColor(QPalette::Active, QPalette::Text, textColor.darker(int(100.f * contrast)));
    palette.setColor(QPalette::Inactive, QPalette::Text, textColor.darker(int(100.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::Text, textColor.darker(int(200.f * contrast)));

    palette.setColor(QPalette::Active, QPalette::BrightText, textColor.lighter(int(200.f * contrast)));
    palette.setColor(QPalette::Inactive, QPalette::BrightText, textColor.lighter(int(200.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::BrightText, textColor.darker(int(100.f * contrast)));

    // Disabled menu item text shadow
    palette.setColor(QPalette::Light, windowColor.lighter(int(240.f * contrast)));
    palette.setColor(QPalette::Midlight, windowColor.lighter(int(210.f * contrast)));
    palette.setColor(QPalette::Mid, windowColor);
    palette.setColor(QPalette::Dark, windowColor.darker(int(100.f * contrast)));
    palette.setColor(QPalette::Shadow, windowColor.darker(int(650.f * contrast)));

    // Highlight (selected list box row, selected grid cell background, selected tab text)
    palette.setColor(QPalette::Active, QPalette::Highlight, highlightColor);
    palette.setColor(QPalette::Inactive, QPalette::Highlight, windowColor.lighter(int(200.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, windowColor.lighter(int(100.f * contrast)));

    // Text of highlighted elements
    palette.setColor(QPalette::Active, QPalette::HighlightedText, textColor.lighter(int(150.f * contrast)));
    palette.setColor(QPalette::Inactive, QPalette::HighlightedText, textColor.lighter(int(150.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, textColor.darker(int(130.f * contrast)));

    return palette;
}

bool TrenchBroomApp::loadStyleSheets() {
    const auto path = IO::SystemPaths::findResourceFile("stylesheets/base.qss");

    logger().info() << "Loading StyleSheets from: " << path.string().c_str();

    if (!builder) {
        builder = QSSBuilder::fromFile(path);

        if (!builder) {
            return false;
        }

        builder->addReplacement("TEXT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::WindowText);
        });

        builder->addReplacement("DISABLED_TEXT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::WindowText, QPalette::ColorGroup::Disabled);
        });

        builder->addReplacement("DISABLED_BUTTON_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Button, QPalette::ColorGroup::Disabled);
        });

        builder->addReplacement("HIGHLIGHTED_TEXT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::HighlightedText);
        });

        builder->addReplacement("BRIGHT_TEXT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::BrightText);
        });

        builder->addReplacement("PLACEHOLDER_TEXT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::PlaceholderText);
        });

        builder->addReplacement("WINDOW_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Window);
        });

        builder->addReplacement("BUTTON_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Button);
        });

        builder->addReplacement("BUTTON_TEXT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::ButtonText);
        });

        builder->addReplacement("BASE_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Base);
        });

        builder->addReplacement("ALTERNATE_BASE_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::AlternateBase);
        });

        builder->addReplacement("LIGHT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Light);
        });

        builder->addReplacement("MIDLIGHT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Midlight);
        });

        builder->addReplacement("MIDHALF_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Mid, 150);
        });

        builder->addReplacement("MID_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Mid);
        });

        builder->addReplacement("DARK_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Dark);
        });

        builder->addReplacement("SHADOW_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Shadow);
        });

        builder->addReplacement("BRIGHT_HIGHLIGHT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Highlight, 130);
        });

        builder->addReplacement("LIGHT_HIGHLIGHT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Highlight, 80);
        });

        builder->addReplacement("HIGHLIGHT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Highlight);
        });

        builder->addReplacement("MID_HIGHLIGHT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Highlight, -180);
        });

        builder->addReplacement("DARK_HIGHLIGHT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Highlight, -200);
        });

        /* ------------------------------------------------------------------------------------------- */

        builder->addReplacement("BORDER_RADIUS", []() {
            return QString::asprintf("%dpx", LayoutConstants::BorderRadius);
        });

        builder->addReplacement("BORDER_SIZE", []() {
            return QString::asprintf("%dpx", LayoutConstants::BorderSize);
        });

        /* ------------------------------------------------------------------------------------------- */

        builder->addReplacement("NARROW_V_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::NarrowVMargin);
        });

        builder->addReplacement("NARROW_H_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::NarrowHMargin);
        });

        builder->addReplacement("MEDIUM_V_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::MediumVMargin);
        });

        builder->addReplacement("MEDIUM_H_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::MediumHMargin);
        });

        builder->addReplacement("WIDE_V_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::WideVMargin);
        });

        builder->addReplacement("WIDE_H_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::WideHMargin);
        });

        builder->addReplacement("NARROW_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::NarrowHMargin);
        });

        builder->addReplacement("MEDIUM_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::MediumHMargin);
        });

        builder->addReplacement("WIDE_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::WideHMargin);
        });

        /* ------------------------------------------------------------------------------------------- */

        builder->addReplacement("UI_FONT_SIZE", []() {
            return QString::asprintf("%dpx", pref(Preferences::UIFontSize));
        });

        builder->addReplacement("UI_MID_FONT_SIZE", []() {
            return QString::asprintf("%dpx", pref(Preferences::UIFontSize) - 1);
        });

        builder->addReplacement("UI_SMALL_FONT_SIZE", []() {
            return QString::asprintf("%dpx", pref(Preferences::UIFontSize) - 2);
        });

        builder->addReplacement("UI_TITLE_FONT_SIZE", []() {
            return QString::asprintf("%dpx", pref(Preferences::UIFontSize) + 1);
        });
        builder->addReplacement("RENDER_FONT_SIZE", []() {
            return QString::asprintf("%dpx", pref(Preferences::RendererFontSize));
        });

        builder->addReplacement("CONSOLE_FONT_SIZE", []() {
            return QString::asprintf("%dpx", pref(Preferences::ConsoleFontSize));
        });

        builder->addReplacement("TOOLBAR_ICON_SIZE", []() {
            return QString::asprintf("%dpx", pref(Preferences::ToolBarIconsSize));
        });
    }

    builder->update();

    logger().info() << "Setting DynamicStyleSheets to main application...";
    qApp->setStyleSheet(builder->getRenderedText());

    return true;
}

void TrenchBroomApp::loadStyle() {
    // We can't use auto mnemonics in TrenchBroom. e.g. by default with Qt, Alt+D opens the
    // "Debug" menu, Alt+S activates the "Show default properties" checkbox in the entity
    // inspector. Flying with Alt held down and pressing WASD is a fundamental behaviour in
    // TB, so we can't have shortcuts randomly activating.
    //
    // Previously were calling `qt_set_sequence_auto_mnemonic(false);` in main(), but it
    // turns out we also need to suppress an Alt press followed by release from focusing the
    // menu bar (https://github.com/TrenchBroom/TrenchBroom/issues/3140), so the following
    // QProxyStyle disables that completely.

    logger().info() << "Setup Style and Palette...";
    class TrenchBroomProxyStyle : public QProxyStyle {
      public:
        explicit TrenchBroomProxyStyle(const QString &key) : QProxyStyle{key} {
        }

        explicit TrenchBroomProxyStyle(QStyle *style = nullptr) : QProxyStyle{style} {
        }

        int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override {
            return hint == QStyle::SH_MenuBar_AltKeyNavigation ? 0
                                                               : QProxyStyle::styleHint(hint, option, widget, returnData);
        }
    };

    setStyle(new TrenchBroomProxyStyle{"Fusion"});
    setPalette(darkPalette());
    qApp->setPalette(darkPalette());

}

std::vector<std::filesystem::path> TrenchBroomApp::recentDocuments() const {
    return m_recentDocuments->recentDocuments();
}

void TrenchBroomApp::addRecentDocumentMenu(QMenu &menu) {
    m_recentDocuments->addMenu(menu);
}

void TrenchBroomApp::removeRecentDocumentMenu(QMenu &menu) {
    m_recentDocuments->removeMenu(menu);
}

void TrenchBroomApp::updateRecentDocument(const std::filesystem::path &path) {
    m_recentDocuments->updatePath(path);
}

bool TrenchBroomApp::openDocument(const std::filesystem::path &path) {
    const auto checkFileExists = [&]() {
        return IO::Disk::pathInfo(path) == IO::PathInfo::File ? Result<void>{} : Result<void>{
            Error{"'" + path.string() + "' not found"}};
    };

    auto *frame = static_cast<MapFrame *>(nullptr);
    try {
        auto &gameFactory = Model::GameFactory::instance();
        return checkFileExists().or_else([&](const auto &e) {
            m_recentDocuments->removePath(path);
            return Result<void>{e};
        }).and_then([&]() { return gameFactory.detectGame(path); }).and_then([&](const auto &gameNameAndMapFormat) {
            auto [gameName, mapFormat] = gameNameAndMapFormat;

            if (gameName.empty() || mapFormat == Model::MapFormat::Unknown) {
                if (!GameDialog::showOpenDocumentDialog(nullptr, gameName, mapFormat)) {
                    return Result<bool>{false};
                }
            }

            frame = m_frameManager->newFrame();

            auto game = gameFactory.createGame(gameName, frame->logger());
            ensure(game.get() != nullptr, "game is null");

            closeWelcomeWindow();
            return frame->openDocument(game, mapFormat, path);
        }).transform_error([&](const auto &e) {
            if (frame) {
                frame->close();
            }
            QMessageBox::critical(nullptr, "TrenchBroom", e.msg.c_str(), QMessageBox::Ok);
            logger().error() << e.msg.c_str();
            return false;
        }).value();
    } catch (const Exception &e) {
        if (frame) {
            frame->close();
        }
        QMessageBox::critical(nullptr, "TrenchBroom", e.what(), QMessageBox::Ok);
        logger().error() << e.what();
        return false;
    } catch (...) {
        if (frame) {
            frame->close();
        }
        throw;
    }
}

void TrenchBroomApp::openPreferences() {
    auto dialog = PreferenceDialog{topDocument()};
    dialog.exec();
}

void TrenchBroomApp::openAbout() {
    AboutDialog::showAboutDialog();
}

bool TrenchBroomApp::initializeGameFactory() {
    const auto gamePathConfig = Model::GamePathConfig{
        IO::SystemPaths::findResourceDirectories("games"), IO::SystemPaths::userDataDirectory() / "games",
    };
    auto &gameFactory = Model::GameFactory::instance();
    return gameFactory.initialize(gamePathConfig).transform([](auto errors) {
        if (!errors.empty()) {
            const auto msg = fmt::format(R"(Some game configurations could not be loaded. The following errors occurred:

{})", kdl::str_join(errors, "\n\n"));

            QMessageBox::critical(nullptr, "TrenchBroom", QString::fromStdString(msg), QMessageBox::Ok);
            defaultQtLogger.error() << msg;
        }
    }).if_error([](auto e) { defaultQtLogger.error() << e.msg;; }).is_success();
}

bool TrenchBroomApp::newDocument() {
    auto *frame = static_cast<MapFrame *>(nullptr);
    try {
        auto gameName = std::string{};
        auto mapFormat = Model::MapFormat::Unknown;
        if (!GameDialog::showNewDocumentDialog(nullptr, gameName, mapFormat)) {
            return false;
        }

        frame = m_frameManager->newFrame();

        auto &gameFactory = Model::GameFactory::instance();
        auto game = gameFactory.createGame(gameName, frame->logger());
        ensure(game.get() != nullptr, "game is null");

        closeWelcomeWindow();
        return frame->newDocument(game, mapFormat).transform_error([&](auto e) {
            frame->close();

            QMessageBox::critical(nullptr, "", QString::fromStdString(e.msg));
            defaultQtLogger.error() << e.msg;
            return false;
        }).value();
    } catch (const Exception &e) {
        if (frame) {
            frame->close();
        }

        QMessageBox::critical(nullptr, "", e.what());
        defaultQtLogger.error() << e.what();
        return false;
    }
}

void TrenchBroomApp::openDocument() {
    const auto pathStr = QFileDialog::getOpenFileName(nullptr, tr("Open Map"), fileDialogDefaultDirectory(FileDialogDir::Map), "Map files (*.map);;Any files (*.*)");

    if (const auto path = IO::pathFromQString(pathStr); !path.empty()) {
        updateFileDialogDefaultDirectoryWithFilename(FileDialogDir::Map, pathStr);
        openDocument(path);
    }
}

void TrenchBroomApp::showManual() {
    const auto manualPath = IO::SystemPaths::findResourceFile("manual/index.html");
    const auto manualPathString = manualPath.string();
    const auto manualPathUrl = QUrl::fromLocalFile(QString::fromStdString(manualPathString));
    QDesktopServices::openUrl(manualPathUrl);
}

void TrenchBroomApp::showPreferences() {
    openPreferences();
}

void TrenchBroomApp::showAboutDialog() {
    openAbout();
}

void TrenchBroomApp::debugShowCrashReportDialog() {
    const auto reportPath = IO::SystemPaths::userDataDirectory() / "crashreport.txt";
    const auto mapPath = IO::SystemPaths::userDataDirectory() / "crashreport.map";
    const auto logPath = IO::SystemPaths::userDataDirectory() / "crashreport.log";

    auto dialog = CrashDialog{"Debug crash", reportPath, mapPath, logPath};
    dialog.exec();
}

/**
 * If we catch exceptions in main() that are otherwise uncaught, Qt prints a warning to
 * override QCoreApplication::notify() and catch exceptions there instead.
 */
bool TrenchBroomApp::notify(QObject *receiver, QEvent *event) {
#ifdef _MSC_VER
    __try
    {
      return QApplication::notify(receiver, event);

      // We have to choose between capturing the stack trace (using __try/__except) and
      // getting the C++ exception object (using C++ try/catch) - take the stack trace.
    }
    __except (TrenchBroomUnhandledExceptionFilter(GetExceptionInformation()))
    {
      // Unreachable, see TrenchBroomUnhandledExceptionFilter
      return false;
    }
#else
    try {
        return QApplication::notify(receiver, event);
    } catch (const std::exception &e) {
        // Unfortunately we can't portably get the stack trace of the exception itself
        TrenchBroom::View::reportCrashAndExit("<uncaught exception>", e.what());
    }
#endif
}

#ifdef __APPLE__

bool TrenchBroomApp::event(QEvent *event) {
    if (event->type() == QEvent::FileOpen) {
        const auto *openEvent = static_cast<QFileOpenEvent *>(event);
        const auto path = std::filesystem::path{openEvent->file().toStdString()};
        if (openDocument(path)) {
            closeWelcomeWindow();
            return true;
        }
        return false;
    } else if (event->type() == QEvent::ApplicationActivate) {
        if (m_frameManager && m_frameManager->allFramesClosed()) {
            showWelcomeWindow();
        }
    }
    return QApplication::event(event);
}

#endif

void TrenchBroomApp::openFilesOrWelcomeFrame(const QStringList &fileNames) {
    const auto filesToOpen = useSDI() && !fileNames.empty() ? QStringList{fileNames.front()} : fileNames;

    auto anyDocumentOpened = false;
    for (const auto &fileName : filesToOpen) {
        const auto path = IO::pathFromQString(fileName);
        if (!path.empty() && openDocument(path)) {
            anyDocumentOpened = true;
        }
    }

    if (!anyDocumentOpened) {
        showWelcomeWindow();
    }
}

void TrenchBroomApp::showWelcomeWindow() {
    if (!m_welcomeWindow) {
        // must be initialized after m_recentDocuments!
        m_welcomeWindow = std::make_unique<WelcomeWindow>();
    }
    m_welcomeWindow->show();
}

void TrenchBroomApp::closeWelcomeWindow() {
    if (m_welcomeWindow) {
        m_welcomeWindow->close();
    }
}

bool TrenchBroomApp::useSDI() {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

void TrenchBroomApp::reloadStyle(bool reloadFonts, bool reloadStyleSheets) {
    if (reloadStyleSheets) {
        loadStyleSheets();
    }

    loadStyle();

    if (reloadFonts) {
        m_UI_Font = loadFont(pref(Preferences::UIFontPath), pref(Preferences::UIFontSize));
        m_ConsoleFont = loadFont(pref(Preferences::ConsoleFontPath), pref(Preferences::ConsoleFontSize));
        m_RenderFont = loadFont(pref(Preferences::RendererFontPath), pref(Preferences::RendererFontSize));
    }

    QApplication::setFont(m_UI_Font);

    for (const auto &widget : QApplication::allWidgets()) {
        widget->update();
    }

    QCoreApplication::processEvents();
}

const QFont &TrenchBroomApp::getUIFont() const {
    return m_UI_Font;
}

const QFont &TrenchBroomApp::getConsoleFont() const {
    return m_ConsoleFont;
}

const QFont &TrenchBroomApp::getRenderFont() const {
    return m_RenderFont;
}

QFont TrenchBroomApp::loadFont(const std::filesystem::path &path, const int size) {
    auto file = IO::SystemPaths::findResourceFile(path);

    QFontDatabase database;
    if (database.hasFamily(path.string().c_str())) {
        logger().info() << "Using system font: " << path.string().c_str();
        QFont font(path.string().c_str(), size);
        font.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
        font.setStyleStrategy(QFont::PreferQuality);
        font.setStyleHint(QFont::StyleHint::Monospace);

        return font;
    }

    logger().info() << "Loading font: " << path.string().c_str();

    if (!exists(file)) {
        logger().warn() << "Font does not exist: " << path.string().c_str();
        logger().warn() << "Falling back to default font: " << QFont{}.rawName().toStdString();

        return QFont{};
    }

    int font_id = database.addApplicationFont(file.c_str());
    auto font_family = database.applicationFontFamilies(font_id);

    if (font_family.empty()) {
        logger().warn() << "Unable to load font: " << path.string().c_str();
        logger().warn() << "Falling back to default font: " << QFont{}.rawName().toStdString();

        return QFont{};
    }

    QFont font(font_family[0], size);
    font.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
    font.setStyleStrategy(QFont::PreferQuality);
    font.setStyleHint(QFont::StyleHint::Monospace);

    return font;
}

const QFont &TrenchBroomApp::getMUiFont() const {
    return m_UI_Font;
}

void TrenchBroomApp::setMUiFont(const QFont &mUiFont) {
    m_UI_Font = mUiFont;
}

const QFont &TrenchBroomApp::getMConsoleFont() const {
    return m_ConsoleFont;
}

void TrenchBroomApp::setMConsoleFont(const QFont &mConsoleFont) {
    m_ConsoleFont = mConsoleFont;
}

const QFont &TrenchBroomApp::getMRenderFont() const {
    return m_RenderFont;
}

void TrenchBroomApp::setMRenderFont(const QFont &mRenderFont) {
    m_RenderFont = mRenderFont;
}

MapFrame *TrenchBroomApp::getCurrentMapFrame() const {
    return currentMapFrame;
}

void TrenchBroomApp::setCurrentMapFrame(MapFrame *currentMapFrame) {
    TrenchBroomApp::currentMapFrame = currentMapFrame;
}

Logger &TrenchBroomApp::logger() const {
    return defaultQtLogger;
}

namespace {
std::string makeCrashReport(const std::string &stacktrace, const std::string &reason) {
    auto ss = std::stringstream{};
    ss << "Operating System    : " << QSysInfo::prettyProductName().toStdString() << std::endl;
    ss << "Qt Version          : " << qVersion() << std::endl;
    ss << "GL_VENDOR           : " << GLContextManager::GLVendor << std::endl;
    ss << "GL_RENDERER         : " << GLContextManager::GLRenderer << std::endl;
    ss << "GL_VERSION          : " << GLContextManager::GLVersion << std::endl;
    ss << "TrenchBroom Version : " << getBuildVersion().toStdString() << std::endl;
    ss << "TrenchBroom Build   : " << getBuildIdStr().toStdString() << std::endl;
    ss << "Reason              : " << reason << std::endl;
    ss << "\n--------------- [ stack trace ] ---------------" << std::endl;
    ss << stacktrace;
    ss << "--------------- [ stack trace ] ---------------" << std::endl;
    return ss.str();
}

// returns the empty path for unsaved maps, or if we can't determine the current map
std::filesystem::path savedMapPath() {
    const auto document = topDocument();
    return document && document->path().is_absolute() ? document->path() : std::filesystem::path{};
}

std::filesystem::path crashReportBasePath() {
    const auto mapPath = savedMapPath();
    const auto crashLogPath = !mapPath.empty() ? mapPath.parent_path() / mapPath.stem() += "-crash.txt" :
                              IO::pathFromQString(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)) /
                              "trenchbroom-crash.txt";

    // ensure it doesn't exist
    auto index = 0;
    auto testCrashLogPath = crashLogPath;
    while (IO::Disk::pathInfo(testCrashLogPath) == IO::PathInfo::File) {
        ++index;

        const auto testCrashLogName = fmt::format("{}-{}.txt", crashLogPath.stem().string(), index);
        testCrashLogPath = crashLogPath.parent_path() / testCrashLogName;
    }

    return kdl::path_remove_extension(testCrashLogPath);
}

bool inReportCrashAndExit = false;
bool crashReportGuiEnabled = true;
} // namespace

void setCrashReportGUIEnbled(const bool guiEnabled) {
    crashReportGuiEnabled = guiEnabled;
}

void reportCrashAndExit(const std::string &stacktrace, const std::string &reason) {
    // just abort if we reenter reportCrashAndExit (i.e. if it crashes)
    if (inReportCrashAndExit) {
        std::abort();
    }

    inReportCrashAndExit = true;

    // get the crash report as a string
    const auto report = makeCrashReport(stacktrace, reason);
    defaultQtLogger.error() << report;

    // write it to the crash log file
    const auto basePath = crashReportBasePath();

    // ensure the containing directory exists
    IO::Disk::createDirectory(basePath.parent_path()).transform([&](auto) {
        const auto reportPath = kdl::path_add_extension(basePath, ".txt");
        auto logPath = kdl::path_add_extension(basePath, ".log");
        auto mapPath = kdl::path_add_extension(basePath, ".map");

        IO::Disk::withOutputStream(reportPath, [&](auto &stream) {
            stream << report;
            defaultQtLogger.error() << "wrote crash log to " << reportPath.string();
        }).transform_error([](const auto &e) {
            defaultQtLogger.error() << "could not write crash log: " << e.msg;
        });

        // save the map
        auto doc = topDocument();
        if (doc.get()) {
            doc->saveDocumentTo(mapPath);
            defaultQtLogger.error() << "wrote map to " << mapPath.string();
        } else {
            mapPath = std::filesystem::path{};
        }

        // Copy the log file
        if (!QFile::copy(IO::pathAsQString(IO::SystemPaths::logFilePath()), IO::pathAsQString(logPath))) {
            logPath = std::filesystem::path{};
        }

        if (crashReportGuiEnabled) {
            auto dialog = CrashDialog{reason, reportPath, mapPath, logPath};
            dialog.exec();
        }
    }).transform_error([](const auto &e) {
        defaultQtLogger.error() << "could not create crash folder: " << e.msg;
    });

    // write the crash log to stderr
    defaultQtLogger.error() << "crash log:";
    defaultQtLogger.error() << report;

    std::abort();
}

bool isReportingCrash() {
    return inReportCrashAndExit;
}

#if defined(_WIN32) && defined(_MSC_VER)
LONG WINAPI TrenchBroomUnhandledExceptionFilter(PEXCEPTION_POINTERS pExceptionPtrs)
{
  reportCrashAndExit(
    TrenchBroomStackWalker::getStackTraceFromContext(pExceptionPtrs->ContextRecord),
    std::to_string(pExceptionPtrs->ExceptionRecord->ExceptionCode));
  // return EXCEPTION_EXECUTE_HANDLER; unreachable
}
#else

static void CrashHandler(int /* signum */) {
    TrenchBroom::View::reportCrashAndExit(TrenchBroom::TrenchBroomStackWalker::getStackTrace(), "SIGSEGV");
}

#endif
} // namespace View
} // namespace TrenchBroom
