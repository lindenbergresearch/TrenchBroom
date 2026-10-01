#include "ContextOverlayData.h"

#include "Model/BrushNode.h"
#include "Model/BrushFace.h"
#include "Model/EntityNode.h"
#include "Model/GroupNode.h"
#include "Model/LayerNode.h"
#include "Model/PatchNode.h"
#include "Model/WorldNode.h"
#include "View/MapDocument.h"
#include "View/MapViewToolBox.h"

#include <QObject>
#include <QStringList>
#include <set>
#include <unordered_set>

namespace TrenchBroom::View {
namespace {
QString text(const std::string& value) { return QString::fromStdString(value); }
QString number(const double value) { return QString::number(value == 0.0 ? 0.0 : value, 'f', 1); }
QVariantMap vectorData(const vm::vec3& v) {
    return {{"x", v.x()}, {"y", v.y()}, {"z", v.z()},
        {"text", QString("%1, %2, %3").arg(number(v.x()), number(v.y()), number(v.z()))}};
}

QString nodeType(const Model::Node* node) {
    if (dynamic_cast<const Model::GroupNode*>(node)) return "group";
    if (dynamic_cast<const Model::EntityNode*>(node)) return "entity";
    if (dynamic_cast<const Model::BrushNode*>(node)) return "brush";
    if (dynamic_cast<const Model::PatchNode*>(node)) return "patch";
    if (dynamic_cast<const Model::LayerNode*>(node)) return "layer";
    return "none";
}

QString typeName(const QString& type) {
    if (type == "group") return QObject::tr("Group");
    if (type == "entity") return QObject::tr("Entity");
    if (type == "brush") return QObject::tr("Brush");
    if (type == "patch") return QObject::tr("Patch");
    if (type == "layer") return QObject::tr("Layer");
    if (type == "faces") return QObject::tr("Face selection");
    if (type == "multi") return QObject::tr("Multi-selection");
    return QObject::tr("No selection");
}
} // namespace

QVariantMap contextOverlayData(const MapDocument& document, MapViewToolBox& tools) {
    QString toolId = "select";
    QString toolName = QObject::tr("Select / Move");
    if (tools.assembleBrushToolActive()) { toolId = "assemble"; toolName = QObject::tr("Assemble Brush"); }
    else if (tools.clipToolActive()) { toolId = "clip"; toolName = QObject::tr("Clip"); }
    else if (tools.rotateObjectsToolActive()) { toolId = "rotate"; toolName = QObject::tr("Rotate"); }
    else if (tools.scaleObjectsToolActive()) { toolId = "scale"; toolName = QObject::tr("Scale"); }
    else if (tools.shearObjectsToolActive()) { toolId = "shear"; toolName = QObject::tr("Shear"); }
    else if (tools.vertexToolActive()) { toolId = "vertex"; toolName = QObject::tr("Vertex"); }
    else if (tools.edgeToolActive()) { toolId = "edge"; toolName = QObject::tr("Edge"); }
    else if (tools.faceToolActive()) { toolId = "face"; toolName = QObject::tr("Face"); }
    else if (!document.hasSelectedNodes() && !document.hasSelectedBrushFaces())
        toolName = QObject::tr("Select / Draw Brush");

    QVariantMap tool{{"id", toolId}, {"name", toolName}, {"dragging", tools.dragging()}, {toolId, true}};
    if (toolId == "rotate") {
        tool.insert("angle", tools.rotateToolAngle());
        tool.insert("center", vectorData(tools.rotateToolCenter()));
    }
    QVariantMap result{{"tool", tool}, {"loaded", document.world() != nullptr}};
    if (!document.world()) return result;

    const auto& nodes = document.selectedNodes().nodes();
    const auto& faces = document.selectedBrushFaces();
    const auto type = !faces.empty() ? QString("faces") : nodes.empty() ? QString("none")
        : nodes.size() > 1 ? QString("multi") : nodeType(nodes.front());
    const auto count = !faces.empty() ? faces.size() : nodes.size();
    QVariantMap selection{{"type", type}, {"typeName", typeName(type)}, {type, true},
        {"count", qulonglong(count)}, {"empty", count == 0}, {"multiple", count > 1}};
    selection.insert("name", type == "multi" ? QObject::tr("%1 objects").arg(count)
        : type == "faces" ? QObject::tr("%1 faces").arg(count)
        : nodes.empty() ? typeName(type) : text(nodes.front()->name()));
    result.insert("mode", faces.empty() ? "objects" : "faces");
    result.insert("currentTexture", text(document.currentTextureName()));
    result.insert("layer", text(document.currentLayer()->name()));
    result.insert("group", document.currentGroup() ? text(document.currentGroup()->name()) : QString{});

    size_t groups = 0, entities = 0, brushes = 0, patches = 0, faceCount = 0;
    std::set<QString> textures;
    std::unordered_set<const Model::Node*> visited;
    std::vector<const Model::Node*> pending(nodes.begin(), nodes.end());
    while (!pending.empty()) {
        const auto* node = pending.back();
        pending.pop_back();
        if (!visited.insert(node).second) continue;
        if (dynamic_cast<const Model::GroupNode*>(node)) ++groups;
        else if (dynamic_cast<const Model::EntityNode*>(node)) ++entities;
        else if (const auto* brush = dynamic_cast<const Model::BrushNode*>(node)) {
            ++brushes;
            faceCount += brush->brush().faceCount();
            for (const auto& face : brush->brush().faces()) textures.insert(text(face.attributes().textureName()));
        } else if (const auto* patch = dynamic_cast<const Model::PatchNode*>(node)) {
            ++patches;
            textures.insert(text(patch->patch().textureName()));
        }
        pending.insert(pending.end(), node->children().begin(), node->children().end());
    }

    vm::bbox3::builder faceBounds;
    if (!faces.empty()) {
        std::unordered_set<const Model::BrushNode*> faceBrushes;
        for (const auto& handle : faces) {
            faceBrushes.insert(handle.node());
            const auto& face = handle.face();
            textures.insert(text(face.attributes().textureName()));
            for (const auto& vertex : face.vertexPositions()) faceBounds.add(vertex);
        }
        brushes = faceBrushes.size();
        faceCount = faces.size();
        if (faces.size() == 1) {
            const auto& face = faces.front().face();
            selection.insert("normal", vectorData(face.normal()));
            selection.insert("orientation", vectorData(face.normal()).value("text"));
            selection.insert("orientationLabel", QObject::tr("Face normal"));
            selection.insert("textureRotation", face.attributes().rotation());
        }
    }
    if (count > 0) {
        const auto bounds = faces.empty() ? document.selectionBounds() : faceBounds.bounds();
        selection.insert("position", vectorData(bounds.center()));
        selection.insert("positionLabel", QObject::tr("Bounds center"));
        selection.insert("size", vectorData(bounds.size()));
        selection.insert("min", vectorData(bounds.min));
        selection.insert("max", vectorData(bounds.max));
    }
    if (nodes.size() == 1 && faces.empty()) {
        const auto* node = nodes.front();
        selection.insert("children", qulonglong(node->childCount()));
        selection.insert("descendants", qulonglong(visited.size() - 1));
        if (const auto* entityNode = dynamic_cast<const Model::EntityNode*>(node)) {
            const auto& entity = entityNode->entity();
            QVariantMap properties;
            QVariantList propertyList;
            for (const auto& property : entity.properties()) {
                properties.insert(text(property.key()), text(property.value()));
                propertyList.append(QVariantMap{{"key", text(property.key())}, {"value", text(property.value())}});
            }
            result.insert("entity", QVariantMap{{"classname", text(entity.classname())},
                {"properties", properties}, {"propertyList", propertyList}, {"origin", vectorData(entity.origin())}});
            selection.insert("name", text(entity.classname()));
            if (const auto* name = entity.property("targetname"); name && !name->empty())
                selection.insert("name", text(*name) + " · " + text(entity.classname()));
            if (!node->hasChildren()) {
                selection.insert("position", vectorData(entity.origin()));
                selection.insert("positionLabel", QObject::tr("Origin"));
            }
            for (const auto* key : {"mangle", "angles", "angle"}) {
                if (const auto* value = entity.property(key); value && !value->empty()) {
                    selection.insert("orientation", text(*value));
                    selection.insert("orientationLabel", text(key));
                    break;
                }
            }
        }
    }
    QVariantList textureList;
    QStringList textureNames;
    for (const auto& name : textures) {
        if (name.isEmpty()) continue;
        textureList.append(QVariantMap{{"name", name}});
        textureNames.append(name);
    }
    result.insert("textures", textureList);
    result.insert("textureCount", textureList.size());
    result.insert("textureSummary", textureNames.mid(0, 4).join(", ")
        + (textureNames.size() > 4 ? QObject::tr(" (+%1)").arg(textureNames.size() - 4) : QString{}));
    result.insert("counts", QVariantMap{{"groups", qulonglong(groups)}, {"entities", qulonglong(entities)},
        {"brushes", qulonglong(brushes)}, {"patches", qulonglong(patches)}, {"faces", qulonglong(faceCount)}});
    result.insert("selection", selection);
    return result;
}
} // namespace TrenchBroom::View
