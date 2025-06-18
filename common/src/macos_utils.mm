#include "macos_utils.h"
#include <QMainWindow>
#include <QWindow>
#include <AppKit/AppKit.h>

/**
 * @brief Sets the title bar color of a QMainWindow on macOS.
 *
 * This function customizes the title bar of a QMainWindow to appear transparent
 * and applies a specified background color. It utilizes macOS-specific APIs to
 * achieve this effect, and only works on macOS 10.14 or newer.
 *
 * @param window Pointer to the QWindow whose title bar should be customized.
 * @param color QColor representing the desired background color of the title bar.
 *
 * @note This function is only applicable on macOS and requires Qt 5.15 or newer.
 *       It uses ARC (Automatic Reference Counting) for memory management.
 */
void setWindowTitleBarColor(QWindow* window, QColor color) {

    if (!window) {
        qWarning("Failed to retrieve QWindow handle.");
        return;
    }

    // Retrieve the native QNSView handle from the Qt window ID
    WId nativeId = window->winId();
    NSView* qnsView = (__bridge NSView*)reinterpret_cast<void*>(nativeId);

    // Retrieve the parent NSWindow from the QNSView
    NSWindow* nsWindow = qnsView.window;
    if (!nsWindow) {
        qWarning("Failed to retrieve native NSWindow.");
        return;
    }

    // Convert QColor to NSColor
    NSColor* nsColor = [NSColor colorWithRed:color.redF()
                                       green:color.greenF()
                                        blue:color.blueF()
                                       alpha:1.0];

    // Set the title bar to appear transparent and apply the background color
    if (@available(macOS 10.14, *)) {
        [nsWindow setTitlebarAppearsTransparent:YES];
        [nsWindow setBackgroundColor:nsColor];
    } else {
        qWarning("setTitlebarAppearsTransparent is not available on macOS < 10.14.");
    }
}

/**
 * @brief Sets the title bar color of a QMainWindow on macOS.
 *
 * This function customizes the title bar of a QMainWindow to appear transparent
 * and applies a specified background color. It utilizes macOS-specific APIs to
 * achieve this effect, and only works on macOS 10.14 or newer.
 *
 * @param mainWindow Pointer to the QMainWindow whose title bar should be customized.
 * @param color QColor representing the desired background color of the title bar.
 *
 * @note This function is only applicable on macOS and requires Qt 5.15 or newer.
 *       It uses ARC (Automatic Reference Counting) for memory management.
 */
void setWindowTitleBarColor(QMainWindow* mainWindow, QColor color) {
    // Get the QWindow handle from the QMainWindow
    QWindow* window = mainWindow->windowHandle();
    setWindowTitleBarColor(window, color);
}
