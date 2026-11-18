#pragma once

#include "View/Theme/ThemeManager.h"

#include <QString>
#include <QStringList>

#include <filesystem>

namespace TrenchBroom {
// A checked, single-pass compiler for ${theme.name} references. Failed updates
// retain the last valid stylesheet. Literal colors and object selectors are intact.
class QSSBuilder {
  private:
    QString m_text, m_source, m_renderedText, m_compiledSource;
    QStringList m_errors;
    ThemeValues m_dependencies;
    bool m_changed = false;
    bool m_hasCompiled = false;
    quint64 m_compilationCount = 0;

  public:
    bool loadFile(const std::filesystem::path& path);
    void setText(const QString& text, const QString& source = QStringLiteral("<stylesheet>"));
    bool update(const ThemeValues& values);
    const QString& getText() const { return m_text; }
    const QString& getRenderedText() const { return m_renderedText; }
    const QStringList& errors() const { return m_errors; }
    bool changed() const { return m_changed; }
    quint64 compilationCount() const { return m_compilationCount; }
};
} // namespace TrenchBroom
