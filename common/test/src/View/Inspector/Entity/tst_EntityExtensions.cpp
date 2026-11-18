#include "Error.h"
#include "PreferenceManager.h"
#include <kdl/result.h>
#include "View/Tools/Brush/BrushUpstreamTest.h"

#include "Assets/EntityDefinition.h"
#include "Assets/EntityModel.h"
#include "IO/ELParser.h"
#include "IO/FgdParser.h"
#include "IO/NodeReader.h"
#include "IO/SimpleParserStatus.h"
#include "Model/EntityNode.h"
#include "Model/GroupNode.h"
#include "Preferences.h"
#include "View/Inspector/Entity/EntityAudioPreview.h"
#include "View/Inspector/Layer/LayerEditor.h"
#include "View/Inspector/Layer/LayerListBox.h"
#include "View/Dialogs/NodeSourceDialog.h"
#include "View/Inspector/ViewSettings/ViewEditor.h"

#include <QCheckBox>
#include <QElapsedTimer>
#include <QTest>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextLayout>
#include <QToolButton>
#include <QTreeWidgetItemIterator>

#include <catch2/catch.hpp>
#include <vm/mat_ext.h>

namespace TrenchBroom::View {
namespace {
    void writeExtensionFile(const QString& name, const QByteArray& bytes) {
        REQUIRE(QDir{}.mkpath(QFileInfo{name}.absolutePath()));
        QFile file{name};
        REQUIRE(file.open(QIODevice::WriteOnly));
        REQUIRE(file.write(bytes) == bytes.size());
    }

    QByteArray silentWave() {
        auto bytes = QByteArray{};
        const auto u16 = [&](quint16 n) {
            bytes += char(n);
            bytes += char(n >> 8);
        };
        const auto u32 = [&](quint32 n) {
            for (int i = 0; i < 4; ++i)
                bytes += char(n >> (i * 8));
        };
        bytes += "RIFF";
        u32(36 + 32000);
        bytes += "WAVEfmt ";
        u32(16);
        u16(1);
        u16(1);
        u32(8000);
        u32(8000);
        u16(1);
        u16(8);
        bytes += "data";
        u32(32000);
        bytes += QByteArray(32000, char(128));
        return bytes;
    }

    QByteArray oneFilePak(const QByteArray& name, const QByteArray& data) {
        auto bytes = QByteArray{"PACK"};
        const auto u32 = [&](quint32 n) {
            for (int i = 0; i < 4; ++i)
                bytes += char(n >> (i * 8));
        };
        u32(12 + data.size());
        u32(64);
        bytes += data;
        bytes += name;
        bytes += QByteArray(56 - name.size(), '\0');
        u32(12);
        u32(data.size());
        return bytes;
    }

    QTreeWidgetItem* extensionItem(LayerListBox& tree, const Model::Node* node) {
        for (QTreeWidgetItemIterator it{&tree}; *it; ++it)
            if ((*it)->data(0, Qt::UserRole).value<quintptr>() == reinterpret_cast<quintptr>(node)) return *it;
        return nullptr;
    }
} // namespace

TEST_CASE("Entity model properties work without a model expression", "[entity-extensions][model]") {
    Model::Entity entity{
        {}, {{"classname", "misc_model"}, {"model", R"(models\rock.obj)"}, {"origin", "100 200 300"}, {"angle", "90"}, {"modelscale_vec", "2 3 4"}, {"skin", "2"}, {"frame", "3"}}};
    CHECK(entity.modelSpecification() == Assets::ModelSpecification{"models/rock.obj", 2, 3});
    CHECK(vm::is_equal(entity.modelTransformation() * vm::vec3{1, 0, 0}, vm::vec3{100, 202, 300}, 0.00001));
    CHECK(vm::is_equal(entity.modelTransformation() * vm::vec3{0, 0, 1}, vm::vec3{100, 200, 304}, 0.00001));
    entity.addOrUpdateProperty({}, "skin", "-1");
    entity.addOrUpdateProperty({}, "frame", "99999999999999999999999999999");
    CHECK(entity.modelSpecification().skinIndex == 0);
    CHECK(entity.modelSpecification().frameIndex == 0);
    entity.addOrUpdateProperty({}, "model", "*1");
    CHECK(entity.modelSpecification().path.empty());
    entity.addOrUpdateProperty({}, "model", "models/rock.obj");
    entity.setPointEntity({}, false);
    CHECK(entity.modelSpecification().path.empty());
}

TEST_CASE("FGD model expressions and game scaling retain priority", "[entity-extensions][model]") {
    Assets::PointEntityDefinition definition{
        "misc_model", Color{}, vm::bbox3{8.0}, "", {}, Assets::ModelDefinition{IO::ELParser::parseStrict(R"({path: "models/defined.obj", scale: 3})")}, {}};
    Model::Entity entity{{}, {{"classname", "misc_model"}, {"model", "models/property.obj"}, {"scale", "20"}}};
    entity.setDefinition({}, &definition);
    CHECK(entity.modelSpecification().path == "models/defined.obj");
    CHECK(entity.modelTransformation() * vm::vec3{1, 0, 0} == vm::vec3{3, 0, 0});
    Model::EntityPropertyConfig config{IO::ELParser::parseStrict("2")};
    Model::Entity scaled{config, {{"classname", "misc_model"}, {"model", "models/property.obj"}, {"scale", "20"}}};
    CHECK(scaled.modelTransformation() * vm::vec3{1, 0, 0} == vm::vec3{2, 0, 0});
}

TEST_CASE_METHOD(BrushUpstreamTest, "Property models load through the game model manager", "[entity-extensions][model]") {
    QTemporaryDir folder;
    writeExtensionFile(folder.path() + "/models/rock.obj", "o Rock\nv 0 0 0\nv 16 0 0\nv 0 24 0\nf 1 2 3\n");
    game->setGamePath(std::filesystem::path{folder.path().toStdString()}, logger);
    newMap();
    auto* entity = new Model::EntityNode{{}, {{"classname", "misc_model"}, {"model", "models/rock.obj"}, {"origin", "128 0 0"}}};
    REQUIRE_FALSE(document->addNodes({{document->world()->defaultLayer(), {entity}}}).empty());
    REQUIRE(entity->entity().model());
    CHECK(entity->modelBounds().min.x() == Approx(128));
    CHECK(entity->modelBounds().max.x() == Approx(144));
    document->selectNodes({entity});
    REQUIRE(document->setProperty("model", "models/missing.obj"));
    CHECK_FALSE(entity->entity().model());
    document->undoCommand();
    CHECK(entity->entity().model());
}

TEST_CASE("FGD source retains included class text and location", "[entity-extensions][source]") {
    QTemporaryDir folder;
    const QByteArray included = "// Included class\n@PointClass base(Common) = misc_model : \"Model [example]\"\n[\n model(string) : \"Model\"\n]\n";
    writeExtensionFile(folder.path() + "/sub/models.fgd", included);
    const std::string main = "@BaseClass = Common [ angle(integer) : \"Angle\" : 0 ]\n@include \"sub/models.fgd\"\n@SolidClass = func_detail []\n";
    NullLogger logger;
    IO::SimpleParserStatus status{logger};
    IO::FgdParser parser{main, Color{}, std::filesystem::path{(folder.path() + "/main.fgd").toStdString()}};
    auto definitions = parser.parseDefinitions(status);
    REQUIRE(definitions.size() == 2);
    const auto& source = definitions.front()->source();
    REQUIRE(source);
    CHECK(source->path == std::filesystem::path{(folder.path() + "/sub/models.fgd").toStdString()});
    CHECK(source->line == 2);
    CHECK(source->text == included.mid(included.indexOf('@')).trimmed().toStdString());
    CHECK(definitions.front()->propertyDefinition("angle"));
    REQUIRE(definitions.back()->source());
    CHECK(definitions.back()->source()->path.filename() == "main.fgd");
    CHECK(definitions.back()->source()->text == "@SolidClass = func_detail []");
}

TEST_CASE_METHOD(BrushUpstreamTest, "Source snapshots preserve map positions and selection", "[entity-extensions][source]") {
    newMap();
    auto* brush = addBrush(vm::bbox3{{0, 0, 0}, {32, 48, 64}}, "stone");
    auto* other = addBrush(vm::bbox3{{100, 0, 0}, {116, 16, 16}});
    document->selectNodes({other});
    brush->setFilePosition(111, 10);
    brush->brush().faces().front().setFilePosition(113, 1);
    document->world()->setFilePosition(1, 200);
    const auto modified = document->modified();
    const auto pages = nodeSourcePages(*document, *brush);
    REQUIRE(pages.size() == 1);
    CHECK(pages.front().text.contains("stone"));
    CHECK(pages.front().text.contains("worldspawn"));
    CHECK(brush->lineNumber() == 111);
    CHECK(brush->containsLine(120));
    CHECK(brush->brush().faces().front().lineNumber() == 113);
    CHECK(document->world()->containsLine(199));
    CHECK(document->selectedNodes().nodes() == std::vector<Model::Node*>{other});
    CHECK(document->modified() == modified);
    IO::SimpleParserStatus status{logger};
    auto parsed = IO::NodeReader::read(pages.front().text.toStdString(), document->world()->mapFormat(), document->worldBounds(), document->world()->entityPropertyConfig(), status);
    REQUIRE(parsed.size() == 1);
    auto cleanup = std::unique_ptr<Model::Node>{parsed.front()};
    REQUIRE(cleanup->childCount() == 1);
    CHECK(dynamic_cast<Model::BrushNode*>(cleanup->children().front()));
}

TEST_CASE_METHOD(BrushUpstreamTest, "Hidden locked entities expose FGD and MAP source from the tree", "[entity-extensions][source]") {
    newMap();
    IO::SimpleParserStatus status{logger};
    IO::FgdParser parser{"@PointClass = sample []", Color{}};
    auto definitions = parser.parseDefinitions(status);
    auto* entity = new Model::EntityNode{{}, {{"classname", "sample"}, {"origin", "5 6 7"}}};
    document->addNodes({{document->world()->defaultLayer(), {entity}}});
    entity->setDefinition(definitions.front().get());
    document->hide({entity});
    document->lock({entity});
    LayerEditor editor{document, nullptr};
    auto* tree = editor.findChild<LayerListBox*>();
    REQUIRE(tree);
    tree->setSelectionSyncEnabled(false);
    auto* item = extensionItem(*tree, entity);
    REQUIRE(item);
    tree->setCurrentItem(item, 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    auto* source = editor.findChild<QToolButton*>("layerShowSource");
    REQUIRE(source);
    CHECK(source->isEnabled());
    source->click();
    auto* dialog = editor.findChild<NodeSourceDialog*>();
    REQUIRE(dialog);
    auto* tabs = dialog->findChild<QTabWidget*>();
    REQUIRE(tabs);
    REQUIRE(tabs->count() == 2);
    auto* fgd = tabs->widget(0)->findChild<SourceTextEdit*>();
    auto* map = tabs->widget(1)->findChild<SourceTextEdit*>();
    REQUIRE(fgd);
    REQUIRE(map);
    CHECK(fgd->isReadOnly());
    CHECK(fgd->toPlainText() == "@PointClass = sample []");
    CHECK(map->toPlainText().contains("5 6 7"));
    CHECK(document->selectedNodes().empty());
    dialog->close();
    entity->setDefinition(nullptr);
}

TEST_CASE_METHOD(BrushUpstreamTest, "Brush source retains parent entity and group without siblings", "[entity-extensions][source]") {
    newMap();
    const Model::BrushBuilder builder{document->world()->mapFormat(), document->worldBounds()};
    document->addNodes({{document->world()->defaultLayer(), {new Model::GroupNode{Model::Group{"Unrelated group"}}}}});
    auto* group = new Model::GroupNode{Model::Group{"Source group"}};
    auto* entity = new Model::EntityNode{{}, {{"classname", "func_wall"}}};
    auto* brush = new Model::BrushNode{builder.createCube(32, "inspected_texture").value()};
    auto* sibling = new Model::BrushNode{builder.createCube(16, "excluded_texture").value()};
    entity->addChildren({brush, sibling});
    group->addChild(entity);
    document->addNodes({{document->world()->defaultLayer(), {group}}});
    const auto pages = nodeSourcePages(*document, *brush);
    REQUIRE(pages.size() == 1);
    CHECK(pages.front().text.contains("func_wall"));
    CHECK(pages.front().text.contains("Source group"));
    REQUIRE(group->persistentId());
    CHECK(pages.front().text.contains(QString{"\"_tb_id\" \"%1\""}.arg(*group->persistentId())));
    CHECK(pages.front().text.contains("inspected_texture"));
    CHECK_FALSE(pages.front().text.contains("excluded_texture"));
    CHECK(brush->parent() == entity);
    CHECK(entity->childCount() == 2);
}

TEST_CASE("Source syntax colors can be configured independently", "[entity-extensions][source]") {
    SourceTextEdit editor{false};
    editor.setStyleSheet("QPlainTextEdit { background: #152535; color: #eeeeee; font-size: 15px; qproperty-propertyColor: #112233; qproperty-stringColor: #445566; qproperty-numberColor: "
                         "#778899; qproperty-commentColor: #aabbcc; }");
    editor.ensurePolished();
    editor.setPlainText("\"origin\" \"1 2 3\" // text\n( -16 32 64 )\n/* comment\n\nstill comment */ .5");
    QCoreApplication::processEvents();
    CHECK(editor.propertyColor() == QColor{"#112233"});
    CHECK(editor.font().pixelSize() == 15);
    const auto formats = editor.document()->firstBlock().layout()->formats();
    REQUIRE(formats.size() == 3);
    CHECK(formats.front().format.foreground().color() == QColor{"#112233"});
    CHECK(formats[1].format.foreground().color() == QColor{"#445566"});
    CHECK(formats.back().format.foreground().color() == QColor{"#aabbcc"});
    CHECK(editor.document()->findBlockByNumber(2).userState() == 1);
    CHECK(editor.document()->findBlockByNumber(3).userState() == 1);
    CHECK(editor.document()->findBlockByNumber(4).userState() == 0);
}

TEST_CASE("Audio file references are recognized only on point entities", "[entity-extensions][audio]") {
    Model::Entity entity{{}, {{"classname", "target_speaker"}, {"noise", R"(*world\water.WAV)"}, {"sound", "other.ogg"}}};
    CHECK(entityAudioPath(entity) == std::filesystem::path{"world/water.WAV"});
    entity.removeProperty({}, "noise");
    CHECK(entityAudioPath(entity) == std::filesystem::path{"other.ogg"});
    entity.addOrUpdateProperty({}, "sound", "3");
    CHECK_FALSE(entityAudioPath(entity));
    entity.addOrUpdateProperty({}, "custom_audio", "music/theme.mp3");
    CHECK(entityAudioPath(entity) == std::filesystem::path{"music/theme.mp3"});
    entity.setPointEntity({}, false);
    CHECK_FALSE(entityAudioPath(entity));
}

TEST_CASE_METHOD(BrushUpstreamTest, "Audio resolves loose and archived game resources", "[entity-extensions][audio]") {
    QTemporaryDir folder;
    const auto wave = silentWave();
    writeExtensionFile(folder.path() + "/sound/world/loose.wav", wave);
    writeExtensionFile(folder.path() + "/pak0.pak", oneFilePak("sound/world/packed.wav", wave));
    game->setGamePath(std::filesystem::path{folder.path().toStdString()}, logger);
    newMap();
    for (const auto& path : {"world/loose.wav", "sound/world/loose.wav", "world/packed.wav", "sound/world/packed.wav"}) {
        auto audio = readEntityAudio(*document, path);
        REQUIRE(audio.is_success());
        CHECK(audio.value() == wave);
    }
    CHECK(readEntityAudio(*document, std::filesystem::path{(folder.path() + "/sound/world/loose.wav").toStdString()}).is_success());
    CHECK_FALSE(readEntityAudio(*document, "missing.wav").is_success());
}

TEST_CASE_METHOD(BrushUpstreamTest, "Audio selection preview stops and follows once and loop settings", "[entity-extensions][audio]") {
    const TemporarilySetPref enabled{Preferences::PreviewEntityAudio, true};
    const TemporarilySetPref loop{Preferences::LoopEntityAudio, false};
    const TemporarilySetPref volume{Preferences::EntityAudioVolume, 0};
    QTemporaryDir folder;
    writeExtensionFile(folder.path() + "/sound/preview.wav", silentWave());
    game->setGamePath(std::filesystem::path{folder.path().toStdString()}, logger);
    newMap();
    auto* entity = new Model::EntityNode{{}, {{"classname", "target_speaker"}, {"noise", "preview.wav"}}};
    auto* other = new Model::EntityNode{{}, {{"classname", "light"}}};
    document->addNodes({{document->world()->defaultLayer(), {entity, other}}});
    setPref(Preferences::PreviewEntityAudio, true);
    setPref(Preferences::LoopEntityAudio, false);
    setPref(Preferences::EntityAudioVolume, 0);
    EntityAudioPreview preview{document};
    auto* player = preview.findChild<QMediaPlayer*>();
    REQUIRE(player);
    document->selectNodes({entity});
    REQUIRE(!player->source().isEmpty());
    CHECK(player->loops() == QMediaPlayer::Once);
    const auto url = player->source();
    CHECK(QFile::exists(url.toLocalFile()));
    setPref(Preferences::LoopEntityAudio, true);
    PreferenceManager::instance().preferenceDidChangeNotifier(Preferences::LoopEntityAudio.path());
    CHECK(player->loops() == QMediaPlayer::Infinite);
    CHECK(player->source() == url);
    document->selectNodes({other});
    CHECK(player->source().isEmpty());
    CHECK_FALSE(QFile::exists(url.toLocalFile()));
    document->deselectAll();
    document->selectNodes({entity});
    REQUIRE(!player->source().isEmpty());
    setPref(Preferences::PreviewEntityAudio, false);
    PreferenceManager::instance().preferenceDidChangeNotifier(Preferences::PreviewEntityAudio.path());
    CHECK(player->source().isEmpty());
    setPref(Preferences::PreviewEntityAudio, true);
    PreferenceManager::instance().preferenceDidChangeNotifier(Preferences::PreviewEntityAudio.path());
    REQUIRE(!player->source().isEmpty());
    document->deleteObjects();
    CHECK(player->source().isEmpty());
}

TEST_CASE_METHOD(BrushUpstreamTest, "Native audio preview completes once and resumes in a loop", "[.native-audio][audio]") {
    const TemporarilySetPref enabled{Preferences::PreviewEntityAudio, true};
    const TemporarilySetPref loop{Preferences::LoopEntityAudio, false};
    const TemporarilySetPref volume{Preferences::EntityAudioVolume, 0};
    QTemporaryDir folder;
    writeExtensionFile(folder.path() + "/sound/native.wav", silentWave());
    game->setGamePath(std::filesystem::path{folder.path().toStdString()}, logger);
    newMap();
    auto* entity = new Model::EntityNode{{}, {{"classname", "target_speaker"}, {"noise", "native.wav"}}};
    document->addNodes({{document->world()->defaultLayer(), {entity}}});
    EntityAudioPreview preview{document};
    auto* player = preview.findChild<QMediaPlayer*>();
    auto* output = preview.findChild<QAudioOutput*>();
    REQUIRE(player);
    REQUIRE(output);
    document->selectNodes({entity});
    REQUIRE(output->volume() == 0.0f);
    const auto source = player->source();
    REQUIRE_FALSE(source.isEmpty());
    const auto waitFor = [](auto predicate, int timeout) {
        QElapsedTimer timer;
        timer.start();
        while (!predicate() && timer.elapsed() < timeout) QTest::qWait(10);
        return predicate();
    };
    INFO(player->errorString().toStdString());
    REQUIRE(waitFor([&] { return player->duration() > 0; }, 5000));
    REQUIRE(waitFor([&] { return player->mediaStatus() == QMediaPlayer::EndOfMedia; }, 8000));
    CHECK(player->error() == QMediaPlayer::NoError);
    CHECK(player->source() == source);
    setPref(Preferences::LoopEntityAudio, true);
    PreferenceManager::instance().preferenceDidChangeNotifier(Preferences::LoopEntityAudio.path());
    REQUIRE(waitFor([&] { return player->playbackState() == QMediaPlayer::PlayingState; }, 5000));
    QTest::qWait(4500);
    CHECK(player->playbackState() == QMediaPlayer::PlayingState);
    CHECK(player->source() == source);
    CHECK(player->error() == QMediaPlayer::NoError);
    document->deselectAll();
    CHECK(player->source().isEmpty());
    CHECK_FALSE(QFile::exists(source.toLocalFile()));
}

TEST_CASE_METHOD(BrushUpstreamTest, "Audio preview controls persist and disable dependent options", "[entity-extensions][audio]") {
    const TemporarilySetPref enabledPref{Preferences::PreviewEntityAudio, true};
    const TemporarilySetPref loopPref{Preferences::LoopEntityAudio, false};
    const TemporarilySetPref volumePref{Preferences::EntityAudioVolume, 60};
    newMap();
    ViewEditor editor{document};
    auto* enabled = editor.findChild<QCheckBox*>("previewEntityAudio");
    auto* loop = editor.findChild<QCheckBox*>("loopEntityAudio");
    auto* volume = editor.findChild<SliderWithLabel*>("entityAudioVolume");
    REQUIRE(enabled);
    REQUIRE(loop);
    REQUIRE(volume);
    setPref(Preferences::PreviewEntityAudio, true);
    PreferenceManager::instance().preferenceDidChangeNotifier(Preferences::PreviewEntityAudio.path());
    CHECK(loop->isEnabled());
    loop->click();
    CHECK(pref(Preferences::LoopEntityAudio) == loop->isChecked());
    volume->setValue(37);
    CHECK(pref(Preferences::EntityAudioVolume) == 37);
    enabled->click();
    PreferenceManager::instance().preferenceDidChangeNotifier(Preferences::PreviewEntityAudio.path());
    CHECK_FALSE(pref(Preferences::PreviewEntityAudio));
    CHECK_FALSE(loop->isEnabled());
    CHECK_FALSE(volume->isEnabled());
}

TEST_CASE_METHOD(BrushUpstreamTest, "Destroying an active audio preview removes its temporary resource", "[entity-extensions][audio]") {
    const TemporarilySetPref enabled{Preferences::PreviewEntityAudio, true};
    const TemporarilySetPref volume{Preferences::EntityAudioVolume, 0};
    QTemporaryDir folder;
    writeExtensionFile(folder.path() + "/sound/preview.wav", silentWave());
    game->setGamePath(std::filesystem::path{folder.path().toStdString()}, logger);
    newMap();
    auto* entity = new Model::EntityNode{{}, {{"classname", "target_speaker"}, {"noise", "preview.wav"}}};
    document->addNodes({{document->world()->defaultLayer(), {entity}}});
    auto preview = std::make_unique<EntityAudioPreview>(document);
    auto* player = preview->findChild<QMediaPlayer*>();
    REQUIRE(player);
    document->selectNodes({entity});
    const auto path = player->source().toLocalFile();
    REQUIRE(QFile::exists(path));
    preview.reset();
    CHECK_FALSE(QFile::exists(path));
}
} // namespace TrenchBroom::View
