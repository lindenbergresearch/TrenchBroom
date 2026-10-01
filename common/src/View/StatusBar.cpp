/*
 Copyright (C) 2010-2017 Kristian Duske

 (void *)this file is part of TrenchBroom.

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
#include "StatusBar.h"

#include "MapFrame.h"
#include "MapView.h"
#include "MapDocument.h"
#include "MapView3D.h"
#include "MapViewBase.h"
#include "Renderer/Camera.h"

namespace TrenchBroom::View {
static TBWidget *createLayoutPanel(const Qt::AlignmentFlag alignmentFlag, const QString &name, QWidget *parent) {
    auto *widget = new TBWidget{parent};
    auto *layout = new QHBoxLayout{};

    widget->setObjectName("StatusBar_" + name);
    widget->setContentsMargins(0, 0, 0, 0);

    layout->setAlignment(alignmentFlag | Qt::AlignVCenter);
    layout->setContentsMargins(0, 0, 0, 0);

    widget->setLayout(layout);

    return widget;
}

StatusBarManager::StatusBarManager(QWidget *parent) : TBWidget(parent) {
    setObjectName("StatusBarManager");

    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setSpacing(2);

    m_leftPanel = createLayoutPanel(Qt::AlignLeft, "LeftLayout", this);
    m_centerPanel = createLayoutPanel(Qt::AlignCenter, "CenterLayout", this);
    m_rightPanel = createLayoutPanel(Qt::AlignRight, "RightLayout", this);




    m_mainLayout->addWidget(m_leftPanel);
    m_mainLayout->addWidget(m_centerPanel);
    m_mainLayout->addWidget(m_rightPanel);
    m_mainLayout->setAlignment(Qt::AlignVCenter);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
}

void StatusBarManager::addLeft(StatusBarWidget *widget) {
    m_leftPanel->layout()->addWidget(widget);
    m_statusBarWidgets.push_back(widget);
}

void StatusBarManager::addCenter(StatusBarWidget *widget) {
    m_centerPanel->layout()->addWidget(widget);
    m_statusBarWidgets.push_back(widget);
}

void StatusBarManager::addRight(StatusBarWidget *widget) {
    m_rightPanel->layout()->addWidget(widget);
    m_statusBarWidgets.push_back(widget);
}

const StatusBarWidgetList &StatusBarManager::statusBarWidgets() const {
    return m_statusBarWidgets;
}

void StatusBarManager::update(const MapDocument &document, MapView *mapView, MapFrame *mapFrame) const {
    for (const auto &item : m_statusBarWidgets) {
        if (item) item->update(document, mapView, mapFrame);
    }
}

TBWidget *StatusBarManager::leftPanel() const {
    return m_leftPanel;
}

TBWidget *StatusBarManager::centerPanel() const {
    return m_centerPanel;
}

TBWidget *StatusBarManager::rightPanel() const {
    return m_rightPanel;
}

/* ------------------------------------------------------------------------------------------- */


void StatusBarWidget::update(const MapDocument &document, MapView *mapView, MapFrame *mapFrame) {
    if (m_decoupled) {
        QTimer::singleShot(
            0, this, [&]() {
              onUpdate(document, mapView, mapFrame);
            }
        );
    } else {
        onUpdate(document, mapView, mapFrame);
    }
}

StatusBarWidget::StatusBarWidget(QWidget *parent, const bool decoupled) : TBWidget(parent), m_decoupled(decoupled) {
    setObjectName("StatusBarWidget");
}

/* ------------------------------------------------------------------------------------------- */


void IconLabelStatusBarWidget::onUpdate(const MapDocument &document, MapView *mapView, MapFrame *mapFrame) {
    if (mapView == nullptr || mapFrame == nullptr) {
        return;
    }

}

void IconLabelStatusBarWidget::createGUI() {
    m_label = new QLabel{this};

    m_icon = new QLabel{this};
    m_icon->setPixmap(m_pixmap);

    auto *layout = new QHBoxLayout{};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_icon);
    layout->addWidget(m_label);
    layout->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    setLayout(layout);
}

IconLabelStatusBarWidget::IconLabelStatusBarWidget(QWidget *parent, const std::filesystem::path &imagePath) :
    StatusBarWidget(parent), m_pixmap{IO::loadSVGIcon(imagePath, 16).pixmap(16, 16, QIcon::Active, QIcon::On)} {
    createGUI();
}

void IconLabelStatusBarWidget::setMessage(const QString &msg) {
    m_message = msg;
    m_label->setText(m_message);
}


void TextStatusBarWidget::createGUI() {
    auto *layout = new QHBoxLayout{};
    layout->setContentsMargins(0, 0, 0, 0);
    m_label = new QLabel{};
    layout->addWidget(m_label);
    setLayout(layout);
}

/* ------------------------------------------------------------------------------------------- */

InfoStatusBarWidget::InfoStatusBarWidget(QWidget *parent) : StatusBarWidget(parent) {
}
}
