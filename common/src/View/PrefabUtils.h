#pragma once

#include "IO/PrefabLibrary.h"

#include <memory>
#include <vector>

class QWidget;
class QMimeData;

namespace TrenchBroom::Model { class Node; }
namespace TrenchBroom::View {
class MapDocument;
class MapView;
class PrefabPreviewGeometry;

inline constexpr auto PrefabMimeType = "application/x-trenchbroom-prefab";

struct PrefabSnapshot {
    IO::PrefabInfo info;
    QByteArray fragment;
    QImage preview;
    std::shared_ptr<const PrefabPreviewGeometry> previewGeometry;
    std::vector<IO::PrefabAsset> assets;
};

std::vector<Model::Node*> prefabSelection(const MapDocument& document);
QString suggestedPrefabName(const std::vector<Model::Node*>& nodes);
PrefabSnapshot makePrefabSnapshot(MapDocument& document, const std::vector<Model::Node*>& nodes);
bool savePrefabDialog(const std::shared_ptr<MapDocument>& document, const std::vector<Model::Node*>& nodes,
    QWidget* parent, const QString& folder = {}, const IO::PrefabLibrary& library = IO::PrefabLibrary{});
bool insertPrefab(const std::shared_ptr<MapDocument>& document, MapView* view, const QString& path,
    bool linked, QWidget* parent, const IO::PrefabLibrary& library = IO::PrefabLibrary{});
bool insertPrefabMime(const std::shared_ptr<MapDocument>& document, MapView* view, const QMimeData& mime, QWidget* parent);
}
