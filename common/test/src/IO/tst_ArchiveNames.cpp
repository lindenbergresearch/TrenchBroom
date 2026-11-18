#include "IO/DiskIO.h"
#include "IO/DkPakFileSystem.h"
#include "IO/File.h"
#include "IO/IdPakFileSystem.h"
#include "IO/PathInfo.h"
#include "IO/Reader.h"
#include "IO/TraversalMode.h"
#include "IO/WadFileSystem.h"
#include "IO/ZipFileSystem.h"
#include "kdl/path_utils.h"
#include "TestSourcePath.h"

#include <catch2/catch.hpp>

namespace TrenchBroom::IO {
namespace {
    std::filesystem::path archiveNameFixture(const char* name) { return testSourceRoot() / "common/test/fixture/IO/ArchiveNames" / name; }
    template <typename FS> void checkLegacyArchiveName(const char* fixture, const char* suffix) {
        auto file = Disk::openFile(archiveNameFixture(fixture));
        REQUIRE(file.is_success());
        auto result = createImageFileSystem<FS>(std::move(file).value());
        REQUIRE(result.is_success());
        auto fs = std::move(result).value();
#ifdef _WIN32
        auto expected = std::filesystem::path{std::wstring{L"fen\u00EAtre"}};
#else
        auto expected = std::filesystem::path{"fen\xEAtre"};
#endif
        expected += suffix;
        auto paths = fs->find("", TraversalMode::Flat);
        REQUIRE(paths.is_success());
        CHECK(paths.value() == std::vector<std::filesystem::path>{expected});
        REQUIRE(static_cast<const FileSystem&>(*fs).pathInfo(expected) == PathInfo::File);
        auto opened = fs->openFile(expected);
        REQUIRE(opened.is_success());
        CHECK(opened.value()->reader().readString(opened.value()->size()) == "hello");
        for (size_t i = 0; i < 4; ++i) {
            REQUIRE(fs->reload().is_success());
            REQUIRE(fs->openFile(expected).is_success());
            CHECK(fs->openFile(expected).value()->reader().readString(5) == "hello");
        }
    }
} // namespace
TEST_CASE("Legacy archive names are readable without encoding exceptions", "[archive-names]") {
    SECTION("WAD") { checkLegacyArchiveName<WadFileSystem>("non_utf8_entry_name.wad", "opaque.D"); }
    SECTION("Quake PAK") { checkLegacyArchiveName<IdPakFileSystem>("idpak_non_utf8_entry_name.pak", ".txt"); }
    SECTION("Daikatana PAK") { checkLegacyArchiveName<DkPakFileSystem>("dkpak_non_utf8_entry_name.pak", ".txt"); }
    SECTION("ZIP") { checkLegacyArchiveName<ZipFileSystem>("non_utf8_filename.zip", ".txt"); }
}
TEST_CASE("Path lookup folds ASCII while preserving Unicode and legacy bytes", "[archive-names]") {
    const auto unicode = std::filesystem::path{u8"Folder/\u65E5\u672C_\u00C4.TXT"};
    CHECK(kdl::path_to_lower(unicode) == std::filesystem::path{u8"folder/\u65E5\u672C_\u00C4.txt"});
    const auto parsed = kdl::parse_utf8_or_latin1_path("Folder/\xC3\xAA.TXT");
    CHECK(parsed == std::filesystem::path{u8"Folder/\u00EA.TXT"});
#ifdef _WIN32
    const auto legacy = std::filesystem::path{L"Folder/\u00EA.TXT"};
    const auto lower = std::filesystem::path{L"folder/\u00EA.txt"};
#else
    const auto legacy = std::filesystem::path{"Folder/\xEA.TXT"};
    const auto lower = std::filesystem::path{"folder/\xEA.txt"};
#endif
    CHECK(kdl::parse_utf8_or_latin1_path("Folder/\xEA.TXT") == legacy);
    CHECK(kdl::path_to_lower(legacy) == lower);
}
} // namespace TrenchBroom::IO
