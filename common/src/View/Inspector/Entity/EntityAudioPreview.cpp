#include "View/Inspector/Entity/EntityAudioPreview.h"

#include "Error.h"
#include "IO/File.h"
#include "IO/FileSystem.h"
#include "Logger.h"
#include "Model/EntityNode.h"
#include "Model/Game.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/Inspector/Layer/LayerListBox.h"
#include "View/Document/MapDocument.h"
#include "View/App/MapFrame.h"
#include "View/Theme/QtUtils.h"

#include <QApplication>
#include <QEvent>
#include <QFile>
#include <QMediaPlayer>
#include <QMediaPlaylist>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

#include <kdl/result.h>
#include <kdl/string_utils.h>

#include <algorithm>
#include <set>

namespace TrenchBroom::View {
namespace {
    constexpr auto MaxAudioBytes = size_t{64 * 1024 * 1024};

    std::optional<std::filesystem::path> audioPath(std::string value) {
        value = kdl::str_trim(value);
        // Quake's streaming flag is part of the sound reference, not its filename.
        if (!value.empty() && value.front() == '*') value.erase(0, 1);
        std::replace(value.begin(), value.end(), '\\', '/');
        if (value.empty() || value.find_first_of("\r\n") != std::string::npos || value.find('\0') != std::string::npos) return {};
        auto path = std::filesystem::path{value}.lexically_normal();
        static const auto extensions = std::set<std::string>{".wav", ".wave", ".ogg", ".mp3", ".flac", ".opus", ".aiff", ".aif", ".m4a", ".aac", ".wma"};
        return extensions.count(kdl::str_to_lower(path.extension().string())) ? std::optional{path} : std::nullopt;
    }

    Result<QByteArray> readLocalAudio(const std::filesystem::path& path) {
        auto file = QFile{QString::fromStdString(path.string())};
        if (!file.open(QIODevice::ReadOnly)) return Error{"Audio file not found: " + path.string()};
        if (file.size() <= 0 || file.size() > static_cast<qint64>(MaxAudioBytes)) return Error{"Audio preview file is empty or exceeds 64 MiB"};
        auto bytes = file.readAll();
        if (bytes.size() != file.size()) return Error{"Could not read audio file: " + path.string()};
        return bytes;
    }
} // namespace

std::optional<std::filesystem::path> entityAudioPath(const Model::Entity& entity) {
    if (!entity.pointEntity()) return {};
    for (const auto* key : {"noise", "sound", "soundfile", "sound_file", "wav", "message", "noise1", "noise2", "noise3"})
        if (const auto* value = entity.property(key))
            if (auto path = audioPath(*value)) return path;
    for (const auto& property : entity.properties())
        if (auto path = audioPath(property.value())) return path;
    return {};
}

Result<QByteArray> readEntityAudio(const MapDocument& document, const std::filesystem::path& path) {
    if (path.is_absolute()) return readLocalAudio(path);
    const auto game = document.game();
    if (const auto* fs = game ? game->fileSystem() : nullptr) {
        for (const auto& candidate : {path, std::filesystem::path{"sound"} / path}) {
            auto result = fs->openFile(candidate);
            if (!result.is_success()) continue;
            const auto file = std::move(result).value();
            if (file->size() == 0 || file->size() > MaxAudioBytes) return Error{"Audio preview file is empty or exceeds 64 MiB"};
            try {
                const auto reader = file->reader().buffer();
                const auto bytes = reader.stringView();
                return QByteArray{bytes.data(), static_cast<int>(bytes.size())};
            } catch (const std::exception& e) {
                return Error{e.what()};
            }
        }
    }
    if (document.path().is_absolute()) {
        auto local = readLocalAudio(document.path().parent_path() / path);
        if (local.is_success()) return local;
    }
    return Error{"Audio resource not found: " + path.string()};
}

EntityAudioPreview::EntityAudioPreview(std::weak_ptr<MapDocument> document, QWidget* parent)
    : QObject{parent}, m_document{std::move(document)}, m_player{new QMediaPlayer{this}}, m_playlist{new QMediaPlaylist{this}} {
    setObjectName("entityAudioPreview");
    m_player->setObjectName("entityAudioPlayer");
    m_player->setPlaylist(m_playlist);
    if (parent) {
        parent->installEventFilter(this);
        m_tree = parent->findChild<LayerListBox*>();
        if (m_tree)
            connect(m_tree, &LayerListBox::itemSelectionChanged, this, [this] {
                m_treeSelection = true;
                refresh();
            });
    }
    if (const auto doc = m_document.lock()) {
        m_connections += doc->selectionDidChangeNotifier.connect([this](const auto&) {
            m_treeSelection = false;
            refresh();
        });
        m_connections += doc->nodesDidChangeNotifier.connect([this](const auto&) { refresh(); });
        m_connections += doc->documentWillBeClearedNotifier.connect([this](auto*) { stop(); });
        m_connections += doc->nodesWillBeRemovedNotifier.connect([this](const auto& nodes) {
            if (m_selected && std::any_of(nodes.begin(), nodes.end(), [this](const auto* node) { return node == m_selected || node->isAncestorOf(m_selected); })) stop();
        });
    }
    m_connections += PreferenceManager::instance().preferenceDidChangeNotifier.connect([this](const auto& path) {
        if (path == Preferences::PreviewEntityAudio.path() || path == Preferences::LoopEntityAudio.path() || path == Preferences::EntityAudioVolume.path()) refresh();
    });
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](const auto state) {
        if (state != Qt::ApplicationActive) stop();
    });
    connect(m_player, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error), this, [this](const auto error) {
        if (error != QMediaPlayer::NoError)
            if (const auto doc = m_document.lock()) doc->logger().warn() << "Audio preview: " << m_player->errorString().toStdString();
    });
}

EntityAudioPreview::~EntityAudioPreview() {
    stop();
    // Release backend file handles before the temporary directory is removed.
    delete m_player;
    delete m_playlist;
}

bool EntityAudioPreview::windowActive() const {
    const auto* frame = qobject_cast<MapFrame*>(parent());
    if (!frame) return true;
    const auto* active = QApplication::activeWindow();
    return frame->isActiveWindow() || (active && findMapFrame(const_cast<QWidget*>(active)) == frame);
}

void EntityAudioPreview::stop() {
    m_player->stop();
    m_playlist->clear();
    if (!m_temporaryFile.isEmpty()) {
        QFile::remove(m_temporaryFile);
        m_temporaryFile.clear();
    }
    m_selected = nullptr;
    m_path.clear();
}

void EntityAudioPreview::refresh() {
    const auto doc = m_document.lock();
    if (!doc || !pref(Preferences::PreviewEntityAudio) || !windowActive()) {
        stop();
        return;
    }
    const auto& selected = doc->selectedNodes().nodes();
    const auto* node = m_treeSelection && m_tree ? m_tree->singleSelectedTreeNode() : selected.size() == 1 ? selected.front() : nullptr;
    const auto* entity = dynamic_cast<const Model::EntityNode*>(node);
    const auto path = entity ? entityAudioPath(entity->entity()) : std::nullopt;
    m_playlist->setPlaybackMode(pref(Preferences::LoopEntityAudio) ? QMediaPlaylist::CurrentItemInLoop : QMediaPlaylist::CurrentItemOnce);
    m_player->setVolume(std::clamp(pref(Preferences::EntityAudioVolume), 0, 100));
    if (entity == m_selected && path.value_or(std::filesystem::path{}) == m_path) {
        // Some backends clear the active playlist item after a single pass, so
        // the media status is already NoMedia rather than EndOfMedia here.
        if (path && pref(Preferences::LoopEntityAudio) && m_player->state() == QMediaPlayer::StoppedState && m_player->error() == QMediaPlayer::NoError && m_playlist->mediaCount() == 1) {
            m_playlist->setCurrentIndex(0);
            m_player->play();
        }
        return;
    }
    stop();
    if (!entity || !path) return;
    m_selected = entity;
    m_path = *path;
    const auto result = readEntityAudio(*doc, *path);
    if (!result.is_success()) {
        (void)result.if_error([&](const auto& error) { doc->logger().warn() << "Audio preview: " << error.msg; });
        return;
    }
    if (!m_directory.isValid()) return;
    auto file = QTemporaryFile{m_directory.filePath("preview-XXXXXX" + QString::fromStdString(path->extension().string()))};
    if (!file.open() || file.write(result.value()) != result.value().size()) {
        doc->logger().warn() << "Could not create temporary audio preview";
        return;
    }
    m_temporaryFile = file.fileName();
    file.setAutoRemove(false);
    file.close();
    m_playlist->addMedia(QUrl::fromLocalFile(m_temporaryFile));
    m_playlist->setCurrentIndex(0);
    m_player->play();
}

bool EntityAudioPreview::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::WindowDeactivate)
        QTimer::singleShot(0, this, [this] {
            if (!windowActive()) stop();
        });
    return QObject::eventFilter(watched, event);
}
} // namespace TrenchBroom::View
