#include "View/Prefabs/PrefabUtils.h"
#include "View/Prefabs/PrefabPreview.h"

#include "Assets/EntityDefinition.h"
#include "Assets/ModelDefinition.h"
#include "Assets/Quake3Shader.h"
#include "Assets/Texture.h"
#include "Assets/TextureManager.h"
#include "IO/DiskIO.h"
#include "IO/File.h"
#include "IO/FileSystem.h"
#include "IO/PathInfo.h"
#include "IO/PathQt.h"
#include "IO/PathMatcher.h"
#include "IO/Reader.h"
#include "IO/SystemPaths.h"
#include "IO/TraversalMode.h"
#include "Model/BrushFaceHandle.h"
#include "Model/BrushNode.h"
#include "Model/EntityNode.h"
#include "Model/EditorContext.h"
#include "Model/GroupNode.h"
#include "Model/LayerNode.h"
#include "Model/MapFormat.h"
#include "Model/NodeQueries.h"
#include "Model/NodeContents.h"
#include "Model/PatchNode.h"
#include "Model/WorldNode.h"
#include "View/Document/MapDocument.h"
#include "View/MapView/MapView.h"
#include "View/Theme/QtUtils.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>

#include <vm/mat_ext.h>
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace TrenchBroom::View {
namespace {
template <typename F> void walk(Model::Node* node, const F& fn) {
    fn(node);
    for (auto* child : node->children()) walk(child, fn);
}

std::vector<Model::Node*> roots(const std::vector<Model::Node*>& nodes) {
    std::vector<Model::Node*> result;
    const auto add = [&](auto&& self, Model::Node* node) -> void {
        if (dynamic_cast<Model::WorldNode*>(node) || dynamic_cast<Model::LayerNode*>(node)) {
            for (auto* child : node->children()) self(self, child);
        } else if (std::find(result.begin(), result.end(), node) == result.end()) result.push_back(node);
    };
    for (auto* node : nodes) if (node) add(add, node);
    const auto candidates = result;
    result.erase(std::remove_if(result.begin(), result.end(), [&](auto* node) {
        return node->isDescendantOf(candidates);
    }), result.end());
    return result;
}

void collectAssets(MapDocument& document, const std::vector<Model::Node*>& nodes, PrefabSnapshot& snapshot) {
    std::set<std::string> textures;
    std::set<QString> resources;
    for (auto* node : nodes) walk(node, [&](Model::Node* child) {
        if (auto* brush = dynamic_cast<Model::BrushNode*>(child)) {
            for (const auto& face : brush->brush().faces())
                if (!face.attributes().textureName().empty() && face.attributes().textureName() != Model::BrushFaceAttributes::NoTextureName)
                    textures.insert(face.attributes().textureName());
        } else if (auto* patch = dynamic_cast<Model::PatchNode*>(child)) {
            if (!patch->patch().textureName().empty() && patch->patch().textureName() != Model::BrushFaceAttributes::NoTextureName)
                textures.insert(patch->patch().textureName());
        }
        else if (auto* entity = dynamic_cast<Model::EntityNode*>(child)) {
            try {
                const auto model = entity->entity().modelSpecification();
                if (!model.path.empty()) resources.insert(IO::pathAsQString(model.path));
            } catch (const std::exception&) { /* Invalid model expressions already have editor diagnostics. */ }
            for (const auto& property : entity->entity().properties()) {
                const auto value = QString::fromStdString(property.value());
                if (value.contains(QRegularExpression("\\.(mdl|md2|md3|obj|bsp|spr|wav|ogg|png|tga|jpg|pcx|wal)$", QRegularExpression::CaseInsensitiveOption)))
                    resources.insert(value);
            }
        }
    });
    std::set<QString> wads;
    std::vector<std::filesystem::path> wadFiles;
    if (const auto& property = document.game()->wadProperty(); property) {
        if (const auto* value = document.world()->entity().property(*property)) {
            for (const auto& path : QString::fromStdString(*value).split(';', Qt::SkipEmptyParts))
                wadFiles.push_back(IO::pathFromQString(path));
        }
    }
    for (const auto& name : textures) {
        const auto* texture = document.textureManager().texture(name);
        if (!texture) { snapshot.info.missingAssets << "Texture: " + QString::fromStdString(name); continue; }
        const auto path = IO::pathAsQString(texture->relativePath());
        bool wadTexture = false;
        for (const auto& wad : wadFiles) {
            // WADs are mounted below textures/<wad filename> by GameFileSystem.
            const auto filename = IO::pathAsQString(wad.filename());
            if (path.split('/').contains(filename)) { wads.insert(IO::pathAsQString(wad)); wadTexture = true; }
        }
        if (!wadTexture && !path.isEmpty()) resources.insert(path);
    }
    const std::vector<std::filesystem::path> searchPaths{document.path().parent_path(), document.game()->gamePath(), IO::SystemPaths::appDirectory()};
    for (const auto& path : wads) {
        const auto absolute = IO::Disk::resolvePath(searchPaths, IO::pathFromQString(path));
        QFile file(IO::pathAsQString(absolute));
        if (!absolute.empty() && file.open(QIODevice::ReadOnly)) {
            const auto data = file.readAll();
            if (file.error() != QFileDevice::NoError) throw std::runtime_error(file.errorString().toStdString());
            snapshot.assets.push_back({path, QFileInfo(path).fileName(), "wad", data});
        } else snapshot.info.missingAssets << path;
    }
    const auto* fs = document.game()->fileSystem();
    if (fs && (!wads.empty() || !resources.empty()) && fs->pathInfo("gfx/palette.lmp") == IO::PathInfo::File)
        resources.insert("gfx/palette.lmp");
    std::set<QString> visited;
    std::set<QString> stored;
    std::vector<std::pair<QString, QByteArray>> shaderSources;
    bool scannedShaders = false;
    const auto addImage = [&](const std::filesystem::path& image) {
        if (image.empty() || IO::pathAsQString(image).startsWith('$')) return;
        const auto path = IO::pathAsQString(image);
        if (image.extension().empty() && fs) {
            for (const auto* extension : {".tga", ".png", ".jpg", ".jpeg", ".dds", ".pcx"}) {
                if (fs->pathInfo(IO::pathFromQString(path + extension)) == IO::PathInfo::File) { resources.insert(path + extension); return; }
            }
        }
        resources.insert(path);
    };
    while (!resources.empty()) {
        auto resource = *resources.begin(); resources.erase(resources.begin());
        resource.replace('\\', '/');
        if (resource.startsWith('*') || resource.startsWith('$') || !visited.insert(resource).second) continue;
        try { resource = IO::PrefabLibrary::normalizedPath(resource); }
        catch (const std::exception&) { snapshot.info.missingAssets << resource; continue; }
        bool found = false;
        auto path = IO::pathFromQString(resource);
        if (fs && fs->pathInfo(path) != IO::PathInfo::File && resource.contains(QRegularExpression("\\.(wav|ogg)$", QRegularExpression::CaseInsensitiveOption))) {
            if (fs->pathInfo(IO::pathFromQString("sound/" + resource)) == IO::PathInfo::File) { resource = "sound/" + resource; path = IO::pathFromQString(resource); }
        }
        if (fs) fs->openFile(path).transform([&](const auto& file) {
            if (const auto* shaderFile = dynamic_cast<const IO::ObjectFile<Assets::Quake3Shader>*>(file.get())) {
                const auto& shader = shaderFile->object();
                addImage(shader.editorImage); addImage(shader.lightImage);
                for (const auto& stage : shader.stages) addImage(stage.map);
                if (!scannedShaders) {
                    scannedShaders = true;
                    fs->find({}, IO::TraversalMode::Recursive, IO::makeExtensionPathMatcher({".shader"})).transform([&](const auto& scripts) {
                        for (const auto& script : scripts) fs->openFile(script).transform([&](const auto& source) {
                            auto reader = source->reader().buffer();
                            shaderSources.emplace_back(IO::pathAsQString(script), QByteArray(reader.begin(), int(reader.end() - reader.begin())));
                        });
                    });
                }
                for (const auto& [script, data] : shaderSources) {
                    const auto text = QString::fromUtf8(data);
                    const QRegularExpression definition("(?:^|\\s)" + QRegularExpression::escape(IO::pathAsQString(shader.shaderPath)) + "\\s*\\{");
                    if (definition.match(text).hasMatch()) resources.insert(script);
                }
                // Synthetic shaders have no script. Their source image is sufficient.
                addImage(shader.shaderPath);
                found = true;
                return;
            }
            auto reader = file->reader().buffer();
            const auto data = QByteArray(reader.begin(), int(reader.end() - reader.begin()));
            if (stored.insert(resource).second) snapshot.assets.push_back({resource, resource, "resource", data});
            // Shader stages and model skin tables contain game-relative asset names.
            // Keep original scripts intact, including animation frames not used by the preview.
            const auto extension = QString::fromStdString(path.extension().string()).toLower();
            if (extension == ".shader" || extension == ".md2" || extension == ".md3" || extension == ".skin" || extension == ".mtl") {
                const QRegularExpression references("(?:textures|models|players|skins|env|gfx)/[A-Za-z0-9_./-]+");
                auto matches = references.globalMatch(QString::fromLatin1(data));
                while (matches.hasNext()) addImage(IO::pathFromQString(matches.next().captured()));
            }
            found = true;
        });
        if (!found) snapshot.info.missingAssets << resource;
    }
    snapshot.info.missingAssets.removeDuplicates();
}
}

std::vector<Model::Node*> prefabSelection(const MapDocument& document) {
    auto nodes = document.selectedNodes().nodes();
    for (const auto& face : document.selectedBrushFaces()) nodes.push_back(face.node());
    return roots(nodes);
}

QString suggestedPrefabName(const std::vector<Model::Node*>& nodes) {
    if (nodes.size() == 1) {
        if (const auto* group = dynamic_cast<Model::GroupNode*>(nodes.front())) return QString::fromStdString(group->group().name());
        if (const auto* entity = dynamic_cast<Model::EntityNode*>(nodes.front())) {
            if (const auto* name = entity->entity().property("targetname"); name && !name->empty()) return QString::fromStdString(*name);
            return QString::fromStdString(entity->entity().classname());
        }
        if (const auto* layer = dynamic_cast<Model::LayerNode*>(nodes.front())) return QString::fromStdString(layer->name());
    }
    return "Prefab";
}

PrefabSnapshot makePrefabSnapshot(MapDocument& document, const std::vector<Model::Node*>& nodes) {
    const auto selected = roots(nodes);
    if (selected.empty()) throw std::runtime_error("Select at least one object to create a prefab.");
    PrefabSnapshot snapshot;
    snapshot.info.game = QString::fromStdString(document.game()->gameName());
    std::ostringstream format;
    format << document.world()->mapFormat();
    snapshot.info.mapFormat = QString::fromStdString(format.str());
    auto group = std::make_unique<Model::GroupNode>(Model::Group("Prefab"));
    std::map<Model::EntityNode*, Model::EntityNode*> partialEntities;
    for (auto* node : selected) {
        auto* clone = node->cloneRecursively(document.worldBounds(), Model::SetLinkId::keep);
        if (auto* entity = dynamic_cast<Model::EntityNode*>(node->parent()); entity
            && (dynamic_cast<Model::BrushNode*>(node) || dynamic_cast<Model::PatchNode*>(node))) {
            auto*& partial = partialEntities[entity];
            if (!partial) { partial = static_cast<Model::EntityNode*>(entity->clone(document.worldBounds(), Model::SetLinkId::keep)); group->addChild(partial); }
            partial->addChild(clone);
        } else group->addChild(clone);
    }
    snapshot.previewGeometry = std::make_shared<PrefabPreviewGeometry>(*group, document.game().get());
    snapshot.preview = snapshot.previewGeometry->render();
    const auto bounds = group->physicalBounds();
    const auto size = bounds.size();
    snapshot.info.size = QVector3D(float(size.x()), float(size.y()), float(size.z()));
    const auto translation = vm::translation_matrix(-bounds.center());
    Model::IdType groupId = 1;
    walk(group.get(), [&](Model::Node* node) {
        if (auto* brush = dynamic_cast<Model::BrushNode*>(node)) {
            auto value = brush->brush();
            const auto result = value.transform(document.worldBounds(), translation, true);
            if (result.is_error()) throw std::runtime_error("Cannot center prefab geometry within map bounds.");
            brush->setBrush(std::move(value));
        } else if (auto* patch = dynamic_cast<Model::PatchNode*>(node)) {
            auto value = patch->patch();
            if (value.textureName().empty()) value.setTextureName(Model::BrushFaceAttributes::NoTextureName);
            value.transform(translation); patch->setPatch(std::move(value));
        } else if (auto* entity = dynamic_cast<Model::EntityNode*>(node)) {
            auto value = entity->entity(); value.transform(document.world()->entityPropertyConfig(), translation); entity->setEntity(std::move(value));
        } else if (auto* child = dynamic_cast<Model::GroupNode*>(node)) {
            child->setPersistentId(groupId++);
            auto value = child->group(); value.transform(translation); child->setGroup(std::move(value));
        }
    });
    std::ostringstream stream;
    document.game()->writeNodesToStream(*document.world(), {group.get()}, stream);
    snapshot.fragment = QByteArray::fromStdString(stream.str());
    collectAssets(document, selected, snapshot);
    return snapshot;
}

bool savePrefabDialog(const std::shared_ptr<MapDocument>& document, const std::vector<Model::Node*>& nodes, QWidget* parent, const QString& folder, const IO::PrefabLibrary& library) {
    if (!document || !document->world() || nodes.empty()) return false;
    try {
        auto snapshot = makePrefabSnapshot(*document, nodes);
        QDialog dialog(parent);
        dialog.setWindowTitle(QObject::tr("Save Selection as Prefab"));
        dialog.setObjectName("SavePrefabDialog");
        auto* outer = new QVBoxLayout(&dialog);
        auto* columns = new QHBoxLayout;
        auto* fields = new QWidget;
        auto* layout = new QFormLayout(fields);
        layout->setContentsMargins(0,0,0,0);
        auto* name = new QLineEdit(folder + suggestedPrefabName(nodes));
        name->setObjectName("PrefabPath");
        name->setPlaceholderText("furniture/couch");
        name->setMinimumWidth(260);
        auto* description = new QPlainTextEdit;
        description->setObjectName("PrefabDescription");
        description->setMinimumHeight(100);
        layout->addRow(QObject::tr("Name / path"), name);
        layout->addRow(QObject::tr("Description"), description);
        if (!snapshot.info.missingAssets.empty()) {
            auto summary = snapshot.info.missingAssets.mid(0, 6).join('\n');
            if (snapshot.info.missingAssets.size() > 6) summary += QObject::tr("\n… and %1 more").arg(snapshot.info.missingAssets.size() - 6);
            auto* missing = new QLabel(QObject::tr("Unavailable resources (recorded in the prefab):\n%1").arg(summary));
            missing->setWordWrap(true); missing->setMaximumWidth(380);
            missing->setToolTip(snapshot.info.missingAssets.join('\n')); layout->addRow(missing);
        }
        auto* previewColumn = new QVBoxLayout;
        auto* preview = new PrefabPreview(snapshot.previewGeometry);
        preview->setOrientation(snapshot.info.previewYaw, snapshot.info.previewPitch);
        auto* brightnessRow = new QHBoxLayout;
        auto* brightnessLabel = new QLabel(QObject::tr("Brightness"));
        brightnessLabel->setObjectName("PrefabBrightnessLabel");
        auto* brightnessSlider = new QSlider(Qt::Horizontal);
        brightnessSlider->setObjectName("PrefabPreviewBrightnessSlider");
        brightnessSlider->setRange(50,150);
        brightnessSlider->setAccessibleName(QObject::tr("Preview brightness"));
        auto* brightness = new QSpinBox;
        brightness->setObjectName("PrefabPreviewBrightness");
        brightness->setRange(50,150); brightness->setSingleStep(5); brightness->setSuffix(" %");
        brightness->setValue(QSettings{}.value("PrefabBrowser/PreviewBrightness", 100).toInt());
        brightnessSlider->setValue(brightness->value());
        preview->setBrightness(brightness->value()/100.0);
        QObject::connect(brightnessSlider, &QSlider::valueChanged, brightness, &QSpinBox::setValue);
        QObject::connect(brightness, qOverload<int>(&QSpinBox::valueChanged), preview, [preview, brightnessSlider](int value) {
            brightnessSlider->setValue(value); preview->setBrightness(value/100.0);
        });
        brightnessRow->addWidget(brightnessLabel); brightnessRow->addWidget(brightnessSlider,1); brightnessRow->addWidget(brightness);
        auto* hint = new QLabel(QObject::tr("Drag to rotate. Double-click to reset the view."));
        hint->setObjectName("PrefabPreviewHint"); hint->setWordWrap(true);
        auto* reset = new QPushButton(QObject::tr("Reset View"));
        reset->setObjectName("PrefabPreviewReset");
        QObject::connect(reset, &QPushButton::clicked, preview, [preview] { preview->setOrientation(-45,30); });
        previewColumn->addWidget(preview, 1); previewColumn->addWidget(hint); previewColumn->addLayout(brightnessRow); previewColumn->addWidget(reset);
        columns->addWidget(fields, 1); columns->addLayout(previewColumn, 1);
        outer->addLayout(columns, 1);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        outer->addWidget(buttons);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
            try {
                snapshot.info.path = name->text(); snapshot.info.description = description->toPlainText();
                snapshot.info.previewYaw = preview->yaw(); snapshot.info.previewPitch = preview->pitch();
                snapshot.info.previewBrightness = preview->brightness();
                library.save(snapshot.info, snapshot.fragment, preview->image(), snapshot.assets);
                QSettings{}.setValue("PrefabBrowser/PreviewBrightness", brightness->value());
                dialog.accept();
            } catch (const std::exception& e) { QMessageBox::warning(&dialog, QObject::tr("Cannot Save Prefab"), QString::fromUtf8(e.what())); }
        });
        name->selectAll();
        return dialog.exec() == QDialog::Accepted;
    } catch (const std::exception& e) {
        QMessageBox::warning(parent, QObject::tr("Cannot Create Prefab"), QString::fromUtf8(e.what()));
        return false;
    }
}

bool insertPrefab(const std::shared_ptr<MapDocument>& document, MapView* view, const QString& path, bool linked, QWidget* parent, const IO::PrefabLibrary& library) {
    if (!document || !document->world() || !document->editorContext().canChangeSelection()) return false;
    try {
        const auto info = library.load(path);
        if (info.game != QString::fromStdString(document->game()->gameName()))
            throw std::runtime_error(QString("This prefab uses the game configuration %1.").arg(info.game).toStdString());
        const auto fragment = library.fragment(path);
        const auto assets = library.materializeAssets(info);
        const auto reference = document->referenceBounds();
        auto transaction = Transaction(document, "Insert Prefab");
        if (!info.assets.empty()) {
            auto mods = document->mods();
            if (QFileInfo::exists(assets.resourceDirectory)) {
                const auto resourceRoot = assets.resourceDirectory.toStdString();
                if (std::find(mods.begin(), mods.end(), resourceRoot) == mods.end()) { mods.push_back(resourceRoot); document->setMods(mods); }
            }
            if (const auto& property = document->game()->wadProperty(); property) {
                auto value = QString::fromStdString(document->world()->entity().property(*property) ? *document->world()->entity().property(*property) : "");
                auto wads = value.split(';', Qt::SkipEmptyParts);
                for (const auto& wad : assets.wadFiles) if (!wads.contains(wad)) wads << wad;
                auto entity = document->world()->entity();
                entity.addOrUpdateProperty(document->world()->entityPropertyConfig(), *property, wads.join(';').toStdString());
                if (!document->swapNodeContents("Add Prefab WADs", {{document->world(), Model::NodeContents(std::move(entity))}}, {}))
                    { transaction.cancel(); return false; }
            }
        }
        auto* group = document->insertPrefab(fragment.toStdString(), info.label.toStdString(), info.id.toStdString(), linked);
        if (!group) { transaction.cancel(); throw std::runtime_error("Cannot insert prefab here. A linked prefab cannot contain itself."); }
        if (view) {
            document->hide({group});
            const auto delta = view->pasteObjectsDelta(group->logicalBounds(), reference);
            document->show({group}); document->selectNodes({group});
            if (!document->translateObjects(delta)) { transaction.cancel(); return false; }
        }
        return transaction.commit();
    } catch (const std::exception& e) {
        QMessageBox::warning(parent, QObject::tr("Cannot Insert Prefab"), QString::fromUtf8(e.what()));
        return false;
    }
}

bool insertPrefabMime(const std::shared_ptr<MapDocument>& document, MapView* view, const QMimeData& mime, QWidget* parent) {
    const auto object = QJsonDocument::fromJson(mime.data(PrefabMimeType)).object();
    return insertPrefab(document, view, object.value("path").toString(), object.value("linked").toBool(), parent);
}
}
