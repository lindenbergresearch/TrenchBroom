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

#include <QEvent>
#include <QMenu>
#include <QPainter>
#include <QStyleOption>

namespace TrenchBroom::View {
namespace {
bool isStyledPopup(const QWidget* widget) {
    return widget && (qobject_cast<const QMenu*>(widget) || widget->windowType() == Qt::ToolTip);
}
}

TrenchBroomProxyStyle::TrenchBroomProxyStyle(const QString& key) : QProxyStyle(key) {}
TrenchBroomProxyStyle::TrenchBroomProxyStyle(QStyle* style) : QProxyStyle(style) {}

QPixmap TrenchBroomProxyStyle::generatedIconPixmap(QIcon::Mode mode, const QPixmap& pixmap, const QStyleOption* option) const {
    return mode == QIcon::Disabled ? IO::disabledIconPixmap(pixmap) : QProxyStyle::generatedIconPixmap(mode,pixmap,option);
}

void TrenchBroomProxyStyle::polish(QWidget* widget) {
    QProxyStyle::polish(widget);
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
