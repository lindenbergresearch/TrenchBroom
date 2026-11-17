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

#include "MapViewBar.h"

#include <QHBoxLayout>
#include <QResizeEvent>
#include <QStackedLayout>

#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/MapDocument.h"
#include "View/ViewConstants.h"
#include "View/ViewEditor.h"

namespace TrenchBroom {
namespace View {
MapViewBar::MapViewBar(std::weak_ptr<MapDocument> document, QWidget *parent)
    : ContainerBar(Sides::BottomSide, parent), m_document(document), m_toolBook(nullptr), m_viewEditor(nullptr) {
    createGui(document);
}

QStackedLayout *MapViewBar::toolBook() {
    return m_toolBook;
}

void MapViewBar::createGui(std::weak_ptr<MapDocument> document) {
    setAttribute(Qt::WA_MacSmallSize);

    m_toolBook = new QStackedLayout();
    m_toolBook->setAlignment(Qt::AlignVCenter);

    m_viewEditor = new ViewPopupEditor(std::move(document));

#ifdef __APPLE__
    const auto vMargin = 0;
#else
    const auto vMargin = LayoutConstants::MediumVMargin;
#endif

    auto *horizontalLayout = new QHBoxLayout{};
    horizontalLayout->setContentsMargins(LayoutConstants::WideHMargin, vMargin, LayoutConstants::WideHMargin, vMargin);
    horizontalLayout->setSpacing(LayoutConstants::WideHMargin);
    horizontalLayout->addLayout(m_toolBook, 1);
    horizontalLayout->addWidget(m_viewEditor, 0, Qt::AlignVCenter);

    if (pref(Preferences::DebugMode) && false) {
        m_panel = new TitledPanel("DEBUG Panel", this, false, true);

        auto *mainLayout = new QVBoxLayout{};
        mainLayout->setContentsMargins(
            LayoutConstants::NoMargin,
            LayoutConstants::NoMargin,
            LayoutConstants::NoMargin,
            LayoutConstants::NoMargin
        );

        mainLayout->addLayout(horizontalLayout);
        mainLayout->addWidget(m_panel);

        setLayout(mainLayout);
    } else {
        setLayout(horizontalLayout);
    }
}

MapViewBar::~MapViewBar() {}
} // namespace View
} // namespace TrenchBroom
