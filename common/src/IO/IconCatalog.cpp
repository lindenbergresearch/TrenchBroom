#include "IO/IconCatalog.h"

#include "Ensure.h"
#include "IO/PathQt.h"
#include "IO/SystemPaths.h"
#include "IO/VectorIcon.h"
#include "Logger.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QPointer>
#include <QSet>
#include <QThread>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>

namespace TrenchBroom::IO {
namespace {

using Directories = std::vector<std::filesystem::path>;

QList<QFileInfo> resourceFiles(const std::filesystem::path& root, const QString& suffix) {
    const auto directory = QDir(pathAsQString(root));
    auto iterator = QDirIterator(directory.absolutePath(), QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    auto files = QList<QFileInfo>{};
    while (iterator.hasNext()) {
        const auto file = QFileInfo(iterator.next());
        if (file.suffix().compare(suffix, Qt::CaseInsensitive) == 0) files.append(file);
    }
    std::ranges::sort(files, [](const auto& left, const auto& right) {
        const auto a = left.absoluteFilePath();
        const auto b = right.absoluteFilePath();
        const auto levelsA = a.count('/');
        const auto levelsB = b.count('/');
        return levelsA == levelsB ? a < b : levelsA < levelsB;
    });
    return files;
}

std::optional<QJsonArray> configuredFonts(const Directories& directories, Logger& logger) {
    for (const auto& directory : directories) {
        auto file = QFile(pathAsQString(directory / "icon-fonts.json"));
        if (!file.exists()) continue;
        auto error = QJsonParseError{};
        if (file.open(QIODevice::ReadOnly)) {
            const auto document = QJsonDocument::fromJson(file.readAll(), &error);
            const auto fonts = document.object().value("fonts");
            if (error.error == QJsonParseError::NoError && fonts.isArray()) return fonts.toArray();
        }
        logger.warn(QString("Invalid icon font configuration %1, expected a fonts array").arg(file.fileName()));
        break;
    }
    return std::nullopt;
}

QList<QFileInfo> fontFiles(const Directories& directories, Logger& logger) {
    const auto configured = configuredFonts(directories, logger);
    auto files = QList<QFileInfo>{};
    if (!configured) {
        for (const auto& directory : directories) files.append(resourceFiles(directory, "ttf"));
        return files;
    }

    for (const auto& entry : *configured) {
        const auto name = QDir::cleanPath(QDir::fromNativeSeparators(entry.toString()));
        if (!entry.isString() || QDir::isAbsolutePath(name) || name.startsWith("../")
            || QFileInfo(name).suffix().compare("ttf", Qt::CaseInsensitive) != 0) {
            logger.warn(QString("Invalid icon font path %1, expected a relative .ttf path").arg(entry.toString()));
            continue;
        }

        const auto path = pathFromQString(name);
        const auto match = std::ranges::find_if(directories,
            [&](const auto& directory) { return QFileInfo(pathAsQString(directory / path)).isFile(); });
        if (match != directories.end()) files.append(QFileInfo(pathAsQString(*match / path)));
        else logger.warn(QString("Icon font %1 was not found under fonts/icons").arg(name));
    }
    return files;
}

} // namespace

IconCatalog::IconCatalog(const Directories& svgDirectories, const Directories& fontDirectories,
                         Logger& logger, QObject* parent) : QObject(parent) {
    indexSvgs(svgDirectories, logger);
    indexFonts(fontDirectories, logger);
    assignNames(logger);
}

const IconCatalog& IconCatalog::instance() {
    ensure(qGuiApp && qGuiApp->thread() == QThread::currentThread(), "IconCatalog requires the GUI thread");
    static QPointer<IconCatalog> catalog;
    if (!catalog) {
        catalog = new IconCatalog(SystemPaths::findResourceDirectories("images"),
            SystemPaths::findResourceDirectories("fonts/icons"), defaultQtLogger, qGuiApp);
    }
    return *catalog;
}

void IconCatalog::indexSvgs(const Directories& directories, Logger& logger) {
    auto seen = QSet<QString>{};
    auto counts = QMap<QString, int>{};
    for (const auto& root : directories) {
        const auto directory = QDir(pathAsQString(root));
        for (const auto& file : resourceFiles(root, "svg")) {
            const auto canonical = file.canonicalFilePath();
            if (canonical.isEmpty() || seen.contains(canonical)) continue;
            seen.insert(canonical);
            ++counts[file.absolutePath()];
            auto entry = Entry{};
            entry.name = file.completeBaseName();
            entry.svgPath = file.absoluteFilePath();
            entry.legacyPath = directory.relativeFilePath(entry.svgPath);
            entry.source = entry.svgPath;
            m_entries.append(std::move(entry));
        }
    }
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        logger.info(QString("Indexed %1 SVG icons in %2").arg(it.value()).arg(it.key()));
}

void IconCatalog::indexFonts(const Directories& directories, Logger& logger) {
    const auto files = fontFiles(directories, logger);
    if (files.isEmpty()) return;
    auto library = FT_Library{};
    if (FT_Init_FreeType(&library) != 0) {
        logger.warn() << "Could not initialize FreeType for icon fonts";
        return;
    }
    const auto freeType = std::unique_ptr<std::remove_pointer_t<FT_Library>, decltype(&FT_Done_FreeType)>(
        library, &FT_Done_FreeType);
    auto seen = QSet<QString>{};
    for (const auto& info : files) {
        const auto path = info.canonicalFilePath();
        if (path.isEmpty() || seen.contains(path)) continue;
        seen.insert(path);
        auto file = QFile(path);
        if (!file.open(QIODevice::ReadOnly)) {
            logger.warn(QString("Could not read icon font %1").arg(path));
            continue;
        }
        const auto data = file.readAll();
        auto face = FT_Face{};
        if (data.isEmpty() || data.size() > std::numeric_limits<FT_Long>::max()
            || FT_New_Memory_Face(library, reinterpret_cast<const FT_Byte*>(data.constData()),
                FT_Long(data.size()), 0, &face) != 0) {
            logger.warn(QString("Could not open icon font %1").arg(path));
            continue;
        }
        const auto faceOwner = std::unique_ptr<std::remove_pointer_t<FT_Face>, decltype(&FT_Done_Face)>(face, &FT_Done_Face);
        const auto raw = QRawFont(data, 64.0, QFont::PreferNoHinting);
        if (!raw.isValid() || !FT_HAS_GLYPH_NAMES(face)) {
            logger.warn(QString("Icon font has no usable named glyphs %1").arg(path));
            continue;
        }
        const auto source = QString("%1 (%2)").arg(raw.familyName(), path);
        auto count = 0;
        // Use glyph indices rather than the cmap. Ligature-only icons can
        // have a glyph name and an outline without a Unicode mapping.
        for (auto glyph = FT_UInt{1}; glyph < FT_ULong(face->num_glyphs); ++glyph) {
            auto name = std::array<char, 256>{};
            if (FT_Get_Glyph_Name(face, glyph, name.data(), FT_UInt(name.size())) != 0 || name.front() == '\0')
                continue;
            if (FT_Load_Glyph(face, glyph, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP) != 0
                || face->glyph->format != FT_GLYPH_FORMAT_OUTLINE || face->glyph->outline.n_points == 0)
                continue;
            auto entry = Entry{};
            entry.name = QString::fromLatin1(name.data());
            entry.source = source;
            entry.font = raw;
            entry.glyph = glyph;
            m_entries.append(std::move(entry));
            ++count;
        }
        logger.info(QString("Indexed %1 icons in font %2").arg(count).arg(source));
    }
}

void IconCatalog::assignNames(Logger& logger) {
    auto reserved = QSet<QString>{};
    for (const auto& entry : m_entries) reserved.insert(entry.name);
    auto assigned = QSet<QString>{};
    auto nextSuffix = QHash<QString, int>{};
    auto aliases = QHash<QString, QString>{};
    for (auto& entry : m_entries) {
        if (assigned.contains(entry.name)) {
            const auto original = entry.name;
            auto& suffix = nextSuffix[original];
            do {
                entry.name = original + '_' + QString::number(++suffix);
            } while (reserved.contains(entry.name) || assigned.contains(entry.name));
            logger.warn(QString("Duplicate icon name %1 in %2 indexed as %3").arg(original, entry.source, entry.name));
        }
        assigned.insert(entry.name);
        if (!entry.svgPath.isEmpty()) {
            aliases.tryEmplace(entry.legacyPath, entry.name);
            aliases.tryEmplace(entry.svgPath, entry.name);
        }
    }
    // Older macOS bundles flattened the SVG subdirectories. Resolve those
    // filenames after the real relative paths have claimed their aliases.
    for (const auto& entry : m_entries)
        if (!entry.svgPath.isEmpty()) aliases.tryEmplace(QFileInfo(entry.svgPath).fileName(), entry.name);
    std::ranges::sort(m_entries, [](const auto& a, const auto& b) {
        const auto comparison = QString::compare(a.name, b.name, Qt::CaseInsensitive);
        return comparison == 0 ? a.name < b.name : comparison < 0;
    });
    for (auto i = qsizetype{0}; i < m_entries.size(); ++i) m_byName.insert(m_entries[i].name, i);
    for (auto it = aliases.cbegin(); it != aliases.cend(); ++it) m_svgAliases.insert(it.key(), m_byName.value(it.value()));
}

QIcon IconCatalog::icon(const QString& name) const {
    const auto it = m_byName.constFind(name);
    if (it == m_byName.cend()) return {};
    const auto index = it.value();
    const auto& entry = m_entries[index];
    if (!entry.svgPath.isEmpty()) return VectorIcon(pathFromQString(entry.svgPath)).icon();
    if (const auto* cached = m_glyphIcons.object(index)) return *cached;
    const auto icon = VectorIcon::glyphIcon(entry.font, entry.glyph);
    m_glyphIcons.insert(index, new QIcon(icon));
    return icon;
}

QString IconCatalog::canonicalName(const QString& nameOrPath) const {
    const auto key = QDir::cleanPath(QDir::fromNativeSeparators(nameOrPath));
    if (const auto it = m_svgAliases.constFind(key); it != m_svgAliases.cend()) return m_entries[it.value()].name;
    if (m_byName.contains(nameOrPath)) return nameOrPath;
    return {};
}

} // namespace TrenchBroom::IO
