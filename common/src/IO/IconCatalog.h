#pragma once

#include "IO/VectorIcon.h"

#include <QCache>
#include <QHash>
#include <QIcon>
#include <QList>
#include <QObject>
#include <QRawFont>

#include <filesystem>
#include <vector>

namespace TrenchBroom {
class Logger;

namespace IO {

class IconCatalog : public QObject {
public:
    struct Entry {
        QString name;
        QString source;

    private:
        friend class IconCatalog;
        QString svgPath;
        QString legacyPath;
        QRawFont font;
        quint32 glyph = 0;
    };

    IconCatalog(const std::vector<std::filesystem::path>& svgDirectories,
                const std::vector<std::filesystem::path>& fontDirectories,
                Logger& logger, QObject* parent = nullptr);

    static const IconCatalog& instance();
    const QList<Entry>& entries() const { return m_entries; }
    QIcon icon(const QString& name, const IconStyle& style = {}, const QString& themeColor = {}) const;
    QSize sourceSize(const QString& name, int glyphExtent) const;
    QString canonicalName(const QString& nameOrPath) const;

private:
    void indexSvgs(const std::vector<std::filesystem::path>& directories, Logger& logger);
    void indexFonts(const std::vector<std::filesystem::path>& directories, Logger& logger);
    void assignNames(Logger& logger);
    VectorIcon sourceIcon(qsizetype index) const;

    QList<Entry> m_entries;
    QHash<QString, qsizetype> m_byName;
    QHash<QString, qsizetype> m_svgAliases;
    mutable QCache<qsizetype, VectorIcon> m_glyphIcons{256};
};

} // namespace IO
} // namespace TrenchBroom
