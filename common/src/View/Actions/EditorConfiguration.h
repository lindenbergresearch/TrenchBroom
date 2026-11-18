#pragma once

#include <QJsonObject>
#include <QKeySequence>
#include <QMap>
#include <QStringList>
#include <array>
#include <optional>
#include <vector>

namespace TrenchBroom::View {
enum EditorActionTarget { MainMenuTarget = 1, PopupMenuTarget = 2, MainToolbarTarget = 4, EditorToolbarTarget = 8 };
enum class EditorToolbarEdge { Left, Top, Right, Bottom };
enum class EditorToolbarAlignment { Start, Center, End };
enum class EditorToolbarArrangement { Sequential, Parallel };

struct EditorActionDefinition {
    QString id, label, command, icon, tooltip, menuPath = "Editor Actions";
    std::optional<QKeySequence> shortcut;
    int targets = EditorToolbarTarget;
    int order = 10000, toolbarGroup = 100;
    bool enabled = true;
    bool operator==(const EditorActionDefinition&) const = default;
};

struct EditorToolbarDefinition {
    QString id, label;
    EditorToolbarEdge edge = EditorToolbarEdge::Left;
    EditorToolbarAlignment alignment = EditorToolbarAlignment::Center;
    int iconSize = 24, minimumIconSize = 12;
    bool enabled = true;
    QStringList actions;
    bool operator==(const EditorToolbarDefinition&) const = default;
};

struct EditorConfiguration {
    QMap<QString, EditorActionDefinition> actions;
    std::vector<EditorToolbarDefinition> toolbars;
    std::array<EditorToolbarArrangement, 4> arrangements{};
    bool operator==(const EditorConfiguration&) const = default;
};

QString editorToolbarEdgeName(EditorToolbarEdge edge);
QJsonObject editorActionJson(const EditorActionDefinition& action);
QJsonObject editorToolbarJson(const EditorToolbarDefinition& toolbar);
QByteArray editorConfigurationJson(const EditorConfiguration& config, const EditorConfiguration* defaults = nullptr);
// Transactional: invalid input never modifies result. Commands are native editor
// command IDs; configurations cannot execute arbitrary scripts or shell text.
bool readEditorConfiguration(const QByteArray& bytes, const EditorConfiguration& defaults, const QStringList& commands, EditorConfiguration& result, QStringList& errors);
} // namespace TrenchBroom::View
