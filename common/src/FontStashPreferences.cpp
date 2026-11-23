#include "FontStashPreferences.h"

#include "PreferenceManager.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace TrenchBroom {

Preference<QString>& fontStashPreference() {
    return PreferenceManager::instance().dynamicPreference("Editor/FontStash SDF", QString{});
}

const FontStashPreferences& fontStashPreferences() {
    static const auto defaults = FontStashPreferences{};
    if (!PreferenceManager::isInitialized()) return defaults;

    const auto& stored = pref(fontStashPreference());
    static auto lastValue = QString{};
    static auto settings = defaults;
    if (lastValue != stored) {
        const auto values = QJsonDocument::fromJson(stored.toUtf8()).object();
        settings = {
            values.value("enabled").toBool(defaults.enabled),
            FontStash::validatedSdfSettings({
                values.value("textWeight").toDouble(defaults.sdf.textWeight),
                values.value("unitsPerPixel").toInt(defaults.sdf.unitsPerPixel),
                values.value("distancePerTexel").toDouble(defaults.sdf.distancePerTexel),
                values.value("padding").toInt(defaults.sdf.padding)})};
        lastValue = stored;
    }
    return settings;
}

void applyFontStashPreferences(const FontStashPreferences& settings) {
    const auto values = QJsonObject{
        {"enabled", settings.enabled}, {"textWeight", settings.sdf.textWeight},
        {"unitsPerPixel", settings.sdf.unitsPerPixel}, {"distancePerTexel", settings.sdf.distancePerTexel},
        {"padding", settings.sdf.padding}};
    setPref(fontStashPreference(),
        settings == FontStashPreferences{} ? QString{} : QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Compact)));
}

} // namespace TrenchBroom
