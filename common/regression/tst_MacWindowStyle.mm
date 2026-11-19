#include "View/Inspector/FloatingInspectorTool.h"
#include "View/Theme/TrenchBroomProxyStyle.h"

#include <QApplication>
#include <QLineEdit>
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
    const auto restore = qScopeGuard([&] {
        qApp->setStyleSheet({});
        qApp->setStyle(QStyleFactory::create(originalStyle));
        qApp->setPalette(originalPalette);
        qApp->setStyleSheet(originalSheet);
    });
    qApp->setStyleSheet({});
    qApp->setStyle(new TrenchBroomProxyStyle("Fusion"));
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
}
}
