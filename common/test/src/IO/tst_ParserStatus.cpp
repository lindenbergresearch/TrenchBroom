#include "IO/ParserStatus.h"
#include "IO/FgdParser.h"
#include "IO/DefParser.h"
#include "IO/EntParser.h"
#include "IO/Quake3ShaderParser.h"
#include "IO/WorldReader.h"
#include "Assets/EntityDefinition.h"
#include "Assets/Quake3Shader.h"
#include "Exceptions.h"
#include "Logger.h"
#include "Model/EntityProperties.h"
#include "Model/MapFormat.h"
#include "Model/WorldNode.h"

#include <QTemporaryDir>
#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>
#include "Catch2.h"

namespace TrenchBroom::IO {
namespace {
NullLogger silentLogger;
class CapturingStatus : public ParserStatus {
    void doProgress(double) override {}
    void doLog(LogLevel level, const std::string& text) override { messages.push_back({level, text}); }
public:
    struct Message { LogLevel level; std::string text; };
    std::vector<Message> messages;
    explicit CapturingStatus(std::string name = {}) : ParserStatus(silentLogger, name) {}
    const std::string& containing(const std::string& text) const {
        const auto found = std::find_if(messages.begin(), messages.end(), [&](const auto& m) { return m.text.find(text) != std::string::npos; });
        REQUIRE(found != messages.end());
        return found->text;
    }
};
void write(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path);
    file << text;
    REQUIRE(file.good());
}
}
TEST_CASE("Parser status owns exact source lines and durable diagnostic locations", "[parser-status]") {
    CapturingStatus status;
    std::string text = "first\n  original text\nlast";
    status.setSource(text, "test.map");
    const auto here = status.location(2, 3);
    CHECK(here.line == 2);
    CHECK(here.column == 3);
    CHECK(here.sourceName == "test.map");
    CHECK(here.sourceLine == "  original text");
    text.assign("changed");
    status.warn(2, 3, "Look here");
    REQUIRE(status.messages.size() == 1);
    CHECK(status.messages[0].text == "test.map:2:3: Look here\n2 |   original text\n  |   ^");
    status.setSource("another", "other.fgd");
    status.error(here, "Saved location");
    CHECK(status.messages.back().text == "test.map:2:3: Saved location\n2 |   original text\n  |   ^");
}
TEST_CASE("Source excerpts handle all tokenizer line endings empty lines and missing positions", "[parser-status]") {
    for (const std::string newline : {"\n", "\r\n", "\r"}) {
        CapturingStatus status("file");
        status.setSource("one" + newline + newline + "three" + newline);
        CHECK(status.location(1).sourceLine == "one");
        CHECK(status.location(2).hasSourceLine);
        CHECK(status.location(2).sourceLine.empty());
        CHECK(status.location(3).sourceLine == "three");
        CHECK(status.location(4).hasSourceLine);
        CHECK(status.location(4).sourceLine.empty());
        CHECK_FALSE(status.location(5).hasSourceLine);
        CHECK_FALSE(status.location(0).hasSourceLine);
        status.warn(3, "Whole line");
        CHECK(status.messages.back().text == "file:3: Whole line\n3 | three");
        status.warn(3, std::numeric_limits<size_t>::max(), "Past the end");
        CHECK(status.messages.back().text.ends_with("\n  |      ^"));
    }
    CapturingStatus legacy("legacy");
    legacy.warn(3, 7, "No source");
    CHECK(legacy.messages.back().text == "legacy: No source (line 3, column 7)");
    legacy.info("No location");
    CHECK(legacy.messages.back().text == "legacy: No location (unknown position)");
}
TEST_CASE("Caret prefixes preserve tabs and account for UTF-8 byte columns", "[parser-status]") {
    CapturingStatus status;
    const std::string text = "\té = 1";
    status.setSource(text);
    status.warn(1, text.find('1') + 1, "Value");
    CHECK(status.messages.back().text == "line 1, column 7: Value\n1 | \té = 1\n  | \t    ^");
    const std::string combining = "e\xcc\x81 = 2";
    status.setSource(combining);
    status.warn(1, combining.find('2') + 1, "Value");
    CHECK(status.messages.back().text.ends_with("\n  |     ^"));
}
TEST_CASE("Nested parser sources restore after successful operations and exceptions", "[parser-status]") {
    CapturingStatus status;
    status.setSource("outer", "outer.fgd");
    {
        const auto source = status.scopedSource("inner", "inner.fgd");
        CHECK(status.location(1).sourceName == "inner.fgd");
        {
            const auto shared = status.scopedSource("inner");
            CHECK(status.location(1).sourceName == "inner.fgd");
        }
    }
    CHECK(status.location(1).sourceName == "outer.fgd");
    CHECK_THROWS_AS(([&] {
        const auto source = status.scopedSource("bad", "bad.fgd");
        throw std::runtime_error("test");
    }()), std::runtime_error);
    CHECK(status.location(1).sourceLine == "outer");
}
TEST_CASE("Parser exceptions retain structured positions and gain source context exactly once", "[parser-status]") {
    CapturingStatus status;
    status.setSource("one\n  broken", "broken.map");
    const ParserException original(2, 3, "Expected brace");
    const auto enriched = status.withContext(original);
    CHECK(enriched.line() == 2);
    CHECK(enriched.column() == 3);
    CHECK(enriched.hasSourceContext());
    CHECK(std::string(enriched.what()) == "broken.map:2:3: Expected brace\n2 |   broken\n  |   ^");
    status.setSource("different", "other.map");
    CHECK(std::string(status.withContext(enriched).what()) == enriched.what());
    status.setSource("one\n  broken", "broken.map");
    CHECK_THROWS_WITH(status.errorAndThrow(2, 3, "Stopped"), "broken.map:2:3: Stopped\n2 |   broken\n  |   ^");
    REQUIRE(status.messages.size() == 1);
    CHECK(status.messages.front().level == LogLevel::Error);
}
TEST_CASE("FGD includes keep original lines for immediate and deferred diagnostics", "[parser-status]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const auto root = std::filesystem::path(folder.path().toStdString());
    const std::string child = "@PointClass base(Missing) = child [\n value(string) : \"First\"\n value(string) : \"Second\"\n]\n";
    const std::string main = "@include \"child.fgd\"\n@PointClass = root [\n custom(Unrecognized) : \"Label\"\n]\n";
    write(root / "child.fgd", child);
    write(root / "main.fgd", main);
    CapturingStatus status;
    FgdParser parser(main, Color{1, 1, 1, 1}, root / "main.fgd");
    const auto definitions = parser.parseDefinitions(status);
    CHECK(definitions.size() == 2);
    const auto& duplicate = status.containing("Skipping duplicate property definition");
    CHECK(duplicate.find("child.fgd:3:") != std::string::npos);
    CHECK(duplicate.find("3 |  value(string) : \"Second\"") != std::string::npos);
    const auto& inheritance = status.containing("No matching super class");
    CHECK(inheritance.find("child.fgd:1:13:") != std::string::npos);
    CHECK(inheritance.find("1 | @PointClass base(Missing) = child [") != std::string::npos);
    const auto& property = status.containing("Unknown property definition type");
    CHECK(property.find("main.fgd:3:") != std::string::npos);
    CHECK(property.find("3 |  custom(Unrecognized)") != std::string::npos);
}
TEST_CASE("Syntax errors in included files keep their child context when unwinding", "[parser-status]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const auto root = std::filesystem::path(folder.path().toStdString());
    write(root / "child.fgd", "@PointClass = [\n");
    CapturingStatus status;
    status.setSource("previous", "previous.txt");
    FgdParser parser("@include \"child.fgd\"\n", Color{1, 1, 1, 1}, root / "main.fgd");
    try {
        parser.parseDefinitions(status);
        FAIL("Expected invalid child definition to throw");
    } catch (const ParserException& e) {
        CHECK(e.hasSourceContext());
        CHECK(std::string(e.what()).find("child.fgd:1:") != std::string::npos);
        CHECK(std::string(e.what()).find("1 | @PointClass = [") != std::string::npos);
    }
    CHECK(status.location(1).sourceName == "previous.txt");
}
TEST_CASE("Map diagnostics retain source through parsing and node validation", "[parser-status]") {
    const std::string text = "{\n\"classname\" \"worldspawn\"\n\"classname\" \"other\"\n}\n{\n\"classname\" \"light\"\n\"_tb_layer\" \"invalid\"\n}\n";
    CapturingStatus status("test.map");
    WorldReader reader(text, Model::MapFormat::Standard, Model::EntityPropertyConfig{});
    const auto world = reader.read(vm::bbox3{32768.0}, status);
    REQUIRE(world);
    CHECK(status.containing("Ignoring duplicate entity property").find("3 | \"classname\" \"other\"") != std::string::npos);
    CHECK(status.containing("Adding object to default layer").find("5 | {") != std::string::npos);
    CHECK(status.location(3).sourceLine == "\"classname\" \"other\"");
}
TEST_CASE("Shader warnings include the original directive and parameter position", "[parser-status]") {
    CapturingStatus status("effects.shader");
    Quake3ShaderParser parser("textures/test\n{\n {\n  blendFunc BAD GL_ONE\n }\n}\n");
    CHECK(parser.parse(status).size() == 1);
    const auto& warning = status.containing("Unknown blendFunc source factor");
    CHECK(warning.find("effects.shader:4:13:") != std::string::npos);
    CHECK(warning.find("4 |   blendFunc BAD GL_ONE\n  |             ^") != std::string::npos);
}
TEST_CASE("Map syntax exceptions include the offending original text", "[parser-status]") {
    CapturingStatus status("broken.map");
    WorldReader reader("  invalid\n", Model::MapFormat::Standard, Model::EntityPropertyConfig{});
    try {
        reader.read(vm::bbox3{32768.0}, status);
        FAIL("Expected invalid map syntax to throw");
    } catch (const ParserException& e) {
        CHECK(e.line() == 1);
        CHECK(e.column() == 3);
        CHECK(e.hasSourceContext());
        CHECK(std::string(e.what()).find("broken.map:1:3:") != std::string::npos);
        CHECK(std::string(e.what()).find("1 |   invalid\n  |   ^") != std::string::npos);
    }
}
TEST_CASE("ENT parser warnings use the original XML line without inventing a column", "[parser-status]") {
    CapturingStatus status("entities.ent");
    EntParser parser("<classes>\n<point name=\"test\">\n<real key=\"speed\" name=\"Speed\" value=\"x\"/>\n</point>\n</classes>\n", Color{1, 1, 1, 1});
    parser.parseDefinitions(status);
    REQUIRE_FALSE(status.messages.empty());
    CHECK(status.messages.back().text.find("3 | <real key=\"speed\"") != std::string::npos);
}
} // namespace TrenchBroom::IO
