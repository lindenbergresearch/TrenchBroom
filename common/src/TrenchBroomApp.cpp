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
#include "macos_utils.h"
#include "Result.h"
#include "Version.h"
#include "TrenchBroomStackWalker.h"
#include "View/AboutDialog.h"
#include "View/Actions.h"
#include "View/Console.h"
#include "View/FontUtils.h"
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
#include "View/TrenchBroomProxyStyle.h"
#include "View/RecentDocuments.h"
#include "View/WelcomeWindow.h"

#ifdef __APPLE__

#include "View/MainMenuBuilder.h"
#ifndef QT_NO_SESSIONMANAGER
#include <QSessionManager>
#endif

#endif

#include <QColor>
#include <QCommandLineParser>
#include <QDebug>
#include <QDesktopServices>
#include <QEvent>
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

namespace TrenchBroom::View {
namespace {
QFont fontForStyleSheet(QFont font) {
    // A named face overrides QSS family/weight/style in Qt 5. Keep the selected
    // family, weight and style, letting Qt match the face after QSS is applied.
    font.setStyleName({});
    return font;
}

QString fontWeightForStyleSheet(const QFont& font) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt 5's QSS parser divides numeric weights by eight.
    return QString::number(font.weight() * 8);
#else
    return QString::number(font.weight());
#endif
}

QString fontStyleForStyleSheet(const QFont& font) {
    if (font.style() == QFont::StyleOblique) return "oblique";
    return font.italic() ? "italic" : "normal";
}
}

/* ------------------------------------------------------------------------------------------- */

/**
 * Returns the singleton instance of the application.
 */
TrenchBroomApp &TrenchBroomApp::instance() {
    return *dynamic_cast<TrenchBroomApp *>(qApp);
}

/* ------------------------------------------------------------------------------------------- */


// returns the topmost MapDocument as a shared pointer, or the empty shared pointer
std::shared_ptr<MapDocument> topDocument() {
    if (const auto *frameManager = TrenchBroomApp::instance().frameManager()) {
        if (const auto *frame = frameManager->topFrame()) {
            return frame->document();
        }
    }

    return {};
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
    setAttribute(Qt::AA_Use96Dpi);
    setAttribute(Qt::AA_UseHighDpiPixmaps);

    // Don't show icons in menus, they are scaled down and don't look very good.
    setAttribute(Qt::AA_DontShowIconsInMenus);

    m_signalHandler = SignalHandler();

    #if defined(_WIN32) && defined(_MSC_VER)
    // with MSVC, set our own handler for segfaults so we can access the context
    // pointer, to allow StackWalker to read the backtrace.
    // see also: http://crashrpt.sourceforge.net/docs/html/exception_handling.html
    SetUnhandledExceptionFilter(TrenchBroomUnhandledExceptionFilter);
    #else
    m_signalHandler.setupHandler(CrashHandler);
    #endif

    logger().info() << "Application   : " << applicationName() << getBuildVersion();
    logger().info() << "Build         : " << getBuildIdStr();
    logger().info() << "OS Version    : " << QSysInfo::prettyProductName();
    logger().info() << "Qt Version    : " << qVersion();
    logger().info() << "Log Level     : " << levelAttributes[logger().logLevel()].label.toStdString();


    if (pref(Preferences::DebugMode)) {
        logger().info() << "Debug Mode    : enabled";
    }

    logger().info() << "-";

    std::string colorSpace{};
    switch (pref(Preferences::RendererColorSpace)) {
        case QSurfaceFormat::ColorSpace::DefaultColorSpace:
            colorSpace = "DefaultColorSpace";
            break;
        case QSurfaceFormat::ColorSpace::sRGBColorSpace:
            colorSpace = "sRGBColorSpace";
            break;
        default:
            colorSpace = "UnknownColorSpace";
    }

    std::string swapBehavior{};
    switch (pref(Preferences::RendererSwapBehavior)) {
        case QSurfaceFormat::SwapBehavior::SingleBuffer:
            swapBehavior = "SingleBuffer";
            break;
        case QSurfaceFormat::SwapBehavior::DoubleBuffer:
            swapBehavior = "DoubleBuffer";
            break;
        case QSurfaceFormat::SwapBehavior::TripleBuffer:
            swapBehavior = "TripleBuffer";
            break;
        case QSurfaceFormat::SwapBehavior::DefaultSwapBehavior:
            swapBehavior = "SingleBuffer";
            break;
        default:
            swapBehavior = "UnknownSwapBehavior";
    }

    logger().info() << "Setting SurfaceFormat Options: ";
    logger().info() << "Color Buffer  : " << pref(Preferences::RendererDepthBufferSize) << "bit";
    logger().info() << "Color Space   : " << colorSpace;
    logger().info() << "MSAA Samples  : " << pref(Preferences::RendererSamples);
    logger().info() << "Swap Behavior : " << swapBehavior;
    logger().info() << "Swap Interval : " << pref(Preferences::RendererSwapInterval);
    logger().info() << "-";

    // Needs to be "" otherwise Qt adds this to the paths returned by QStandardPaths
    // which would cause preferences to move from where they were with wx
    setOrganizationName("");
    setOrganizationDomain("io.github.trenchbroom");

    if (!initializeGameFactory()) {
        logger().error() << "Unable to initialize GameFactory!";
        QCoreApplication::exit(1);
        return;
    }

    m_UI_Font = loadFont(pref(Preferences::UIFontPath), uiFontPixelSize());
    m_ConsoleFont = loadFont(pref(Preferences::ConsoleFontPath), pref(Preferences::ConsoleFontSize));
    m_RenderFont = loadFont(pref(Preferences::RendererFontPath), pref(Preferences::RendererFontSize));

    /* ------------------------------------------------------------------------------------------- */
    loadStyle();
    QApplication::setFont(fontForStyleSheet(m_UI_Font));

    if (!loadStyleSheets()) {
        logger().info() << "Shutdown due to init failure.";
        QCoreApplication::exit(1);
        return;
    }

    /* ------------------------------------------------------------------------------------------- */

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
    #ifndef QT_NO_SESSIONMANAGER
    // Cocoa commits the session before sending Quit. Close editor frames before their palettes.
    setFallbackSessionManagementEnabled(false);
    connect(this, &QGuiApplication::commitDataRequest, this, [this](QSessionManager& session) {
        if (session.allowsInteraction()) {
            if (!m_frameManager->closeAllFrames()) {
                session.cancel();
            }
            session.release();
        }
    }, Qt::DirectConnection);
    #endif

    // The parentless macOS menu bar must outlive startup and its registered recent menu.
    m_defaultMenuBar = std::make_unique<QMenuBar>();
    auto actionMap = std::map<const Action *, QAction *>{};

    auto menuBuilder = MainMenuBuilder{
        *m_defaultMenuBar, actionMap,
        [](const Action &action) {
            auto context = ActionExecutionContext{nullptr, nullptr};
            action.execute(context);
        }
    };

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
    delete m_qssBuilder;
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
    constexpr auto contrast = 1.0f; // internal

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
    m_qssLoadedSuccessful = true;

    logger().info() << "Loading Qt dynamic stylesheets from: " << path.relative_path();

    auto pMsgHandler = [](QtMsgType type, const QMessageLogContext &context, const QString &msg) -> void {
        // Qt stylesheet warnings usually contain "QSS" or "StyleSheet"
        if (type == QtWarningMsg) {
            LOG.error("Error loading style: " + msg);
            APP.m_qssErrors.push_back(msg);
            APP.m_qssLoadedSuccessful = false;
        } else {
            // Output to console normally as well
            printf("%s\n", qPrintable(msg));
        }
    };


    if (!m_qssBuilder) {
        if (!(m_qssBuilder = QSSBuilder::fromFile(path))) {
            return false;
        }

        m_qssBuilder->addReplacement("TEXT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::WindowText);
        });

        m_qssBuilder->addReplacement("DISABLED_TEXT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::WindowText, QPalette::ColorGroup::Disabled);
        });

        m_qssBuilder->addReplacement("DISABLED_BUTTON_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Button, QPalette::ColorGroup::Disabled);
        });

        m_qssBuilder->addReplacement("HIGHLIGHTED_TEXT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::HighlightedText);
        });

        m_qssBuilder->addReplacement("BRIGHT_TEXT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::BrightText);
        });

        m_qssBuilder->addReplacement("PLACEHOLDER_TEXT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::PlaceholderText);
        });

        m_qssBuilder->addReplacement("WINDOW_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Window);
        });

        m_qssBuilder->addReplacement("BUTTON_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Button);
        });

        m_qssBuilder->addReplacement("BUTTON_TEXT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::ButtonText);
        });

        m_qssBuilder->addReplacement("BASE_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Base);
        });

        m_qssBuilder->addReplacement("ALTERNATE_BASE_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::AlternateBase);
        });

        m_qssBuilder->addReplacement("LIGHT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Light);
        });

        m_qssBuilder->addReplacement("MIDLIGHT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Midlight);
        });

        m_qssBuilder->addReplacement("MIDHALF_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Mid, 150);
        });

        m_qssBuilder->addReplacement("MID_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Mid);
        });

        m_qssBuilder->addReplacement("DARK_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Dark);
        });

        m_qssBuilder->addReplacement("SHADOW_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Shadow);
        });

        m_qssBuilder->addReplacement("BRIGHT_HIGHLIGHT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Highlight, 130);
        });

        m_qssBuilder->addReplacement("LIGHT_HIGHLIGHT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Highlight, 80);
        });

        m_qssBuilder->addReplacement("HIGHLIGHT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Highlight);
        });

        m_qssBuilder->addReplacement("MID_HIGHLIGHT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Highlight, -180);
        });

        m_qssBuilder->addReplacement("DARK_HIGHLIGHT_COLOR", []() {
            return toStyleSheetRGBA(qApp->palette(), QPalette::ColorRole::Highlight, -200);
        });

        /* ------------------------------------------------------------------------------------------- */

        m_qssBuilder->addReplacement("BORDER_RADIUS", []() {
            return QString::asprintf("%dpx", LayoutConstants::BorderRadius);
        });

        m_qssBuilder->addReplacement("MENU_CORNER_RADIUS", []() {
            return QString::number(TrenchBroomProxyStyle::MenuCornerRadius) + "px";
        });
        m_qssBuilder->addReplacement("TOOLTIP_CORNER_RADIUS", []() {
            return QString::number(TrenchBroomProxyStyle::ToolTipCornerRadius) + "px";
        });

        m_qssBuilder->addReplacement("BORDER_SIZE", []() {
            return QString::asprintf("%dpx", LayoutConstants::BorderSize);
        });

        /* ------------------------------------------------------------------------------------------- */

        m_qssBuilder->addReplacement("NARROW_V_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::NarrowVMargin);
        });

        m_qssBuilder->addReplacement("NARROW_H_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::NarrowHMargin);
        });

        m_qssBuilder->addReplacement("MEDIUM_V_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::MediumVMargin);
        });

        m_qssBuilder->addReplacement("MEDIUM_H_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::MediumHMargin);
        });

        m_qssBuilder->addReplacement("WIDE_V_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::WideVMargin);
        });

        m_qssBuilder->addReplacement("WIDE_H_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::WideHMargin);
        });

        m_qssBuilder->addReplacement("NARROW_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::NarrowHMargin);
        });

        m_qssBuilder->addReplacement("MEDIUM_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::MediumHMargin);
        });

        m_qssBuilder->addReplacement("WIDE_MARGIN", []() {
            return QString::asprintf("%dpx", LayoutConstants::WideHMargin);
        });

        /* ------------------------------------------------------------------------------------------- */

        m_qssBuilder->addReplacement("UI_FONT_SIZE", []() {
            return QString::asprintf("%dpx", uiFontPixelSize());
        });

        m_qssBuilder->addReplacement("UI_MID_FONT_SIZE", []() {
            return QString::asprintf("%dpx", uiFontPixelSize(-1));
        });

        m_qssBuilder->addReplacement("UI_SMALL_FONT_SIZE", []() {
            return QString::asprintf("%dpx", uiFontPixelSize(-2));
        });

        m_qssBuilder->addReplacement("UI_DETAIL_TEXT_COLOR", []() {
            auto color = qApp->palette().color(QPalette::WindowText);
            color.setAlphaF(color.alphaF() * std::clamp(pref(Preferences::UIDetailTextOpacity), 0.0f, 1.0f));
            return toStyleSheetRGBA(color);
        });
        m_qssBuilder->addReplacement("UI_ERROR_COLOR", [] { return toStyleSheetRGBA(toQColor(pref(Preferences::UIErrorColor))); });

        m_qssBuilder->addReplacement("UI_TITLE_FONT_SIZE", []() { return QString::asprintf("%dpx", qRound(uiFontPixelSize() * 1.4)); });
        m_qssBuilder->addReplacement("UI_HEADER_FONT_SIZE", [] { return QString::number(uiFontPixelSize() * 2) + "px"; });
        m_qssBuilder->addReplacement("UI_DIALOG_TITLE_FONT_SIZE", [] { return QString::number(qRound(uiFontPixelSize() * 1.4) + 2) + "px"; });
        m_qssBuilder->addReplacement("UI_FONT_FAMILY", [this] {
            auto family = m_UI_Font.family();
            family.replace('\\', "\\\\").replace('\"', "\\\"");
            return family;
        });
        m_qssBuilder->addReplacement("UI_FONT_WEIGHT", [this] { return fontWeightForStyleSheet(m_UI_Font); });
        m_qssBuilder->addReplacement("UI_FONT_STYLE", [this] { return fontStyleForStyleSheet(m_UI_Font); });
        m_qssBuilder->addReplacement("CONSOLE_FONT_FAMILY", [this] {
            auto family = m_ConsoleFont.family();
            family.replace('\\', "\\\\").replace('\"', "\\\"");
            return family;
        });
        m_qssBuilder->addReplacement("CONSOLE_FONT_WEIGHT", [this] { return fontWeightForStyleSheet(m_ConsoleFont); });
        m_qssBuilder->addReplacement("CONSOLE_FONT_STYLE", [this] { return fontStyleForStyleSheet(m_ConsoleFont); });
        m_qssBuilder->addReplacement("RENDER_FONT_SIZE", []() {
            return QString::asprintf("%dpx", pref(Preferences::RendererFontSize));
        });

        m_qssBuilder->addReplacement("CONSOLE_FONT_SIZE", []() {
            return QString::asprintf("%dpx", pref(Preferences::ConsoleFontSize));
        });

        m_qssBuilder->addReplacement("TOOLBAR_ICON_SIZE", []() {
            return QString::asprintf("%dpx", pref(Preferences::ToolBarIconsSize));
        });
    }

    m_qssBuilder->update();
    m_qssLoadedSuccessful = !m_qssBuilder->getRenderedText().isEmpty();

    //! install the interceptor hook to monitor startup
    auto oldHandler = qInstallMessageHandler(pMsgHandler);

    qApp->style()->unpolish(qApp);
    qApp->setStyleSheet(m_qssBuilder->getRenderedText());

    //! just a dummy to create a widget which initialized
    //! style sheets to see if it was loaded correctly
    auto test = QMenu{};

    //! restore the old message handler
    qInstallMessageHandler(oldHandler);

    if (!m_qssLoadedSuccessful) {
        QString msg =
            QString{"Error while loading stylesheets from: "} +
            QString{path.filename().string().c_str()} + '\n';

        for (const auto &item : m_qssErrors) {
            msg += "warning: " + item + "\n";
        }

        auto ret = QMessageBox::critical(
            &test, qApp->applicationName(), msg,
            QMessageBox::Abort | QMessageBox::Ignore,
            QMessageBox::Abort
        );

        return ret == QMessageBox::Ignore;
    }

    return true;
}

void TrenchBroomApp::loadStyle() {
    logger().info() << "Setup Style and Palette...";
    delete m_qssBuilder;
    m_qssBuilder = nullptr;

    m_proxyStyle = new TrenchBroomProxyStyle{"Fusion"};
    setStyle(m_proxyStyle);
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
            const auto msg = fmt::format(R"(Some game configurations could not be loaded. The following errors occurred: {})", kdl::str_join(errors, "\n\n"));

            QMessageBox::critical(nullptr, "TrenchBroom", QString::fromStdString(msg), QMessageBox::Ok);
            defaultQtLogger.error() << msg;
        }
    }).if_error([](auto e) { defaultQtLogger.error() << e.msg; }).is_success();
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
    const auto repPath = IO::SystemPaths::userDataDirectory() / "crashreport.txt";
    const auto mapPath = IO::SystemPaths::userDataDirectory() / "crashreport.map";
    const auto logPath = IO::SystemPaths::userDataDirectory() / "crashreport.log";

    auto *report = new CrashReport{
        SignalType(0xEC, "SIGDEBUG", "Call debug crash report"),
        repPath,
        logPath,
        mapPath
    };

    auto dialog = CrashDialog{report};
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
        TrenchBroom::View::reportCrashAndExit("<uncaught exception>", SignalType(0xEC, "SIGEXCEPT", e.what()));
    }
#endif
}

bool TrenchBroomApp::event(QEvent *event) {
    if (event->type() == QEvent::Quit && m_frameManager && !m_frameManager->closeAllFrames()) {
        event->ignore();
        return true;
    }
#ifdef __APPLE__
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
#endif
    return QApplication::event(event);
}

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
    return true;
#endif
}

void TrenchBroomApp::reloadStyle(bool reloadFonts, bool reloadStyleSheets) {
    if (reloadFonts) {
        m_UI_Font = loadFont(pref(Preferences::UIFontPath), uiFontPixelSize());
        m_ConsoleFont = loadFont(pref(Preferences::ConsoleFontPath), pref(Preferences::ConsoleFontSize));
        m_RenderFont = loadFont(pref(Preferences::RendererFontPath), pref(Preferences::RendererFontSize));
    }

    loadStyle();
    QApplication::setFont(fontForStyleSheet(m_UI_Font));
    if (reloadStyleSheets) loadStyleSheets();

    for (auto* widget : QApplication::allWidgets()) {
        // Hidden controls can otherwise retain font attributes from the previous
        // stylesheet after the underlying application style has been replaced.
        repolishWidget(widget);
    }
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

QFont TrenchBroomApp::loadFont(const std::filesystem::path& path, int size) const {
    return View::loadFont(path, size);
}

MapFrame *TrenchBroomApp::getCurrentMapFrame() const {
    return m_currentMapFrame;
}

void TrenchBroomApp::setCurrentMapFrame(MapFrame *currentMapFrame) {
    TrenchBroomApp::m_currentMapFrame = currentMapFrame;
}

Logger &TrenchBroomApp::logger() const {
    return defaultQtLogger;
}

QProxyStyle *TrenchBroomApp::getQProxyStyle() const {
    return m_proxyStyle;
}

void TrenchBroomApp::setQProxyStyle(QProxyStyle *proxyStyle) {
    TrenchBroomApp::m_proxyStyle = proxyStyle;
}

std::stringstream makeCrashReport(const std::string &stacktrace, const std::string &reason) {
    auto ss = std::stringstream{};

    ss << std::endl;
    ss << "Reason              : " << reason << std::endl;
    ss << "TrenchBroom Version : " << getBuildVersion().toStdString() << std::endl;
    ss << "TrenchBroom Build   : " << getBuildIdStr().toStdString() << std::endl;
    ss << "Qt Version          : " << qVersion() << std::endl;
    ss << "Operating System    : " << QSysInfo::prettyProductName().toStdString() << std::endl;
    ss << "Thread              : " << QThread::currentThread()->currentThreadId() << std::endl;

    if (GLContextManager::isInitialized()) {
        ss << "OpenGL Vendor       : " << GLContextManager::GLVendor << std::endl;
        ss << "OpenGL Renderer     : " << GLContextManager::GLRenderer << std::endl;
        ss << "OpenGL Version      : " << GLContextManager::GLVersion << std::endl;
    } else {
        ss << "OpenGL              : " << "<not yet initialized>" << std::endl;
    }

    ss << "--- [ execution stack trace begin ] ---" << std::endl;
    ss << stacktrace;
    ss << "--- [ execution stack trace end ] ---" << std::endl;

    return ss;
}

std::filesystem::path findCrashReportBasePath() {
    const auto document = topDocument();
    const auto prefix = "trenchbroom-crash";

    if (document && document->path().is_absolute()) {
        return document->path().parent_path() / (document->path().stem().string() + "-" + prefix);
    } else {
        const auto documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation).toStdString();
        return std::filesystem::path{documentsPath} / prefix;
    }
}

std::filesystem::path crashReportBasePath() {
    const auto timestamp =
        QDateTime::currentDateTime().
            toString("yyyyMMdd_hhmmsszzz").
            toStdString();

    const auto basePath = findCrashReportBasePath();
    return (basePath.string() + "-" + timestamp);
}

bool inReportCrashAndExit = false;
bool crashReportGuiEnabled = true;

/**
 *
 * @param stacktrace
 * @param reason
 */
void reportCrashAndExit(const std::string &stacktrace, const SignalType &signalType) {
    // just abort if we reenter reportCrashAndExit (i.e. if it crashes)
    if (inReportCrashAndExit) {
        std::abort();
    }
    inReportCrashAndExit = true;

    // get the crash report as a string
    defaultQtLogger.error() << "FATAL: Abort due to application failure: " << signalType.name << ":";

    std::string line;
    std::string report{};
    auto ss = makeCrashReport(stacktrace, signalType.name);

    while (std::getline(ss, line)) {
        defaultQtLogger.error() << line;
        report += line + '\n';
    }

    // write it to the crash log file
    const auto basePath = crashReportBasePath();

    // ensure the containing directory exists
    IO::Disk::createDirectory(basePath.parent_path()).transform([&](auto) {
        auto repPath = kdl::path_add_extension(basePath, ".txt");
        auto logPath = kdl::path_add_extension(basePath, ".log");
        auto mapPath = kdl::path_add_extension(basePath, ".map");

        IO::Disk::withOutputStream(repPath, [&](auto &stream) {
            stream << report;
            defaultQtLogger.error() << "wrote crash log to " << repPath.string();
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
            qApp->style()->unpolish(qApp);
            qApp->setStyleSheet("");

            auto crashReport = new CrashReport(signalType, repPath, mapPath, logPath);
            crashReport->stackTrace = QString::fromStdString(ss.str());

            auto dialog = CrashDialog{crashReport};
            dialog.setWindowFlag(Qt::Tool, true);
            dialog.exec();
        }
    }).transform_error([](const auto &e) {
        defaultQtLogger.error() << "could not create crash folder: " << e.msg;
    });

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

static void CrashHandler(int signum) {
    TrenchBroom::View::reportCrashAndExit(
        TrenchBroom::TrenchBroomStackWalker::getStackTrace(), gSignalHandler.sigList[signum]
    );
}

#endif

SignalType::SignalType(int signalNum, const std::string &name, const std::string &description)
    : signal(signalNum), name(name), description(description) {}

std::string SignalType::toString() const {
    return stringf("Signal(0x%X, %s, »%s«)", signal, name.c_str(), description.c_str());
}

bool SignalHandler::setupHandler(void (*func)(int)) {
    bool result = true;

    for (const auto &item : sigList) {
        auto res = signal(item.second.signal, func);
        if (res == SIG_ERR) {
            defaultQtLogger.warn()
                << "Unable to setup signal handler: error #"
                << errno
                << " while setup "
                << item.second.toString();
            result = false;
        }

        defaultQtLogger.debug() << "Setup signal handler for: " << item.second.name.c_str();
    }

    return result;
}

CrashReport::CrashReport(const SignalType &signalType, const std::filesystem::path &reportPath, const std::filesystem::path &logPath, const std::filesystem::path &mapPath)
    : signalType(signalType), reportPath(reportPath), logPath(logPath), mapPath(mapPath), stackTrace("-"), logContent("-") {}
}
