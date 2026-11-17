#include "Assets/EntityModel.h"
#include "Assets/EntityModelManager.h"
#include "Assets/ModelDefinition.h"
#include "Assets/Palette.h"
#include "Exceptions.h"
#include "IO/AssimpParser.h"
#include "IO/DiskFileSystem.h"
#include "IO/EntityModelLoader.h"
#include "IO/Md2Parser.h"
#include "IO/MdlParser.h"
#include "IO/PathQt.h"
#include "IO/Reader.h"
#include "Logger.h"
#include "Renderer/EntityModelAnimation.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <catch2/catch.hpp>
#include <cstring>
#include <limits>

namespace TrenchBroom::Assets {
namespace {
struct AnimationBytes {
    std::string data;
    void integer(uint32_t value) { for (auto i = 0; i < 4; ++i) data.push_back(char((value >> (i * 8)) & 255)); }
    void real(float value) { uint32_t bits; std::memcpy(&bits, &value, 4); integer(bits); }
    void name(std::string value) { value.resize(16, '\0'); data += value; }
    void vector(float x, float y, float z) { real(x); real(y); real(z); }
    void pose(const std::string& label, unsigned char offset) {
        data.append(8, '\0'); name(label);
        for (const auto x : {offset, static_cast<unsigned char>(offset + 1), offset}) {
            data.push_back(char(x)); data.append(3, '\0');
        }
    }
};

Palette animationPalette() {
    auto colors = std::make_shared<PaletteData>();
    colors->opaqueData.resize(1024, 255);
    colors->index255TransparentData = colors->opaqueData;
    return Palette{colors};
}

AnimationBytes groupedMdl() {
    AnimationBytes bytes;
    bytes.data = "IDPO"; bytes.integer(6);
    bytes.vector(1, 1, 1); bytes.vector(0, 0, 0); bytes.real(0); bytes.vector(0, 0, 0);
    for (const auto value : {1, 1, 1, 3, 1, 2, 0, 0}) bytes.integer(uint32_t(value));
    bytes.real(1);
    bytes.integer(0); bytes.data.push_back('\0'); // one 1x1 skin
    for (auto i = 0; i < 9; ++i) bytes.integer(0); // texture coordinates
    for (const auto value : {1, 0, 1, 2}) bytes.integer(uint32_t(value)); // triangle
    bytes.integer(1); bytes.integer(3); bytes.data.append(8, '\0');
    bytes.real(.1f); bytes.real(.25f); bytes.real(.5f);
    bytes.pose("idle1", 0); bytes.pose("idle2", 2); bytes.pose("idle3", 4);
    bytes.integer(0); bytes.pose("still", 20);
    return bytes;
}

QByteArray animatedGltf(bool skinned) {
    AnimationBytes buffer;
    QJsonArray views, accessors;
    const auto view = [&](int begin) {
        const auto index = views.size();
        views.append(QJsonObject{{"buffer", 0}, {"byteOffset", begin}, {"byteLength", int(buffer.data.size()) - begin}});
        while (buffer.data.size() % 4) buffer.data.push_back('\0');
        return index;
    };
    const auto accessor = [&](int bufferView, int component, const char* type, int count) {
        const auto index = accessors.size();
        accessors.append(QJsonObject{{"bufferView", bufferView}, {"componentType", component}, {"count", count}, {"type", type}});
        return index;
    };
    buffer.vector(0, 0, 0); buffer.vector(1, 0, 0); buffer.vector(0, 1, 0);
    const auto positions = accessor(view(0), 5126, "VEC3", 3);
    auto positionAccessor = accessors[positions].toObject();
    positionAccessor["min"] = QJsonArray{0, 0, 0}; positionAccessor["max"] = QJsonArray{1, 1, 0}; accessors[positions] = positionAccessor;
    auto begin = int(buffer.data.size());
    for (const auto i : {0, 1, 2}) { buffer.data.push_back(char(i)); buffer.data.push_back('\0'); }
    const auto indices = accessor(view(begin), 5123, "SCALAR", 3);
    begin = int(buffer.data.size()); buffer.real(0); buffer.real(1);
    const auto times = accessor(view(begin), 5126, "SCALAR", 2);
    auto timeAccessor = accessors[times].toObject(); timeAccessor["min"] = QJsonArray{0}; timeAccessor["max"] = QJsonArray{1}; accessors[times] = timeAccessor;
    begin = int(buffer.data.size()); buffer.vector(skinned ? 0 : 3, 0, 0); buffer.vector(skinned ? 10 : 13, 0, 0);
    const auto translations = accessor(view(begin), 5126, "VEC3", 2);
    QJsonObject attributes{{"POSITION", positions}};
    QJsonArray skins, nodes;
    if (skinned) {
        begin = int(buffer.data.size());
        for (auto i = 0; i < 3; ++i) { buffer.data.push_back('\1'); buffer.data.append(3, '\0'); }
        attributes["JOINTS_0"] = accessor(view(begin), 5121, "VEC4", 3);
        begin = int(buffer.data.size());
        for (auto i = 0; i < 3; ++i) { buffer.real(1); buffer.vector(0, 0, 0); }
        attributes["WEIGHTS_0"] = accessor(view(begin), 5126, "VEC4", 3);
        begin = int(buffer.data.size());
        for (auto bone = 0; bone < 2; ++bone) {
            for (auto i = 0; i < 16; ++i) buffer.real(i == 13 && bone == 1 ? -2.f : (i % 5 == 0 ? 1.f : 0.f));
        }
        const auto bind = accessor(view(begin), 5126, "MAT4", 2);
        skins.append(QJsonObject{{"joints", QJsonArray{1, 2}}, {"inverseBindMatrices", bind}});
        nodes = QJsonArray{QJsonObject{{"name", "Mesh"}, {"mesh", 0}, {"skin", 0}},
                           QJsonObject{{"name", "Root"}, {"children", QJsonArray{2}}},
                           QJsonObject{{"name", "Tip"}, {"translation", QJsonArray{0, 2, 0}}}};
    } else nodes.append(QJsonObject{{"name", "Moving"}, {"mesh", 0}, {"translation", QJsonArray{3, 0, 0}}});
    const auto encoded = QByteArray::fromStdString(buffer.data).toBase64();
    QJsonObject root{{"asset", QJsonObject{{"version", "2.0"}}}, {"scene", 0}, {"nodes", nodes},
        {"scenes", QJsonArray{QJsonObject{{"nodes", skinned ? QJsonArray{0, 1} : QJsonArray{0}}}}},
        {"meshes", QJsonArray{QJsonObject{{"primitives", QJsonArray{QJsonObject{{"attributes", attributes}, {"indices", indices}}}}}}},
        {"buffers", QJsonArray{QJsonObject{{"byteLength", int(buffer.data.size())}, {"uri", "data:application/octet-stream;base64," + QString::fromLatin1(encoded)}}}},
        {"bufferViews", views}, {"accessors", accessors},
        {"animations", QJsonArray{QJsonObject{{"name", "Idle"},
            {"samplers", QJsonArray{QJsonObject{{"input", times}, {"output", translations}, {"interpolation", "LINEAR"}}}},
            {"channels", QJsonArray{QJsonObject{{"sampler", 0}, {"target", QJsonObject{{"node", skinned ? 1 : 0}, {"path", "translation"}}}}}}}}}};
    if (skinned) root["skins"] = skins;
    return QJsonDocument{root}.toJson();
}
} // namespace

TEST_CASE("Named model sequences loop independently instead of cycling through attacks and death", "[model-animation]") {
    EntityModel model{"test", PitchType::Normal, Orientation::Oriented};
    for (auto i = 0; i < 7; ++i) model.addFrame();
    model.addAnimationsFromFrameNames({"stand1", "stand2", "stand3", "run1", "run2", "death1", "death2"});
    const auto* stand = model.animation(0);
    REQUIRE(stand);
    CHECK(model.animation(2) == stand);
    CHECK(stand->frames == std::vector<size_t>{0, 1, 2});
    CHECK(stand->frameAt(.11) == 1);
    CHECK(stand->frameAt(.21) == 2);
    CHECK(stand->frameAt(.31) == 0);
    CHECK(stand->frameAt(-1) == 0);
    CHECK(stand->frameAt(std::numeric_limits<double>::infinity()) == 0);
    REQUIRE(model.animation(3)); CHECK(model.animation(3)->frames == std::vector<size_t>{3, 4});
    CHECK_FALSE(model.animation(99));
}

TEST_CASE("MDL groups retain timing and load additional poses after the source buffer is gone", "[model-animation]") {
    NullLogger logger;
    const auto initialize = [&] {
        const auto bytes = groupedMdl();
        auto reader = IO::Reader::from(bytes.data.data(), bytes.data.data() + bytes.data.size());
        const auto palette = animationPalette();
        IO::MdlParser parser{"grouped", reader, palette};
        auto model = parser.initializeModel(logger);
        parser.loadFrame(0, *model, logger);
        parser.loadFrame(1, *model, logger);
        return model;
    };
    auto model = initialize();
    REQUIRE(model->frameCount() == 4);
    CHECK(model->surface(0).frameCount() == 4);
    CHECK(model->frame(1)->bounds().min.x() == 20);
    CHECK_FALSE(model->animation(1));
    const auto* idle = model->animation(0); REQUIRE(idle);
    CHECK(idle->duration() == Approx(.5));
    CHECK(idle->frameAt(.11) == 2); CHECK(idle->frameAt(.3) == 3); CHECK(idle->frameAt(.51) == 0);
    REQUIRE_FALSE(model->frame(2)->loaded());
    REQUIRE(model->loadAnimationFrame(2));
    CHECK(model->frame(2)->bounds().min.x() == 2);
    const auto* held = model->frame(2);
    CHECK(model->loadAnimationFrame(2)); CHECK(model->frame(2) == held);
    REQUIRE(model->loadAnimationFrame(3)); CHECK(model->frame(3)->bounds().min.x() == 4);
}

TEST_CASE("MD2 model headers provide animation groups without loading all geometry", "[model-animation]") {
    AnimationBytes bytes;
    bytes.data = "IDP2"; bytes.integer(8);
    for (const auto value : {1, 1, 52, 0, 3, 0, 0, 0, 4, 68, 68, 68, 68, 276, 276}) bytes.integer(uint32_t(value));
    for (const auto* name : {"idle01", "idle02", "run01", "run02"}) {
        bytes.vector(1, 1, 1); bytes.vector(0, 0, 0); bytes.name(name); bytes.data.append(12, '\0');
    }
    auto reader = IO::Reader::from(bytes.data.data(), bytes.data.data() + bytes.data.size());
    const auto palette = animationPalette();
    const IO::DiskFileSystem fs{std::filesystem::temp_directory_path()};
    NullLogger logger;
    IO::Md2Parser parser{"test.md2", reader, palette, fs};
    const auto model = parser.initializeModel(logger);
    REQUIRE(model->animation(0)); REQUIRE(model->animation(2));
    CHECK(model->animation(0)->frames == std::vector<size_t>{0, 1});
    CHECK(model->animation(2)->frames == std::vector<size_t>{2, 3});
    for (const auto* frame : model->frames()) CHECK_FALSE(frame->loaded());
}

TEST_CASE("Imported clips animate nodes and bones without repeated file loading", "[model-animation]") {
    for (const auto skinned : {false, true}) {
        CAPTURE(skinned);
        QTemporaryDir directory;
        const auto path = directory.filePath("animated.gltf");
        QFile file{path}; REQUIRE(file.open(QIODevice::WriteOnly)); file.write(animatedGltf(skinned)); file.close();
        IO::DiskFileSystem fs{IO::pathFromQString(directory.path())};
        NullLogger logger;
        IO::AssimpParser parser{"animated.gltf", fs};
        auto model = parser.initializeModel(logger);
        parser.loadFrame(0, *model, logger);
        const auto* clip = model->animation(0); REQUIRE(clip);
        CHECK(clip->frames.size() == 20);
        CHECK(clip->duration() == Approx(1));
        CHECK(model->frame(0)->bounds().min.x() == Approx(skinned ? 0 : 3).margin(.001));
        REQUIRE(QFile::remove(path));
        const auto middle = clip->frameAt(.51);
        REQUIRE(model->loadAnimationFrame(middle));
        CHECK(model->frame(middle)->bounds().min.x() == Approx(skinned ? 5 : 8).margin(.001));
        CHECK(model->frame(0)->bounds().min.x() == Approx(skinned ? 0 : 3).margin(.001));
    }
}

TEST_CASE("Model animation selection and camera triggers are independent and combine", "[model-animation]") {
    const vm::bbox3f bounds{{-10, -10, -10}, {10, 10, 10}};
    using Renderer::previewModelAnimation;
    CHECK(previewModelAnimation(true, bounds, {1000, 0, 0}, true, false, 32));
    CHECK_FALSE(previewModelAnimation(true, bounds, {1000, 0, 0}, false, true, 32));
    CHECK(previewModelAnimation(false, bounds, {42, 0, 0}, false, true, 32));
    CHECK_FALSE(previewModelAnimation(false, bounds, {43, 0, 0}, true, true, 32));
    CHECK(previewModelAnimation(false, bounds, {0, 0, 0}, false, true, 0));
    CHECK_FALSE(previewModelAnimation(true, bounds, {0, 0, 0}, false, false, 100));
    CHECK_FALSE(previewModelAnimation(false, bounds, {0, 0, 0}, false, true, std::numeric_limits<double>::quiet_NaN()));
}

TEST_CASE("The model manager shares loaded animation poses and does not retry broken frames every loop", "[model-animation]") {
    class AnimationLoader : public IO::EntityModelLoader {
    public:
        mutable int initializations = 0, loads = 0;
    private:
        std::unique_ptr<EntityModel> doInitializeModel(const std::filesystem::path&, Logger&) const override {
            ++initializations;
            auto model = std::make_unique<EntityModel>("test", PitchType::Normal, Orientation::Oriented);
            for (auto i = 0; i < 3; ++i) model->addFrame();
            model->addAnimationsFromFrameNames({"idle1", "idle2", "idle3"});
            return model;
        }
        void doLoadFrame(const std::filesystem::path&, size_t frame, EntityModel& model, Logger&) const override {
            ++loads;
            if (frame == 2) throw GameException{"Broken frame"};
            model.loadFrame(frame, "idle", vm::bbox3f{1.f});
        }
    } loader;
    NullLogger logger;
    EntityModelManager manager{GL_LINEAR, GL_LINEAR, logger};
    manager.setLoader(&loader);
    ModelSpecification spec{"test.mdl", 0, 0};
    const auto* animation = manager.animation(spec); REQUIRE(animation);
    CHECK(loader.initializations == 1); CHECK(loader.loads == 0);
    const auto* pose = manager.frame(spec); REQUIRE(pose);
    CHECK(manager.frame(spec) == pose); CHECK(loader.loads == 1);
    spec.frameIndex = 1; REQUIRE(manager.frame(spec)); CHECK(loader.loads == 2);
    spec.frameIndex = 2; CHECK_FALSE(manager.frame(spec)); CHECK_FALSE(manager.frame(spec));
    CHECK(loader.loads == 3);
    CHECK(manager.animation(spec) == animation);
    manager.clear(); REQUIRE(manager.animation(spec)); CHECK(loader.initializations == 2);
}

TEST_CASE("Model animation playback follows elapsed time and changes speed continuously", "[model-animation]") {
    const EntityModel::Animation clip{"idle", {0, 1, 2}, {.1, .2, .3}};
    Renderer::EntityModelAnimationPlayback playback;
    CHECK(playback.frame(clip, 100, 1) == 0);
    CHECK(playback.frame(clip, 100.11, 1) == 1);
    CHECK(playback.frame(clip, 100.17, 2) == 2);
    CHECK(playback.frame(clip, 100.27, 0) == 2);
    CHECK(playback.frame(clip, 100.38, 1) == 0);
    playback = {};
    CHECK(playback.frame(clip, 200, 1) == 0);
}
} // namespace TrenchBroom::Assets
