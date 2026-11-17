#include "Error.h"
#include "Assets/TextureManager.h"
#include "Assets/Texture.h"
#include "IO/GameConfigParser.h"
#include "IO/FileSystem.h"
#include "IO/PathInfo.h"
#include "IO/PathQt.h"
#include "IO/PrefabLibrary.h"
#include "IO/SystemPaths.h"
#include "Logger.h"
#include "Model/BrushBuilder.h"
#include "Model/BrushFace.h"
#include "Model/BrushNode.h"
#include "Model/EntityNode.h"
#include "Model/GameImpl.h"
#include "Model/GroupNode.h"
#include "Model/LayerNode.h"
#include "Model/NodeContents.h"
#include "Model/PatchNode.h"
#include "Model/WorldNode.h"
#include "View/MapDocumentCommandFacade.h"
#include "View/PasteType.h"
#include "View/PrefabBrowser.h"
#include "View/PrefabUtils.h"
#include "View/PrefabPreview.h"

#include <QAction>
#include <QBuffer>
#include <QApplication>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSlider>
#include <QToolButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTimer>

#include <catch2/catch.hpp>
#include <kdl/vector_utils.h>
#include <vm/mat_ext.h>

#include <sstream>
#include <algorithm>

namespace TrenchBroom::View {
namespace {
QImage prefabTestImage() {
    QImage image(64, 48, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::green);
    return image;
}

class PrefabBrowserSettings {
    QVariantMap m_saved;
    const QStringList m_keys{"PrefabBrowser/View", "PrefabBrowser/Folder", "PrefabBrowser/Zoom", "PrefabBrowser/Linked", "PrefabBrowser/Brightness", "PrefabBrowser/PreviewBrightness"};
public:
    PrefabBrowserSettings() {
        QSettings settings;
        for (const auto& key : m_keys) { m_saved[key] = settings.value(key); settings.remove(key); }
    }
    ~PrefabBrowserSettings() {
        QSettings settings;
        for (const auto& key : m_keys) {
            if (m_saved[key].isValid()) settings.setValue(key, m_saved[key]);
            else settings.remove(key);
        }
    }
};

class PrefabTest {
public:
    PrefabBrowserSettings browserSettings;
    NullLogger logger;
    Model::GameConfig config = IO::GameConfigParser(R"CFG({
        "version": 8, "name": "PrefabTest",
        "fileformats": [{"format": "Standard"}, {"format": "Quake3 (legacy)"}],
        "filesystem": {"searchpath": ".", "packageformat": {"extension": ".pak", "format": "idpak"} },
        "textures": {"root": "textures", "format": {"extensions": [".png"], "format": "image"}, "attribute": "wad"},
        "entities": {"definitions": ["Generic.fgd"], "defaultcolor": "0.6 0.6 0.6 1.0"}
    })CFG", std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path()
        / "app/resources/games/Generic/GameConfig.cfg").parse();
    std::shared_ptr<Model::GameImpl> game = std::make_shared<Model::GameImpl>(config, std::filesystem::path{}, logger);
    std::shared_ptr<MapDocument> document = MapDocumentCommandFacade::newMapDocument();

    PrefabTest() {
        REQUIRE(document->newDocument(Model::MapFormat::Standard, vm::bbox3(8192.0), game).is_success());
        document->selectAllNodes(); document->deleteObjects();
    }

    Model::BrushNode* cube(double size = 32) {
        Model::BrushBuilder builder(document->world()->mapFormat(), document->worldBounds());
        return new Model::BrushNode(builder.createCube(size, "").value());
    }

    static void flush() {
        QCoreApplication::sendPostedEvents(); QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(); QCoreApplication::processEvents();
    }
};
}

TEST_CASE("Prefab paths and atomic storage preserve bundles and reject unsafe names", "[prefabs]") {
    QTemporaryDir temporary;
    REQUIRE(temporary.isValid());
    IO::PrefabLibrary library(temporary.path() + "/prefabs");
    CHECK(library.entries().empty());
    for (const auto* invalid : {"", "../outside", "/absolute", "a//b", "a/./b", "a/../b", ".assets/entry", "C:/escape", "a:b", "name/"}) {
        CAPTURE(invalid); CHECK_THROWS(library.directory(invalid));
    }
    CHECK(IO::PrefabLibrary::normalizedPath("outside\\trees\\SmallOak") == "outside/trees/SmallOak");
    IO::PrefabInfo entry;
    entry.path = "furniture/couch"; entry.description = "Two seat couch"; entry.game = "Quake"; entry.size = {128, 64, 48};
    const std::vector<IO::PrefabAsset> assets{{"original/a.wad", "shared.wad", "wad", "WAD A"},
        {"original/b.wad", "shared.wad", "wad", "WAD B"}, {"progs/tree.mdl", "progs/tree.mdl", "resource", "model bytes"}};
    const auto saved = library.save(entry, "{\n\"classname\" \"light\"\n}\n", prefabTestImage(), assets);
    CHECK(saved.label == "couch");
    REQUIRE(saved.assets.size() == 3);
    CHECK(saved.assets[0].toObject().value("Cache") != saved.assets[1].toObject().value("Cache"));
    const auto loaded = library.load(entry.path);
    CHECK(loaded.description == entry.description);
    CHECK(loaded.size == entry.size);
    CHECK(library.entries().size() == 1);
    CHECK(QFileInfo::exists(library.directory(entry.path) + "/preview.png"));
    CHECK_THROWS(library.save(entry, "replacement", prefabTestImage(), {}));
    CHECK(library.fragment(entry.path).contains("light"));
    const auto imported = library.materializeAssets(saved);
    CHECK(library.materializeAssets(saved).resourceDirectory == imported.resourceDirectory);
    CHECK(library.materializeAssets(saved).wadFiles == imported.wadFiles);
    CHECK(QFileInfo::exists(imported.resourceDirectory + "/progs/tree.mdl"));
    library.remove(entry.path);
    CHECK(library.entries().empty());
    CHECK(QFileInfo::exists(imported.resourceDirectory + "/progs/tree.mdl"));
    REQUIRE(imported.wadFiles.size() == 2);
    CHECK(QFileInfo::exists(imported.wadFiles.front()));
}

TEST_CASE("A prefab asset is verified before being imported", "[prefabs]") {
    QTemporaryDir temporary;
    IO::PrefabLibrary library(temporary.path());
    IO::PrefabInfo info; info.path = "BigRock";
    const auto saved = library.save(info, "fragment", prefabTestImage(), {{"rock.png", "textures/rock.png", "resource", "original"}});
    QFile asset(library.directory(info.path) + '/' + saved.assets[0].toObject().value("File").toString());
    REQUIRE(asset.open(QIODevice::WriteOnly)); asset.write("changed"); asset.close();
    CHECK_THROWS(library.materializeAssets(saved));
    CHECK(library.load(info.path).id == saved.id);
}

TEST_CASE("Identical WAD contents share one cache file across names, prefabs and insertions", "[prefabs]") {
    QTemporaryDir temporary;
    IO::PrefabLibrary library(temporary.path());
    IO::PrefabInfo info; info.path = "furniture/couch";
    const auto first = library.save(info, "fragment", prefabTestImage(), {{"a/original.wad", "original.wad", "wad", "identical WAD bytes"}});
    info.path = "outside/rock";
    const auto second = library.save(info, "fragment", prefabTestImage(), {{"b/renamed.WAD", "renamed.WAD", "wad", "identical WAD bytes"}});
    const auto a = library.materializeAssets(first), b = library.materializeAssets(second);
    REQUIRE(a.wadFiles.size() == 1); CHECK(a.wadFiles == b.wadFiles);
    CHECK(first.assets[0].toObject().value("Cache") == second.assets[0].toObject().value("Cache"));
    CHECK_FALSE(QDir(library.directory(first.path)).exists("assets"));
    CHECK_FALSE(QDir(library.directory(second.path)).exists("assets"));
    CHECK_FALSE(QDir(library.root()).exists(".assets/" + first.id + "/wads"));
    const auto digest = first.assets[0].toObject().value("SHA256").toString();
    CHECK(QDir(library.root() + "/.wad-cache/" + digest).entryList(QDir::Files | QDir::NoDotAndDotDot).size() == 1);
    library.remove(first.path); library.remove(second.path);
    CHECK(QFileInfo::exists(a.wadFiles.front()));
    QFile cached(a.wadFiles.front()); REQUIRE(cached.open(QIODevice::ReadOnly));
    CHECK(cached.readAll() == "identical WAD bytes"); cached.close();
    REQUIRE(cached.open(QIODevice::WriteOnly)); cached.write("corrupt cached contents"); cached.close();
    CHECK_THROWS(library.materializeAssets(first));
}

TEST_CASE("Legacy WAD bundles migrate to the shared cache while imported map paths remain valid", "[prefabs]") {
    QTemporaryDir temporary;
    IO::PrefabLibrary library(temporary.path());
    IO::PrefabInfo info; info.path = "legacy/couch";
    const auto initial = library.save(info, "fragment", prefabTestImage(), {});
    const QByteArray data("Legacy WAD bytes");
    const auto digest = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    const auto writeFile = [](const QString& path, const QByteArray& bytes) {
        REQUIRE(QDir{}.mkpath(QFileInfo(path).absolutePath()));
        QFile file(path); REQUIRE(file.open(QIODevice::WriteOnly)); REQUIRE(file.write(bytes) == bytes.size());
    };
    const auto folder = library.directory(info.path);
    const auto bundled = folder + "/assets/old/legacy.wad";
    const auto imported = library.root() + "/.assets/" + initial.id + "/wads/old/legacy.wad";
    const auto orphan = library.root() + "/.assets/orphan/wads/renamed.wad";
    writeFile(bundled, data); writeFile(imported, data); writeFile(orphan, data);
    QFile json(folder + "/prefab.json"); REQUIRE(json.open(QIODevice::ReadOnly));
    auto descriptor = QJsonDocument::fromJson(json.readAll()).object(); json.close();
    descriptor["Version"] = 1;
    descriptor["Assets"] = QJsonArray{QJsonObject{{"Kind", "wad"}, {"Source", "old/legacy.wad"},
        {"File", "assets/old/legacy.wad"}, {"ResourcePath", "old/legacy.wad"}, {"SHA256", digest}}};
    writeFile(folder + "/prefab.json", QJsonDocument(descriptor).toJson());
    const auto migrated = library.load(info.path);
    CHECK(migrated.id == initial.id);
    REQUIRE(migrated.assets.size() == 1);
    CHECK(migrated.assets[0].toObject().value("Storage").toString() == "shared");
    CHECK_FALSE(QFileInfo::exists(bundled));
    const auto paths = library.materializeAssets(migrated);
    REQUIRE(paths.wadFiles.size() == 1); REQUIRE(QFileInfo::exists(imported));
    QFile old(imported); REQUIRE(old.open(QIODevice::ReadOnly)); CHECK(old.readAll() == data); old.close();
    library.migrateLegacyImports();
    CHECK(QFileInfo::exists(orphan));
    std::error_code error;
    const auto probe = temporary.filePath("hardlink-probe");
    std::filesystem::create_hard_link(IO::pathFromQString(paths.wadFiles.front()), IO::pathFromQString(probe), error);
    if (!error) {
        CHECK(std::filesystem::equivalent(IO::pathFromQString(imported), IO::pathFromQString(paths.wadFiles.front())));
        CHECK(std::filesystem::equivalent(IO::pathFromQString(orphan), IO::pathFromQString(paths.wadFiles.front())));
    }
    REQUIRE(json.open(QIODevice::ReadOnly));
    CHECK(QJsonDocument::fromJson(json.readAll()).object().value("Version").toInt() == 2);
}

TEST_CASE_METHOD(PrefabTest, "Saving a partial brush entity preserves properties without changing the document", "[prefabs]") {
    auto* entity = new Model::EntityNode(Model::Entity({}, {{"classname", "func_detail"}, {"targetname", "couch"}, {"message", R"(quoted \"value\")"}}));
    auto* brush = cube(); auto* sibling = cube(64);
    entity->addChildren({brush, sibling});
    document->addNodes({{document->world()->defaultLayer(), {entity}}});
    document->selectNodes({brush});
    const auto bounds = brush->logicalBounds();
    const auto modified = document->modificationCount();
    const auto snapshot = makePrefabSnapshot(*document, prefabSelection(*document));
    CHECK(document->modificationCount() == modified);
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{brush});
    CHECK(brush->logicalBounds() == bounds);
    CHECK(entity->childCount() == 2);
    REQUIRE(!snapshot.preview.isNull());
    CHECK(snapshot.info.size == QVector3D(32, 32, 32));
    auto* inserted = document->insertPrefab(snapshot.fragment.toStdString(), "couch", "test-id", false);
    REQUIRE(inserted);
    REQUIRE(inserted->childCount() == 1);
    auto* importedEntity = dynamic_cast<Model::EntityNode*>(inserted->children()[0]);
    REQUIRE(importedEntity);
    CHECK(importedEntity->childCount() == 1);
    CHECK(importedEntity->entity().classname() == "func_detail");
    CHECK(*importedEntity->entity().property("message") == R"(quoted \"value\")");
    CHECK(suggestedPrefabName({entity}) == "couch");
}

TEST_CASE_METHOD(PrefabTest, "Dragging a prefab preview changes only its view and reset restores the image", "[prefabs]") {
    Model::BrushBuilder builder(Model::MapFormat::Standard, document->worldBounds());
    auto* group = new Model::GroupNode(Model::Group("Sofa"));
    group->addChild(new Model::BrushNode(builder.createCuboid(vm::bbox3({-64,-32,0}, {64,32,16}), "").value()));
    group->addChild(new Model::BrushNode(builder.createCuboid(vm::bbox3({-64,16,16}, {64,32,64}), "").value()));
    document->addNodes({{document->world()->defaultLayer(), {group}}}); document->selectNodes({group});
    const auto modification = document->modificationCount();
    const auto bounds = group->logicalBounds();
    const auto snapshot = makePrefabSnapshot(*document, {group});
    PrefabPreview preview(snapshot.previewGeometry);
    preview.resize(320,240); preview.show(); flush();
    CHECK(preview.image() == snapshot.preview);
    const QPoint start(100,100), end(180,135);
    QMouseEvent down(QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent move(QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent up(QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&preview, &down); QApplication::sendEvent(&preview, &move); QApplication::sendEvent(&preview, &up);
    CHECK(preview.yaw() == Approx(-85)); CHECK(preview.pitch() == Approx(47.5));
    CHECK(preview.image() != snapshot.preview);
    CHECK(document->modificationCount() == modification); CHECK(group->logicalBounds() == bounds);
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{group});
    QMouseEvent reset(QEvent::MouseButtonDblClick, start, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&preview, &reset);
    CHECK(preview.yaw() == -45); CHECK(preview.pitch() == 30); CHECK(preview.image() == snapshot.preview);
    preview.setBrightness(0.5); CHECK(preview.image() != snapshot.preview);
    preview.setBrightness(1.5); CHECK(preview.image() != snapshot.preview);
    preview.setBrightness(99); CHECK(preview.brightness() == 1.5);
    preview.setBrightness(-99); CHECK(preview.brightness() == 0.5);
    preview.setBrightness(1); CHECK(preview.image() == snapshot.preview);
    preview.setOrientation(10000, -1000); CHECK(std::abs(preview.yaw()) <= 180); CHECK(preview.pitch() == -85);
    const auto lastImage = preview.image();
    document->deleteObjects(); CHECK(preview.image() == lastImage);
}

TEST_CASE_METHOD(PrefabTest, "Prefab previews load texture pixels even when editor CPU buffers were released", "[prefabs]") {
    QTemporaryDir temporary;
    REQUIRE(QDir{}.mkpath(temporary.path() + "/textures"));
    QImage checker(16,16,QImage::Format_ARGB32);
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x)
        checker.setPixelColor(x,y, ((x/4 + y/4) % 2) ? Qt::red : Qt::blue);
    const auto path = temporary.path() + "/textures/checker.png";
    REQUIRE(checker.save(path));
    game->setGamePath(IO::pathFromQString(temporary.path()), logger);
    // Matches a prepared editor texture: dimensions and resource path remain,
    // but no CPU pixels are retained after the GPU upload.
    Assets::Texture texture("checker", 16,16, GL_RGBA);
    texture.setRelativePath("textures/checker.png");
    REQUIRE(texture.buffersIfUnprepared().empty());
    Model::BrushBuilder builder(Model::MapFormat::Standard, document->worldBounds());
    auto brush = builder.createCube(64, "checker").value();
    for (auto& face : brush.faces()) face.setTexture(&texture);
    auto node = Model::BrushNode(std::move(brush));
    const PrefabPreviewGeometry geometry(node, game.get());
    const auto preview = geometry.render();
    int red = 0, blue = 0;
    for (int y = 0; y < preview.height(); ++y) for (int x = 0; x < preview.width(); ++x) {
        const auto pixel = preview.pixelColor(x,y);
        if (pixel.alpha() == 255 && pixel.red() > 2*pixel.blue() && pixel.red() > 2*pixel.green()) ++red;
        if (pixel.alpha() == 255 && pixel.blue() > 2*pixel.red() && pixel.blue() > 2*pixel.green()) ++blue;
    }
    CHECK(red > 1000); CHECK(blue > 1000);
    REQUIRE(QFile::remove(path));
    CHECK(geometry.render() == preview); // Rotation/rendering reads only the snapshot.
    CHECK(geometry.render({512,384}, 135,-30) != preview);
}

TEST_CASE_METHOD(PrefabTest, "Intersecting prefab faces use per-pixel depth from both sides", "[prefabs]") {
    const auto solid = [](const char* name, unsigned char r, unsigned char g, unsigned char b) {
        Assets::TextureBuffer buffer(4);
        buffer.data()[0] = r; buffer.data()[1] = g; buffer.data()[2] = b; buffer.data()[3] = 255;
        return Assets::Texture(name,1,1,Color(0.5f,0.5f,0.5f,1.0f),std::move(buffer),GL_RGBA,Assets::TextureType::Opaque);
    };
    auto red = solid("red",255,0,0), blue = solid("blue",0,0,255);
    Model::BrushBuilder builder(Model::MapFormat::Standard, document->worldBounds());
    auto group = Model::GroupNode(Model::Group("Crossing faces"));
    const auto add = [&](Assets::Texture& texture, double angle) {
        auto brush = builder.createCuboid(vm::bbox3({-64,-48,-2},{64,48,2}), texture.name()).value();
        REQUIRE(brush.transform(document->worldBounds(), vm::rotation_matrix(vm::vec3{0,1,0}, angle), true).is_success());
        for (auto& face : brush.faces()) face.setTexture(&texture);
        group.addChild(new Model::BrushNode(std::move(brush)));
    };
    add(red,0.45); add(blue,-0.45);
    PrefabPreviewGeometry geometry(group);
    const auto above = geometry.render({512,384},0,85);
    CHECK(above.pixelColor(256,100).red() > above.pixelColor(256,100).blue());
    CHECK(above.pixelColor(256,284).blue() > above.pixelColor(256,284).red());
    const auto below = geometry.render({512,384},0,-85);
    CHECK(below.pixelColor(256,100).red() > below.pixelColor(256,100).blue());
    CHECK(below.pixelColor(256,284).blue() > below.pixelColor(256,284).red());
    if (qEnvironmentVariableIsSet("TB_PREVIEW_TEST_IMAGES")) above.save(QDir::tempPath() + "/tb-prefab-depth-above.png");
}

TEST_CASE_METHOD(PrefabTest, "The save dialog persists the chosen preview angle and cancel leaves no prefab", "[prefabs]") {
    QTemporaryDir temporary;
    IO::PrefabLibrary library(temporary.path());
    auto* brush = cube(); document->addNodes({{document->world()->defaultLayer(), {brush}}}); document->selectNodes({brush});
    const auto modified = document->modificationCount();
    bool found = false;
    QByteArray expectedImage;
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog) return;
        auto* preview = dialog->findChild<PrefabPreview*>("PrefabPreview");
        auto* name = dialog->findChild<QLineEdit*>("PrefabPath");
        if (!preview || !name) { dialog->reject(); return; }
        found = true; name->setText("views/rotated");
        dialog->findChild<QPlainTextEdit*>("PrefabDescription")->setPlainText("Chosen angle");
        preview->setOrientation(127,48);
        auto* brightness = dialog->findChild<QSpinBox*>("PrefabPreviewBrightness");
        auto* slider = dialog->findChild<QSlider*>("PrefabPreviewBrightnessSlider");
        REQUIRE(brightness); REQUIRE(slider);
        brightness->setValue(140);
        CHECK(preview->brightness() == Approx(1.4)); CHECK(slider->value() == 140);
        QBuffer buffer(&expectedImage); buffer.open(QIODevice::WriteOnly); preview->image().save(&buffer,"PNG");
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
    });
    REQUIRE(savePrefabDialog(document, {brush}, nullptr, {}, library)); REQUIRE(found);
    const auto saved = library.load("views/rotated");
    CHECK(saved.previewYaw == 127); CHECK(saved.previewPitch == 48); CHECK(saved.description == "Chosen angle");
    CHECK(saved.previewBrightness == Approx(1.4));
    QFile png(library.directory(saved.path) + "/preview.png"); REQUIRE(png.open(QIODevice::ReadOnly)); CHECK(png.readAll() == expectedImage);
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog) return;
        if (auto* preview = dialog->findChild<PrefabPreview*>()) preview->setOrientation(-150,-40);
        dialog->reject();
    });
    CHECK_FALSE(savePrefabDialog(document, {brush}, nullptr, {}, library));
    CHECK(library.entries().size() == 1); CHECK(document->modificationCount() == modified);
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{brush});
}

TEST_CASE("Prefab brightness changes color without changing transparency", "[prefabs]") {
    QImage source(2,1,QImage::Format_ARGB32_Premultiplied);
    source.setPixelColor(0,0,QColor(80,120,160,128)); source.setPixelColor(1,0,Qt::transparent);
    const auto dark = prefabPreviewBrightness(source,0.5), bright = prefabPreviewBrightness(source,1.5);
    CHECK(dark.pixelColor(0,0).alpha() == 128); CHECK(bright.pixelColor(0,0).alpha() == 128);
    CHECK(dark.pixelColor(1,0).alpha() == 0); CHECK(bright.pixelColor(1,0).alpha() == 0);
    CHECK(dark.pixelColor(0,0).red() == Approx(40).margin(2));
    CHECK(bright.pixelColor(0,0).blue() == Approx(240).margin(2));
    CHECK(prefabPreviewBrightness(source,1) == source);
    CHECK(source.pixelColor(0,0).red() == Approx(80).margin(1));
}

TEST_CASE_METHOD(PrefabTest, "Prefab zoom relayouts existing tree and tile widgets without reloading", "[prefabs]") {
    QTemporaryDir temporary; IO::PrefabLibrary library(temporary.path());
    auto* brush = cube(); document->addNodes({{document->world()->defaultLayer(), {brush}}});
    auto snapshot = makePrefabSnapshot(*document,{brush});
    for (const auto* path : {"Long descriptive first prefab name", "Second prefab"}) {
        snapshot.info.path = path; library.save(snapshot.info,snapshot.fragment,snapshot.preview,{});
    }
    PrefabBrowser browser(document,nullptr,nullptr,library.root());
    browser.resize(940,750); browser.show(); flush();
    auto* zoom = browser.findChild<QSpinBox*>("PrefabZoom");
    auto* tree = browser.findChild<QTreeWidget*>("PrefabTree");
    auto* tiles = browser.findChild<QListWidget*>("PrefabTiles");
    REQUIRE(zoom); REQUIRE(tree); REQUIRE(tiles);
    SECTION("Default item style") {}
    SECTION("Stylesheet borders and padding") {
        tiles->setStyleSheet("QListWidget::item { border: 2px solid #2468ac; padding: 3px; }"); flush();
    }
    auto* row = tree->topLevelItem(0); REQUIRE(row);
    auto* card = tree->itemWidget(row,0); REQUIRE(card);
    tree->setCurrentItem(row);
    auto* thumbnail = card->findChild<QLabel*>("PrefabThumbnail"); REQUIRE(thumbnail);
    for (const auto percent : {300,50,175,100}) {
        zoom->setValue(percent); flush();
        CHECK(tree->itemWidget(row,0) == card); CHECK(tree->currentItem() == row);
        CHECK(thumbnail->size() == QSize(96,72)*(percent/100.0));
        CHECK(card->height() == tree->visualItemRect(row).height());
        CHECK(tree->visualItemRect(tree->topLevelItem(1)).top() > tree->visualItemRect(row).bottom());
    }
    browser.findChild<QAction*>("PrefabTileView")->trigger(); flush();
    REQUIRE(tiles->count() == 2);
    auto* tile = tiles->item(0); auto* tileCard = tiles->itemWidget(tile); REQUIRE(tileCard);
    auto* tileImage = tileCard->findChild<QLabel*>("PrefabThumbnail"); REQUIRE(tileImage);
    for (const auto percent : {300,50,200,100}) {
        zoom->setValue(percent); flush();
        CHECK(tiles->itemWidget(tile) == tileCard);
        CHECK(tileImage->size() == QSize(128,96)*(percent/100.0));
        CHECK(tileCard->size() == tileCard->sizeHint());
        CHECK(tiles->visualItemRect(tile).contains(tileCard->geometry()));
        CHECK_FALSE(tiles->visualItemRect(tile).intersects(tiles->visualItemRect(tiles->item(1))));
        CHECK(tileCard->findChild<QWidget*>("PrefabName")->geometry().top() > tileImage->geometry().bottom());
    }
}

TEST_CASE_METHOD(PrefabTest, "Prefab browser controls brightness and linked insertion without a mode dropdown", "[prefabs]") {
    QTemporaryDir temporary; IO::PrefabLibrary library(temporary.path());
    auto* brush = cube(); document->addNodes({{document->world()->defaultLayer(), {brush}}});
    auto snapshot = makePrefabSnapshot(*document,{brush}); snapshot.info.path = "Rock";
    QImage preview(64,48,QImage::Format_ARGB32_Premultiplied); preview.fill(QColor(80,120,160));
    library.save(snapshot.info,snapshot.fragment,preview,{});
    PrefabBrowser browser(document,nullptr,nullptr,library.root()); browser.resize(800,600); browser.show(); flush();
    auto* linked = browser.findChild<QAction*>("PrefabLinkedInsert");
    auto* brightness = browser.findChild<QSpinBox*>("PrefabBrightness");
    auto* slider = browser.findChild<QSlider*>("PrefabBrightnessSlider");
    REQUIRE(linked); REQUIRE(brightness); REQUIRE(slider);
    CHECK(linked->isCheckable()); CHECK_FALSE(linked->isChecked()); CHECK_FALSE(linked->icon().isNull());
    CHECK(linked->toolTip() == "Use linked groups on insert"); CHECK(brightness->minimum() == 50); CHECK(brightness->maximum() == 150);
    auto* tree = browser.findChild<QTreeWidget*>("PrefabTree"); auto* item = tree->topLevelItem(0);
    tree->setCurrentItem(item); flush();
    auto* thumbnail = tree->itemWidget(item,0)->findChild<QLabel*>("PrefabThumbnail"); REQUIRE(thumbnail);
    const auto color = [&] { const auto image = thumbnail->pixmap(Qt::ReturnByValue).toImage(); return image.pixelColor(image.width()/2,image.height()/2); };
    brightness->setValue(150); flush(); CHECK(slider->value() == 150); CHECK(color().red() == Approx(120).margin(1));
    slider->setValue(50); flush(); CHECK(brightness->value() == 50); CHECK(color().blue() == Approx(80).margin(1));
    auto* insert = browser.findChild<QAction*>("PrefabInsert"); REQUIRE(insert->isEnabled());
    insert->trigger(); auto* copy = dynamic_cast<Model::GroupNode*>(document->selectedNodes().nodes().front()); REQUIRE(copy);
    linked->trigger(); CHECK(linked->isChecked()); insert->trigger();
    auto* first = dynamic_cast<Model::GroupNode*>(document->selectedNodes().nodes().front()); REQUIRE(first);
    insert->trigger(); auto* second = dynamic_cast<Model::GroupNode*>(document->selectedNodes().nodes().front()); REQUIRE(second);
    CHECK(first->linkId() == second->linkId()); CHECK(copy->linkId() != first->linkId());
    PrefabBrowser restored(document,nullptr,nullptr,library.root());
    CHECK(restored.findChild<QAction*>("PrefabLinkedInsert")->isChecked());
    CHECK(restored.findChild<QSpinBox*>("PrefabBrightness")->value() == 50);
}

TEST_CASE_METHOD(PrefabTest, "Point entity preview boxes remain complete from the rear and below", "[prefabs]") {
    auto* entity = new Model::EntityNode(Model::Entity({}, {{"classname", "info_player_start"}}));
    document->addNodes({{document->world()->defaultLayer(), {entity}}});
    const auto snapshot = makePrefabSnapshot(*document, {entity});
    const auto bounds = [](const QImage& image) {
        QRect rect;
        for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x)
            if (image.pixelColor(x,y).alpha() > 0) rect = rect.united(QRect(x,y,1,1));
        return rect;
    };
    const auto front = bounds(snapshot.previewGeometry->render({512,384}, -45,30));
    const auto rear = bounds(snapshot.previewGeometry->render({512,384}, 135,30));
    const auto below = bounds(snapshot.previewGeometry->render({512,384}, 135,-30));
    REQUIRE(front.width() > 0); REQUIRE(front.height() > 0);
    CHECK(std::abs(front.width() - rear.width()) <= 2); CHECK(std::abs(front.height() - rear.height()) <= 2);
    CHECK(std::abs(front.width() - below.width()) <= 2); CHECK(std::abs(front.height() - below.height()) <= 2);
}

TEST_CASE_METHOD(PrefabTest, "Copy prefabs are independent and Link uses the edited instance with undo and redo", "[prefabs]") {
    auto* brush = cube(); document->addNodes({{document->world()->defaultLayer(), {brush}}});
    const auto snapshot = makePrefabSnapshot(*document, {brush});
    const auto text = snapshot.fragment.toStdString();
    auto* copyA = document->insertPrefab(text, "Rock", "prefab-id", false);
    auto* copyB = document->insertPrefab(text, "Rock", "prefab-id", false);
    REQUIRE(copyA); REQUIRE(copyB);
    CHECK(copyA->linkId() != copyB->linkId());
    CHECK(dynamic_cast<Model::BrushNode*>(copyA->children()[0])->linkId() != dynamic_cast<Model::BrushNode*>(copyB->children()[0])->linkId());
    auto* first = document->insertPrefab(text, "Rock", "prefab-id", true);
    REQUIRE(first);
    document->openGroup(first);
    document->selectNodes({first->children()[0]});
    REQUIRE(document->translateObjects({48, 0, 0}));
    document->closeGroup();
    const auto changedBounds = first->logicalBounds();
    auto* second = document->insertPrefab(text, "Rock", "prefab-id", true);
    REQUIRE(second);
    CHECK(second->linkId() == first->linkId());
    CHECK(second->logicalBounds() == changedBounds);
    CHECK(second->children()[0]->logicalBounds() == first->children()[0]->logicalBounds());
    const auto count = document->world()->defaultLayer()->childCount();
    document->undoCommand(); CHECK(document->world()->defaultLayer()->childCount() == count - 1);
    document->redoCommand(); CHECK(document->world()->defaultLayer()->childCount() == count);
    {
        QTemporaryDir folder;
        const auto path = IO::pathFromQString(folder.filePath("linked.map"));
        document->saveDocumentTo(path);
        auto reopened = MapDocumentCommandFacade::newMapDocument();
        REQUIRE(reopened->loadDocument(Model::MapFormat::Standard, document->worldBounds(), game, path).is_success());
        auto* third = reopened->insertPrefab(text, "Rock", "prefab-id", true);
        REQUIRE(third);
        CHECK(third->logicalBounds() == changedBounds);
    }
    document->openGroup(first);
    const auto beforeFailure = document->world()->defaultLayer()->descendantCount();
    CHECK_FALSE(document->insertPrefab(text, "Recursive", "prefab-id", true));
    CHECK(document->world()->defaultLayer()->descendantCount() == beforeFailure);
    document->closeGroup();
}

TEST_CASE_METHOD(PrefabTest, "A selected patch is serialized inside a reusable prefab group", "[prefabs]") {
    REQUIRE(document->newDocument(Model::MapFormat::Quake3_Legacy, vm::bbox3(8192.0), game).is_success());
    std::vector<Model::BezierPatch::Point> points;
    for (int row = 0; row < 3; ++row) for (int col = 0; col < 3; ++col)
        points.push_back({double(col * 16), double(row * 16), double(row == 1 ? 8 : 0), col / 2.0, row / 2.0});
    auto* patch = new Model::PatchNode(Model::BezierPatch(3, 3, points, ""));
    document->addNodes({{document->world()->defaultLayer(), {patch}}});
    const auto snapshot = makePrefabSnapshot(*document, {patch});
    auto* imported = document->insertPrefab(snapshot.fragment.toStdString(), "Patch", "patch-id", false);
    REQUIRE(imported);
    REQUIRE(imported->childCount() == 1);
    CHECK(dynamic_cast<Model::PatchNode*>(imported->children()[0]));
}

TEST_CASE_METHOD(PrefabTest, "Nested links stay internal to each independent prefab copy", "[prefabs]") {
    auto* source = new Model::GroupNode(Model::Group("Source"));
    auto* a = new Model::GroupNode(Model::Group("Linked part"));
    a->addChild(cube());
    auto* b = a->cloneRecursively(document->worldBounds(), Model::SetLinkId::keep);
    source->addChildren({a, b});
    document->addNodes({{document->world()->defaultLayer(), {source}}});
    const auto snapshot = makePrefabSnapshot(*document, {source});
    auto* first = document->insertPrefab(snapshot.fragment.toStdString(), "Source", "outer-id", false);
    auto* second = document->insertPrefab(snapshot.fragment.toStdString(), "Source", "outer-id", false);
    REQUIRE(first); REQUIRE(second);
    auto* firstContainer = dynamic_cast<Model::GroupNode*>(first->children()[0]);
    auto* secondContainer = dynamic_cast<Model::GroupNode*>(second->children()[0]);
    REQUIRE(firstContainer); REQUIRE(secondContainer);
    REQUIRE(firstContainer->childCount() == 2);
    const auto* firstA = dynamic_cast<Model::GroupNode*>(firstContainer->children()[0]);
    const auto* firstB = dynamic_cast<Model::GroupNode*>(firstContainer->children()[1]);
    const auto* secondA = dynamic_cast<Model::GroupNode*>(secondContainer->children()[0]);
    REQUIRE(firstA); REQUIRE(firstB); REQUIRE(secondA);
    CHECK(firstA->linkId() == firstB->linkId());
    CHECK(firstA->linkId() != a->linkId());
    CHECK(firstA->linkId() != secondA->linkId());
}

TEST_CASE_METHOD(PrefabTest, "Inserting registers bundled WADs and resources in one undoable operation", "[prefabs]") {
    QTemporaryDir temporary;
    REQUIRE(temporary.isValid());
    game->setGamePath(IO::pathFromQString(temporary.path()), logger);
    IO::PrefabLibrary library(temporary.path() + "/library");
    auto* brush = cube(); document->addNodes({{document->world()->defaultLayer(), {brush}}});
    document->selectNodes({brush});
    auto snapshot = makePrefabSnapshot(*document, {brush});
    snapshot.info.path = "bundled/rock";
    const auto sourceRoot = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path();
    QFile wad(IO::pathAsQString(sourceRoot / "common/test/fixture/IO/Wad/cr8_czg.wad"));
    REQUIRE(wad.open(QIODevice::ReadOnly));
    snapshot.assets.push_back({wad.fileName(), "cr8_czg.wad", "wad", wad.readAll()});
    QBuffer png; REQUIRE(png.open(QIODevice::WriteOnly)); REQUIRE(prefabTestImage().save(&png, "PNG"));
    snapshot.assets.push_back({"textures/imported.png", "textures/imported.png", "resource", png.data()});
    const auto saved = library.save(snapshot.info, snapshot.fragment, snapshot.preview, snapshot.assets);
    const auto originalCount = document->world()->defaultLayer()->childCount();
    const auto originalProperties = document->world()->entity().properties();
    REQUIRE(insertPrefab(document, nullptr, saved.path, false, nullptr, library));
    CHECK(document->world()->defaultLayer()->childCount() == originalCount + 1);
    const auto* importedWads = document->world()->entity().property("wad");
    REQUIRE(importedWads);
    const auto importedPath = QString::fromStdString(*importedWads);
    CHECK(QFileInfo::exists(importedPath));
    REQUIRE(game->fileSystem());
    CHECK(game->fileSystem()->pathInfo("textures/imported.png") == IO::PathInfo::File);
    document->undoCommand();
    CHECK(document->world()->defaultLayer()->childCount() == originalCount);
    CHECK(document->world()->entity().properties() == originalProperties);
    CHECK(document->mods().empty());
    document->redoCommand();
    CHECK(document->world()->defaultLayer()->childCount() == originalCount + 1);
    library.remove(saved.path);
    CHECK(QFileInfo::exists(importedPath));
    CHECK(game->fileSystem()->pathInfo("textures/imported.png") == IO::PathInfo::File);
}

TEST_CASE_METHOD(PrefabTest, "Saving collects the physical WAD of used textures rather than its virtual collection path", "[prefabs]") {
    QTemporaryDir temporary;
    REQUIRE(QDir{}.mkpath(temporary.path() + "/gfx"));
    QFile palette(temporary.path() + "/gfx/palette.lmp");
    REQUIRE(palette.open(QIODevice::WriteOnly)); REQUIRE(palette.write(QByteArray(768, char(127))) == 768); palette.close();
    config.textureConfig.extensions = {".D"};
    config.textureConfig.palette = "gfx/palette.lmp";
    game->setGamePath(IO::pathFromQString(temporary.path()), logger);
    const auto sourceRoot = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path();
    const auto wad = sourceRoot / "common/test/fixture/IO/Wad/cr8_czg.wad";
    auto world = document->world()->entity();
    world.addOrUpdateProperty(document->world()->entityPropertyConfig(), "wad", wad.string());
    REQUIRE(document->swapNodeContents("Load Test WAD", {{document->world(), Model::NodeContents(std::move(world))}}, {}));
    REQUIRE(!document->textureManager().textures().empty());
    const auto& textures = document->textureManager().textures();
    const auto used = std::find_if(textures.begin(), textures.end(), [](const auto* texture) {
        return texture->relativePath().generic_string().find("cr8_czg.wad/") != std::string::npos;
    });
    REQUIRE(used != textures.end());
    const auto* texture = *used;
    Model::BrushBuilder builder(document->world()->mapFormat(), document->worldBounds());
    auto* brush = new Model::BrushNode(builder.createCube(32, texture->name()).value());
    document->addNodes({{document->world()->defaultLayer(), {brush}}});
    const auto snapshot = makePrefabSnapshot(*document, {brush});
    REQUIRE(snapshot.assets.size() == 2);
    // Quake WAD resources must also be decoded through the mounted WAD and
    // game's palette when their editor-side CPU pixels are no longer available.
    Assets::Texture prepared(texture->name(), texture->width(), texture->height(), texture->format(), texture->type());
    prepared.setRelativePath(texture->relativePath());
    auto copy = brush->brush();
    for (auto& face : copy.faces()) face.setTexture(&prepared);
    Model::BrushNode node(std::move(copy));
    CHECK(PrefabPreviewGeometry(node, game.get()).render() == snapshot.preview);
    const auto bundledWad = std::find_if(snapshot.assets.begin(), snapshot.assets.end(), [](const auto& asset) { return asset.kind == "wad"; });
    REQUIRE(bundledWad != snapshot.assets.end());
    CHECK(bundledWad->source == IO::pathAsQString(wad));
    CHECK(std::any_of(snapshot.assets.begin(), snapshot.assets.end(), [](const auto& asset) { return asset.resourcePath == "gfx/palette.lmp"; }));
    CHECK(snapshot.info.missingAssets.empty());
}

TEST_CASE_METHOD(PrefabTest, "Prefab browser exposes grouped previews, filtering, zoom and selection-sensitive actions", "[prefabs]") {
    QTemporaryDir temporary;
    const IO::PrefabLibrary library(temporary.path());
    auto* brush = cube(); document->addNodes({{document->world()->defaultLayer(), {brush}}});
    auto snapshot = makePrefabSnapshot(*document, {brush});
    snapshot.info.path = "PrefabRegression/furniture/couch";
    if (QFileInfo::exists(library.directory(snapshot.info.path))) library.remove(snapshot.info.path);
    library.save(snapshot.info, snapshot.fragment, snapshot.preview, snapshot.assets);
    PrefabBrowser browser(document, nullptr, nullptr, library.root());
    browser.resize(600, 500); browser.show(); flush();
    auto* add = browser.findChild<QAction*>("PrefabAdd");
    auto* insert = browser.findChild<QAction*>("PrefabInsert");
    auto* remove = browser.findChild<QAction*>("PrefabRemove");
    auto* tree = browser.findChild<QTreeWidget*>("PrefabTree");
    REQUIRE(add); REQUIRE(insert); REQUIRE(remove); REQUIRE(tree);
    CHECK_FALSE(add->isEnabled()); CHECK_FALSE(insert->isEnabled()); CHECK_FALSE(remove->isEnabled());
    document->selectNodes({brush}); CHECK(add->isEnabled());
    auto* item = tree->topLevelItem(0);
    while (item->childCount()) item = item->child(0);
    tree->setCurrentItem(item); flush();
    CHECK(insert->isEnabled()); CHECK(remove->isEnabled());
    const auto originalHeight = item->sizeHint(0).height();
    browser.findChild<QSpinBox*>("PrefabZoom")->setValue(200); flush();
    CHECK(item->sizeHint(0).height() > originalHeight);
    browser.findChild<QLineEdit*>("PrefabFilter")->setText("does-not-exist"); flush();
    CHECK(item->isHidden());
    browser.findChild<QLineEdit*>("PrefabFilter")->clear(); flush(); CHECK_FALSE(item->isHidden());
    library.remove(snapshot.info.path);
}

TEST_CASE_METHOD(PrefabTest, "Shader resources retain their original definition and referenced images", "[prefabs]") {
    QTemporaryDir temporary;
    REQUIRE(QDir{}.mkpath(temporary.path() + "/scripts"));
    REQUIRE(QDir{}.mkpath(temporary.path() + "/textures"));
    REQUIRE(prefabTestImage().save(temporary.path() + "/textures/preview.png"));
    REQUIRE(prefabTestImage().save(temporary.path() + "/textures/stage.png"));
    QFile script(temporary.path() + "/scripts/prefab.shader");
    REQUIRE(script.open(QIODevice::WriteOnly));
    script.write("textures/custom\n{\n qer_editorimage textures/preview.png\n {\n map textures/stage.png\n }\n}\n"); script.close();
    config.textureConfig.property.reset();
    config.textureConfig.extensions.clear();
    config.textureConfig.shaderSearchPath = "scripts";
    game->setGamePath(IO::pathFromQString(temporary.path()), logger);
    document->reloadTextureCollections();
    const auto& textures = document->textureManager().textures();
    const auto used = std::find_if(textures.begin(), textures.end(), [](const auto* texture) { return texture->relativePath() == "textures/custom"; });
    REQUIRE(used != textures.end());
    Model::BrushBuilder builder(document->world()->mapFormat(), document->worldBounds());
    auto* brush = new Model::BrushNode(builder.createCube(32, (*used)->name()).value());
    document->addNodes({{document->world()->defaultLayer(), {brush}}});
    const auto snapshot = makePrefabSnapshot(*document, {brush});
    std::vector<QString> paths;
    for (const auto& asset : snapshot.assets) paths.push_back(asset.resourcePath);
    CHECK(std::find(paths.begin(), paths.end(), "scripts/prefab.shader") != paths.end());
    CHECK(std::find(paths.begin(), paths.end(), "textures/preview.png") != paths.end());
    CHECK(std::find(paths.begin(), paths.end(), "textures/stage.png") != paths.end());
    CHECK(snapshot.info.missingAssets.empty());
}

TEST_CASE_METHOD(PrefabTest, "Tile view navigates folders, keeps selection across views and places labels below previews", "[prefabs]") {
    QTemporaryDir temporary;
    const IO::PrefabLibrary library(temporary.path());
    auto* brush = cube(); document->addNodes({{document->world()->defaultLayer(), {brush}}});
    auto snapshot = makePrefabSnapshot(*document, {brush});
    for (const auto* path : {"BigRock", "furniture/couch", "furniture/chair", "outside/trees/SmallOak", "outside/trees/LargeOak"}) {
        snapshot.info.path = path;
        library.save(snapshot.info, snapshot.fragment, snapshot.preview, snapshot.assets);
    }
    PrefabBrowser browser(document, nullptr, nullptr, library.root());
    browser.resize(740, 560); browser.show(); flush();
    auto* tilesAction = browser.findChild<QAction*>("PrefabTileView");
    auto* treeAction = browser.findChild<QAction*>("PrefabTreeView");
    auto* list = browser.findChild<QListWidget*>("PrefabTiles");
    auto* up = browser.findChild<QToolButton*>("PrefabUp");
    auto* location = browser.findChild<QLabel*>("PrefabLocation");
    auto* insert = browser.findChild<QAction*>("PrefabInsert");
    auto* tree = browser.findChild<QTreeWidget*>("PrefabTree");
    REQUIRE(tilesAction); REQUIRE(treeAction); REQUIRE(list); REQUIRE(up); REQUIRE(location); REQUIRE(insert); REQUIRE(tree);
    tilesAction->trigger(); flush();
    CHECK(tilesAction->isChecked()); CHECK_FALSE(treeAction->isChecked());
    CHECK(list->isVisible()); CHECK_FALSE(tree->isVisible());
    CHECK(list->count() == 3); CHECK_FALSE(up->isEnabled());
    const auto find = [list](const QString& path, int role) -> QListWidgetItem* {
        for (int i = 0; i < list->count(); ++i) if (list->item(i)->data(role).toString() == path) return list->item(i);
        return nullptr;
    };
    auto* folder = find("furniture", Qt::UserRole + 1);
    REQUIRE(folder);
    list->setCurrentItem(folder); flush(); CHECK_FALSE(insert->isEnabled());
    CHECK_FALSE(folder->flags() & Qt::ItemIsDragEnabled);
    list->itemDoubleClicked(folder); flush();
    CHECK(location->toolTip() == "furniture"); CHECK(up->isEnabled());
    REQUIRE(list->count() == 2);
    auto* couch = find("furniture/couch", Qt::UserRole);
    auto* chair = find("furniture/chair", Qt::UserRole);
    REQUIRE(couch); REQUIRE(chair);
    const auto couchRect = list->visualItemRect(couch);
    const auto chairRect = list->visualItemRect(chair);
    CHECK(couchRect.top() == chairRect.top()); CHECK(couchRect.left() != chairRect.left());
    auto* card = list->itemWidget(couch);
    REQUIRE(card);
    auto* image = card->findChild<QLabel*>("PrefabThumbnail");
    auto* name = card->findChild<QWidget*>("PrefabName");
    REQUIRE(image); REQUIRE(name);
    CHECK(name->geometry().top() > image->geometry().bottom());
    list->setCurrentItem(couch); flush(); CHECK(insert->isEnabled());
    const auto count = document->world()->defaultLayer()->childCount();
    list->itemDoubleClicked(couch); flush();
    CHECK(document->world()->defaultLayer()->childCount() == count + 1);
    treeAction->trigger(); flush();
    CHECK(tree->isVisible()); CHECK(tree->currentItem()->data(0, Qt::UserRole).toString() == "furniture/couch");
    tilesAction->trigger(); flush(); CHECK(location->toolTip() == "furniture");
    up->click(); flush(); CHECK(list->count() == 3);
    auto* outside = find("outside", Qt::UserRole + 1); REQUIRE(outside); list->itemDoubleClicked(outside); flush();
    auto* trees = find("outside/trees", Qt::UserRole + 1); REQUIRE(trees); list->itemDoubleClicked(trees); flush();
    CHECK(list->count() == 2); CHECK(location->toolTip() == "outside/trees");
    browser.findChild<QLineEdit*>("PrefabFilter")->setText("couch"); flush();
    CHECK(list->count() == 2); CHECK(list->item(0)->isHidden()); CHECK(list->item(1)->isHidden());
    browser.findChild<QLineEdit*>("PrefabFilter")->clear(); flush();
    {
        PrefabBrowser restored(document, nullptr, nullptr, library.root()); restored.show(); flush();
        CHECK(restored.findChild<QAction*>("PrefabTileView")->isChecked());
        CHECK(restored.findChild<QLabel*>("PrefabLocation")->toolTip() == "outside/trees");
        CHECK(restored.findChild<QListWidget*>("PrefabTiles")->count() == 2);
    }
    location->linkActivated("#"); flush();
    CHECK(list->count() == 3); CHECK_FALSE(up->isEnabled());
    treeAction->trigger(); flush(); CHECK(tree->selectedItems().empty());
}
}
