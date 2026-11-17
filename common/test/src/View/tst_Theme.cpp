#include "Error.h"
#include "IO/PathQt.h"
#include "IO/VectorIcon.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "QSSBuilder.h"
#include "ThemeManager.h"
#include "View/FontUtils.h"

#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QScopeGuard>
#include <QTemporaryDir>

#include <catch2/catch.hpp>
#include <limits>

namespace TrenchBroom {
namespace {
    std::filesystem::path themeResource(const char* file) {
        return std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path().parent_path().parent_path() / "app/resources/stylesheets" / file;
    }
    void writeThemeFixture(const QString& path, const QByteArray& bytes) {
        QFile file{path};
        REQUIRE(file.open(QIODevice::WriteOnly));
        REQUIRE(file.write(bytes) == bytes.size());
    }
    ThemeSnapshot testThemeSnapshot() {
        const QFont font{"Inter"};
        return ThemeManager::preferenceValues(font, font, font);
    }
} // namespace

TEST_CASE("Theme compiler resolves exact modern and legacy names", "[theme]") {
    QSSBuilder builder;
    builder.setText(R"(/* ${color.unknown} #UNKNOWN_COLOR */
QWidget#TEXT_COLOR { color: ${color.text}; background: #TEXT_COLOR; border-color: #ABCDEF; }
QLabel { color: ${color.text.detail}; font-family: ${family.ui}; }
QFrame { image: url("icons.svg#UNKNOWN_ID"); }
)",
                    "example.qss");
    auto values = ThemeValues{{"color.text", ThemeValue::color(Qt::red)}, {"color.text.detail", ThemeValue::color(Qt::blue)}, {"family.ui", ThemeValue::family("Font with spaces")}};
    REQUIRE(builder.update(values));
    CHECK(builder.changed());
    CHECK(builder.getRenderedText().contains("QWidget#TEXT_COLOR"));
    CHECK(builder.getRenderedText().contains("color: rgba(255, 0, 0, 255)"));
    CHECK(builder.getRenderedText().contains("background: rgba(255, 0, 0, 255)"));
    CHECK(builder.getRenderedText().contains("color: rgba(0, 0, 255, 255)"));
    CHECK(builder.getRenderedText().contains("font-family: \"Font with spaces\""));
    CHECK(builder.getRenderedText().contains("#ABCDEF"));
    CHECK(builder.getRenderedText().contains("icons.svg#UNKNOWN_ID"));
    CHECK(builder.getText().contains("${color.text}"));
}

TEST_CASE("Theme compiler errors retain the last valid stylesheet with locations", "[theme]") {
    QSSBuilder builder;
    auto values = ThemeValues{{"color.text", ThemeValue::color(Qt::red)}};
    builder.setText("QWidget { color: ${color.text}; }", "example.qss");
    REQUIRE(builder.update(values));
    const auto valid = builder.getRenderedText();
    for (const auto& source : {"QWidget {\n color: ${color.missing};\n}", "QWidget { color: #TEXT_COLOR_UNKNOWN; }", "QWidget { font-family: \"#UI_FONT_FAMLIY\"; }", "QWidget { color: ${color.text; }", "QWidget { /* broken",
                               "QWidget { font-family: \"broken; }"}) {
        CAPTURE(source);
        builder.setText(source, "example.qss");
        CHECK_FALSE(builder.update(values));
        REQUIRE_FALSE(builder.errors().isEmpty());
        CHECK(builder.errors().front().startsWith("example.qss:"));
        CHECK(builder.getRenderedText() == valid);
        CHECK_FALSE(builder.changed());
    }
    builder.setText("QWidget {\n color: ${color.missing};\n}", "example.qss");
    CHECK_FALSE(builder.update(values));
    CHECK(builder.errors().front().startsWith("example.qss:2:9:"));
}

TEST_CASE("Theme compilation only repeats for used values or changed templates", "[theme]") {
    auto values = ThemeValues{{"color.text", ThemeValue::color(Qt::red)}};
    QSSBuilder builder;
    builder.setText("QWidget { color: ${color.text}; }");
    REQUIRE(builder.update(values));
    CHECK(builder.compilationCount() == 1);
    REQUIRE(builder.update(values));
    CHECK_FALSE(builder.changed());
    CHECK(builder.compilationCount() == 1);
    values.insert("color.unused", ThemeValue::color(Qt::blue));
    REQUIRE(builder.update(values));
    CHECK(builder.compilationCount() == 1);
    values["color.text"] = ThemeValue::color(Qt::green);
    REQUIRE(builder.update(values));
    CHECK(builder.changed());
    CHECK(builder.compilationCount() == 2);
    builder.setText("");
    REQUIRE(builder.update(values));
    CHECK(builder.changed());
    const auto count = builder.compilationCount();
    REQUIRE(builder.update(values));
    CHECK(builder.compilationCount() == count);
    CHECK_FALSE(builder.changed());
}

TEST_CASE("Existing stylesheet files retain BOM-based Unicode decoding", "[theme]") {
    QTemporaryDir folder;
    const auto source = QStringLiteral("/* Größe */ QLabel { color: ${color.text}; }");
    auto utf16 = QByteArray::fromHex("fffe");
    for (const auto c : source) {
        utf16 += char(c.unicode() & 0xff);
        utf16 += char(c.unicode() >> 8);
    }
    for (const auto& bytes : {QByteArray::fromHex("efbbbf") + source.toUtf8(), utf16}) {
        const auto file = folder.filePath("legacy.qss");
        writeThemeFixture(file, bytes);
        QSSBuilder builder;
        REQUIRE(builder.loadFile(IO::pathFromQString(file)));
        CHECK(builder.getText() == source);
        REQUIRE(builder.update({{"color.text", ThemeValue::color(Qt::red)}}));
        CHECK(builder.getRenderedText().contains("rgba(255, 0, 0, 255)"));
    }
}

TEST_CASE("Theme values reject invalid types and escape font families", "[theme]") {
    CHECK_FALSE(ThemeValue::color(QColor{}).isValid());
    CHECK_FALSE(ThemeValue::length(-1).isValid());
    CHECK_FALSE(ThemeValue::length(std::numeric_limits<double>::infinity()).isValid());
    CHECK_FALSE(ThemeValue::number(std::numeric_limits<double>::quiet_NaN()).isValid());
    CHECK_FALSE(ThemeValue::family("bad\nfamily").isValid());
    CHECK_FALSE(ThemeValue::keyword("normal; color:red").isValid());
    CHECK_FALSE((ThemeValue{ThemeValue::Type::Length, QString{"12px"}}).isValid());
    CHECK_FALSE((ThemeValue{ThemeValue::Type::FontWeight, 42.5}).isValid());
    CHECK(ThemeValue::color(QColor{10, 20, 30, 40}).toStyleSheet() == "rgba(10, 20, 30, 40)");
    CHECK(ThemeValue::length(2.5).toStyleSheet() == "2.5px");
    CHECK(ThemeValue::number(1e-12).toStyleSheet() == "0.000000000001");
    CHECK(ThemeValue::length(1e-7).toStyleSheet() == "0.0000001px");
    QSSBuilder builder;
    builder.setText("QLabel { font-family: ${family.ui}; } QFrame { font-family: \"#UI_FONT_FAMILY\"; }");
    const QString family = "A \"family\"; color:red; /*";
    REQUIRE(builder.update({{"family.ui", ThemeValue::family(family)}}));
    CHECK(builder.getRenderedText().count("\"A \\\"family\\\"; color:red; /*\"") == 2);
    builder.setText("QLabel { color: ${color.text}; }");
    CHECK_FALSE(builder.update({{"color.text", ThemeValue::color(QColor{})}}));
}

TEST_CASE("Theme definitions resolve typed references and user overrides", "[theme]") {
    ThemeDefinitions definitions;
    QStringList errors;
    REQUIRE(ThemeManager::readDefinition(R"({"version":1,"colors":{"panel":"#123456","label":"${color.text}"},"lengths":{"radius.custom":9}})", "default.json", definitions, errors));
    REQUIRE(ThemeManager::readDefinition(R"({"version":1,"colors":{"panel":"#abcdef","accent":"${color.panel}"}})", "personal.json", definitions, errors));
    auto snapshot = testThemeSnapshot();
    const auto text = snapshot.values.value("color.text");
    REQUIRE(ThemeManager::resolve(snapshot, definitions, errors));
    CHECK(snapshot.values.value("color.panel").value.value<QColor>() == QColor{"#abcdef"});
    CHECK(snapshot.values.value("color.accent") == snapshot.values.value("color.panel"));
    CHECK(snapshot.values.value("color.label") == text);
    CHECK(snapshot.values.value("length.radius.custom").value.toDouble() == 9);
}

TEST_CASE("Broken theme definitions do not modify a valid snapshot", "[theme]") {
    for (const auto& bytes :
         {R"({"version":1,"colors":{"a":"${color.b}","b":"${color.a}"}})", R"({"version":1,"colors":{"a":"${color.absent}"}})", R"({"version":1,"colors":{"a":"${length.radius.menu}"}})",
          R"({"version":1,"colors":{"a":"not a color"}})", R"({"version":1,"lengths":{"bad":"12px"}})", R"({"version":1,"lengths":{"bad":-5}})",
          R"({"version":1,"fontWeights":{"bad":12.5}})", R"({"version":1,"numbers":{"icon.disabledOpacity":2}})",
          R"({"version":1,"numbers":{"icon.brightness":-0.1}})", R"({"version":1,"numbers":{"icon.brightness":1.1}})", R"({"version":1,"colors":{"text":"#ffffff"}})"}) {
        CAPTURE(bytes);
        auto snapshot = testThemeSnapshot();
        const auto original = snapshot.values;
        ThemeDefinitions definitions;
        QStringList errors;
        REQUIRE(ThemeManager::readDefinition(bytes, "broken.json", definitions, errors));
        CHECK_FALSE(ThemeManager::resolve(snapshot, definitions, errors));
        REQUIRE_FALSE(errors.isEmpty());
        CHECK(errors.front().startsWith("broken.json:"));
        CHECK(snapshot.values == original);
    }
}

TEST_CASE("Theme JSON diagnostics reject schema and syntax errors", "[theme]") {
    for (const auto& bytes : {"{\n wrong }", "[]", R"({"version":2})", R"({"version":1,"colours":{}})", R"({"version":1,"colors":[]})", R"({"version":1,"colors":{"wrong name":"red"}})"}) {
        ThemeDefinitions definitions;
        QStringList errors;
        CHECK_FALSE(ThemeManager::readDefinition(bytes, "bad.json", definitions, errors));
        REQUIRE_FALSE(errors.isEmpty());
        CHECK(errors.front().startsWith("bad.json:"));
        CHECK(definitions.isEmpty());
    }
}

TEST_CASE("The shipped stylesheet and theme compile with existing preferences", "[theme]") {
    const TemporarilySetPref minimum{Preferences::UIFontMinimumSize, 17};
    const TemporarilySetPref size{Preferences::UIFontSize, 13};
    const TemporarilySetPref opacity{Preferences::UIDetailTextOpacity, 0.6f};
    const QFont font{"Inter"};
    ThemeSnapshot theme;
    QStringList errors;
    REQUIRE(ThemeManager::prepare(font, font, font, themeResource("theme.json"), {}, theme, errors));
    QSSBuilder builder;
    REQUIRE(builder.loadFile(themeResource("base.qss")));
    REQUIRE(builder.update(theme.values));
    CHECK(theme.values.value("length.font.ui").value.toDouble() == 17);
    CHECK(theme.values.value("length.font.small").value.toDouble() == 17);
    CHECK(theme.values.value("color.window").value.value<QColor>() == theme.palette.color(QPalette::Window));
    CHECK(theme.values.value("color.text.detail").value.value<QColor>().alphaF() == Approx(0.6).margin(0.005));
    CHECK(theme.values.value("color.source.background").value.value<QColor>() == QColor{"#282b30"});
    const auto iconBrightness = theme.values.value("number.icon.brightness").value.toDouble();
    CHECK(iconBrightness >= 0);
    CHECK(iconBrightness <= 1);
    CHECK(builder.getRenderedText().contains("qproperty-brightness: " + QString::number(iconBrightness)));
    CHECK(builder.getRenderedText().contains("font-size: 17px"));
    CHECK(ThemeManager::isRelevantPreference(Preferences::UIFontSize.path()));
    CHECK(ThemeManager::isRelevantPreference(Preferences::LayerTreeEntityIconColor.path()));
    CHECK_FALSE(ThemeManager::isRelevantPreference(Preferences::GridAlpha.path()));
}

TEST_CASE("Personal theme files are optional and invalid overrides are transactional", "[theme]") {
    QTemporaryDir folder;
    const QFont font{"Inter"};
    const auto overrides = IO::pathFromQString(folder.filePath("theme.json"));
    ThemeSnapshot theme;
    QStringList errors;
    REQUIRE(ThemeManager::prepare(font, font, font, themeResource("theme.json"), overrides, theme, errors));
    writeThemeFixture(IO::pathAsQString(overrides), R"({"version":1,"colors":{"source.background":"#102030"},"lengths":{"radius.menu":19}})");
    REQUIRE(ThemeManager::prepare(font, font, font, themeResource("theme.json"), overrides, theme, errors));
    CHECK(theme.values.value("color.source.background").value.value<QColor>() == QColor{"#102030"});
    CHECK(theme.values.value("length.radius.menu").value.toDouble() == 19);
    const auto valid = theme.values;
    writeThemeFixture(IO::pathAsQString(overrides), R"({"version":1,"colors":{"source.background":"bad"}})");
    CHECK_FALSE(ThemeManager::prepare(font, font, font, themeResource("theme.json"), overrides, theme, errors));
    CHECK(theme.values == valid);
}

TEST_CASE("Theme publication reports only actual changes", "[theme]") {
    ThemeManager manager;
    auto theme = testThemeSnapshot();
    QStringList changed;
    int notifications = 0;
    auto connection = manager.themeDidChangeNotifier.connect([&](const auto& names) {
        changed = names;
        ++notifications;
    });
    REQUIRE(manager.publish(theme));
    CHECK(notifications == 1);
    CHECK(manager.revision() == 1);
    CHECK_FALSE(manager.publish(theme));
    CHECK(notifications == 1);
    theme.values.insert("color.test", ThemeValue::color(Qt::red));
    REQUIRE(manager.publish(theme));
    CHECK(changed == QStringList{"color.test"});
    CHECK(manager.revision() == 2);
    theme.values.remove("color.test");
    REQUIRE(manager.publish(theme));
    CHECK(changed == QStringList{"color.test"});
}

TEST_CASE("Existing vector icons follow theme colors without replacing QIcons", "[theme][vector-icon]") {
    auto& appearance = IO::IconAppearance::instance();
    const auto oldNormalize = appearance.normalizeBrightness();
    const auto oldBrightness = appearance.brightness();
    const auto restoreAppearance = qScopeGuard([&] { appearance.setNormalizeBrightness(oldNormalize); appearance.setBrightness(oldBrightness); });
    appearance.setNormalizeBrightness(true);
    appearance.setBrightness(0.75);
    auto& manager = ThemeManager::instance();
    const auto original = manager.current();
    const auto restore = qScopeGuard([&] { manager.publish(original); });
    QTemporaryDir folder;
    const auto file = folder.filePath("test.svg");
    writeThemeFixture(file, R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><rect width="16" height="16" fill="white"/></svg>)");
    auto theme = testThemeSnapshot();
    theme.values.insert("color.test", ThemeValue::color(Qt::red));
    manager.publish(theme);
    const auto icon = IO::VectorIcon{IO::pathFromQString(file)}.themedIcon("color.test", Qt::green);
    const auto key = icon.cacheKey();
    CHECK(icon.pixmap(16).toImage().pixelColor(8, 8) == QColor(191, 0, 0));
    theme.values["color.test"] = ThemeValue::color(Qt::blue);
    manager.publish(theme);
    CHECK(icon.cacheKey() == key);
    CHECK(icon.pixmap(16).toImage().pixelColor(8, 8) == QColor(0, 0, 191));
    theme.values.remove("color.test");
    manager.publish(theme);
    CHECK(icon.pixmap(16).toImage().pixelColor(8, 8) == QColor(0, 191, 0));
}
} // namespace TrenchBroom
