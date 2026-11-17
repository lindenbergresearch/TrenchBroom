#include "View/FloatingInspectorTool.h"

#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QSizeGrip>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

namespace TrenchBroom::View {
namespace {
Qt::WindowFlags toolFlags() {
#ifdef __APPLE__
    return Qt::Tool | Qt::WindowCloseButtonHint;
#else
    // Server-drawn decorations on X11/Wayland do not deliver their mouse events
    // to widgets. Provide a single window-level drag handle on those platforms.
    return Qt::Tool | Qt::FramelessWindowHint;
#endif
}
}

FloatingInspectorTool::FloatingInspectorTool(
    QWidget* parent, std::function<void()> redock, DragCallbacks callbacks)
    : QWidget(parent, toolFlags()), m_redock(std::move(redock)), m_callbacks(std::move(callbacks)) {
    auto* box = new QVBoxLayout(this);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(0);
#ifndef __APPLE__
    m_chrome = new QWidget(this);
    m_chrome->setObjectName("InspectorWindowTitle");
    auto* row = new QHBoxLayout(m_chrome);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    m_caption = new QLabel(m_chrome);
    m_caption->setObjectName("InspectorWindowCaption");
    m_caption->setAttribute(Qt::WA_TransparentForMouseEvents);
    row->addWidget(m_caption, 1);
    auto* closeButton = new QToolButton(m_chrome);
    closeButton->setText(QStringLiteral("×"));
    closeButton->setToolTip(tr("Dock tool back"));
    connect(closeButton, &QToolButton::clicked, this, &QWidget::close);
    row->addWidget(closeButton);
    row->addWidget(new QSizeGrip(m_chrome));
    box->addWidget(m_chrome);
    m_chrome->installEventFilter(this);
#endif
}

FloatingInspectorTool::~FloatingInspectorTool() {
    if (m_removeNativeHandler) m_removeNativeHandler();
    if (m_pressed && qApp) qApp->removeEventFilter(this);
}

void FloatingInspectorTool::setToolTitle(const QString& title) {
    setWindowTitle(title);
    if (m_caption) m_caption->setText(title);
}

void FloatingInspectorTool::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
#ifdef __APPLE__
    if (QGuiApplication::platformName() == "cocoa" && !m_removeNativeHandler) {
        m_removeNativeHandler = installMacToolTitleHandler(*this, [this](auto type, const auto& point) {
            return titleEvent(type, point);
        });
    }
#endif
}

void FloatingInspectorTool::hideEvent(QHideEvent* event) {
    if (m_removeNativeHandler) {
        auto remove = std::exchange(m_removeNativeHandler, {});
        remove();
    }
    cancelTitleDrag();
    QWidget::hideEvent(event);
}

void FloatingInspectorTool::closeEvent(QCloseEvent* event) {
    cancelTitleDrag();
    m_redock();
    event->accept();
}

void FloatingInspectorTool::cancelTitleDrag() {
    if (!m_pressed) return;
    m_pressed = false;
    m_dragging = false;
    if (m_chrome && qApp) qApp->removeEventFilter(this);
    move(m_startWindow);
    m_callbacks.cancelled();
}

bool FloatingInspectorTool::titleEvent(ToolTitleEvent type, const QPoint& position) {
    if (type == ToolTitleEvent::Press) {
        m_pressed = true;
        m_startPointer = position;
        m_startWindow = pos();
        if (m_chrome) qApp->installEventFilter(this);
        m_callbacks.pressed();
        return true;
    }
    if (!m_pressed) return false;
    if (type == ToolTitleEvent::Move) {
        const auto delta = position - m_startPointer;
        if (m_dragging || delta.manhattanLength() >= QApplication::startDragDistance()) {
            m_dragging = true;
            move(m_startWindow + delta);
            m_callbacks.moved(position);
        }
    } else if (type == ToolTitleEvent::Release) {
        const bool dragged = std::exchange(m_dragging, false);
        m_pressed = false;
        if (m_chrome) qApp->removeEventFilter(this);
        if (dragged) m_callbacks.finished(position);
        else m_callbacks.cancelled();
    } else {
        cancelTitleDrag();
    }
    return true;
}

bool FloatingInspectorTool::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_chrome && event->type() == QEvent::MouseButtonPress
        && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
        return titleEvent(ToolTitleEvent::Press, static_cast<QMouseEvent*>(event)->globalPos());
    }
    if (m_pressed && m_chrome) {
        if (event->type() == QEvent::MouseMove) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (!mouse->buttons().testFlag(Qt::LeftButton)) {
                cancelTitleDrag();
                return false;
            }
            return titleEvent(ToolTitleEvent::Move, mouse->globalPos());
        }
        if (event->type() == QEvent::MouseButtonRelease
            && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton)
            return titleEvent(ToolTitleEvent::Release, static_cast<QMouseEvent*>(event)->globalPos());
        if (event->type() == QEvent::ApplicationDeactivate
            || (watched == m_chrome && event->type() == QEvent::UngrabMouse)) {
            cancelTitleDrag();
            return false;
        }
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)
            return titleEvent(ToolTitleEvent::Cancel, {});
    }
    return QWidget::eventFilter(watched, event);
}
}
