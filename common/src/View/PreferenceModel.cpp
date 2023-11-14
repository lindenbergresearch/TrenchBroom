#include <ranges>

#include "PreferenceModel.h"
#include "Preferences.h"
#include "QtUtils.h"
#include "PreferenceManager.h"
#include "TrenchBroomApp.h"

#include <kdl/path_utils.h>

namespace TrenchBroom::View {


PreferenceModel::PreferenceModel(QObject *parent) : QAbstractTableModel(parent) {
    initialize();
}

void PreferenceModel::reset() {
}

int PreferenceModel::rowCount(const QModelIndex &parent) const {
    return (int) data().size();
}

int PreferenceModel::columnCount(const QModelIndex &parent) const {
    return Columns::count;
}

QVariant PreferenceModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < count) {
        return columnNames[section];
    }

    return QVariant{};
}


QVariant PreferenceModel::data(const QModelIndex &index, int role) const {
    auto valid =
        index.isValid() &&
        index.column() < count &&
        index.row() < int(data().size());

    auto preferenceBase = data().at(size_t(index.row()));

    if (!valid) {
        return QVariant{};
    }

    auto info = getInfo(preferenceBase);

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case Index:
                return QString{}.sprintf("%d", index.row());
            case Context:
                return QString::fromStdString(kdl::path_front(preferenceBase->path()).string());
            case Path:
                return QString::fromStdString(kdl::path_pop_front(preferenceBase->path()).generic_string());
            case Type:
                return std::get<0>(info);
            case Default:
                return std::get<1>(info);
            case Value:
                return std::get<2>(info);
            default:
                return "?";
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
        // invalid
        if (!preferenceBase->valid() &&
            (index.column() == Value || index.column() == Default || index.column() == Type)) {
            return QBrush(COLOR_ROLE(BrightText));
        }

        // default value was overwritten
        if (preferenceBase->valid() && !preferenceBase->isDefault() && index.column() == Value) {
            auto highlightColor = COLOR_ROLE(Highlight);
            return QBrush{highlightColor};
        }
    }

    return QVariant();
}

bool PreferenceModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    return QAbstractItemModel::setData(index, value, role);
}

Qt::ItemFlags PreferenceModel::flags(const QModelIndex &index) const {
    auto valid =
        index.isValid() &&
        index.column() < count &&
        index.row() < int(data().size());

    auto preferenceBase = data().at(size_t(index.row()));

    if (valid && preferenceBase->valid()) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    } else {
        return Qt::NoItemFlags;
    }
}

void PreferenceModel::initialize() {
    // collect preferences except colors and key-sequences
    auto ref = Preferences::staticPreferences();
    std::copy_if(ref.begin(), ref.end(), std::back_inserter(m_preferences),
        [](PreferenceBase *base) {
          return
              !dynamic_cast<Preference<Color> *>(base) &&
              !dynamic_cast<Preference<QKeySequence> *>(base);
        }
    );
}

std::vector<PreferenceBase *> PreferenceModel::data() const {
    return m_preferences;
}

size_t PreferenceModel::preferenceCount() const {
    return data().size();
}

StringTuple3 PreferenceModel::getInfo(PreferenceBase *preferenceBase) const {
    QString name = "unknown";
    QString value = "nothing";
    QString defaultValue = "nothing";

    if (preferenceBase == nullptr) {
        name = "NULL";
        value = "-";
        defaultValue = "-";

        return StringTuple3(name, defaultValue, value);
    }

    if (Preference<bool> *preference = dynamic_cast<Preference<bool> *>(preferenceBase)) {
        name = "boolean";
        value = pref(*preference) ? "true" : "false";
        defaultValue = preference->defaultValue() ? "true" : "false";
    }

    if (Preference<QString> *preference = dynamic_cast<Preference<QString> *>(preferenceBase)) {
        name = "QString";
        value = pref(*preference);
        defaultValue = preference->defaultValue();
    }

    if (Preference<float> *preference = dynamic_cast<Preference<float> *>(preferenceBase)) {
        name = "float";
        value = QString{}.sprintf("%f", pref(*preference));
        defaultValue = QString{}.sprintf("%f", preference->defaultValue());
    }

    if (Preference<double> *preference = dynamic_cast<Preference<double> *>(preferenceBase)) {
        name = "double";
        value = QString{}.sprintf("%f", pref(*preference));
        defaultValue = QString{}.sprintf("%f", preference->defaultValue());
    }

    if (Preference<int> *preference = dynamic_cast<Preference<int> *>(preferenceBase)) {
        name = "int";
        value = QString{}.sprintf("%d", pref(*preference));//yvonne.lehmann@plus.aok.de
        defaultValue = QString{}.sprintf("%d", preference->defaultValue());
    }

    if (Preference<Color> *preference = dynamic_cast<Preference<Color> *>(preferenceBase)) {
        QColor color = toQColor(pref(*preference));
        QColor defaultColor = toQColor(preference->defaultValue());
        name = "Color";
        value = color.name().toUpper();
        defaultValue = defaultColor.name().toUpper();
    }

    if (Preference<std::filesystem::path> *preference = dynamic_cast<Preference<std::filesystem::path> *>(preferenceBase)) {
        name = "path";
        value = QString::fromStdString(pref(*preference).string());
        defaultValue = QString::fromStdString(preference->defaultValue().string());
    };

    if (Preference<QKeySequence> *preference = dynamic_cast<Preference<QKeySequence> *>(preferenceBase)) {
        name = "QKeySequence";
        value = pref(*preference).toString();
        defaultValue = preference->defaultValue().toString();
    };

    return StringTuple3(name, defaultValue, value);
}

PreferenceBase *PreferenceModel::dataAt(const size_t &index) {
    return data().at(index);
}
}
