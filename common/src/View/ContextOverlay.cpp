#include "ContextOverlay.h"

#include "IO/SystemPaths.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/ContextOverlayData.h"
#include "View/MapDocument.h"
#include "View/MapViewToolBox.h"

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStyle>
#include <algorithm>
#include <stdexcept>

namespace TrenchBroom::View {
namespace {
QString readThemeFile(const QString& directory, const QString& name, QStringList& files) {
    const auto relative = QDir::cleanPath(name);
    if (name.isEmpty() || QDir::isAbsolutePath(relative) || relative == ".." || relative.startsWith("../"))
        throw std::runtime_error("Overlay files must use relative paths within the theme folder");
    const auto path = QDir(directory).filePath(relative);
    files.append(path); // Also watch missing files after a failed reload.
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly) || file.size() > 256 * 1024)
        throw std::runtime_error(("Cannot read overlay file (maximum 256 KB): " + path).toStdString());

    return QString::fromUtf8(file.readAll());
}
} // namespace

QString ContextOverlay::defaultThemeDirectory() {
    return QString::fromStdString(IO::SystemPaths::findResourceFile("defaults/editor-overlay/overlay.json").parent_path().string());
}

ContextOverlay::Theme ContextOverlay::readTheme(const QString& directory, QStringList& files) {
    QJsonParseError error;
    const auto json = QJsonDocument::fromJson(readThemeFile(directory, "overlay.json", files).toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !json.isObject())
        throw std::runtime_error(("Invalid overlay.json: " + error.errorString()).toStdString());

    const auto object = json.object();
    if (object.value("version").toInt() != 1) throw std::runtime_error("Unsupported overlay theme version (expected 1)");

    Theme theme;
    theme.width = std::clamp(object.value("width").toInt(340), 160, 1200);
    theme.margin = std::clamp(object.value("margin").toInt(16), 0, 100);
    theme.style = readThemeFile(directory, object.value("stylesheet").toString("style.qss"), files);

    if (object.contains("textStylesheet"))
        theme.textStyle = readThemeFile(directory, object.value("textStylesheet").toString(), files);

    theme.content = ContextOverlayTemplate(readThemeFile(directory, object.value("template").toString("default.html"), files));
    const auto rules = object.value("rules");
    if (!rules.isUndefined() && !rules.isArray()) throw std::runtime_error("Overlay rules must be an array");
    if (rules.toArray().size() > 64) throw std::runtime_error("Overlay themes support at most 64 rules");

    for (const auto& value : rules.toArray()) {
        if (!value.isObject()) throw std::runtime_error("Each overlay rule must be an object");
        const auto rule = value.toObject();
        theme.rules.push_back({rule.value("tool").toString(), rule.value("selection").toString(),
            ContextOverlayTemplate(readThemeFile(directory, rule.value("template").toString(), files))});
    }

    return theme;
}

ContextOverlay::ContextOverlay(std::weak_ptr<MapDocument> document, MapViewToolBox& tools, QWidget* parent)
    : QLabel(parent), m_document(std::move(document)), m_tools(tools) {
    setObjectName("EditorContextOverlay");
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setTextInteractionFlags(Qt::NoTextInteraction);
    setTextFormat(Qt::RichText);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setWordWrap(true);
    hide();

    parent->installEventFilter(this);
    m_updateTimer.setSingleShot(true);
    m_updateTimer.setInterval(33);
    connect(&m_updateTimer, &QTimer::timeout, this, &ContextOverlay::updateContent);

    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(150);
    connect(&m_reloadTimer, &QTimer::timeout, this, &ContextOverlay::reloadTheme);

    const auto reload = [this](const QString&) { m_reloadTimer.start(); };
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, reload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, reload);

    const auto changed = [this](auto&&...) { scheduleUpdate(); };
    if (const auto doc = m_document.lock()) {
        m_connections += doc->documentWasNewedNotifier.connect(changed);
        m_connections += doc->documentWasLoadedNotifier.connect(changed);
        m_connections += doc->documentWasClearedNotifier.connect(changed);
        m_connections += doc->documentWillBeClearedNotifier.connect([this](auto*) { hide(); });
        m_connections += doc->selectionDidChangeNotifier.connect(changed);
        m_connections += doc->nodesDidChangeNotifier.connect(changed);
        m_connections += doc->nodesWereAddedNotifier.connect(changed);
        m_connections += doc->nodesWereRemovedNotifier.connect(changed);
        m_connections += doc->brushFacesDidChangeNotifier.connect(changed);
        m_connections += doc->currentLayerDidChangeNotifier.connect(changed);
        m_connections += doc->currentTextureNameDidChangeNotifier.connect(changed);
        m_connections += doc->groupWasOpenedNotifier.connect(changed);
        m_connections += doc->groupWasClosedNotifier.connect(changed);
        m_connections += doc->transactionDoneNotifier.connect(changed);
        m_connections += doc->transactionUndoneNotifier.connect(changed);
    }
    m_connections += tools.toolActivatedNotifier.connect(changed);
    m_connections += tools.toolDeactivatedNotifier.connect(changed);
    m_connections += tools.refreshViewsNotifier.connect(changed);
    m_connections += tools.toolHandleSelectionChangedNotifier.connect(changed);
    m_connections += PreferenceManager::instance().preferenceDidChangeNotifier.connect([this](const auto& path) {
        if (path == Preferences::ContextOverlayTheme.path()) reloadTheme();
        else if (path == Preferences::ShowContextOverlay.path()) scheduleUpdate();
    });

    // Keep a usable built-in theme if a custom file is missing or malformed.
    m_theme.content = ContextOverlayTemplate("<b>{{tool.name}}</b><br>{{selection.name}}");
    m_theme.style = "QLabel#EditorContextOverlay { color: white; background: rgba(25,28,34,220); padding: 12px; border-radius: 8px; }";
    try {
        QStringList files;
        m_theme = readTheme(defaultThemeDirectory(), files);
    } catch (const std::exception&) { /* The minimal theme above works without resources. */ }
    reloadTheme();
}

void ContextOverlay::reloadTheme() {
    m_directory = pref(Preferences::ContextOverlayTheme);
    if (m_directory.isEmpty()) m_directory = defaultThemeDirectory();
    m_themeFiles.clear();
    try {
        auto theme = readTheme(m_directory, m_themeFiles);
        m_theme = std::move(theme);
        m_configurationError.clear();
    } catch (const std::exception& error) {
        m_configurationError = QString::fromUtf8(error.what());
    }
    setProperty("configurationError", m_configurationError);
    setStyleSheet(m_theme.style);
    watchTheme();
    scheduleUpdate();
}

void ContextOverlay::watchTheme() {
    const auto oldPaths = m_watcher.files() + m_watcher.directories();
    if (!oldPaths.isEmpty()) m_watcher.removePaths(oldPaths);
    QStringList paths;
    const auto addExisting = [&paths](const QString& path) {
        if (QFileInfo::exists(path) && !paths.contains(path)) paths.append(path);
    };
    // Watching directories also handles editors that save by atomically renaming files.
    addExisting(m_directory);
    addExisting(QFileInfo(m_directory).absolutePath());
    for (const auto& file : m_themeFiles) {
        addExisting(file);
        addExisting(QFileInfo(file).absolutePath());
    }
    if (!paths.isEmpty()) m_watcher.addPaths(paths);
}

void ContextOverlay::scheduleUpdate() {
    if (!m_updateTimer.isActive()) m_updateTimer.start();
}

void ContextOverlay::updateContent() {
    const auto document = m_document.lock();
    if (!pref(Preferences::ShowContextOverlay) || !parentWidget()->isVisible() || !document || !document->world()) {
        hide();
        return;
    }
    const auto data = contextOverlayData(*document, m_tools);
    const auto tool = data.value("tool").toMap().value("id").toString();
    const auto selection = data.value("selection").toMap().value("type").toString();
    const ContextOverlayTemplate* content = &m_theme.content;
    for (const auto& rule : m_theme.rules) {
        if ((rule.tool.isEmpty() || rule.tool == tool) && (rule.selection.isEmpty() || rule.selection == selection)) {
            content = &rule.content;
            break;
        }
    }
    if (property("tool").toString() != tool || property("selection").toString() != selection) {
        setProperty("tool", tool);
        setProperty("selection", selection);
        style()->unpolish(this);
        style()->polish(this);
    }
    auto html = "<style type=\"text/css\">" + m_theme.textStyle + "</style>" + content->render(data);
    if (!m_configurationError.isEmpty())
        html += "<p><small>" + tr("Overlay configuration error: %1").arg(m_configurationError).toHtmlEscaped() + "</small></p>";
    if (html != text()) setText(html);
    place();
    show();
    raise();
}

void ContextOverlay::place() {
    const auto* view = parentWidget();
    const int margin = std::min({m_theme.margin, view->width() / 4, view->height() / 4});
    const int width = std::max(1, std::min(m_theme.width, view->width() - 2 * margin));
    setFixedWidth(width);
    const int height = std::max(1, std::min(heightForWidth(width), view->height() - 2 * margin));
    resize(width, height);
    move(view->width() - width - margin, margin);
}

bool ContextOverlay::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget()) {
        if (event->type() == QEvent::Resize) place();
        else if (event->type() == QEvent::Show || event->type() == QEvent::MouseButtonRelease
                 || event->type() == QEvent::MouseButtonPress || event->type() == QEvent::KeyRelease)
            scheduleUpdate();
    }
    return QLabel::eventFilter(watched, event);
}
} // namespace TrenchBroom::View
