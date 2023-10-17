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

#include "FormWithSectionsLayout.h"

#include <QBoxLayout>
#include <QLabel>

#include "View/BorderLine.h"
#include "View/QtUtils.h"
#include "View/ViewConstants.h"
#include <Preferences.h>
#include <PreferenceManager.h>

namespace TrenchBroom {
namespace View {
void FormWithSectionsLayout::addSection(const QString &title, const QString &info) {
    if (rowCount() > 0) {
        auto *lineLayout = new QVBoxLayout();
        auto border = new BorderLine(BorderLine::Direction::Horizontal, 2);
        border->setForegroundRole(QPalette::Midlight);
        lineLayout->setContentsMargins(LayoutConstants::WideHMargin, LayoutConstants::MediumVMargin, LayoutConstants::WideHMargin, LayoutConstants::MediumVMargin);
        lineLayout->addWidget(border);
        QFormLayout::addRow(lineLayout);
    }

    auto *titleLayout = new QVBoxLayout();
    titleLayout->setContentsMargins(LayoutConstants::WideHMargin+ LayoutConstants::MediumHMargin, 0, LayoutConstants::WideHMargin, 0);
    titleLayout->setSpacing(0);

    auto titlew = makeTitle(new QLabel(title));
    titlew->setForegroundRole(QPalette::HighlightedText);
    titleLayout->addWidget(titlew);

    if (!info.isEmpty()) {
        auto *infoLabel = new QLabel{info};
        infoLabel->setWordWrap(true);
        makeInfo(infoLabel);

        titleLayout->addSpacing(LayoutConstants::NarrowVMargin);
        titleLayout->addWidget(infoLabel);
    }

    titleLayout->addSpacing(0);
    QFormLayout::addRow(titleLayout);
}

void FormWithSectionsLayout::addRow(QWidget *label, QWidget *field, QWidget *additional) {
    insertRow(rowCount(), label, field, additional);
}

void FormWithSectionsLayout::addRow(QWidget *label, QLayout *field, QLayout *additional) {
    insertRow(rowCount(), label, field, additional);
}

void FormWithSectionsLayout::addRow(const QString &labelText, QWidget *field, QWidget *additional) {
    insertRow(rowCount(), labelText, field, additional);
}

void FormWithSectionsLayout::addRow(const QString &labelText, QLayout *field, QLayout *additional) {
    insertRow(rowCount(), labelText, field, additional);
}

void FormWithSectionsLayout::addRow(QWidget *field) {
    insertRow(rowCount(), field);
}

void FormWithSectionsLayout::addRow(QLayout *field) {
    insertRow(rowCount(), field);
}

void FormWithSectionsLayout::insertRow(const int row, QWidget *label, QWidget *field, QWidget *additional) {
    auto *labelLayout = new QHBoxLayout();
    labelLayout->setContentsMargins(LayoutConstants::WideHMargin + LayoutConstants::MediumHMargin, 0, LayoutConstants::WideHMargin * 2, 0);
    labelLayout->addWidget(label);

    auto *fieldLayout = new QHBoxLayout();
    fieldLayout->setContentsMargins(0, 0, LayoutConstants::WideHMargin, 0);
    fieldLayout->setSpacing(LayoutConstants::WideHMargin);
    fieldLayout->addWidget(field);
    if (additional) fieldLayout->addWidget(additional);

    field->setMinimumHeight(getCommonFieldHeight());
    label->setMinimumHeight(getCommonFieldHeight());

    setLayout(row, QFormLayout::LabelRole, labelLayout);
    setLayout(row, QFormLayout::FieldRole, fieldLayout);
}

void FormWithSectionsLayout::insertRow(const int row, QWidget *label, QLayout *field, QLayout *additional) {
    auto *labelLayout = new QHBoxLayout();
    labelLayout->setContentsMargins(LayoutConstants::WideHMargin + LayoutConstants::MediumHMargin, 0, LayoutConstants::WideHMargin * 2, 0);
    labelLayout->addWidget(label);

    auto *fieldLayout = new QHBoxLayout();
    fieldLayout->setContentsMargins(0, 0, LayoutConstants::WideHMargin, 0);
    fieldLayout->setSpacing(LayoutConstants::WideHMargin);
    fieldLayout->addLayout(field);
    if (additional) fieldLayout->addLayout(additional);
    label->setMinimumHeight(getCommonFieldHeight());

    setLayout(row, QFormLayout::LabelRole, labelLayout);
    setLayout(row, QFormLayout::FieldRole, fieldLayout);
}

void FormWithSectionsLayout::insertRow(const int row, const QString &labelText, QWidget *field, QWidget *additional) {
    insertRow(row, new QLabel(labelText), field, additional);
}

void FormWithSectionsLayout::insertRow(const int row, const QString &labelText, QLayout *field, QLayout *additional) {
    insertRow(row, new QLabel(labelText), field, additional);
}

void FormWithSectionsLayout::insertRow(int row, QWidget *field, QWidget *additional) {
    auto *layout = new QHBoxLayout();
    layout->setContentsMargins(LayoutConstants::WideHMargin * 2, 0, LayoutConstants::WideHMargin * 2, 0);
    layout->addWidget(field);
    if (additional) layout->addWidget(additional);
    QFormLayout::insertRow(row, layout);
}

void FormWithSectionsLayout::insertRow(int row, QLayout *field, QLayout *additional) {
    auto *layout = new QHBoxLayout();
    layout->setContentsMargins(LayoutConstants::WideHMargin * 2, 0, LayoutConstants::WideHMargin * 2, 0);
    layout->addLayout(field);
    if (additional) layout->addLayout(additional);
    QFormLayout::insertRow(row, layout);
}
} // namespace View
} // namespace TrenchBroom
