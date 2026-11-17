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

#include "View/FloatingInspectorTool.h"
#include <QPointer>

namespace TrenchBroom::View {
std::function<void()> installMacToolTitleHandler(
    QWidget& window, std::function<bool(ToolTitleEvent, const QPoint&)> handler, bool alreadyPressed) {
    __weak NSView* nativeView = (__bridge NSView*)reinterpret_cast<void*>(window.winId());
    if (!nativeView.window) return {};
    const QPointer<QWidget> guard(&window);
    __block bool tracking = alreadyPressed;
    id monitor = [NSEvent addLocalMonitorForEventsMatchingMask:
        NSEventMaskLeftMouseDown | NSEventMaskLeftMouseDragged | NSEventMaskLeftMouseUp | NSEventMaskKeyDown
        handler:^NSEvent*(NSEvent* event) {
            NSView* view = nativeView;
            NSWindow* source = view.window;
            if (!guard || !source) return event;
            const auto globalPosition = [&] {
                auto location = event.locationInWindow;
                if (event.window != source) {
                    if (event.window) location = [event.window convertPointToScreen:location];
                    location = [source convertPointFromScreen:location];
                }
                const auto point = [view convertPoint:location fromView:nil];
                return guard->mapToGlobal(QPoint(qRound(point.x), qRound(point.y)));
            };
            if (event.type == NSEventTypeLeftMouseDown) {
                if (alreadyPressed || tracking || event.window != source || event.clickCount != 1) return event;
                const auto location = event.locationInWindow;
                const auto content = [source.contentView convertRect:source.contentView.bounds toView:nil];
                if (location.y < NSMaxY(content)) return event;
                // Leave the native close/minimize/zoom controls to AppKit.
                for (const auto buttonType : {NSWindowCloseButton, NSWindowMiniaturizeButton, NSWindowZoomButton}) {
                    NSButton* button = [source standardWindowButton:buttonType];
                    if (button && !button.hidden && NSPointInRect(location, [button convertRect:button.bounds toView:nil]))
                        return event;
                }
                tracking = handler(ToolTitleEvent::Press, globalPosition());
                return tracking ? nil : event;
            }
            if (!tracking) return event;
            if (event.type == NSEventTypeLeftMouseDragged) {
                tracking = handler(ToolTitleEvent::Move, globalPosition());
                return tracking ? nil : event;
            }
            if (event.type == NSEventTypeLeftMouseUp) {
                tracking = false;
                return handler(ToolTitleEvent::Release, globalPosition()) ? nil : event;
            }
            if (event.type == NSEventTypeKeyDown && event.keyCode == 53) {
                tracking = false;
                return handler(ToolTitleEvent::Cancel, {}) ? nil : event;
            }
            return event;
        }];
    auto* notifications = NSNotificationCenter.defaultCenter;
    id resign = [notifications addObserverForName:NSApplicationDidResignActiveNotification object:NSApp queue:nil
        usingBlock:^(NSNotification*) {
            if (!guard || !tracking) return;
            tracking = false;
            handler(ToolTitleEvent::Cancel, {});
        }];
    return [monitor, notifications, resign] {
        [NSEvent removeMonitor:monitor];
        [notifications removeObserver:resign];
    };
}
}
