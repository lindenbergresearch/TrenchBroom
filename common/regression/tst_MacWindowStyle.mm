#include "View/Inspector/FloatingInspectorTool.h"
#include "View/Theme/TrenchBroomProxyStyle.h"
#include "View/Theme/ThemeManager.h"
#include "TestSourcePath.h"

#include <QApplication>
#include <QLineEdit>
#include <QFontDatabase>
#include <QMainWindow>
#include <QScopeGuard>
#include <QStyleFactory>
#include <QTest>
#include <QVBoxLayout>
#include <QWindow>

#import <AppKit/AppKit.h>

#include "Catch2.h"

namespace TrenchBroom::View {
namespace {
NSWindow* nativeWindow(QWidget& widget) {
    auto* handle = widget.windowHandle();
    if (!handle) return nil;
    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(handle->winId());
    return view.window;
}

void checkTitleBar(QWidget& widget, const QColor& expected) {
    NSWindow* window = nativeWindow(widget);
    REQUIRE(window != nil);
    CHECK(window.titlebarAppearsTransparent);
    NSColor* color = [window.backgroundColor colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
    REQUIRE(color != nil);
    CHECK(color.redComponent == Approx(expected.redF()).margin(0.005));
    CHECK(color.greenComponent == Approx(expected.greenF()).margin(0.005));
    CHECK(color.blueComponent == Approx(expected.blueF()).margin(0.005));
    const auto name = expected.lightnessF() < 0.5 ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua;
    CHECK([window.appearance.name isEqualToString:name]);
}

void focus(QWidget& widget) {
    [NSApp activate];
    [nativeWindow(widget) makeKeyAndOrderFront:nil];
    widget.activateWindow();
    QTest::qWait(150);
    REQUIRE(QGuiApplication::focusWindow() == widget.windowHandle());
}
}

TEST_CASE("Native title bars follow focus and theme changes without repainting", "[.native-titlebar][theme]") {
    REQUIRE(QGuiApplication::platformName() == "cocoa");
    const auto originalPalette = qApp->palette();
    const auto originalSheet = qApp->styleSheet();
    const auto originalStyle = qApp->style()->objectName();
    const auto originalFont = qApp->font();
    const auto originalTheme = ThemeManager::instance().current();
    const auto restore = qScopeGuard([&] {
        ThemeManager::instance().publish(originalTheme);
        qApp->setStyleSheet({});
        qApp->setStyle(QStyleFactory::create(originalStyle));
        qApp->setPalette(originalPalette);
        qApp->setFont(originalFont);
        qApp->setStyleSheet(originalSheet);
    });
    ThemeManager::instance().publish({});
    qApp->setStyleSheet({});
    auto* proxy = new TrenchBroomProxyStyle("Fusion");
    qApp->setStyle(proxy);
    auto palette = originalPalette;
    const QColor dark("#203040"), light("#dce3e8");
    palette.setColor(QPalette::Window, dark);
    qApp->setPalette(palette);
    QMainWindow editor;
    editor.setWindowTitle("Title bar regression: editor");
    editor.resize(400, 200);
    FloatingInspectorTool tool(&editor, [] {}, {[] {}, [](const QPoint&) {}, [](const QPoint&) {}, [] {}});
    tool.setToolTitle("Title bar regression: inspector");
    tool.layout()->addWidget(new QLineEdit("Tool focus"));
    editor.show();
    tool.show();
    QTest::qWait(150);
    checkTitleBar(editor, dark);
    checkTitleBar(tool, dark);
    editor.setUpdatesEnabled(false);
    tool.setUpdatesEnabled(false);
    for (int i = 0; i < 3; ++i) {
        focus(editor);
        checkTitleBar(editor, dark);
        checkTitleBar(tool, dark);
        // Reproduce native chrome being reset while the editor is not painted.
        nativeWindow(editor).titlebarAppearsTransparent = NO;
        nativeWindow(editor).backgroundColor = NSColor.whiteColor;
        focus(tool);
        checkTitleBar(editor, dark);
        checkTitleBar(tool, dark);
    }
    palette.setColor(QPalette::Window, light);
    qApp->setPalette(palette);
    QTest::qWait(150);
    checkTitleBar(editor, light);
    checkTitleBar(tool, light);
    tool.setWindowFlag(Qt::WindowStaysOnTopHint, true);
    tool.show();
    QTest::qWait(150);
    checkTitleBar(tool, light);
    // Cocoa can reset chrome after Qt's queued style/focus work has finished.
    // A native window update must restore it without another QWidget event.
    for (QWidget* window : {static_cast<QWidget*>(&editor), static_cast<QWidget*>(&tool)}) {
        nativeWindow(*window).titlebarAppearsTransparent = NO;
        nativeWindow(*window).backgroundColor = NSColor.brownColor;
        nativeWindow(*window).appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
        [nativeWindow(*window) update];
        checkTitleBar(*window, light);
    }
    auto font = originalFont;
    font.setPointSize(17);
    qApp->setFont(font);
    tool.findChild<QLineEdit*>()->setFont(QFont("Menlo", 14));
    qApp->setStyleSheet("QLineEdit { font-size: 18px; } QMainWindow { background-color: #987654; }");
    const auto fontId = QFontDatabase::addApplicationFont(QString::fromStdString(
        (testSourceRoot() / "app/resources/fonts/bebas-neue/BebasNeue-Regular.ttf").string()));
    REQUIRE(fontId >= 0);
    const auto removeFont = qScopeGuard([&] { QFontDatabase::removeApplicationFont(fontId); });
    // A native-derived widget palette must not overwrite the theme's desired
    // chrome color during font selection or stylesheet replacement.
    auto theme = originalTheme;
    theme.values["color.window"] = ThemeValue::color(dark);
    ThemeManager::instance().publish(std::move(theme));
    proxy->refreshWindowChrome();
    QTest::qWait(150);
    checkTitleBar(editor, dark);
    checkTitleBar(tool, dark);
    nativeWindow(editor).backgroundColor = NSColor.brownColor;
    [nativeWindow(editor) update];
    checkTitleBar(editor, dark);
}
}
