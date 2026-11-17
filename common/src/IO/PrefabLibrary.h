#pragma once

#include <QByteArray>
#include <QImage>
#include <QJsonArray>
#include <QStringList>
#include <QVector3D>

#include <vector>

class QJsonObject;

namespace TrenchBroom::IO {
struct PrefabAsset {
    QString source;
    QString resourcePath;
    QString kind;
    QByteArray data;
};

struct PrefabInfo {
    QString id;
    QString path;
    QString label;
    QString description;
    QString game;
    QString mapFormat;
    QVector3D size;
    double previewYaw = -45.0;
    double previewPitch = 30.0;
    double previewBrightness = 1.0;
    QJsonArray assets;
    QStringList missingAssets;
};

struct PrefabAssetPaths {
    QString resourceDirectory;
    QStringList wadFiles;
};

// A prefab is published by renaming a complete temporary directory. Readers never
// see an entry whose fragment, assets or thumbnail are still being written.
class PrefabLibrary {
    QString m_root;

    QString storeWad(const QString& digest, const QString& name, const QByteArray& data) const;
    void migrateWads(const QString& directory, QJsonObject& descriptor) const;

public:
    explicit PrefabLibrary(QString root = {});
    const QString& root() const;
    static QString normalizedPath(QString path);
    QString directory(const QString& path) const;
    std::vector<PrefabInfo> entries(QStringList* errors = nullptr) const;
    PrefabInfo load(const QString& path) const;
    QByteArray fragment(const QString& path) const;
    PrefabInfo save(PrefabInfo info, const QByteArray& fragment, const QImage& preview,
        const std::vector<PrefabAsset>& assets) const;
    void remove(const QString& path) const;

    // Imported maps must retain their assets even after the library entry is removed.
    // Content-addressed files are shared between insertions and never overwritten.
    PrefabAssetPaths materializeAssets(const PrefabInfo& info) const;
    void migrateLegacyImports() const;
};
}
