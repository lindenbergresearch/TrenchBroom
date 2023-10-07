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

#include "Inspector.h"

#include <QVBoxLayout>

#include "View/EntityInspector.h"
#include "View/FaceInspector.h"
#include "View/MapInspector.h"
#include "View/MapViewBar.h"
#include "View/QtUtils.h"
#include "View/TabBar.h"
#include "View/TabBook.h"
#include "QTabWidget"
#include "QTabBar.h"
#include "IO/ResourceUtils.h"

namespace TrenchBroom {
namespace View {
Inspector::Inspector(std::weak_ptr<MapDocument> document, GLContextManager &contextManager, QWidget *parent
) : QWidget(parent), m_tabBook(nullptr), m_mapInspector(nullptr), m_entityInspector(nullptr), m_faceInspector(nullptr), m_syncTabBarEventFilter(nullptr) {
//    m_tabBook = new TabBook();
//
//    m_mapInspector = new MapInspector(document);
//    m_entityInspector = new EntityInspector(document, contextManager);
//    m_faceInspector = new FaceInspector(document, contextManager);
//
//    m_tabBook->addPage(m_mapInspector, "Map");
//    m_tabBook->addPage(m_faceInspector, "Face");
//    m_tabBook->addPage(m_entityInspector, "Entity");
//
//    auto *layout = new QVBoxLayout();
//    layout->setContentsMargins(LayoutConstants::NarrowHMargin, 0, LayoutConstants::NarrowHMargin, 0);
//    layout->addWidget(m_tabBook);
//    setLayout(layout);


    m_tabBook = new TabBook();

    auto tabs = new QTabWidget(this);
    m_mapInspector = new MapInspector(document);
    m_entityInspector = new EntityInspector(document, contextManager);
    m_faceInspector = new FaceInspector(document, contextManager);

    tabs->addTab(m_mapInspector, "Map");
    tabs->addTab(m_faceInspector, "Face");
    tabs->addTab(m_entityInspector, "Entity");

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(LayoutConstants::NarrowHMargin, LayoutConstants::MediumVMargin+2, 0, LayoutConstants::NarrowHMargin);
    layout->addWidget(tabs);
    setLayout(layout);
}

void Inspector::connectTopWidgets(MapViewBar *mapViewBar) {
    if (m_syncTabBarEventFilter != nullptr) {
        delete std::exchange(m_syncTabBarEventFilter, nullptr);
    }

    m_syncTabBarEventFilter = new SyncHeightEventFilter(mapViewBar, m_tabBook->tabBar(), this);
}

void Inspector::switchToPage(const InspectorPage page) {
    m_tabBook->switchToPage(static_cast<int>(page));
}

bool Inspector::cancelMouseDrag() {
    return m_faceInspector->cancelMouseDrag();
}

FaceInspector *Inspector::faceInspector() {
    return m_faceInspector;
}
} // namespace View
} // namespace TrenchBroom
