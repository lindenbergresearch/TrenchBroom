#include "View/Preferences/FontUtils.h"

#include "IO/PathQt.h"
#include "IO/SystemPaths.h"
#include "Logger.h"
#include "Preferences.h"
#include "PreferenceManager.h"

#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontInfo>
#include <QHash>
#include <QRawFont>

#include <algorithm>
#include <array>
#include <cmath>

namespace TrenchBroom::View {
namespace {
QFont fileFont(const QString &path) {
    // Registrations must outlive fonts held by widgets. Cache face metadata too,
    // so choosing a Semibold/Italic file retains that face within its family.
    static QHash<QString, QFont> fonts;
    if (const auto found = fonts.constFind(path); found != fonts.cend()) return found.value();

    const auto id = QFontDatabase::addApplicationFont(path);
    const auto families = QFontDatabase::applicationFontFamilies(id);
    auto font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);

    if (!families.isEmpty()) {
        font = QFont(families.front());
        const auto raw = QRawFont(path, 26.0, QFont::PreferNoHinting);
        if (raw.isValid()) {
            font.setStyleName(raw.styleName());
            font.setWeight(QFont::Weight(raw.weight()));
            font.setStyle(raw.style());
        }
    } else {
        defaultQtLogger.warn() << "Unable to load font: " << path.toStdString();
    }

    fonts.insert(path, font);
    return font;
}

int minimumSize() {
    return std::clamp(pref(Preferences::UIFontMinimumSize), 8, 32);
}
}

QFont::Weight normalizedFontWeight(int weight) {
    if (weight >= 0 && weight <= 99) {
        constexpr std::array legacy{0, 12, 25, 50, 57, 63, 75, 81, 87, 99};
        constexpr std::array css{100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
        const auto next = std::lower_bound(legacy.begin(), legacy.end(), weight);
        const auto index = size_t(next - legacy.begin());
        if (index == 0 || *next == weight) return QFont::Weight(css[index]);
        const auto fraction = qreal(weight - legacy[index - 1]) / (legacy[index] - legacy[index - 1]);
        return QFont::Weight(qRound(css[index - 1] + fraction * (css[index] - css[index - 1])));
    }
    return QFont::Weight(std::clamp(weight, 1, 1000));
}

QFont loadFont(const std::filesystem::path &path, int pixelSize) {
    auto font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    if (!path.empty() && path != "[system]") {
        const auto name = IO::pathAsQString(path);
        if (QFontDatabase::hasFamily(name)) {
            font = QFont(name);
        } else {
            const auto file = IO::SystemPaths::findResourceFile(path);
            if (!file.empty() && QFileInfo::exists(IO::pathAsQString(file))) {
                const auto info = QFileInfo(IO::pathAsQString(file));
                const auto canonical = info.canonicalFilePath();
                font = fileFont(canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : canonical);
            }
        }
    }

    font.setPixelSize(std::clamp(pixelSize, 1, 256));
    font.setHintingPreference(QFont::PreferNoHinting);
    font.setStyleStrategy(QFont::PreferAntialias);

    defaultQtLogger.info() << "Loading font: " << font.family() << " style: " << font.styleName() << " hinting: "
                           << font.hintingPreference() << " -> " << font.toString();

    return font;
}

int uiFontPixelSize(int adjustment) {
    return std::clamp(std::clamp(pref(Preferences::UIFontSize), 1, 96) + adjustment, minimumSize(), 256);
}

QFont adjustedFont(const QFont &source, qreal pixelDelta) {
    auto font = source;
    const auto resolved = font.pixelSize() > 0 ? font.pixelSize() : QFontInfo(font).pixelSize();
    const auto size = std::clamp(qreal(std::max(1, resolved)) + pixelDelta, qreal(minimumSize()), 256.0);
    font.setPixelSize(qRound(size));
    return font;
}

QFont detailFont(const QFont &source, qreal reduction) {
    auto font = adjustedFont(source, -reduction);
    font.setBold(false);
    // A file-backed title may specify a bold style by name. Let the font matcher
    // choose its regular face for the detail line.
    font.setStyleName({});
    return font;
}

QFont fontForStyleSheet(QFont font) {
    // Qt 5's named font face otherwise overrides family/weight/style from QSS.
    font.setStyleName({});
    return font;
}
}
