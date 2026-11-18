#include "Error.h"
#include <kdl/result.h>
#include "IO/DiskFileSystem.h"
#include "IO/File.h"
#include "IO/PathInfo.h"
#include "IO/PathQt.h"
#include "IO/Reader.h"
#include "IO/TraversalMode.h"
#include "IO/VirtualFileSystem.h"

#include <QScopeGuard>
#include <QTemporaryDir>
#include <catch2/catch.hpp>
#include <fstream>
#include <future>

namespace TrenchBroom::IO {
namespace {
    struct DiskCacheFixture {
        QTemporaryDir directory;
        std::filesystem::path root = pathFromQString(directory.path());
        DiskCacheFixture() { REQUIRE(directory.isValid()); }
        void put(const std::filesystem::path& name, const std::string& content = "data") {
            std::filesystem::create_directories((root / name).parent_path());
            std::ofstream stream{root / name, std::ios::binary};
            stream << content;
            REQUIRE(stream.good());
        }
    };
    std::string cachedFileContents(const FileSystem& fs, const std::filesystem::path& name) {
        auto file = fs.openFile(name);
        REQUIRE(file.is_success());
        return file.value()->reader().readString(file.value()->size());
    }
} // namespace
TEST_CASE("Disk cache resolves original casing and refreshes external changes", "[disk-cache]") {
    DiskCacheFixture env;
    env.put("Textures/Stone.WAL", "first");
    DiskFileSystem fs{env.root};
    CHECK(fs.pathInfo("") == PathInfo::Directory);
    CHECK(fs.pathInfo(".") == PathInfo::Directory);
    CHECK(fs.pathInfo("textures/") == PathInfo::Directory);
    CHECK(fs.pathInfo("../outside") == PathInfo::Unknown);
    CHECK_FALSE(fs.makeAbsolute("../outside").is_success());
    CHECK(fs.pathInfo("textures/../Textures/Stone.WAL") == PathInfo::File);
    CHECK(cachedFileContents(fs, "TEXTURES/stone.wal") == "first");
    auto found = fs.find("TEXTURES/", TraversalMode::Flat);
    REQUIRE(found.is_success());
    CHECK(found.value() == std::vector<std::filesystem::path>{"Textures/Stone.WAL"});
    env.put("Textures/Stone.WAL", "updated");
    CHECK(cachedFileContents(fs, "textures/stone.wal") == "updated"); // Only directory metadata is cached.
    env.put("Textures/new.txt");
    CHECK(fs.pathInfo("Textures/new.txt") == PathInfo::Unknown);
    std::filesystem::rename(env.root / "Textures/Stone.WAL", env.root / "Textures/Renamed.WAL");
    CHECK(fs.pathInfo("Textures/Stone.WAL") == PathInfo::File);
    CHECK_FALSE(fs.openFile("Textures/Stone.WAL").is_success());
    REQUIRE(fs.reload().is_success());
    CHECK(fs.pathInfo("Textures/Stone.WAL") == PathInfo::Unknown);
    CHECK(fs.pathInfo("textures/renamed.wal") == PathInfo::File);
    CHECK(fs.pathInfo("Textures/new.txt") == PathInfo::File);
    CHECK(cachedFileContents(fs, "Textures/Renamed.WAL") == "updated");
}
TEST_CASE("Disk cache handles absent roots and filesystem moves", "[disk-cache]") {
    DiskCacheFixture env;
    DiskFileSystem missing{env.root / "not-yet-created"};
    CHECK(missing.pathInfo("") == PathInfo::Unknown);
    CHECK_FALSE(missing.find("", TraversalMode::Recursive).is_success());
    env.put("not-yet-created/a.txt", "live");
    REQUIRE(missing.reload().is_success());
    CHECK(cachedFileContents(missing, "a.txt") == "live");
    DiskFileSystem moved{std::move(missing)};
    CHECK(cachedFileContents(moved, "a.txt") == "live");
    CHECK(missing.pathInfo("") == PathInfo::Unknown);
    DiskFileSystem assigned{env.root};
    assigned = std::move(moved);
    CHECK(cachedFileContents(assigned, "a.txt") == "live");
    std::filesystem::remove_all(env.root / "not-yet-created");
    REQUIRE(assigned.reload().is_success());
    CHECK(assigned.pathInfo("") == PathInfo::Unknown);
    CHECK_FALSE(assigned.openFile("a.txt").is_success());
}
TEST_CASE("Writable disk cache tracks atomic writes copies moves and deletions", "[disk-cache]") {
    DiskCacheFixture env;
    WritableDiskFileSystem fs{env.root};
    REQUIRE(fs.createDirectory("Folder").is_success());
    CHECK(fs.pathInfo("Folder") == PathInfo::Directory);
    REQUIRE(fs.createFileAtomic("Folder/config.cfg", "one").is_success());
    CHECK(cachedFileContents(fs, "folder/CONFIG.cfg") == "one");
    CHECK(fs.pathInfo("Folder/config.cfg.tmp") == PathInfo::Unknown);
    REQUIRE(fs.createFileAtomic("Folder/config.cfg", "two").is_success());
    CHECK(cachedFileContents(fs, "Folder/config.cfg") == "two");
    REQUIRE(fs.copyFile("Folder/config.cfg", "copy.cfg").is_success());
    CHECK(cachedFileContents(fs, "copy.cfg") == "two");
    REQUIRE(fs.moveFile("copy.cfg", "Folder").is_success());
    CHECK(fs.pathInfo("copy.cfg") == PathInfo::Unknown);
    CHECK(cachedFileContents(fs, "Folder/copy.cfg") == "two");
    REQUIRE(fs.deleteFile("Folder/copy.cfg").is_success());
    CHECK(fs.pathInfo("Folder/copy.cfg") == PathInfo::Unknown);
    CHECK_FALSE(fs.createFile("missing/parent/file", "bad").is_success());
    CHECK(cachedFileContents(fs, "Folder/config.cfg") == "two");
}
TEST_CASE("Virtual cache reload preserves mount precedence and case-insensitive names", "[disk-cache]") {
    DiskCacheFixture base, overlay;
    base.put("sample.txt", "base");
    overlay.put("SAMPLE.TXT", "overlay");
    VirtualFileSystem fs;
    fs.mount({}, std::make_unique<DiskFileSystem>(base.root));
    fs.mount({}, std::make_unique<DiskFileSystem>(overlay.root));
    CHECK(cachedFileContents(fs, "sample.txt") == "overlay");
    auto found = fs.find("", TraversalMode::Flat);
    REQUIRE(found.is_success());
    CHECK(found.value() == std::vector<std::filesystem::path>{"SAMPLE.TXT"});
    overlay.put("new.txt");
    CHECK(fs.pathInfo("new.txt") == PathInfo::Unknown);
    REQUIRE(fs.reload().is_success());
    CHECK(fs.pathInfo("new.txt") == PathInfo::File);

    DiskCacheFixture writable;
    WritableVirtualFileSystem userFs{VirtualFileSystem{}, std::make_unique<WritableDiskFileSystem>(writable.root)};
    REQUIRE(userFs.createFileAtomic("profile.cfg", "profile").is_success());
    CHECK(cachedFileContents(userFs, "profile.cfg") == "profile");
    writable.put("external.cfg");
    REQUIRE(userFs.reload().is_success());
    CHECK(userFs.pathInfo("external.cfg") == PathInfo::File);
}
TEST_CASE("Disk cache follows directory aliases without following symlink cycles", "[disk-cache]") {
    DiskCacheFixture env;
    env.put("folder/file.txt");
    auto error = std::error_code{};
    std::filesystem::create_directory_symlink(env.root / "folder", env.root / "alias", error);
    if (error) {
        WARN("Directory symlinks are unavailable on this platform");
        return;
    }
    std::filesystem::create_directory_symlink(env.root, env.root / "folder/loop");
    std::filesystem::create_symlink(env.root / "missing", env.root / "broken");
    DiskFileSystem fs{env.root};
    CHECK(cachedFileContents(fs, "alias/file.txt") == "data");
    CHECK(fs.pathInfo("broken") == PathInfo::Unknown);
    auto found = fs.find("", TraversalMode::Recursive);
    REQUIRE(found.is_success());
    CHECK(found.value().size() == 6); // folder + alias, each with file and a non-recursed loop.
    CHECK(fs.pathInfo("folder/loop") == PathInfo::Directory);
    CHECK(fs.pathInfo("folder/loop/folder/file.txt") == PathInfo::Unknown);
}
TEST_CASE("Disk cache readers remain consistent during reload", "[disk-cache]") {
    DiskCacheFixture env;
    for (int i = 0; i < 24; ++i)
        env.put("folder/" + std::to_string(i) + ".txt");
    DiskFileSystem fs{env.root};
    std::vector<std::future<bool>> readers;
    for (int i = 0; i < 4; ++i)
        readers.push_back(std::async(std::launch::async, [&]() {
            for (int j = 0; j < 200; ++j) {
                if (fs.pathInfo("FOLDER/17.TXT") != PathInfo::File) return false;
                auto paths = fs.find("folder", TraversalMode::Flat);
                if (!paths.is_success() || paths.value().size() != 24) return false;
            }
            return true;
        }));
    for (int i = 0; i < 12; ++i)
        REQUIRE(fs.reload().is_success());
    for (auto& reader : readers)
        CHECK(reader.get());
}
} // namespace TrenchBroom::IO

#ifndef _WIN32
namespace TrenchBroom::IO {
TEST_CASE("Failed disk scans preserve the previous complete cache", "[disk-cache]") {
    DiskCacheFixture env;
    env.put("locked/previous.txt");
    DiskFileSystem fs{env.root};
    std::filesystem::permissions(env.root / "locked", std::filesystem::perms::none);
    const auto restore = qScopeGuard([&]() { std::filesystem::permissions(env.root / "locked", std::filesystem::perms::owner_all); });
    std::error_code error;
    std::filesystem::directory_iterator probe{env.root / "locked", error};
    if (!error) {
        WARN("This account bypasses directory permissions");
        return;
    }
    CHECK_FALSE(fs.reload().is_success());
    CHECK(fs.pathInfo("locked/previous.txt") == PathInfo::File);
}
} // namespace TrenchBroom::IO
#endif
