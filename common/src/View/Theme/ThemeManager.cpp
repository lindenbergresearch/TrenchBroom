#include "View/Theme/ThemeManager.h"

#include "IO/PathQt.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "View/Preferences/FontUtils.h"
#include "View/Theme/QtUtils.h"
#include "View/Theme/ViewConstants.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <functional>

namespace TrenchBroom {
ThemeValue ThemeValue::color(const QColor &value) {
    return {Type::Color, value};
}

ThemeValue ThemeValue::length(qreal value) {
    return {Type::Length, value};
}

ThemeValue ThemeValue::number(qreal value) {
    return {Type::Number, value};
}

ThemeValue ThemeValue::family(const QString &value) {
    return {Type::FontFamily, value};
}

ThemeValue ThemeValue::weight(int value) {
    return {Type::FontWeight, value};
}

ThemeValue ThemeValue::keyword(const QString &value) {
    return {Type::Keyword, value};
}

bool ThemeValue::isValid() const {
    const auto numeric =
        value.typeId() == QMetaType::Double || value.typeId() == QMetaType::Int || value.typeId() == QMetaType::UInt ||
        value.typeId() == QMetaType::LongLong || value.typeId() == QMetaType::ULongLong;
    switch (type) {
        case Type::Color:
            return value.typeId() == QMetaType::QColor && value.value<QColor>().isValid();
        case Type::Length:
            return numeric && std::isfinite(value.toDouble()) && value.toDouble() >= 0 && value.toDouble() <= 4096;
        case Type::Number:
            return numeric && std::isfinite(value.toDouble()) && std::abs(value.toDouble()) <= 1e6;
        case Type::FontWeight:
            return numeric && std::isfinite(value.toDouble()) && std::floor(value.toDouble()) == value.toDouble() &&
                   value.toDouble() >= 0 && value.toDouble() <= 1000;
        case Type::FontFamily:
            return value.typeId() == QMetaType::QString && !value.toString().isEmpty() &&
                   !value.toString().contains(QRegularExpression{"[\\r\\n\\x00]"});
        case Type::Keyword:
            return value.typeId() == QMetaType::QString &&
                   QRegularExpression{"^[A-Za-z][A-Za-z0-9_-]*$"}.match(value.toString()).hasMatch();
    }
    return false;
}

QString ThemeValue::toStyleSheet(const bool insideQuotes) const {
    if (!isValid()) return {};
    const auto decimal = [](const double value) {
        if (value == 0) return QStringLiteral("0");
        // CSS numeric values should not depend on scientific-notation support.
        const auto precision = std::clamp(14 - static_cast<int>(std::floor(std::log10(std::abs(value)))), 0, 340);
        auto text = QString::number(value, 'f', precision);
        if (text.contains('.')) {
            while (text.endsWith('0')) text.chop(1);
            if (text.endsWith('.')) text.chop(1);
        }
        return text;
    };

    switch (type) {
        case Type::Color: {
            const auto color = value.value<QColor>();
            return QStringLiteral("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
        }
        case Type::Length:
            return decimal(value.toDouble()) + "px";
        case Type::Number:
            return decimal(value.toDouble());
        case Type::FontWeight:
            return QString::number(value.toInt());
        case Type::FontFamily: {
            auto family = value.toString();
            family.replace('\\', "\\\\").replace('"', "\\\"").replace('\'', "\\'");
            return insideQuotes ? family : '"' + family + '"';
        }
        case Type::Keyword:
            return value.toString();
    }

    return {};
}

namespace {
using namespace View;

QPalette preferencePalette() {
    const auto brightness = pref(Preferences::UIBrightness);
    constexpr auto contrast = 1.0f; // internal

    const auto hl_color = pref(Preferences::UIHighlightColor);
    const auto tint_color = pref(Preferences::UIWindowTintColor);
    const auto text_color = pref(Preferences::UITextColor);

    const auto windowColor = toQColor(tint_color, brightness);
    const auto textColor = toQColor(text_color, brightness);
    const auto highlightColor = toQColor(hl_color, brightness);

    // Build an initial palette based on the button color
    auto palette = QPalette{windowColor};

    // Window colors
    palette.setColor(QPalette::Active, QPalette::Window, windowColor.lighter(int(130.f * contrast)));
    palette.setColor(QPalette::Inactive, QPalette::Window, windowColor.lighter(int(130.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::Window, windowColor.darker(int(260.f * contrast)));

    // List box backgrounds, text entry backgrounds, menu backgrounds
    palette.setColor(QPalette::Base, windowColor.darker(int(125.f * contrast)));
    palette.setColor(QPalette::AlternateBase, windowColor.darker(int(160.f * contrast)));

    // Placeholder text color
    palette.setColor(QPalette::Active, QPalette::PlaceholderText, textColor.darker(int(150.f * contrast)));
    palette.setColor(QPalette::Inactive, QPalette::PlaceholderText, textColor.darker(int(170.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, textColor.darker(int(210.f * contrast)));

    // Button text
    palette.setColor(QPalette::Active, QPalette::ButtonText, textColor.lighter(int(100.f * contrast)));
    palette.setColor(QPalette::Inactive, QPalette::ButtonText, textColor.lighter(int(100.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, textColor.darker(int(200.f * contrast)));

    // Button
    palette.setColor(QPalette::Active, QPalette::Button, windowColor.lighter(int(190.f * contrast)));
    palette.setColor(QPalette::Inactive, QPalette::Button, windowColor.lighter(int(190.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::Button, windowColor.darker(int(100.f * contrast)));

    // WindowText is supposed to be against QPalette::Window
    palette.setColor(QPalette::Active, QPalette::WindowText, textColor);
    palette.setColor(QPalette::Inactive, QPalette::WindowText, textColor);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, textColor.darker(int(200.f * contrast)));

    // Menu text, text edit text, table cell text
    palette.setColor(QPalette::Active, QPalette::Text, textColor.darker(int(100.f * contrast)));
    palette.setColor(QPalette::Inactive, QPalette::Text, textColor.darker(int(100.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::Text, textColor.darker(int(200.f * contrast)));

    palette.setColor(QPalette::Active, QPalette::BrightText, textColor.lighter(int(200.f * contrast)));
    palette.setColor(QPalette::Inactive, QPalette::BrightText, textColor.lighter(int(200.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::BrightText, textColor.darker(int(100.f * contrast)));

    // Disabled menu item text shadow
    palette.setColor(QPalette::Light, windowColor.lighter(int(240.f * contrast)));
    palette.setColor(QPalette::Midlight, windowColor.lighter(int(210.f * contrast)));
    palette.setColor(QPalette::Mid, windowColor);
    palette.setColor(QPalette::Dark, windowColor.darker(int(100.f * contrast)));
    palette.setColor(QPalette::Shadow, windowColor.darker(int(650.f * contrast)));

    // Highlight (selected list box row, selected grid cell background, selected tab text)
    palette.setColor(QPalette::Active, QPalette::Highlight, highlightColor);
    palette.setColor(QPalette::Inactive, QPalette::Highlight, windowColor.lighter(int(200.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, windowColor.lighter(int(100.f * contrast)));

    // Text of highlighted elements
    palette.setColor(QPalette::Active, QPalette::HighlightedText, textColor.lighter(int(150.f * contrast)));
    palette.setColor(QPalette::Inactive, QPalette::HighlightedText, textColor.lighter(int(150.f * contrast)));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, textColor.darker(int(130.f * contrast)));

    return palette;
}

struct DefinitionType {
   const char *section;
   const char *prefix;
   ThemeValue::Type type;
};

const DefinitionType definitionTypes[] = {{"colors", "color.", ThemeValue::Type::Color},
                                          {"lengths", "length.", ThemeValue::Type::Length},
                                          {"numbers", "number.", ThemeValue::Type::Number},
                                          {"fontFamilies", "family.", ThemeValue::Type::FontFamily},
                                          {"fontWeights", "weight.", ThemeValue::Type::FontWeight},
                                          {"keywords", "keyword.", ThemeValue::Type::Keyword}};

bool preferenceBound(const QString &name) {
    return name.startsWith("color.") || name.startsWith("length.font.") || name.startsWith("family.") ||
           name.startsWith("weight.") || name.startsWith("keyword.font.") ||
           name == "number.text.detailOpacity" || name == "length.icon.toolbar";
}

QString fontStyle(const QFont &font) {
    return font.style() == QFont::StyleOblique ? "oblique" : font.italic() ? "italic" : "normal";
}

bool readFile(const std::filesystem::path &path, const bool optional, ThemeDefinitions &definitions, QStringList &errors) {
    QFile file{IO::pathAsQString(path)};

    if (optional && !QFileInfo::exists(file.fileName())) return true;
    if (!file.open(QIODevice::ReadOnly)) {
        errors << file.fileName() + ": " + file.errorString();
        return false;
    }

    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        errors << file.fileName() + ": " + file.errorString();
        return false;
    }

    return ThemeManager::readDefinition(bytes, file.fileName(), definitions, errors);
}
} // namespace

ThemeManager &ThemeManager::instance() {
    static ThemeManager manager;
    return manager;
}

ThemeSnapshot ThemeManager::preferenceValues(const QFont &ui, const QFont &console, const QFont &renderer) {
    auto snapshot = ThemeSnapshot{};
    snapshot.palette = preferencePalette();
    auto &values = snapshot.values;

    const auto color = [&](const QString &name, QPalette::ColorRole role, QPalette::ColorGroup group = QPalette::Active, int adjustment = 100) {
        auto result = snapshot.palette.color(group, role);
        result = adjustment < 0 ? result.darker(-adjustment) : result.lighter(adjustment);
        values.insert("color." + name, ThemeValue::color(result));
    };

    /** text color shades */
    color("text", QPalette::WindowText);
    color("text.disabled", QPalette::WindowText, QPalette::Disabled);
    color("text.highlighted", QPalette::HighlightedText);
    color("text.bright", QPalette::BrightText);
    color("text.placeholder", QPalette::PlaceholderText);
    color("button.disabled", QPalette::Button, QPalette::Disabled);

    /** basic color shades */
    color("window", QPalette::Window);
    color("button", QPalette::Button);
    color("text.button", QPalette::ButtonText);
    color("base", QPalette::Base);
    color("base.alternate", QPalette::AlternateBase);
    color("light", QPalette::Light);
    color("midlight", QPalette::Midlight);
    color("midhalf", QPalette::Mid, QPalette::Active, 150);
    color("mid", QPalette::Mid);
    color("dark", QPalette::Dark);
    color("shadow", QPalette::Shadow);

    /** highlight color shades */
    color("highlight.bright", QPalette::Highlight, QPalette::Active, 130);
    color("highlight.light", QPalette::Highlight, QPalette::Active, 80);
    color("highlight", QPalette::Highlight);
    color("highlight.mid", QPalette::Highlight, QPalette::Active, -180);
    color("highlight.dark", QPalette::Highlight, QPalette::Active, -200);

    auto detail = values["color.text"].value.value<QColor>();
    const auto opacity = std::clamp(pref(Preferences::UIDetailTextOpacity), 0.0f, 1.0f);

    detail.setAlphaF(detail.alphaF() * opacity);
    values.insert("color.text.detail", ThemeValue::color(detail));
    values.insert("number.text.detailOpacity", ThemeValue::number(opacity));
    values.insert("color.error", ThemeValue::color(View::toQColor(pref(Preferences::UIErrorColor))));
    values.insert("color.warning", ThemeValue::color(View::toQColor(pref(Preferences::UIWarningColor))));
    const std::pair<const char*, Preference<Color>*> logColors[] = {{"trace", &Preferences::LogTraceColor},
                                                                    {"debug", &Preferences::LogDebugColor},
                                                                    {"info", &Preferences::LogInfoColor},
                                                                    {"warning", &Preferences::LogWarningColor},
                                                                    {"error", &Preferences::LogErrorColor}};
    for (const auto& [name, preference] : logColors)
        values.insert(QStringLiteral("color.log.") + name, ThemeValue::color(View::toQColor(pref(*preference))));

    const std::pair<const char *, Preference<Color> *> iconColors[] = {{"layer", &Preferences::LayerTreeLayerIconColor},
                                                                       {"group", &Preferences::LayerTreeGroupIconColor},
                                                                       {"entity",
                                                                        &Preferences::LayerTreeEntityIconColor},
                                                                       {"brush", &Preferences::LayerTreeBrushIconColor},
                                                                       {"patch", &Preferences::LayerTreePatchIconColor},
                                                                       {"typeGroup",
                                                                        &Preferences::LayerTreeTypeGroupIconColor}};
    for (const auto &[name, preference] : iconColors)
        values.insert(QStringLiteral("color.icon.") + name, ThemeValue::color(View::toQColor(pref(*preference))));

    const auto length = [&](const QString &name, const qreal value) {
        values.insert("length." + name, ThemeValue::length(value));
    };

    length("radius.menu", 12);
    length("radius.tooltip", 8);
    length("margin.narrow.h", LayoutConstants::NarrowHMargin);
    length("margin.narrow.v", LayoutConstants::NarrowVMargin);
    length("margin.medium.h", LayoutConstants::MediumHMargin);
    length("margin.medium.v", LayoutConstants::MediumVMargin);
    length("margin.wide.h", LayoutConstants::WideHMargin);
    length("margin.wide.v", LayoutConstants::WideVMargin);
    length("margin.narrow", LayoutConstants::NarrowHMargin);
    length("margin.medium", LayoutConstants::MediumHMargin);
    length("margin.wide", LayoutConstants::WideHMargin);

    length("font.ui", View::uiFontPixelSize());
    length("font.subtitle", View::uiFontPixelSize(-1));
    length("font.small", View::uiFontPixelSize(-2));
    length("font.tiny", View::uiFontPixelSize(-4));
    length("font.title", qRound(View::uiFontPixelSize() * 1.4));
    length("font.header", View::uiFontPixelSize() * 2);
    length("font.dialogTitle", qRound(View::uiFontPixelSize() * 1.4) + 2);

    length("font.console", std::clamp(pref(Preferences::ConsoleFontSize), 1, 256));
    length("font.renderer", std::clamp(pref(Preferences::RendererFontSize), 1, 256));
    length("icon.toolbar", std::clamp(pref(Preferences::ToolBarIconsSize), 1, 256));

    const std::pair<const char *, const QFont *> fonts[] = {{"ui", &ui}, {"console", &console},
                                                            {"renderer", &renderer}};
    for (const auto &[name, font] : fonts) {
        values.insert(QStringLiteral("family.") + name, ThemeValue::family(font->family()));
        values.insert(QStringLiteral("weight.") + name, ThemeValue::weight(font->weight()));
        values.insert(QStringLiteral("keyword.font.") + name, ThemeValue::keyword(fontStyle(*font)));
    }
    return snapshot;
}

const QMap<QString, QString> &ThemeManager::legacyNames() {
    static const QMap<QString, QString> names = {{"TEXT_COLOR", "color.text"},
                                                 {"DISABLED_TEXT_COLOR", "color.text.disabled"},
                                                 {"DISABLED_BUTTON_COLOR", "color.button.disabled"},
                                                 {"HIGHLIGHTED_TEXT_COLOR", "color.text.highlighted"},
                                                 {"BRIGHT_TEXT_COLOR", "color.text.bright"},
                                                 {"PLACEHOLDER_TEXT_COLOR", "color.text.placeholder"},
                                                 {"WINDOW_COLOR", "color.window"},
                                                 {"BUTTON_COLOR", "color.button"},
                                                 {"BUTTON_TEXT_COLOR", "color.text.button"},
                                                 {"BASE_COLOR", "color.base"},
                                                 {"ALTERNATE_BASE_COLOR", "color.base.alternate"},
                                                 {"LIGHT_COLOR", "color.light"},
                                                 {"MIDLIGHT_COLOR", "color.midlight"},
                                                 {"MIDHALF_COLOR", "color.midhalf"},
                                                 {"MID_COLOR", "color.mid"},
                                                 {"DARK_COLOR", "color.dark"},
                                                 {"SHADOW_COLOR", "color.shadow"},
                                                 {"BRIGHT_HIGHLIGHT_COLOR", "color.highlight.bright"},
                                                 {"LIGHT_HIGHLIGHT_COLOR", "color.highlight.light"},
                                                 {"HIGHLIGHT_COLOR", "color.highlight"},
                                                 {"MID_HIGHLIGHT_COLOR", "color.highlight.mid"},
                                                 {"DARK_HIGHLIGHT_COLOR", "color.highlight.dark"},
                                                 {"BORDER_RADIUS", "length.radius.border"},
                                                 {"BORDER_SIZE", "length.border"},
                                                 {"MENU_CORNER_RADIUS", "length.radius.menu"},
                                                 {"TOOLTIP_CORNER_RADIUS", "length.radius.tooltip"},
                                                 {"NARROW_H_MARGIN", "length.margin.narrow.h"},
                                                 {"NARROW_V_MARGIN", "length.margin.narrow.v"},
                                                 {"NARROW_MARGIN", "length.margin.narrow"},
                                                 {"MEDIUM_H_MARGIN", "length.margin.medium.h"},
                                                 {"MEDIUM_V_MARGIN", "length.margin.medium.v"},
                                                 {"MEDIUM_MARGIN", "length.margin.medium"},
                                                 {"WIDE_H_MARGIN", "length.margin.wide.h"},
                                                 {"WIDE_V_MARGIN", "length.margin.wide.v"},
                                                 {"WIDE_MARGIN", "length.margin.wide"},
                                                 {"UI_FONT_SIZE", "length.font.ui"},
                                                 {"UI_MID_FONT_SIZE", "length.font.subtitle"},
                                                 {"UI_SMALL_FONT_SIZE", "length.font.small"},
                                                 {"UI_TITLE_FONT_SIZE", "length.font.title"},
                                                 {"UI_HEADER_FONT_SIZE", "length.font.header"},
                                                 {"UI_DIALOG_TITLE_FONT_SIZE", "length.font.dialogTitle"},
                                                 {"UI_FONT_FAMILY", "family.ui"},
                                                 {"UI_FONT_WEIGHT", "weight.ui"},
                                                 {"UI_FONT_STYLE", "keyword.font.ui"},
                                                 {"CONSOLE_FONT_FAMILY", "family.console"},
                                                 {"CONSOLE_FONT_WEIGHT", "weight.console"},
                                                 {"CONSOLE_FONT_STYLE", "keyword.font.console"},
                                                 {"CONSOLE_FONT_SIZE", "length.font.console"},
                                                 {"RENDER_FONT_SIZE", "length.font.renderer"},
                                                 {"TOOLBAR_ICON_SIZE", "length.icon.toolbar"},
                                                 {"UI_DETAIL_TEXT_COLOR", "color.text.detail"},
                                                 {"UI_ERROR_COLOR", "color.error"}};
    return names;
}

bool ThemeManager::isRelevantPreference(const std::filesystem::path &path) {
    const PreferenceBase* preferences[] = {&Preferences::UIBrightness,
                                           &Preferences::UIHighlightColor,
                                           &Preferences::UIWindowTintColor,
                                           &Preferences::UITextColor,
                                           &Preferences::UIErrorColor,
                                           &Preferences::UIWarningColor,
                                           &Preferences::LogTraceColor,
                                           &Preferences::LogDebugColor,
                                           &Preferences::LogInfoColor,
                                           &Preferences::LogWarningColor,
                                           &Preferences::LogErrorColor,
                                           &Preferences::UIDetailTextOpacity,
                                           &Preferences::UIFontPath,
                                           &Preferences::UIFontSize,
                                           &Preferences::UIFontMinimumSize,
                                           &Preferences::UIListFontWeight,
                                           &Preferences::UIListOpticalSize,
                                           &Preferences::UIListTypoLineMetrics,
                                           &Preferences::UIListSubpixelPositioning,
                                           &Preferences::UIGlobalTypography,
                                           &Preferences::ConsoleFontPath,
                                           &Preferences::ConsoleFontSize,
                                           &Preferences::RendererFontPath,
                                           &Preferences::RendererFontSize,
                                           &Preferences::ToolBarIconsSize,
                                           &Preferences::LayerTreeLayerIconColor,
                                           &Preferences::LayerTreeGroupIconColor,
                                           &Preferences::LayerTreeEntityIconColor,
                                           &Preferences::LayerTreeBrushIconColor,
                                           &Preferences::LayerTreePatchIconColor,
                                           &Preferences::LayerTreeTypeGroupIconColor};
    return std::any_of(std::begin(preferences), std::end(preferences), [&](const auto *preference) {
        return path == preference->path();
    });
}

bool ThemeManager::readDefinition(const QByteArray &bytes, const QString &source, ThemeDefinitions &definitions, QStringList &errors) {
    auto parseError = QJsonParseError{};
    const auto document = QJsonDocument::fromJson(bytes, &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        const auto prefix = bytes.left(static_cast<int>(parseError.offset));
        errors << QStringLiteral("%1:%2:%3: %4")
            .arg(source)
            .arg(prefix.count('\n') + 1)
            .arg(prefix.size() - prefix.lastIndexOf('\n'))
            .arg(parseError.error == QJsonParseError::NoError ? "Theme must be a JSON object"
                                                              : parseError.errorString());
        return false;
    }

    const auto object = document.object();
    if (!object.value("version").isDouble() || object.value("version").toDouble() != 1) {
        errors << source + ": expected theme version 1";
        return false;
    }

    auto candidate = definitions;
    for (auto section = object.begin(); section != object.end(); ++section) {
        // not needed here
        if (section.key() == "version") continue;

        const auto type = std::find_if(std::begin(definitionTypes), std::end(definitionTypes), [&](const auto &candidateType) {
            return section.key() == candidateType.section;
        });

        if (type == std::end(definitionTypes) || !section.value().isObject()) {
            errors << source + ": invalid theme section '" + section.key() + "'";
            continue;
        }

        // look for well-defined constant definitions
        const auto entries = section.value().toObject();
        for (auto entry = entries.begin(); entry != entries.end(); ++entry) {
            if (!QRegularExpression{
                "^[a-z][A-Za-z0-9_]*(?:\\.[A-Za-z][A-Za-z0-9_]*)*$"}.match(entry.key()).hasMatch()) {
                errors << source + ": invalid theme name '" + entry.key() + "'";
                continue;
            }

            candidate[section.key()][entry.key()] = QVariantMap{{"value", entry.value().toVariant()},
                                                                {"source", source}};
        }
    }

    if (!errors.isEmpty()) return false;
    definitions = std::move(candidate);

    return true;
}

bool ThemeManager::resolve(ThemeSnapshot &snapshot, const ThemeDefinitions &definitions, QStringList &errors) {
    auto result = snapshot.values;
    auto raw = QMap<QString, std::pair<ThemeValue::Type, QVariantMap>>{};
    for (const auto &type : definitionTypes) {
        const auto entries = definitions.value(type.section);
        for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
            const auto name = QString::fromLatin1(type.prefix) + it.key();
            if (result.contains(name) && preferenceBound(name)) {
                errors
                    << it.value().toMap().value("source").toString() + ": '" + name + "' is controlled by Preferences";
                continue;
            }
            raw.insert(name, {type.type, it.value().toMap()});
        }
    }
    auto resolving = QSet<QString>{};
    auto resolved = QSet<QString>{};
    std::function<bool(const QString &)> resolveValue;
    resolveValue = [&](const QString &name) {
        if (resolved.contains(name)) return true;
        const auto entry = raw.constFind(name);
        if (entry == raw.cend()) return result.contains(name);
        const auto location = entry->second.value("source").toString() + ": " + name + ": ";
        if (resolving.contains(name) || resolving.size() >= 128) {
            errors << location + "cyclic or excessively deep theme reference";
            return false;
        }
        resolving.insert(name);
        const auto input = entry->second.value("value");
        auto value = ThemeValue{entry->first, input};
        const auto string = input.toString();
        if (input.typeId() == QMetaType::QString && string.startsWith("${") && string.endsWith('}')) {
            const auto reference = string.mid(2, string.size() - 3);
            if (!resolveValue(reference)) {
                errors << location + "unresolved reference '" + reference + "'";
                resolving.remove(name);
                return false;
            }
            value = result.value(reference);
            if (value.type != entry->first) {
                errors << location + "reference has the wrong value type";
                resolving.remove(name);
                return false;
            }
        } else if (entry->first == ThemeValue::Type::Color && input.typeId() == QMetaType::QString) {
            value = ThemeValue::color(QColor{string});
        } else if (entry->first == ThemeValue::Type::Color ||
                   ((entry->first == ThemeValue::Type::Length || entry->first == ThemeValue::Type::Number ||
                     entry->first == ThemeValue::Type::FontWeight) &&
                    input.typeId() != QMetaType::Double && input.typeId() != QMetaType::Int &&
                    input.typeId() != QMetaType::LongLong) ||
                   ((entry->first == ThemeValue::Type::FontFamily || entry->first == ThemeValue::Type::Keyword) &&
                    input.typeId() != QMetaType::QString)) {
            errors << location + "value has the wrong type";
            resolving.remove(name);
            return false;
        }
        if (!value.isValid() || ((name == "number.icon.brightness" || name == "number.icon.disabledOpacity" ||
                                  name == "number.icon.disabledBrightness") &&
                                 (value.type != ThemeValue::Type::Number || value.value.toDouble() < 0 ||
                                  value.value.toDouble() > 1))) {
            errors << location + "invalid value or value outside the allowed range";
            resolving.remove(name);
            return false;
        }
        result.insert(name, value);
        resolved.insert(name);
        resolving.remove(name);
        return true;
    };
    for (auto it = raw.cbegin(); it != raw.cend(); ++it)
        resolveValue(it.key());
    if (!errors.isEmpty()) return false;
    snapshot.values = std::move(result);
    return true;
}

bool ThemeManager::prepare(const QFont &ui, const QFont &console, const QFont &renderer, const std::filesystem::path &defaults, const std::filesystem::path &overrides, ThemeSnapshot &result,
                           QStringList &errors) {
    errors.clear();
    auto definitions = ThemeDefinitions{};
    if (!readFile(defaults, false, definitions, errors) ||
        (!overrides.empty() && !readFile(overrides, true, definitions, errors)))
        return false;
    auto candidate = preferenceValues(ui, console, renderer);
    if (!resolve(candidate, definitions, errors)) return false;
    result = std::move(candidate);
    return true;
}

bool ThemeManager::publish(ThemeSnapshot snapshot) {
    if (snapshot.values == m_current.values && snapshot.palette == m_current.palette) return false;
    auto changed = QStringList{};
    for (auto it = snapshot.values.cbegin(); it != snapshot.values.cend(); ++it)
        if (!m_current.values.contains(it.key()) || m_current.values.value(it.key()) != it.value()) changed << it.key();
    for (auto it = m_current.values.cbegin(); it != m_current.values.cend(); ++it)
        if (!snapshot.values.contains(it.key())) changed << it.key();
    m_current = std::move(snapshot);
    ++m_revision;
    themeDidChangeNotifier(changed);
    return true;
}

QColor ThemeManager::color(const QString &name, const QColor &fallback) const {
    const auto it = m_current.values.constFind(name);
    return it != m_current.values.cend() && it->type == ThemeValue::Type::Color ? it->value.value<QColor>() : fallback;
}

qreal ThemeManager::number(const QString &name, const qreal fallback) const {
    const auto it = m_current.values.constFind(name);
    return it != m_current.values.cend() &&
           (it->type == ThemeValue::Type::Number || it->type == ThemeValue::Type::Length) ? it->value.toDouble()
                                                                                          : fallback;
}
} // namespace TrenchBroom
