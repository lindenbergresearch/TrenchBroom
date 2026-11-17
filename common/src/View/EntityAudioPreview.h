#pragma once

#include "NotifierConnection.h"
#include "Result.h"

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QTemporaryDir>

#include <filesystem>
#include <memory>
#include <optional>

class QMediaPlayer;
class QMediaPlaylist;
class QWidget;

namespace TrenchBroom::Model {
class Entity;
class EntityNode;
} // namespace TrenchBroom::Model

namespace TrenchBroom::View {
class LayerListBox;
class MapDocument;

std::optional<std::filesystem::path> entityAudioPath(const Model::Entity& entity);
// Resolves the same resource search path as textures/models, including game archives.
Result<QByteArray> readEntityAudio(const MapDocument& document, const std::filesystem::path& path);

class EntityAudioPreview : public QObject {
    Q_OBJECT
  private:
    std::weak_ptr<MapDocument> m_document;
    QPointer<LayerListBox> m_tree;
    QMediaPlayer* m_player;
    QMediaPlaylist* m_playlist;
    QTemporaryDir m_directory;
    QString m_temporaryFile;
    const Model::EntityNode* m_selected = nullptr;
    std::filesystem::path m_path;
    bool m_treeSelection = false;
    NotifierConnection m_connections;

    void refresh();
    bool windowActive() const;

  public:
    explicit EntityAudioPreview(std::weak_ptr<MapDocument> document, QWidget* parent = nullptr);
    ~EntityAudioPreview() override;
    void stop();

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};
} // namespace TrenchBroom::View
