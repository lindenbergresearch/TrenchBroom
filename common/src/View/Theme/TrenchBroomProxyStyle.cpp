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

#include "View/Theme/TrenchBroomProxyStyle.h"
#include "IO/VectorIcon.h"
#include "View/Theme/ThemeManager.h"
#include "View/Theme/QtUtils.h"
#include "View/Preferences/FontUtils.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#ifdef __APPLE__
#include "macos_utils.h"
#endif

#include <QApplication>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QTextDocument>
#include <QTabBar>
#include <QStyleOption>
#include <utility>

namespace TrenchBroom::View {
namespace {
bool isStyledPopup(const QWidget* widget) {
    return widget && (qobject_cast<const QMenu*>(widget) || widget->windowType() == Qt::ToolTip);
}
}

TrenchBroomProxyStyle::TrenchBroomProxyStyle(const QString& key) : QProxyStyle(key) { initializePolicies(); }
TrenchBroomProxyStyle::TrenchBroomProxyStyle(QStyle* style) : QProxyStyle(style) { initializePolicies(); }

void TrenchBroomProxyStyle::initializePolicies() {
    m_typographyTimer.setSingleShot(true);
    connect(&m_typographyTimer, &QTimer::timeout, this, [this] {
        const auto pending = std::exchange(m_pendingTypography, {});
        for (const auto& widget : pending) if (widget) {
            widget->setProperty("tbTypographyQueued", false);
            updateTypography(widget);
        }
    });
#ifdef __APPLE__
    m_titleBarTimer.setSingleShot(true);
    connect(&m_titleBarTimer, &QTimer::timeout, this, [this] {
        const auto pending = std::exchange(m_pendingTitleBars, {});
        for (const auto& widget : pending)
            if (widget && widget->isVisible() && widget->windowHandle())
                setWindowTitleBarColor(widget->windowHandle(), widget->palette().color(QPalette::Active, QPalette::Window));
    });
    // Tool panels can take native focus while Qt still considers their parent
    // active. Refresh both windows, after Cocoa/Qt finish changing activation.
    connect(qApp, &QGuiApplication::focusWindowChanged, this, [this] { refreshTitleBars(); });
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this] { refreshTitleBars(); });
#endif
}

#ifdef __APPLE__
void TrenchBroomProxyStyle::queueTitleBar(QWidget* widget) {
    if (QGuiApplication::platformName() != "cocoa" || !widget || !widget->isWindow()
        || widget->windowFlags().testFlag(Qt::FramelessWindowHint)
        || widget->windowType() == Qt::Popup || widget->windowType() == Qt::ToolTip) return;
    if (!m_pendingTitleBars.contains(widget)) m_pendingTitleBars.append(widget);
    m_titleBarTimer.start(0);
}

void TrenchBroomProxyStyle::refreshTitleBars() {
    for (auto* widget : QApplication::topLevelWidgets()) queueTitleBar(widget);
}
#endif

void TrenchBroomProxyStyle::queueTypography(QWidget* widget) {
    if (m_updatingTypography || widget->property("tbTypographyQueued").toBool()) return;
    widget->setProperty("tbTypographyQueued", true);
    m_pendingTypography.append(widget);
    m_typographyTimer.start(0);
}

void TrenchBroomProxyStyle::updateTypography(QWidget* widget) {
    if (!widget || m_updatingTypography || !PreferenceManager::isInitialized()) return;
    const bool active = pref(Preferences::UIGlobalTypography);
    if (!active && !widget->property("tbTypographyManaged").toBool()) return;
    const auto textWidget = qobject_cast<QLabel*>(widget) || qobject_cast<QAbstractButton*>(widget)
        || qobject_cast<QLineEdit*>(widget) || qobject_cast<QAbstractItemView*>(widget)
        || qobject_cast<QAbstractSpinBox*>(widget) || qobject_cast<QComboBox*>(widget)
        || qobject_cast<QMenu*>(widget) || qobject_cast<QMenuBar*>(widget)
        || qobject_cast<QGroupBox*>(widget) || qobject_cast<QTabBar*>(widget)
        || qobject_cast<QTextEdit*>(widget) || qobject_cast<QPlainTextEdit*>(widget)
        || widget->objectName() == "ItemCellStyle" || widget->objectName() == "LayerTreeRow";
    if (!textWidget) return;
    auto font = widget->font();
    if (active) {
        // Rich-text formats resolve bold/italic per fragment. Do not make a
        // default weight variation override those document-owned formats.
        bool weightAxis = !qobject_cast<QTextEdit*>(widget) && !qobject_cast<QPlainTextEdit*>(widget);
        if (const auto* label = qobject_cast<QLabel*>(widget))
            weightAxis = label->textFormat() == Qt::PlainText
                || (label->textFormat() == Qt::AutoText && !Qt::mightBeRichText(label->text()));
        font = styledTextFont(font, listFontSettings(), weightAxis);
    } else {
        font.unsetVariableAxis("wght");
        font.unsetVariableAxis("opsz");
        QFont axisMask;
        axisMask.setVariableAxis("wght", 400);
        font.setResolveMask(font.resolveMask() & ~axisMask.resolveMask());
    }
    widget->setProperty("tbTypographyManaged", active);
    if (font == widget->font() && font.resolveMask() == widget->font().resolveMask()) return;
    m_updatingTypography = true;
    const QPointer<QWidget> guard{widget};
    widget->setFont(font);
    if (guard) {
        guard->updateGeometry();
        guard->update();
    }
    m_updatingTypography = false;
}

void TrenchBroomProxyStyle::refreshTypography() {
    QList<QPointer<QWidget>> widgets;
    for (auto* widget : QApplication::allWidgets()) widgets.append(widget);
    for (const auto& widget : widgets) if (widget) updateTypography(widget);
}

void TrenchBroomProxyStyle::drawItemText(QPainter* painter, const QRect& rect, const int flags,
    const QPalette& palette, const bool enabled, const QString& text, const QPalette::ColorRole role) const {
    if (!PreferenceManager::isInitialized() || !pref(Preferences::UIGlobalTypography)) {
        QProxyStyle::drawItemText(painter, rect, flags, palette, enabled, text, role);
        return;
    }
    const auto settings = listFontSettings();
    painter->save();
    painter->setFont(styledTextFont(painter->font(), settings));
    painter->setRenderHint(QPainter::TextAntialiasing);
    painter->setRenderHint(QPainter::VerticalSubpixelPositioning, settings.subpixelPositioning);
    // Keep Qt's wrapping, alignment and mnemonic handling for standard controls.
    QProxyStyle::drawItemText(painter, rect, flags, palette, enabled, text, role);
    painter->restore();
}

QPixmap TrenchBroomProxyStyle::generatedIconPixmap(QIcon::Mode mode, const QPixmap& pixmap, const QStyleOption* option) const {
    return mode == QIcon::Disabled ? IO::disabledIconPixmap(pixmap) : QProxyStyle::generatedIconPixmap(mode,pixmap,option);
}

void TrenchBroomProxyStyle::polish(QWidget* widget) {
    QProxyStyle::polish(widget);
    widget->installEventFilter(this);
#ifdef __APPLE__
    queueTitleBar(widget);
#endif
    if (PreferenceManager::isInitialized() && pref(Preferences::UIGlobalTypography)) queueTypography(widget);
    if (!isStyledPopup(widget)) return;
    if (auto* menu = qobject_cast<QMenu*>(widget);
        menu && !menu->findChild<QObject*>("ContextMenuPolicy",Qt::FindDirectChildrenOnly)) {
        auto* policy = new QObject(menu); policy->setObjectName("ContextMenuPolicy");
        connect(menu,&QMenu::aboutToShow,policy,[menu] { prepareContextMenu(*menu); });
    }

    // Apply before the native window is created, for every menu and tooltip, including
    // submenus and popups created after application startup. Preserve Qt::Popup/ToolTip.
    if (!widget->windowFlags().testFlag(Qt::FramelessWindowHint))
        widget->setWindowFlag(Qt::FramelessWindowHint, true);
    widget->setAttribute(Qt::WA_TranslucentBackground);
    widget->setAttribute(Qt::WA_OpaquePaintEvent, false);
    widget->setAutoFillBackground(false);
    widget->clearMask();
    widget->installEventFilter(this);
}

void TrenchBroomProxyStyle::unpolish(QWidget* widget) {
    widget->removeEventFilter(this);
    QProxyStyle::unpolish(widget);
}

bool TrenchBroomProxyStyle::eventFilter(QObject* watched, QEvent* event) {
#ifdef __APPLE__
    if (event->type() == QEvent::Show || event->type() == QEvent::WindowActivate
        || event->type() == QEvent::WindowDeactivate || event->type() == QEvent::PaletteChange
        || event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::StyleChange
        || event->type() == QEvent::WinIdChange)
        queueTitleBar(qobject_cast<QWidget*>(watched));
#endif
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange
        || event->type() == QEvent::Show || event->type() == QEvent::PolishRequest) {
        if (auto* widget = qobject_cast<QWidget*>(watched); PreferenceManager::isInitialized()
            &&
            widget && (pref(Preferences::UIGlobalTypography) || widget->property("tbTypographyManaged").toBool()))
            queueTypography(widget);
    }
    if (event->type() == QEvent::Show || event->type() == QEvent::Resize || event->type() == QEvent::StyleChange) {
        if (auto* widget = qobject_cast<QWidget*>(watched); isStyledPopup(widget)) {
            // Remove stale binary masks when a reused tooltip changes text or size.
            // The alpha channel provides smooth edges at any display scale.
            widget->clearMask();
        }
    }
    return QProxyStyle::eventFilter(watched, event);
}

int TrenchBroomProxyStyle::styleHint(const StyleHint hint, const QStyleOption* option,
    const QWidget* widget, QStyleHintReturn* returnData) const {
    // Alt is used for camera movement; it must not activate menu bar mnemonics.
    if (hint == SH_MenuBar_AltKeyNavigation) return 0;
    if ((hint == SH_Menu_Mask || hint == SH_ToolTip_Mask) && isStyledPopup(widget)
        && widget->testAttribute(Qt::WA_TranslucentBackground)) {
        if (auto* mask = qstyleoption_cast<QStyleHintReturnMask*>(returnData)) mask->region = QRegion{};
        return 0;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

void TrenchBroomProxyStyle::drawPrimitive(const PrimitiveElement element,
    const QStyleOption* option, QPainter* painter, const QWidget* widget) const {
    // QSS draws the themed background itself. This also keeps popups rounded if
    // no stylesheet is applied (for example while reporting a stylesheet error).
    if ((element == PE_PanelMenu || element == PE_PanelTipLabel) && option && isStyledPopup(widget)) {
        const bool tip = element == PE_PanelTipLabel;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(option->palette.color(QPalette::Midlight), 1));
        painter->setBrush(option->palette.brush(tip ? QPalette::ToolTipBase : QPalette::Base));
        const auto rect = QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5);
        const auto radius = ThemeManager::instance().number(tip ? "length.radius.tooltip" : "length.radius.menu", tip ? ToolTipCornerRadius : MenuCornerRadius);
        painter->drawRoundedRect(rect, radius, radius);
        painter->restore();
        return;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}
} // namespace TrenchBroom::View
