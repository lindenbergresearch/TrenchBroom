#include "View/Preferences/FontUtils.h"

#include "IO/PathQt.h"
#include "IO/SystemPaths.h"
#include "Logger.h"
#include "Preferences.h"
#include "PreferenceManager.h"

#include <QDir>
#include <QCache>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QHash>
#include <QPainter>
#include <QRawFont>

#include <algorithm>
#include <array>
#include <cmath>

namespace TrenchBroom::View {
namespace {
QCache<QString, FontCapabilities>& capabilitiesCache() {
    static QCache<QString, FontCapabilities> cache{128};
    return cache;
}

QFont fileFont(const QString &path) {
    // Registrations must outlive fonts held by widgets. Cache face metadata too,
    // so choosing a Semibold/Italic file retains that face within its family.
    static QHash<QString, QFont> fonts;
    if (const auto found = fonts.constFind(path); found != fonts.cend()) return found.value();

    const auto id = QFontDatabase::addApplicationFont(path);
    if (id >= 0) capabilitiesCache().clear();
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
    font.unsetVariableAxis("wght");
    font.unsetVariableAxis("opsz");
    return font;
}

FontCapabilities fontCapabilities(const QFont& font) {
    // Axes belong to a face, independent of its size/current variation values.
    const auto key = font.family() + '/' + font.styleName() + '/'
        + QString::number(font.weight()) + '/' + QString::number(font.style());
    auto& cache = capabilitiesCache();
    if (const auto* found = cache.object(key)) return *found;
    FontCapabilities result;
    for (const auto& axis : QFontInfo{font}.variableAxes()) {
        const FontAxisRange range{axis.minimumValue(), axis.maximumValue(), axis.defaultValue()};
        if (!std::isfinite(range.minimum) || !std::isfinite(range.maximum) || range.minimum > range.maximum) continue;
        if (axis.tag() == QFont::Tag{"wght"}) result.weight = range;
        else if (axis.tag() == QFont::Tag{"opsz"}) result.opticalSize = range;
    }
    cache.insert(key, new FontCapabilities{result});
    return result;
}

ListFontSettings listFontSettings() {
    return {std::clamp(pref(Preferences::UIListFontWeight), 0, 1000),
        std::isfinite(pref(Preferences::UIListOpticalSize))
            ? std::clamp(qreal(pref(Preferences::UIListOpticalSize)), 0.0, 144.0) : 0.0,
        pref(Preferences::UIListTypoLineMetrics), pref(Preferences::UIListSubpixelPositioning)};
}

QFont listTextFont(QFont font, const ListFontSettings& settings, const bool preserveEmphasis) {
    const auto axes = fontCapabilities(font);
    const auto keepWeight = preserveEmphasis && font.weight() != QFont::Normal
        && font.weight() != QGuiApplication::font().weight();
    auto weight = int(font.weight());
    if (settings.weight > 0 && !keepWeight)
        weight = std::clamp(settings.weight, 1, 1000);
    if (axes.weight) {
        weight = qRound(std::clamp(qreal(weight), axes.weight->minimum, axes.weight->maximum));
        font.setStyleName({});
        font.setWeight(QFont::Weight(weight));
        // setWeight alone matches named faces and can round 450 back to 400.
        font.setVariableAxis("wght", float(weight));
    } else if (settings.weight > 0 && !keepWeight) {
        font.setStyleName({});
        font.setWeight(QFont::Weight(weight));
    }
    if (axes.opticalSize) {
        const auto points = font.pointSizeF() > 0 ? font.pointSizeF() : QFontInfo{font}.pointSizeF();
        const auto opticalSize = std::isfinite(settings.opticalSize) && settings.opticalSize > 0
            ? settings.opticalSize : points;
        font.setVariableAxis("opsz", float(std::clamp(opticalSize, axes.opticalSize->minimum, axes.opticalSize->maximum)));
    }
    const auto strategy = settings.typoLineMetrics
        ? int(font.styleStrategy()) | QFont::PreferTypoLineMetrics
        : int(font.styleStrategy()) & ~QFont::PreferTypoLineMetrics;
    font.setStyleStrategy(QFont::StyleStrategy(strategy));
    if (settings.subpixelPositioning) font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}

QFont styledTextFont(QFont font, const ListFontSettings& settings, const bool weightAxis) {
    // Work from the final QSS weight; never carry a parent's variation into a
    // bold heading or rich-text fragment. Keep unrelated axes such as width.
    font.unsetVariableAxis("wght");
    font.unsetVariableAxis("opsz");
    auto resolved = settings;
    resolved.weight = 0;
    const auto originalWeight = font.weight();
    const auto originalStyleName = font.styleName();
    const auto originalMask = font.resolveMask();
    auto tuned = listTextFont(font, resolved);
    if (!weightAxis) tuned.unsetVariableAxis("wght");
    // Changing the variation must not make inherited family/size/weight local.
    // FontChange will then continue to propagate future application changes.
    QFont axisMask;
    axisMask.setVariableAxis("wght", 400);
    const auto variationsMask = axisMask.resolveMask();
    tuned.setWeight(originalWeight);
    tuned.setStyleName(originalStyleName);
    tuned.setResolveMask((originalMask & ~variationsMask)
        | (tuned.variableAxisTags().isEmpty() ? 0 : variationsMask));
    return tuned;
}

qreal logicalFontPixelSize(const QFont& font) {
    return font.pixelSize() > 0 ? qreal(font.pixelSize()) : qreal(QFontInfo{font}.pixelSize());
}

QPointF textLineBaseline(const QPainter& painter, const QRectF& rect,
    const QFontMetricsF& metrics, const bool subpixelPositioning) {
    const QPointF baseline{rect.left(), rect.top() + (rect.height() - metrics.height()) / 2.0 + metrics.ascent()};
    if (subpixelPositioning) return baseline;
    // Snap after the device transform: half a logical pixel is a whole physical
    // pixel on Retina, and translated/fractionally scaled painters also work.
    const auto transform = painter.deviceTransform();
    bool invertible = false;
    const auto inverse = transform.inverted(&invertible);
    if (!invertible) return baseline;
    const auto device = transform.map(baseline);
    return inverse.map(QPointF{qreal(qRound(device.x())), qreal(qRound(device.y()))});
}

void paintTextLine(QPainter& painter, const QRectF& rect, const QString& text,
    const QFont& font, const bool subpixelPositioning) {
    painter.setFont(font);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::VerticalSubpixelPositioning, subpixelPositioning);
    const QFontMetricsF metrics{font};
    const auto baseline = textLineBaseline(painter, rect, metrics, subpixelPositioning);
    painter.drawText(baseline,
        metrics.elidedText(text, Qt::ElideRight, std::max(qreal(0), rect.right() - baseline.x())));
}
}
