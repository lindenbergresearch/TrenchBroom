#include "View/Actions/EditorConfiguration.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace TrenchBroom::View {
QString editorToolbarEdgeName(EditorToolbarEdge edge) {
    static const QStringList names{"left", "top", "right", "bottom"};
    return names.value(int(edge));
}

QJsonObject editorActionJson(const EditorActionDefinition& a) {
    QJsonArray targets;
    for (const auto& pair : {std::pair{MainMenuTarget, "mainMenu"}, {PopupMenuTarget, "popupMenu"}, {MainToolbarTarget, "mainToolbar"}, {EditorToolbarTarget, "editorToolbar"}})
        if (a.targets & pair.first) targets.append(pair.second);
    QJsonObject object{{"id", a.id},         {"label", a.label}, {"command", a.command},           {"icon", a.icon},      {"tooltip", a.tooltip}, {"menuPath", a.menuPath},
                       {"targets", targets}, {"order", a.order}, {"toolbarGroup", a.toolbarGroup}, {"enabled", a.enabled}};
    if (a.shortcut) object.insert("shortcut", a.shortcut->toString(QKeySequence::PortableText));
    auto iconStyle = QJsonObject{};
    if (a.iconStyle.color.isValid()) iconStyle.insert("color", a.iconStyle.color.name(QColor::HexArgb));
    if (a.iconStyle.brightness) iconStyle.insert("brightness", *a.iconStyle.brightness);
    if (a.iconStyle.opacity) iconStyle.insert("opacity", *a.iconStyle.opacity);
    if (!iconStyle.isEmpty()) object.insert("iconStyle", iconStyle);
    return object;
}

QJsonObject editorToolbarJson(const EditorToolbarDefinition& t) {
    return {{"id", t.id},
            {"label", t.label},
            {"edge", editorToolbarEdgeName(t.edge)},
            {"alignment", t.alignment == EditorToolbarAlignment::Start ? "start" : t.alignment == EditorToolbarAlignment::End ? "end" : "center"},
            {"iconSize", t.iconSize},
            {"minimumIconSize", t.minimumIconSize},
            {"enabled", t.enabled},
            {"actions", QJsonArray::fromStringList(t.actions)}};
}

QByteArray editorConfigurationJson(const EditorConfiguration& config, const EditorConfiguration* defaults) {
    QJsonObject root{{"version", 1}};
    QJsonArray actions, toolbars;
    for (const auto& action : config.actions) {
        auto object = editorActionJson(action);
        if (defaults && defaults->actions.contains(action.id)) {
            const auto base = editorActionJson(defaults->actions[action.id]);
            for (const auto& key : base.keys())
                if (key != "id" && object.value(key) == base.value(key)) object.remove(key);
            if (!action.shortcut && base.contains("shortcut")) object.insert("shortcut", QJsonValue{});
            if (action.iconStyle == IO::IconStyle{} && base.contains("iconStyle")) object.insert("iconStyle", QJsonObject{});
            if (object.size() == 1) continue;
        }
        actions.append(object);
    }
    root.insert("actions", actions);
    if (!defaults || config.toolbars != defaults->toolbars) {
        for (const auto& toolbar : config.toolbars)
            toolbars.append(editorToolbarJson(toolbar));
        root.insert("toolbars", toolbars);
    }
    QJsonObject sides;
    for (size_t i = 0; i < config.arrangements.size(); ++i) {
        if (!defaults || config.arrangements[i] != defaults->arrangements[i])
            sides.insert(editorToolbarEdgeName(EditorToolbarEdge(i)), config.arrangements[i] == EditorToolbarArrangement::Parallel ? "parallel" : "sequential");
    }
    root.insert("sides", sides);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool readEditorConfiguration(const QByteArray& bytes, const EditorConfiguration& defaults, const QStringList& commands, EditorConfiguration& result, QStringList& errors) {
    auto config = defaults;
    if (bytes.trimmed().isEmpty()) {
        result = config;
        return true;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    const auto fail = [&](const QString& message) {
        errors << message;
        return false;
    };
    if (error.error != QJsonParseError::NoError || !document.isObject()) return fail("Invalid editor configuration: " + error.errorString());
    const auto root = document.object();
    const auto knownFields = [&](const QJsonObject& object, const QStringList& allowed, const QString& location) {
        for (const auto& key : object.keys())
            if (!allowed.contains(key)) return fail("Unknown editor configuration field: " + location + key);
        return true;
    };
    if (!knownFields(root, {"version", "actions", "toolbars", "sides"}, {})) return false;
    if (!root.value("version").isDouble() || root.value("version").toDouble() != 1) return fail("Unsupported editor configuration version");
    const auto integer = [](const QJsonValue& value, int minimum, int maximum) {
        return value.isDouble() && std::isfinite(value.toDouble()) && value.toDouble() == std::floor(value.toDouble()) && value.toDouble() >= minimum && value.toDouble() <= maximum;
    };
    if (root.contains("actions")) {
        if (!root["actions"].isArray() || root["actions"].toArray().size() > 2000) return fail("Actions must be an array with at most 2000 entries");
        QSet<QString> ids;
        for (const auto& value : root["actions"].toArray()) {
            if (!value.isObject()) return fail("Invalid action entry");
            const auto object = value.toObject();
            if (!knownFields(object, {"id", "label", "command", "icon", "iconStyle", "tooltip", "menuPath", "shortcut", "targets", "order", "toolbarGroup", "enabled"}, "actions/")) return false;
            const auto id = object["id"].toString();
            if (id.isEmpty() || id.size() > 512 || id.contains(QChar::Null) || ids.contains(id)) return fail("Invalid or duplicate action ID: " + id);
            ids.insert(id);
            auto action = config.actions.value(id);
            action.id = id;
            for (const auto& pair : {std::pair{"label", &action.label}, {"command", &action.command}, {"icon", &action.icon}, {"tooltip", &action.tooltip}, {"menuPath", &action.menuPath}}) {
                if (!object.contains(pair.first)) continue;
                if (!object[pair.first].isString() || object[pair.first].toString().contains(QChar::Null)) return fail("Invalid action field: " + id + "/" + pair.first);
                *pair.second = object[pair.first].toString();
            }
            if (object.contains("iconStyle")) {
                if (!object["iconStyle"].isObject()) return fail("Invalid icon style: " + id);
                const auto style = object["iconStyle"].toObject();
                if (!knownFields(style, {"color", "brightness", "opacity"}, "actions/" + id + "/iconStyle/")) return false;
                action.iconStyle = {};
                if (style.contains("color")) {
                    action.iconStyle.color = QColor::fromString(style["color"].toString());
                    if (!action.iconStyle.color.isValid()) return fail("Invalid icon color: " + id);
                }
                for (const auto& pair : {std::pair{"brightness", &action.iconStyle.brightness}, {"opacity", &action.iconStyle.opacity}}) {
                    if (!style.contains(pair.first)) continue;
                    const auto amount = style[pair.first].toDouble(-1.0);
                    if (!std::isfinite(amount) || amount < 0 || amount > 1) return fail("Invalid icon style value: " + id + "/" + pair.first);
                    *pair.second = amount;
                }
            }
            if (action.label.trimmed().isEmpty() || !commands.contains(action.command)) return fail("Unknown command or empty action label: " + id);
            const auto menu = action.menuPath.split('/');
            if (menu.size() > 8 || std::any_of(menu.begin(), menu.end(), [](const auto& part) { return part.trimmed().isEmpty(); })) return fail("Invalid menu path: " + id);
            if (object.contains("enabled")) {
                if (!object["enabled"].isBool()) return fail("Invalid enabled flag: " + id);
                action.enabled = object["enabled"].toBool();
            }
            for (const auto& pair : {std::pair{"order", &action.order}, {"toolbarGroup", &action.toolbarGroup}}) {
                if (object.contains(pair.first)) {
                    if (!integer(object[pair.first], 0, 1000000)) return fail("Invalid action order/group: " + id);
                    *pair.second = object[pair.first].toInt();
                }
            }
            if (object.contains("shortcut")) {
                if (object["shortcut"].isNull())
                    action.shortcut.reset();
                else if (!object["shortcut"].isString())
                    return fail("Invalid shortcut: " + id);
                else {
                    const auto text = object["shortcut"].toString();
                    const QKeySequence key{text, QKeySequence::PortableText};
                    if (!text.isEmpty() && key.isEmpty()) return fail("Invalid shortcut: " + id);
                    for (auto stroke = uint{0}; stroke < uint(key.count()); ++stroke)
                        if ((key[stroke] & ~int(Qt::KeyboardModifierMask)) == Qt::Key_unknown) return fail("Invalid shortcut: " + id);
                    action.shortcut = key;
                }
            }
            if (object.contains("targets")) {
                if (!object["targets"].isArray()) return fail("Invalid action targets: " + id);
                action.targets = 0;
                for (const auto& target : object["targets"].toArray()) {
                    if (target == QJsonValue{"mainMenu"})
                        action.targets |= MainMenuTarget;
                    else if (target == QJsonValue{"popupMenu"})
                        action.targets |= PopupMenuTarget;
                    else if (target == QJsonValue{"mainToolbar"})
                        action.targets |= MainToolbarTarget;
                    else if (target == QJsonValue{"editorToolbar"})
                        action.targets |= EditorToolbarTarget;
                    else
                        return fail("Unknown action target: " + id);
                }
            }
            config.actions.insert(id, action);
        }
    }
    if (root.contains("toolbars")) {
        if (!root["toolbars"].isArray() || root["toolbars"].toArray().size() > 64) return fail("Toolbars must be an array with at most 64 entries");
        config.toolbars.clear();
        QSet<QString> ids;
        for (const auto& value : root["toolbars"].toArray()) {
            if (!value.isObject()) return fail("Invalid toolbar entry");
            const auto object = value.toObject();
            if (!knownFields(object, {"id", "label", "edge", "alignment", "iconSize", "minimumIconSize", "enabled", "actions"}, "toolbars/")) return false;
            EditorToolbarDefinition t;
            t.id = object["id"].toString();
            t.label = object["label"].toString();
            if (t.id.isEmpty() || t.label.trimmed().isEmpty() || ids.contains(t.id)) return fail("Invalid toolbar ID or label");
            ids.insert(t.id);
            const auto edge = object["edge"].toString();
            const QStringList edgeNames{"left", "top", "right", "bottom"};
            if (!edgeNames.contains(edge)) return fail("Invalid toolbar edge");
            t.edge = EditorToolbarEdge(edgeNames.indexOf(edge));
            const auto alignment = object["alignment"].toString("center");
            if (alignment != "start" && alignment != "center" && alignment != "end") return fail("Invalid toolbar alignment");
            t.alignment = alignment == "start" ? EditorToolbarAlignment::Start : alignment == "end" ? EditorToolbarAlignment::End : EditorToolbarAlignment::Center;
            for (const auto& pair : {std::pair{"iconSize", &t.iconSize}, {"minimumIconSize", &t.minimumIconSize}}) {
                if (!object.contains(pair.first)) continue;
                if (!integer(object[pair.first], 8, 128)) return fail("Toolbar icon sizes must be between 8 and 128");
                *pair.second = object[pair.first].toInt();
            }
            if (t.minimumIconSize > t.iconSize) return fail("Minimum toolbar size exceeds preferred size");
            if (object.contains("enabled")) {
                if (!object["enabled"].isBool()) return fail("Invalid toolbar enabled flag");
                t.enabled = object["enabled"].toBool();
            }
            if (!object["actions"].isArray() || object["actions"].toArray().size() > 200) return fail("Invalid toolbar actions");
            for (const auto& action : object["actions"].toArray()) {
                if (!action.isString() || (!action.toString().isEmpty() && !config.actions.contains(action.toString()))) return fail("Unknown toolbar action");
                t.actions.push_back(action.toString()); // Empty ID is a separator.
            }
            config.toolbars.push_back(std::move(t));
        }
    }
    if (root.contains("sides")) {
        if (!root["sides"].isObject()) return fail("Invalid toolbar side configuration");
        const auto sides = root["sides"].toObject();
        if (!knownFields(sides, {"left", "top", "right", "bottom"}, "sides/")) return false;
        for (auto i = 0; i < 4; ++i) {
            const auto name = editorToolbarEdgeName(EditorToolbarEdge(i));
            if (!sides.contains(name)) continue;
            const auto mode = sides[name].toString();
            if (mode != "parallel" && mode != "sequential") return fail("Invalid toolbar arrangement");
            config.arrangements[size_t(i)] = mode == "parallel" ? EditorToolbarArrangement::Parallel : EditorToolbarArrangement::Sequential;
        }
    }
    result = std::move(config);
    return true;
}
} // namespace TrenchBroom::View
