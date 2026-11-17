#pragma once

#include <QWidget>

#include <functional>

class QLabel;

namespace TrenchBroom::View {
enum class ToolTitleEvent { Press, Move, Release, Cancel };

class FloatingInspectorTool : public QWidget {
public:
    struct DragCallbacks {
        std::function<void()> pressed;
        std::function<void(const QPoint&)> moved;
        std::function<void(const QPoint&)> finished;
        std::function<void()> cancelled;
    };

    FloatingInspectorTool(QWidget* parent, std::function<void()> redock, DragCallbacks callbacks);
    ~FloatingInspectorTool() override;
    void setToolTitle(const QString& title);
    void cancelTitleDrag();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    std::function<void()> m_redock;
    DragCallbacks m_callbacks;
    std::function<void()> m_removeNativeHandler;
    QWidget* m_chrome = nullptr;
    QLabel* m_caption = nullptr;
    bool m_pressed = false;
    bool m_dragging = false;
    QPoint m_startPointer;
    QPoint m_startWindow;

    bool titleEvent(ToolTitleEvent type, const QPoint& position);
};

#ifdef __APPLE__
std::function<void()> installMacToolTitleHandler(
    QWidget& window, std::function<bool(ToolTitleEvent, const QPoint&)> handler, bool alreadyPressed = false);
#endif
}
