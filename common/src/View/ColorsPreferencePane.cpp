/*
 Copyright (C) 2020 MaxED

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

#include "ColorsPreferencePane.h"

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QTableView>

#include "TrenchBroomApp.h"
#include "View/ColorModel.h"
#include "View/QtUtils.h"

namespace TrenchBroom::View {
ColorsPreferencePane::ColorsPreferencePane(QWidget *parent) : PreferencePane(parent), m_table(nullptr), m_model(nullptr), m_proxy(nullptr) {
    m_model = new ColorModel(this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1); // Filter based on all columns

    m_table = new QTableView();
    m_table->setCornerButtonEnabled(false);
    m_table->setModel(m_proxy);

    m_table->setHorizontalHeader(new QHeaderView(Qt::Horizontal));
    m_table->horizontalHeader()->setSectionResizeMode(ColorModel::Columns::Value, QHeaderView::ResizeMode::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(ColorModel::Columns::Default, QHeaderView::ResizeMode::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(ColorModel::Columns::Context, QHeaderView::ResizeMode::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColorModel::Columns::Path, QHeaderView::ResizeMode::Stretch);
    m_table->horizontalHeader()->resizeSection(ColorModel::Columns::Value, 80);
    m_table->horizontalHeader()->resizeSection(ColorModel::Columns::Default, 80);

    // Tighter than default vertical row height, without the overhead of autoresizing
    m_table->verticalHeader()->setDefaultSectionSize(m_table->fontMetrics().lineSpacing() + LayoutConstants::WideHMargin);

    m_table->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);

    m_table->setSortingEnabled(true);
    m_table->sortByColumn(ColorModel::Columns::Context, Qt::AscendingOrder);
    m_table->hideColumn(ColorModel::Columns::Index);

    QLineEdit *searchBox = createSearchBox();
    makeSmall(searchBox);

    auto *infoLabel = new QLabel(tr("Double-Click on a color to begin editing it."));
    makeInfo(infoLabel);

    m_table->setSortingEnabled(true);
    m_table->sortByColumn(0);

    auto *infoAndSearchLayout = new QHBoxLayout();
    infoAndSearchLayout->setContentsMargins(0, LayoutConstants::MediumHMargin, 0, LayoutConstants::WideHMargin);
    infoAndSearchLayout->setSpacing(LayoutConstants::WideHMargin);
    infoAndSearchLayout->addWidget(infoLabel, 1);
    infoAndSearchLayout->addWidget(searchBox);

    auto *layout = new QVBoxLayout();
    layout->setContentsMargins(LayoutConstants::WideHMargin, LayoutConstants::WideHMargin, LayoutConstants::WideHMargin, LayoutConstants::WideHMargin);
    layout->setSpacing(0);
    layout->addLayout(infoAndSearchLayout);
    layout->addWidget(m_table, 1);
    setLayout(layout);

    setMinimumSize(900, 550);

    connect(searchBox, &QLineEdit::textChanged, this, [&](const QString &newText) {
        m_proxy->setFilterFixedString(newText);
    });

    connect(m_table, &QTableView::doubleClicked, this, [&](const QModelIndex &index) {
        m_model->pickColor(m_proxy->mapToSource(index));
    });
}

bool ColorsPreferencePane::doCanResetToDefaults() {
    return true;
}

void ColorsPreferencePane::doResetToDefaults() {
    m_model->reset();
}

void ColorsPreferencePane::doUpdateControls() {
    m_table->update();
}

bool ColorsPreferencePane::doValidate() {
    return true;
}
} // namespace TrenchBroom::View
