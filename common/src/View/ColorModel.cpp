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

#include "ColorModel.h"

#include <QColorDialog>

#include "PreferenceManager.h"
#include "Preferences.h"
#include "QtUtils.h"
#include "TrenchBroomApp.h"

#include <kdl/path_utils.h>

#include <filesystem>

namespace TrenchBroom::View {

// threshold for using inverted text label to optimize visibility with colored background
static const float BRIGHTNESS_THRESHOLD = 0.65f;

ColorModel::ColorModel(QObject *parent) : QAbstractTableModel(parent), m_colorsCount(0) {
    initialize();
}

void ColorModel::initialize() {
    for (auto *pref: Preferences::staticPreferences()) {
        auto *colorPref = dynamic_cast<Preference<Color> *>(pref);
        if (colorPref != nullptr && !colorPref->isReadOnly()) {
            m_colors.emplace_back(colorPref);
        }
    }

    m_colorsCount = static_cast<int>(m_colors.size());
}

// Reset to default values
void ColorModel::reset() {
    auto &prefs = PreferenceManager::instance();
    for (auto *pref: m_colors) {
        prefs.resetToDefault(*pref);
    }

    emit dataChanged(createIndex(0, 0), createIndex(m_colorsCount - 1, 2));
}

int ColorModel::rowCount(const QModelIndex & /* parent */) const {
    return m_colorsCount;
}

int ColorModel::columnCount(const QModelIndex & /* parent */) const {
    return Columns::count;// Color, Context, Description
}

QVariant ColorModel::headerData(const int section, const Qt::Orientation orientation, const int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < count) {
        return columnNames[section];
    }

    return QVariant();
}

QVariant ColorModel::data(const QModelIndex &index, const int role) const {
    if (!checkIndex(index)) {
        return QVariant();
    }

    auto preference = getColorPreference(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case Columns::Index:
                return index.row();
            case Columns::Context:
                return QString::fromStdString(kdl::path_front(preference->path()).string());
            case Columns::Path:
                return QString::fromStdString(
                    kdl::path_pop_front(preference->path()).generic_string());
            case Columns::Default:
                return toQColor(preference->defaultValue()).name().toUpper();
            case Columns::Value:
                return toQColor(preference->value()).name().toUpper();
                switchDefault();
        }
    }

    auto valueColor = toQColor(pref(*preference));
    auto defaultColor = toQColor(preference->defaultValue());


    // background role
    if (role == Qt::BackgroundRole) {
        // coloring
        if (preference->valid() && index.column() == Value) {
            return QBrush(valueColor);
        }

        if (preference->valid() && index.column() == Default) {
            return QBrush(defaultColor);
        }
    }

    // font role
    if (role == Qt::FontRole) {
        // default
        if (index.column() == Value || index.column() == Default) {
            auto font = TrenchBroomApp::instance().getConsoleFont();
            font.setPointSize(QFont{}.pointSize());
            return font;
        }
    }

    // set foreground color
    if (role == Qt::ForegroundRole) {

        // auto coloring text
        if (preference->valid() && index.column() == Value) {
            auto maxValue = getQColorBrightnessFactor(valueColor);
            return QBrush(
                maxValue > BRIGHTNESS_THRESHOLD ? COLOR_ROLE(Dark) : COLOR_ROLE(BrightText));
        }

        if (preference->valid() && index.column() == Default) {
            auto maxValue = getQColorBrightnessFactor(defaultColor);
            return QBrush(
                maxValue > BRIGHTNESS_THRESHOLD ? COLOR_ROLE(Dark) : COLOR_ROLE(BrightText));
        }

        // default value was overwritten
        if (preference->valid() && !preference->isDefault() && index.column() == Value) {
            auto highlightColor = COLOR_ROLE(Highlight);
            return QBrush{highlightColor};
        }
    }

    return QVariant();
}

bool ColorModel::setData(const QModelIndex &index, const QVariant &value, const int /*role*/) {
    if (!checkIndex(index) || index.column() != 0) {
        return false;
    }

    auto *colorPreference = getColorPreference(index.row());
    // const auto color = toQColor(pref(*colorPreference));

    auto newColor = value.value<QColor>();
    // newColor.setAlpha(color.alpha()); // Keep initial alpha...

    auto &prefs = PreferenceManager::instance();
    prefs.set(*colorPreference, fromQColor(newColor));

    emit dataChanged(index, index);

    // update style
    TrenchBroomApp::instance().reloadStyle(false);

    return true;
}

Qt::ItemFlags ColorModel::flags(const QModelIndex &index) const {
    if (checkIndex(index) && index.column() < Columns::Value) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }

    return Qt::NoItemFlags;
}

void ColorModel::pickColor(const QModelIndex &mi) {
    if (!checkIndex(mi)) {
        return;
    }

    // Get current color
    auto *colorPreference = getColorPreference(mi.row());
    auto color = toQColor(pref(*colorPreference));

    // Show dialog
    auto newColor = QColorDialog::getColor(
        color, nullptr, "Select new color", QColorDialog::DontUseNativeDialog | QColorDialog::ShowAlphaChannel);

    // Apply color (QColorDialog::getColor() returns an invalid color if the user cancels
    // the dialog)
    if (newColor.isValid()) {
        // pickColor() can be called for column 1 or 2 if the user double-clicks those
        // columns, but we always edit column 0 (where the color is displayed)
        setData(index(mi.row(), 0), newColor, Qt::EditRole);
    }
}

Preference<Color> *ColorModel::getColorPreference(const int index) const {
    assert(index < m_colorsCount);
    return m_colors[static_cast<size_t>(index)];
}

bool ColorModel::checkIndex(const QModelIndex &index) const {
    return index.isValid() && index.column() < Columns::count && index.row() < m_colorsCount;
}
}// namespace TrenchBroom::View
