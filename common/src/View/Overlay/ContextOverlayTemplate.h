#pragma once

#include <QString>
#include <QVariantMap>
#include <vector>

namespace TrenchBroom::View {
// Deliberately small, read-only Mustache subset: escaped values, sections,
// inverted sections, lists and comments. Templates are compiled on file changes.
class ContextOverlayTemplate {
    struct Part {
        QChar kind;
        QString value;
        std::vector<Part> children;
    };
    std::vector<Part> m_parts;
    static std::vector<Part> parse(const QString& source, int& offset, const QString& section, int depth);
    static QString render(const std::vector<Part>& parts, std::vector<QVariant>& scopes);
public:
    explicit ContextOverlayTemplate(const QString& source = {});
    QString render(const QVariantMap& data) const;
};
} // namespace TrenchBroom::View
