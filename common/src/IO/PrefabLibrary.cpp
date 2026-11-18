#include "IO/PrefabLibrary.h"

#include "IO/PathQt.h"
#include "IO/SystemPaths.h"

#include <QCryptographicHash>
#include <QCache>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>

namespace TrenchBroom::IO {
namespace {
[[noreturn]] void fail(const QString& message) { throw std::runtime_error(message.toStdString()); }

QByteArray read(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) fail(QString("Cannot read %1: %2").arg(path, file.errorString()));
    const auto data = file.readAll();
    if (file.error() != QFileDevice::NoError) fail(file.errorString());
    return data;
}

void write(const QString& path, const QByteArray& data) {
    if (!QDir{}.mkpath(QFileInfo(path).absolutePath())) fail("Cannot create asset directory");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        fail(QString("Cannot write %1: %2").arg(path, file.errorString()));
}

QString hash(const QByteArray& data) {
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString fileHash(const QString& path) {
    struct Stamp { qint64 size; std::filesystem::file_time_type modified; QDateTime created; QString digest; };
    static QCache<QString, Stamp> cache(512);
    const QFileInfo info(path);
    const auto key = info.absoluteFilePath();
    std::error_code error;
    const auto modified = std::filesystem::last_write_time(pathFromQString(path), error);
    if (const auto* stamp = cache.object(key); stamp && info.isFile()
        && !error && stamp->size == info.size() && stamp->modified == modified && stamp->created == info.birthTime())
        return stamp->digest;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) fail(QString("Cannot read %1: %2").arg(path, file.errorString()));
    QCryptographicHash hasher(QCryptographicHash::Sha256);
    if (!hasher.addData(&file)) fail("Cannot verify prefab asset: " + path);
    const auto digest = QString::fromLatin1(hasher.result().toHex());
    cache.insert(key, new Stamp{info.size(), modified, info.birthTime(), digest});
    return digest;
}

QString checkedDigest(const QJsonObject& asset) {
    const auto digest = asset.value("SHA256").toString();
    if (!QRegularExpression("^[0-9a-f]{64}$").match(digest).hasMatch()) fail("Invalid prefab asset checksum");
    return digest;
}

// Keep paths referenced by existing maps intact. On file systems supporting hard
// links, old copies can share the cached WAD's storage without changing those maps.
void shareStorage(const QString& cached, const QString& oldPath, const QString& digest) {
    if (!QFileInfo::exists(oldPath) || QFileInfo(oldPath).isSymLink()) return;
    std::error_code error;
    if (std::filesystem::equivalent(pathFromQString(cached), pathFromQString(oldPath), error)) return;
    if (fileHash(oldPath) != digest) return;
    QTemporaryFile temporary(QFileInfo(oldPath).absolutePath() + "/.wad-link-XXXXXX");
    if (!temporary.open()) return;
    const auto temporaryPath = temporary.fileName();
    temporary.close();
    if (!QFile::remove(temporaryPath)) return;
    std::filesystem::create_hard_link(pathFromQString(cached), pathFromQString(temporaryPath), error);
    if (error) return; // Leave the original copy intact if hard links are unavailable.
    std::filesystem::rename(pathFromQString(temporaryPath), pathFromQString(oldPath), error);
    // QTemporaryFile removes the temporary link if replacement was unsuccessful.
}

// Never follow library symlinks while reading, publishing or deleting a prefab.
void checkParents(const QString& root, const QString& relative) {
    auto current = root;
    for (const auto& part : relative.split('/')) {
        current += '/' + part;
        if (QFileInfo(current).isSymLink()) fail("Prefab paths cannot contain symbolic links");
    }
}
}

PrefabLibrary::PrefabLibrary(QString root)
    : m_root(QDir::cleanPath(root.isEmpty() ? pathAsQString(SystemPaths::userDataDirectory() / "prefabs")
                                          : QFileInfo(root).absoluteFilePath())) {}

const QString& PrefabLibrary::root() const { return m_root; }

QString PrefabLibrary::normalizedPath(QString path) {
    path = path.trimmed();
    path.replace('\\', '/');
    if (path.isEmpty() || path.startsWith('/') || path.endsWith('/')) fail("Enter a name, optionally preceded by folders (for example furniture/couch).");
    for (const auto& part : path.split('/')) {
        if (part.isEmpty() || part.startsWith('.') || part != part.trimmed()
            || part.contains(QRegularExpression("[\\x00-\\x1f:*?\"<>|]")))
            fail("Prefab names cannot contain empty folders, hidden folders or reserved filename characters.");
    }
    return path;
}

QString PrefabLibrary::directory(const QString& path) const {
    const auto normalized = normalizedPath(path);
    checkParents(m_root, normalized);
    return m_root + '/' + normalized;
}

QString PrefabLibrary::storeWad(const QString& digest, const QString& name, const QByteArray& data) const {
    const auto cache = m_root + "/.wad-cache";
    checkParents(m_root, ".wad-cache/" + digest);
    if (!QDir{}.mkpath(cache)) fail("Cannot create shared prefab WAD cache");
    QLockFile lock(cache + "/write.lock");
    if (!lock.tryLock(5000)) fail("The prefab WAD cache is busy. Please try again.");
    const auto folder = cache + '/' + digest;
    auto existing = QDir(folder).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
    existing.erase(std::remove_if(existing.begin(), existing.end(), [](const auto& file) { return !file.fileName().endsWith(".wad", Qt::CaseInsensitive); }), existing.end());
    if (!existing.empty()) {
        const auto file = existing.front().absoluteFilePath();
        if (fileHash(file) != digest) fail("A shared prefab WAD has been modified: " + file);
        return ".wad-cache/" + digest + '/' + existing.front().fileName();
    }
    auto filename = normalizedPath(QFileInfo(name).fileName());
    if (!filename.endsWith(".wad", Qt::CaseInsensitive)) filename += ".wad";
    const auto relative = ".wad-cache/" + digest + '/' + filename;
    checkParents(m_root, relative);
    if (hash(data) != digest) fail("A prefab WAD changed while being cached");
    write(m_root + '/' + relative, data);
    return relative;
}

void PrefabLibrary::migrateWads(const QString& dir, QJsonObject& descriptor) const {
    const auto legacy = [](const QJsonObject& object) {
        for (const auto& value : object.value("Assets").toArray()) {
            const auto asset = value.toObject();
            if (asset.value("Kind").toString() == "wad" && asset.value("Storage").toString() != "shared") return true;
        }
        return false;
    };
    if (!legacy(descriptor)) return;
    checkParents(dir, ".storage-upgrade.lock");
    QLockFile lock(dir + "/.storage-upgrade.lock");
    if (!lock.tryLock(5000)) fail("This prefab is being upgraded by another editor");
    const auto current = QJsonDocument::fromJson(read(dir + "/prefab.json")).object();
    if (current.value("Id") != descriptor.value("Id") || current.value("Path") != descriptor.value("Path"))
        fail("The prefab changed during its storage upgrade");
    descriptor = current;
    if (!legacy(descriptor)) return;
    QJsonArray assets, wads;
    std::vector<QString> obsolete;
    for (const auto& value : descriptor.value("Assets").toArray()) {
        auto asset = value.toObject();
        if (asset.value("Kind").toString() == "wad") {
            if (asset.value("Storage").toString() != "shared") {
                const auto file = normalizedPath(asset.value("File").toString());
                const auto resource = normalizedPath(asset.value("ResourcePath").toString());
                checkParents(dir, file);
                const auto data = read(dir + '/' + file);
                const auto digest = checkedDigest(asset);
                if (hash(data) != digest) fail("A prefab WAD is corrupt: " + resource);
                const auto cache = storeWad(digest, resource, data);
                asset["Storage"] = "shared"; asset["Cache"] = cache; asset.remove("File");
                obsolete.push_back(dir + '/' + file);
                const auto id = descriptor.value("Id").toString();
                if (QUuid(id).isNull()) fail("Invalid prefab identifier");
                const auto oldImport = ".assets/" + id + "/wads/" + resource;
                checkParents(m_root, oldImport);
                shareStorage(m_root + '/' + cache, m_root + '/' + oldImport, digest);
            }
            wads.append(asset.value("Cache"));
        }
        assets.append(asset);
    }
    descriptor["Version"] = 2; descriptor["Assets"] = assets; descriptor["WadFiles"] = wads;
    write(dir + "/prefab.json", QJsonDocument(descriptor).toJson(QJsonDocument::Indented));
    // The descriptor now references the shared files. Only now remove bundled copies.
    for (const auto& file : obsolete) {
        QFile::remove(file);
        auto parent = QFileInfo(file).absolutePath();
        while (parent != dir && parent.startsWith(dir + '/') && QDir{}.rmdir(parent)) parent = QFileInfo(parent).absolutePath();
    }
}

PrefabInfo PrefabLibrary::load(const QString& path) const {
    const auto dir = directory(path);
    checkParents(dir, "prefab.json");
    QJsonParseError error;
    const auto json = QJsonDocument::fromJson(read(dir + "/prefab.json"), &error);
    if (error.error != QJsonParseError::NoError || !json.isObject()) fail("Invalid prefab.json");
    auto object = json.object();
    const auto version = object.value("Version").toInt();
    if ((version != 1 && version != 2) || object.value("Path").toString() != normalizedPath(path))
        fail("Unsupported prefab version or mismatched prefab path");
    PrefabInfo info;
    info.id = object.value("Id").toString();
    if (QUuid(info.id).isNull()) fail("Invalid prefab identifier");
    info.path = normalizedPath(path);
    info.label = object.value("Label").toString();
    info.description = object.value("Description").toString();
    info.game = object.value("Game").toString();
    info.mapFormat = object.value("MapFormat").toString();
    const auto size = object.value("Size").toArray();
    if (size.size() != 3) fail("Invalid prefab dimensions");
    info.size = QVector3D(float(size[0].toDouble()), float(size[1].toDouble()), float(size[2].toDouble()));
    if (!std::isfinite(info.size.lengthSquared()) || info.size.x() < 0 || info.size.y() < 0 || info.size.z() < 0)
        fail("Invalid prefab dimensions");
    const auto camera = object.value("Preview").toObject();
    info.previewYaw = camera.value("Yaw").toDouble(-45.0);
    info.previewPitch = camera.value("Pitch").toDouble(30.0);
    info.previewBrightness = std::clamp(camera.value("Brightness").toDouble(1.0), 0.5, 1.5);
    if (!QFileInfo::exists(dir + "/fragment.map")) fail("Prefab fragment is missing");
    migrateWads(dir, object);
    info.assets = object.value("Assets").toArray();
    for (const auto& value : object.value("MissingAssets").toArray()) info.missingAssets << value.toString();
    return info;
}

std::vector<PrefabInfo> PrefabLibrary::entries(QStringList* errors) const {
    std::vector<PrefabInfo> result;
    // Explicit recursion excludes the imported asset cache and incomplete saves.
    const auto visit = [&](auto&& self, const QString& relative) -> void {
        const QDir dir(relative.isEmpty() ? m_root : m_root + '/' + relative);
        if (!relative.isEmpty() && QFileInfo::exists(dir.filePath("prefab.json"))) {
            try { result.push_back(load(relative)); }
            catch (const std::exception& e) { if (errors) *errors << relative + ": " + e.what(); }
            return;
        }
        for (const auto& entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name)) {
            if (!entry.fileName().startsWith('.')) self(self, relative.isEmpty() ? entry.fileName() : relative + '/' + entry.fileName());
        }
    };
    visit(visit, {});
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
    return result;
}

QByteArray PrefabLibrary::fragment(const QString& path) const {
    const auto dir = directory(path);
    checkParents(dir, "fragment.map");
    return read(dir + "/fragment.map");
}

PrefabInfo PrefabLibrary::save(PrefabInfo info, const QByteArray& fragmentData, const QImage& preview,
    const std::vector<PrefabAsset>& assets) const {
    info.path = normalizedPath(info.path);
    const auto target = directory(info.path);
    if (QFileInfo::exists(target)) fail("This prefab path already exists. Please choose another name.");
    if (fragmentData.isEmpty() || preview.isNull()) fail("Cannot save an empty prefab");
    auto ancestor = QFileInfo(target).absolutePath();
    while (ancestor != m_root && ancestor.startsWith(m_root + '/')) {
        if (QFileInfo::exists(ancestor + "/prefab.json")) fail("A prefab cannot also be used as a folder.");
        ancestor = QFileInfo(ancestor).absolutePath();
    }
    if (!QDir{}.mkpath(m_root)) fail("Cannot create prefab library");
    QTemporaryDir staging(m_root + "/.pending-XXXXXX");
    if (!staging.isValid()) fail("Cannot create temporary prefab directory");
    info.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    info.label = info.path.section('/', -1);
    info.assets = {};
    QJsonArray wads;
    for (const auto& asset : assets) {
        const auto digest = hash(asset.data);
        const auto resourcePath = normalizedPath(asset.resourcePath);
        if (asset.kind == "wad") {
            const auto cache = storeWad(digest, resourcePath, asset.data);
            info.assets.append(QJsonObject{{"Source", asset.source}, {"ResourcePath", resourcePath},
                {"Storage", "shared"}, {"Cache", cache}, {"Kind", asset.kind}, {"SHA256", digest}});
            wads.append(cache);
            continue;
        }
        const auto file = "assets/" + digest + '/' + resourcePath;
        write(staging.path() + '/' + file, asset.data);
        info.assets.append(QJsonObject{{"Source", asset.source}, {"ResourcePath", resourcePath},
            {"File", file}, {"Kind", asset.kind}, {"SHA256", digest}});
    }
    QJsonArray missing;
    for (const auto& asset : info.missingAssets) missing.append(asset);
    if (!std::isfinite(info.previewYaw) || !std::isfinite(info.previewPitch)) fail("Invalid prefab preview orientation");
    if (!std::isfinite(info.previewBrightness) || info.previewBrightness < 0.5 || info.previewBrightness > 1.5) fail("Invalid prefab preview brightness");
    const QJsonObject object{{"Version", 2}, {"Id", info.id}, {"Label", info.label}, {"Path", info.path},
        {"Description", info.description}, {"Game", info.game}, {"MapFormat", info.mapFormat},
        {"Size", QJsonArray{info.size.x(), info.size.y(), info.size.z()}}, {"WadFiles", wads},
        {"Assets", info.assets}, {"MissingAssets", missing}, {"Preview", QJsonObject{{"Yaw", info.previewYaw}, {"Pitch", info.previewPitch}, {"Brightness", info.previewBrightness}}}};
    write(staging.path() + "/fragment.map", fragmentData);
    write(staging.path() + "/prefab.json", QJsonDocument(object).toJson(QJsonDocument::Indented));
    if (!preview.save(staging.path() + "/preview.png", "PNG")) fail("Cannot write prefab preview");
    if (!QDir{}.mkpath(QFileInfo(target).absolutePath()) || !QDir{}.rename(staging.path(), target))
        fail("Cannot publish prefab. The path may already exist.");
    return info;
}

void PrefabLibrary::remove(const QString& path) const {
    load(path); // Require a valid entry, rather than deleting an arbitrary group.
    if (!QDir(directory(path)).removeRecursively()) fail("Cannot remove prefab");
    auto parent = QFileInfo(directory(path)).absolutePath();
    while (parent != m_root && parent.startsWith(m_root + '/') && QDir{}.rmdir(parent))
        parent = QFileInfo(parent).absolutePath();
}

PrefabAssetPaths PrefabLibrary::materializeAssets(const PrefabInfo& info) const {
    const auto source = directory(info.path);
    const auto target = m_root + "/.assets/" + info.id;
    PrefabAssetPaths result{target + "/resources", {}};
    for (const auto& value : info.assets) {
        const auto asset = value.toObject();
        if (asset.value("Kind").toString() == "wad" && asset.value("Storage").toString() == "shared") {
            const auto digest = checkedDigest(asset);
            const auto cache = asset.value("Cache").toString();
            const auto prefix = ".wad-cache/" + digest + '/';
            if (!cache.startsWith(prefix) || QStringView{cache}.mid(prefix.size()).contains(u'/')
                || normalizedPath(cache.mid(prefix.size())) != cache.mid(prefix.size())) fail("Invalid shared WAD reference");
            checkParents(m_root, cache);
            const auto path = m_root + '/' + cache;
            if (fileHash(path) != digest) fail("A shared prefab WAD is corrupt: " + path);
            if (!result.wadFiles.contains(path)) result.wadFiles << path;
            continue;
        }
        const auto file = normalizedPath(asset.value("File").toString());
        const auto resource = normalizedPath(asset.value("ResourcePath").toString());
        checkParents(source, file);
        const auto data = read(source + '/' + file);
        if (hash(data) != asset.value("SHA256").toString()) fail("A prefab asset is corrupt: " + resource);
        if (asset.value("Kind").toString() == "wad") {
            result.wadFiles << m_root + '/' + storeWad(checkedDigest(asset), resource, data);
            continue;
        }
        const auto destination = "resources/" + resource;
        checkParents(m_root, ".assets/" + info.id + '/' + destination);
        const auto output = target + '/' + destination;
        if (QFileInfo::exists(output)) {
            if (hash(read(output)) != hash(data)) fail("An imported prefab asset has been modified: " + resource);
        } else write(output, data);
    }
    return result;
}

void PrefabLibrary::migrateLegacyImports() const {
    const auto imports = m_root + "/.assets";
    if (!QFileInfo::exists(imports)) return;
    const auto marker = m_root + "/.wad-cache/legacy-imports-v1.done";
    checkParents(m_root, ".wad-cache/legacy-imports-v1.done");
    if (QFileInfo::exists(marker)) return;
    checkParents(m_root, ".assets");
    if (!QDir{}.mkpath(m_root + "/.wad-cache")) fail("Cannot create shared prefab WAD cache");
    QLockFile lock(m_root + "/.wad-cache/upgrade.lock");
    if (!lock.tryLock(0)) return;
    QDirIterator it(imports, {"*.wad", "*.WAD"}, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const auto file = it.next();
        const auto relative = QDir(m_root).relativeFilePath(file);
        checkParents(m_root, relative);
        if (!relative.contains("/wads/")) continue;
        const auto data = read(file);
        const auto digest = hash(data);
        const auto cache = storeWad(digest, QFileInfo(file).fileName(), data);
        shareStorage(m_root + '/' + cache, file, digest);
    }
    write(marker, "Shared WAD cache migration complete\n");
}
}
