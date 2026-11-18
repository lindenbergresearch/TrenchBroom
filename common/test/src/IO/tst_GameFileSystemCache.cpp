#include "Error.h"
#include <kdl/result.h>
#include "IO/DiskFileSystem.h"
#include "IO/PathInfo.h"
#include "IO/PathQt.h"
#include "Logger.h"
#include "Model/GameConfig.h"
#include "Model/GameFileSystem.h"

#include <QTemporaryDir>
#include <catch2/catch.hpp>
#include <fstream>

namespace TrenchBroom::IO {
TEST_CASE("Game asset reload refreshes backing directories before shader discovery", "[disk-cache]") {
    QTemporaryDir temporary;
    REQUIRE(temporary.isValid());
    const auto root = pathFromQString(temporary.path());
    const auto write = [&](const std::filesystem::path& path, const std::string& text) {
        std::filesystem::create_directories((root / path).parent_path());
        std::ofstream stream{root / path, std::ios::binary};
        stream << text;
        REQUIRE(stream.good());
    };
    write("id1/textures/old.png", "dummy");
    write("id1/scripts/old.shader", "textures/old { qer_editorimage textures/old.png }");
    const auto fixture = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path() / "fixture/IO/ArchiveNames/non_utf8_filename.zip";
    std::filesystem::copy_file(fixture, root / "id1/pak0.pk3");
    Model::GameConfig config;
    config.fileSystemConfig.searchPath = "id1";
    config.fileSystemConfig.packageFormat.extensions = {".pk3"};
    config.fileSystemConfig.packageFormat.format = "zip";
    config.textureConfig.root = "textures";
    config.textureConfig.shaderSearchPath = "scripts";
    NullLogger logger;
    Model::GameFileSystem fs;
    fs.initialize(config, root, {}, logger);
    REQUIRE(fs.pathInfo("textures/old") == PathInfo::File);
    write("id1/textures/new.png", "dummy");
    write("id1/scripts/new.shader", "textures/new { qer_editorimage textures/new.png }");
    CHECK(fs.pathInfo("textures/new.png") == PathInfo::Unknown);
    for (int i = 0; i < 3; ++i) {
        REQUIRE(fs.reloadShaders().is_success());
        CHECK(fs.pathInfo("textures/new.png") == PathInfo::File);
        CHECK(fs.pathInfo("textures/new") == PathInfo::File);
        CHECK(fs.pathInfo("textures/old") == PathInfo::File);
    }
}
} // namespace TrenchBroom::IO
