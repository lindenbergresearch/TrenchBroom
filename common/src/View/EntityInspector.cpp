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

#include "EntityInspector.h"

#include <QVBoxLayout>

#include "View/EntityBrowser.h"
#include "View/EntityDefinitionFileChooser.h"
#include "View/MapDocument.h"
#include "View/QtUtils.h"
#include "View/TitleBar.h"
#include "View/TitledPanel.h"

namespace TrenchBroom::View {

EntityInspector::EntityInspector(std::weak_ptr<MapDocument> document, GLContextManager &contextManager, QWidget *parent)
    : QWidget(parent) {
    createGui(std::move(document), contextManager);
}

std::vector<InspectorToolSection> EntityInspector::toolSections() const {
    return {
        {QStringLiteral("entity-browser"), tr("Entity Browser"), m_entityBrowserPanel,
            m_entityBrowserPanel ? m_entityBrowserPanel->getTitleBar() : nullptr, 1},
        {QStringLiteral("entity-definitions"), tr("Entity Definitions"), m_entityDefinitionFileChooser,
            m_entityDefinitionFileChooser ? m_entityDefinitionFileChooser->getTitleBar() : nullptr, 0},
    };
}

void EntityInspector::createGui(std::weak_ptr<MapDocument> document, GLContextManager &contextManager) {
    m_entityBrowserPanel = createEntityBrowser(this, document, contextManager);
    m_entityDefinitionFileChooser = createEntityDefinitionFileChooser(this, document);
}

TitledPanel *EntityInspector::createEntityBrowser(QWidget *parent, std::weak_ptr<MapDocument> document, GLContextManager &contextManager) {
    auto *panel = new TitledPanel(tr("Entity Browser"), parent);
    panel->setObjectName("EntityInspector_BrowserPanel");
    auto *browser = new EntityBrowser(std::move(document), contextManager);
    browser->setMinimumSize(100, 150);

    auto *sizer = new QVBoxLayout();
    sizer->setContentsMargins(0, 0, 0, 0);
    sizer->addWidget(browser, 1);
    panel->getPanel()->setLayout(sizer);

    return panel;
}

TitledPanel *EntityInspector::createEntityDefinitionFileChooser(QWidget *parent, std::weak_ptr<MapDocument> document) {
    auto *panel = new TitledPanel(tr("Entity Definitions"), parent);
    panel->setObjectName("EntityInspector_EntityDefinitionFileChooser");

    auto *entityDefinitionFileChooser = new EntityDefinitionFileChooser(document);

    auto *sizer = new QVBoxLayout();
    sizer->setContentsMargins(0, 0, 0, 0);
    sizer->addWidget(entityDefinitionFileChooser, 1);
    panel->getPanel()->setLayout(sizer);

    return panel;
}

} // namespace TrenchBroom::View
