#pragma once

#include "NotifierConnection.h"
#include "View/ContextOverlayTemplate.h"

#include <QFileSystemWatcher>
#include <QLabel>
#include <QTimer>
#include <memory>

namespace TrenchBroom::View {
class MapDocument;
class MapViewToolBox;

class ContextOverlay : public QLabel {
    Q_OBJECT
    struct Rule {
        QString tool;
        QString selection;
        ContextOverlayTemplate content;
    };
    struct Theme {
        int width = 340;
        int margin = 16;
        QString style;
        QString textStyle;
        ContextOverlayTemplate content;
        std::vector<Rule> rules;
    };

    std::weak_ptr<MapDocument> m_document;
    MapViewToolBox& m_tools;
    Theme m_theme;
    QString m_configurationError;
    QString m_directory;
    QStringList m_themeFiles;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
    QTimer m_updateTimer;
    NotifierConnection m_connections;

public:
    ContextOverlay(std::weak_ptr<MapDocument> document, MapViewToolBox& tools, QWidget* parent);
    static QString defaultThemeDirectory();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    static Theme readTheme(const QString& directory, QStringList& files);
    void reloadTheme();
    void watchTheme();
    void scheduleUpdate();
    void updateContent();
    void place();
};
} // namespace TrenchBroom::View
