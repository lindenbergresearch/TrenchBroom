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

#include "View/BrushShapePalette.h"

#include <QButtonGroup>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "View/DrawShapeTool.h"
#include "View/MapViewToolBox.h"
#include "View/QtUtils.h"
#include <algorithm>
#include <cmath>

namespace TrenchBroom::View {
BrushShapePalette::BrushShapePalette(MapViewToolBox& tools, QWidget* parent)
    : QFrame(parent), m_tools(tools), m_contents(new QWidget), m_scroll(new QScrollArea(this)), m_layout(new QVBoxLayout(this)), m_buttonsLayout(new QVBoxLayout(m_contents)),
      m_opacity(new QGraphicsOpacityEffect(this)) {
    setObjectName("BrushShapePalette");
    setAttribute(Qt::WA_StyledBackground);
    // Blank space around the icons belongs to the palette, not the brush gesture.
    setAttribute(Qt::WA_NoMousePropagation);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
    m_opacity->setOpacity(1);
    setGraphicsEffect(m_opacity);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_contents->setObjectName("BrushShapePaletteContent");
    m_buttonsLayout->setContentsMargins(0, 0, 0, 0);
    m_buttonsLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_scroll->setObjectName("BrushShapePaletteScroll");
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setFocusPolicy(Qt::NoFocus);
    m_scroll->viewport()->setAutoFillBackground(false);
    m_scroll->setWidget(m_contents);
    m_layout->addWidget(m_scroll);
    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    auto& manager = tools.drawShapeTool().extensionManager();
    for (const auto* extension : manager.extensions()) {
        auto* button = createBitmapToggleButton(extension->iconPath().string(), QString::fromStdString(extension->name()), m_contents);
        button->setObjectName("BrushShapeButton" + QString::number(int(extension->kind())));
        button->setProperty("shapeKind", int(extension->kind()));
        button->setAccessibleName(QString::fromStdString(extension->name()));
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        group->addButton(button);
        m_buttonsLayout->addWidget(button);
        m_buttons.emplace_back(extension->kind(), button);
        connect(button, &QToolButton::clicked, this, [this, kind = extension->kind()] {
            if (m_tools.dragging()) m_tools.cancelMouseDrag();
            m_tools.drawShapeTool().extensionManager().setCurrentExtension(kind);
            parentWidget()->setFocus(Qt::MouseFocusReason);
        });
    }
    parent->installEventFilter(this);
    m_connections += manager.currentExtensionDidChangeNotifier.connect([this](size_t) { refresh(); });
    m_connections += tools.toolActivatedNotifier.connect([this](auto&) { refresh(); });
    m_connections += tools.toolDeactivatedNotifier.connect([this](auto&) { refresh(); });
    refresh();
}
int BrushShapePalette::edgeMargin() const {
    return m_edgeMargin;
}
void BrushShapePalette::setEdgeMargin(int margin) {
    m_edgeMargin = std::max(0, margin);
    schedulePlace();
}
int BrushShapePalette::panelPadding() const {
    return m_layout->contentsMargins().left();
}
void BrushShapePalette::setPanelPadding(int padding) {
    padding = std::max(0, padding);
    m_layout->setContentsMargins(padding, padding, padding, padding);
    schedulePlace();
}
int BrushShapePalette::buttonSpacing() const {
    return m_buttonsLayout->spacing();
}
void BrushShapePalette::setButtonSpacing(int spacing) {
    m_buttonsLayout->setSpacing(std::max(0, spacing));
    schedulePlace();
}
qreal BrushShapePalette::overlayOpacity() const {
    return m_opacity->opacity();
}
void BrushShapePalette::setOverlayOpacity(qreal opacity) {
    if (std::isfinite(opacity)) m_opacity->setOpacity(std::clamp(opacity, qreal(0), qreal(1)));
}
bool BrushShapePalette::event(QEvent* event) {
    const auto result = QFrame::event(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::LayoutRequest || event->type() == QEvent::Show) schedulePlace();
    return result;
}
bool BrushShapePalette::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) schedulePlace();
    return QFrame::eventFilter(watched, event);
}
void BrushShapePalette::refresh() {
    const auto kind = m_tools.drawShapeTool().extensionManager().currentExtension().kind();
    for (const auto& [buttonKind, button] : m_buttons) {
        const QSignalBlocker block(button);
        button->setChecked(buttonKind == kind);
    }
    setVisible(m_tools.createToolActive());
    if (isVisible()) {
        raise();
        schedulePlace();
    }
}
void BrushShapePalette::schedulePlace() {
    if (m_placePending) return;
    m_placePending = true;
    QTimer::singleShot(0, this, [this] {
        m_placePending = false;
        place();
    });
}
void BrushShapePalette::place() {
    if (!parentWidget() || !isVisible()) return;
    m_buttonsLayout->activate();
    const auto margins = m_layout->contentsMargins();
    const auto full = m_contents->sizeHint() + QSize(margins.left() + margins.right() + 2 * frameWidth(), margins.top() + margins.bottom() + 2 * frameWidth());
    const auto available = parentWidget()->size() - QSize(2 * m_edgeMargin, 2 * m_edgeMargin);
    const auto height = std::max(1, std::min(full.height(), available.height()));
    const auto scrollWidth = height < full.height() ? style()->pixelMetric(QStyle::PM_ScrollBarExtent) : 0;
    const auto width = std::max(1, std::min(full.width() + scrollWidth, available.width()));
    setGeometry(m_edgeMargin, std::max(0, (parentWidget()->height() - height) / 2), width, height);
}
} // namespace TrenchBroom::View
