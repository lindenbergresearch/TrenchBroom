#pragma once

#include "Notifier.h"

#include <QColor>
#include <QFont>
#include <QMap>
#include <QPalette>
#include <QStringList>
#include <QVariant>

#include <filesystem>

namespace TrenchBroom {
struct ThemeValue {
    enum class Type { Color, Length, Number, FontFamily, FontWeight, Keyword };
    Type type = Type::Number;
    QVariant value;

    static ThemeValue color(const QColor& value);
    static ThemeValue length(qreal value);
    static ThemeValue number(qreal value);
    static ThemeValue family(const QString& value);
    static ThemeValue weight(int value);
    static ThemeValue keyword(const QString& value);
    bool isValid() const;
    QString toStyleSheet(bool insideQuotes = false) const;
    friend bool operator==(const ThemeValue& lhs, const ThemeValue& rhs) { return lhs.type == rhs.type && lhs.value == rhs.value; }
    friend bool operator!=(const ThemeValue& lhs, const ThemeValue& rhs) { return !(lhs == rhs); }
};

using ThemeValues = QMap<QString, ThemeValue>;
using ThemeDefinitions = QMap<QString, QMap<QString, QVariant>>;

struct ThemeSnapshot {
    ThemeValues values;
    QPalette palette;
};

// GUI-thread source of UI colors and metrics for QSS, icons and custom painting.
// Existing preference-bound values take precedence over theme-file defaults.
class ThemeManager {
  private:
    ThemeSnapshot m_current;
    quint64 m_revision = 0;

  public:
    Notifier<const QStringList&> themeDidChangeNotifier;
    static ThemeManager& instance();
    static ThemeSnapshot preferenceValues(const QFont& ui, const QFont& console, const QFont& renderer);
    static const QMap<QString, QString>& legacyNames();
    static bool isRelevantPreference(const std::filesystem::path& path);

    // Typed sections: colors, lengths, numbers, fontFamilies, fontWeights,
    // keywords. Exact ${name} references have cycle and type checks.
    static bool readDefinition(const QByteArray& bytes, const QString& source, ThemeDefinitions& definitions, QStringList& errors);
    static bool resolve(ThemeSnapshot& snapshot, const ThemeDefinitions& definitions, QStringList& errors);
    static bool prepare(const QFont& ui, const QFont& console, const QFont& renderer, const std::filesystem::path& defaults, const std::filesystem::path& overrides, ThemeSnapshot& result,
                        QStringList& errors);

    bool publish(ThemeSnapshot snapshot);
    const ThemeSnapshot& current() const { return m_current; }
    quint64 revision() const { return m_revision; }
    QColor color(const QString& name, const QColor& fallback = {}) const;
    qreal number(const QString& name, qreal fallback) const;
};
} // namespace TrenchBroom
