#include "VectorIcon.h"

#include "Ensure.h"
#include "FontStashPreferences.h"
#include "View/Theme/FontStashText.h"
#include "View/Theme/ThemeManager.h"
#include "IO/PathQt.h"
#include "IO/SystemPaths.h"

#include <QApplication>
#include <QCache>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QGlyphRun>
#include <QIconEngine>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QRgba64>
#include <QPointer>
#include <QStyle>
#include <QStyleOption>
#include <QSvgRenderer>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace TrenchBroom::IO {
namespace {
void ensureGuiThread() {
    ensure(qApp && qApp->thread() == QThread::currentThread(), "VectorIcon requires the GUI thread");
}

QPointer<IconAppearance> appearance;
void deleteAppearance() { delete appearance.data(); }

QCache<QString, VectorIcon>& icons() {
    static QCache<QString, VectorIcon> cache(IconCache::DefaultMaxEntries);
    return cache;
}

QString resolvePath(const std::filesystem::path& path) {
    const auto name = pathAsQString(path);
    const auto resolved = path.is_absolute() || name.startsWith(":/")
        ? name : pathAsQString(SystemPaths::findResourceFile(std::filesystem::path("images") / path));
    return !resolved.isEmpty() && QFileInfo(resolved).isFile() ? resolved : QString{};
}

std::shared_ptr<QSvgRenderer> loadSvg(const std::filesystem::path& path) {
    const auto resolved = resolvePath(path);
    if (resolved.isEmpty()) return {};
    auto renderer = std::make_shared<QSvgRenderer>(resolved);
    return renderer->isValid() ? renderer : nullptr;
}
} // namespace

IconAppearance::IconAppearance() { setObjectName("IconAppearance"); hide(); }

IconAppearance& IconAppearance::instance() {
    ensureGuiThread();
    if (!appearance) { appearance = new IconAppearance; qAddPostRoutine(deleteAppearance); }
    appearance->ensurePolished();
    return *appearance;
}

void IconAppearance::setDisabledBrightness(qreal value) {
    value = std::isfinite(value) ? std::clamp(value,qreal(0),qreal(1)) : qreal(0.35);
    if (value != m_brightness) { m_brightness = value; QPixmapCache::clear(); }
}
void IconAppearance::setDisabledOpacity(qreal value) {
    value = std::isfinite(value) ? std::clamp(value,qreal(0),qreal(1)) : qreal(1);
    if (value != m_opacity) { m_opacity = value; QPixmapCache::clear(); }
}

void IconAppearance::setBrightness(qreal value) {
    value = std::isfinite(value) ? std::clamp(value, qreal(0), qreal(1)) : qreal(0.75);
    if (value != m_targetBrightness) { m_targetBrightness = value; QPixmapCache::clear(); }
}

void IconAppearance::setNormalizeBrightness(const bool enabled) {
    if (enabled != m_normalizeBrightness) { m_normalizeBrightness = enabled; QPixmapCache::clear(); }
}

qreal measureIconBrightness(const QImage& source) {
    if (source.isNull()) return 0;
    const auto image = source.convertToFormat(QImage::Format_RGBA64_Premultiplied);
    auto maxAlpha = quint16{0};
    for (auto y = 0; y < image.height(); ++y) {
        const auto* row = reinterpret_cast<const QRgba64*>(image.constScanLine(y));
        for (auto x = 0; x < image.width(); ++x) maxAlpha = std::max(maxAlpha, row[x].alpha());
    }
    if (maxAlpha == 0) return 0;
    // Relative to the icon's own opacity, so intentionally translucent icons
    // are measured too, without bright rounding noise in almost empty pixels.
    const auto threshold = std::max(1, int(maxAlpha) * 3 / 4);
    auto peak = quint16{0};
    for (auto y = 0; y < image.height(); ++y) {
        const auto* row = reinterpret_cast<const QRgba64*>(image.constScanLine(y));
        for (auto x = 0; x < image.width(); ++x) {
            if (row[x].alpha() < threshold) continue;
            const auto pixel = row[x].unpremultiplied();
            peak = std::max({peak, pixel.red(), pixel.green(), pixel.blue()});
        }
    }
    return qreal(peak) / 65535;
}

void normalizeIconBrightness(QImage& image, qreal sourceBrightness, qreal targetBrightness) {
    if (image.isNull()) return;
    targetBrightness = std::isfinite(targetBrightness) ? std::clamp(targetBrightness, qreal(0), qreal(1)) : qreal(0.75);
    sourceBrightness = std::isfinite(sourceBrightness) ? std::clamp(sourceBrightness, qreal(0), qreal(1)) : 0;
    image = image.convertToFormat(QImage::Format_RGBA64_Premultiplied);
    const auto gain = sourceBrightness > 0 ? targetBrightness / sourceBrightness : 0;
    for (auto y = 0; y < image.height(); ++y) {
        auto* row = reinterpret_cast<QRgba64*>(image.scanLine(y));
        for (auto x = 0; x < image.width(); ++x) {
            const auto pixel = row[x];
            const auto alpha = pixel.alpha();
            const auto channel = [&](const quint16 value) {
                return quint16(qRound(std::clamp(sourceBrightness > 0 ? value * gain : alpha * targetBrightness, qreal(0), qreal(alpha))));
            };
            row[x] = QRgba64::fromRgba64(channel(pixel.red()), channel(pixel.green()), channel(pixel.blue()), alpha);
        }
    }
}

namespace {
QSize fitIconSize(QSizeF aspect, const QSize& bounds) {
    if (bounds.isEmpty()) return {};
    if (aspect.isEmpty()) return bounds;
    aspect.scale(QSizeF(bounds), Qt::KeepAspectRatio);
    return {std::clamp(qRound(aspect.width()), 1, bounds.width()),
            std::clamp(qRound(aspect.height()), 1, bounds.height())};
}

qreal unitValue(qreal value, qreal fallback) {
    return std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : fallback;
}
} // namespace

QPixmap disabledIconPixmap(const QPixmap& source) {
    if (source.isNull()) return {};
    const auto& settings = IconAppearance::instance();
    const auto opacity = settings.disabledOpacity(), brightness = settings.disabledBrightness()*opacity;
    auto image = source.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for (int y=0;y<image.height();++y) {
        auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x=0;x<image.width();++x) {
            const auto pixel = row[x];
            row[x] = qRgba(qRound(qRed(pixel)*brightness), qRound(qGreen(pixel)*brightness),
                qRound(qBlue(pixel)*brightness), qRound(qAlpha(pixel)*opacity));
        }
    }
    auto result = QPixmap::fromImage(image); result.setDevicePixelRatio(source.devicePixelRatio()); return result;
}

struct VectorIcon::Sources {
    std::shared_ptr<QSvgRenderer> on;
    std::shared_ptr<QSvgRenderer> off;
    quint64 id;
    qreal onBrightness = 0, offBrightness = 0;
    QSize svgSize;
    QRawFont font;
    quint32 glyph = 0;
    QRectF glyphBounds;

    static quint64 nextId() {
        static quint64 nextId = 0; // GUI thread only; never recycled on cache reset.
        return ++nextId;
    }

    Sources(const QRawFont& raw, const quint32 index)
        : id(nextId()), font(raw), glyph(index), glyphBounds(raw.pathForGlyph(index).boundingRect()) {}

    explicit Sources(const std::filesystem::path& path) : id(nextId()) {
        auto onPath = path.parent_path() / path.stem();
        auto offPath = onPath;
        onPath += "_on.svg";
        offPath += "_off.svg";
        on = loadSvg(onPath);
        off = loadSvg(offPath);
        const auto base = loadSvg(path);
        if (!on) on = base ? base : off;
        if (!off) off = base ? base : on;
        if (const auto& original = base ? base : off) {
            svgSize = original->defaultSize();
            if (svgSize.isEmpty()) svgSize = original->viewBoxF().size().toSize();
        }
        const auto measure = [](QSvgRenderer* renderer) {
            if (!renderer) return qreal(0);
            auto size = QSizeF(renderer->defaultSize());
            if (size.isEmpty()) size = renderer->viewBoxF().size();
            if (size.isEmpty()) size = QSizeF{128, 128};
            size.scale(QSizeF{128, 128}, Qt::KeepAspectRatio);
            QImage sample{QSize{std::max(1, qRound(size.width())), std::max(1, qRound(size.height()))}, QImage::Format_RGBA64_Premultiplied};
            sample.fill(Qt::transparent);
            { QPainter painter{&sample}; renderer->render(&painter, sample.rect()); }
            return measureIconBrightness(sample);
        };
        onBrightness = measure(on.get());
        offBrightness = off == on ? onBrightness : measure(off.get());
    }

    QSvgRenderer* renderer(const QIcon::State state) const { return (state == QIcon::On ? on : off).get(); }
    qreal brightness(const QIcon::State state) const { return state == QIcon::On ? onBrightness : offBrightness; }
    bool isGlyph() const { return font.isValid() && !glyphBounds.isEmpty(); }

    void renderGlyph(QPainter& painter, const QSize& pixels, const QColor& color, const FontStashPreferences& prefs) const {
        const auto margin = std::min(1.0, std::min(pixels.width(), pixels.height()) / 4.0);
        const auto available = QRectF(QPointF{}, QSizeF(pixels)).adjusted(margin, margin, -margin, -margin);
        const auto scale = std::min(available.width() / glyphBounds.width(), available.height() / glyphBounds.height());
        const auto baseline = available.center() - glyphBounds.center() * scale;
        if (prefs.enabled) {
            const auto raster = View::fontStashGlyph(font, glyph, baseline, scale, prefs.sdf);
            if (!raster.mask.pixels.isNull()) {
                painter.drawImage(raster.mask.origin, raster.mask.pixels);
                painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
                painter.fillRect(QRect(QPoint{}, pixels), color);
                return;
            }
        }
        QGlyphRun run;
        run.setRawFont(font);
        run.setGlyphIndexes({glyph});
        run.setPositions({QPointF{}});
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setPen(color);
        painter.translate(baseline);
        painter.scale(scale, scale);
        painter.drawGlyphRun(QPointF{}, run);
    }
};

class VectorIcon::Engine : public QIconEngine {
    std::shared_ptr<Sources> m_sources;
    IconStyle m_style;
    QString m_themeColor;

    QPixmap rasterize(const QSize& requestedPixels, const qreal devicePixelRatio, const QIcon::Mode mode, const QIcon::State state) {
        ensureGuiThread();
        const auto pixels = actualSize(requestedPixels, mode, state);
        if (pixels.isEmpty()) return {};

        // Include density even for identical pixel dimensions: the pixmap's
        // logical size differs. Generated states also depend on theme/style.
        const auto& settings = IconAppearance::instance();
        const auto tint = m_themeColor.isEmpty() ? m_style.color : ThemeManager::instance().color(m_themeColor, m_style.color);
        const auto normalizeBrightness = settings.normalizeBrightness() || m_style.brightness.has_value();
        const auto brightness = unitValue(m_style.brightness.value_or(settings.brightness()), settings.brightness());
        const auto opacity = unitValue(m_style.opacity.value_or(1.0), 1.0);
        const auto generateMode = !tint.isValid() && !m_style.brightness;
        auto key = QString("tb/vector/%1/%2x%3/%4/%5/%6/%7/%8/%9/%10/%11/%12/%13/%14/%15")
            .arg(m_sources->id).arg(pixels.width()).arg(pixels.height())
            .arg(devicePixelRatio, 0, 'g', 17).arg(int(mode)).arg(int(state))
            .arg(qApp->palette().cacheKey()).arg(quintptr(qApp->style()))
            .arg(tint.isValid() ? qint64(tint.rgba()) : -1)
            .arg(settings.disabledBrightness(),0,'g',17).arg(settings.disabledOpacity(),0,'g',17)
            .arg(normalizeBrightness).arg(brightness, 0, 'g', 17)
            .arg(opacity, 0, 'g', 17).arg(generateMode);
        const auto isGlyph = m_sources->isGlyph();
        const auto fontSettings = isGlyph ? fontStashPreferences() : FontStashPreferences{};
        const auto glyphColor = tint.isValid() ? tint : qApp->palette().color(QPalette::WindowText);
        if (isGlyph) {
            key += QString("/%1/%2/%3/%4/%5").arg(fontSettings.enabled)
                .arg(fontSettings.sdf.textWeight, 0, 'g', 17).arg(fontSettings.sdf.unitsPerPixel)
                .arg(fontSettings.sdf.distancePerTexel, 0, 'g', 17).arg(fontSettings.sdf.padding);
        }
        QPixmap pixmap;
        if (QPixmapCache::find(key, &pixmap)) return pixmap;

        // Paint directly at the output pixel resolution. No downscaling to
        // logical pixels, sharpening or fixed-size per-state buffers.
        // Keep extra precision until brightness is applied: boosting a dark
        // icon must not turn low-alpha antialiasing pixels into dark fringes.
        QImage image(pixels, normalizeBrightness ? QImage::Format_RGBA64_Premultiplied : QImage::Format_ARGB32_Premultiplied);
        if (image.isNull()) return {};
        image.fill(Qt::transparent);
        {
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            if (isGlyph) {
                m_sources->renderGlyph(painter, pixels, glyphColor, fontSettings);
            } else {
                m_sources->renderer(state)->render(&painter, QRectF(QPointF(), QSizeF(pixels)));
                if (tint.isValid()) {
                    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
                    painter.fillRect(image.rect(), tint);
                }
            }
        }
        if (normalizeBrightness) {
            const auto peak = isGlyph ? glyphColor.valueF() : tint.isValid() ? tint.valueF() : m_sources->brightness(state);
            normalizeIconBrightness(image, peak, brightness);
        }
        if (opacity < 1.0) {
            QPainter painter(&image);
            painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            painter.fillRect(image.rect(), QColor::fromRgbF(0, 0, 0, float(opacity)));
        }
        pixmap = QPixmap::fromImage(image);
        pixmap.setDevicePixelRatio(devicePixelRatio);
        // Keep explicitly configured colors distinguishable on selected rows.
        if (mode == QIcon::Disabled) pixmap = disabledIconPixmap(pixmap);
        else if (mode != QIcon::Normal && generateMode) {
            QStyleOption option;
            option.palette = qApp->palette();
            pixmap = qApp->style()->generatedIconPixmap(mode, pixmap, &option);
            pixmap.setDevicePixelRatio(devicePixelRatio);
        }
        QPixmapCache::insert(key, pixmap);
        return pixmap;
    }

public:
    Engine(std::shared_ptr<Sources> sources, const IconStyle& style, const QString& themeColor)
        : m_sources(std::move(sources)), m_style(style), m_themeColor(themeColor) {}

    QIconEngine* clone() const override { return new Engine(m_sources, m_style, m_themeColor); }
    QString key() const override { return "TrenchBroomVectorIcon"; }

    QSize actualSize(const QSize& size, QIcon::Mode, const QIcon::State state) override {
        if (m_sources->isGlyph()) return size;
        const auto* renderer = m_sources->renderer(state);
        if (!renderer || size.isEmpty()) return {};
        auto aspect = QSizeF(renderer->defaultSize());
        if (aspect.isEmpty()) aspect = renderer->viewBoxF().size();
        return fitIconSize(aspect, size);
    }

    QPixmap pixmap(const QSize& size, const QIcon::Mode mode, const QIcon::State state) override {
        return rasterize(size, 1.0, mode, state);
    }

    void paint(QPainter* painter, const QRect& rect, const QIcon::Mode mode, const QIcon::State state) override {
        // deviceTransform includes Retina density as well as painter scaling.
        const auto transform = painter->deviceTransform();
        const auto scale = std::max(std::hypot(transform.m11(), transform.m12()), std::hypot(transform.m21(), transform.m22()));
        if (rect.isEmpty() || !std::isfinite(scale) || scale <= 0) return;
        painter->drawPixmap(rect, rasterize(rect.size() * scale, scale, mode, state));
    }

    bool isNull() override { return !m_sources->isGlyph() && !m_sources->off && !m_sources->on; }

    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale) override {
        return rasterize(size * scale, scale, mode, state);
    }

};

VectorIcon IconCache::load(const std::filesystem::path& path) {
    ensureGuiThread();
    if (path.empty()) return VectorIcon(nullptr, {});
    // Cache resource lookups as well as parsed SVGs. No filesystem access on
    // cache hits; lexical normalization merges equivalent request paths.
    const auto cleanPath = QDir::cleanPath(pathAsQString(path));
    const auto key = QStringLiteral("source/") + cleanPath;
    if (const auto* icon = icons().object(key)) return *icon;
    const auto result = VectorIcon(std::make_shared<VectorIcon::Sources>(pathFromQString(cleanPath)), {});
    // Null results are bounded, too, and can be retried after resetCache().
    icons().insert(key, new VectorIcon(result));
    return result;
}

int IconCache::size() { ensureGuiThread(); return icons().size(); }
int IconCache::maxEntries() { ensureGuiThread(); return icons().maxCost(); }
void IconCache::setMaxEntries(const int count) { ensureGuiThread(); icons().setMaxCost(std::max(0, count)); }
void IconCache::resetCache() { ensureGuiThread(); icons().clear(); }

VectorIcon::VectorIcon(const std::filesystem::path& path) : VectorIcon(IconCache::load(path)) {}
VectorIcon::VectorIcon(std::shared_ptr<Sources> sources, const IconStyle& style, const QString& themeColor)
    : m_sources(std::move(sources)),
      m_icon(m_sources && (m_sources->isGlyph() || m_sources->on || m_sources->off)
          ? QIcon(new Engine(m_sources, style, themeColor)) : QIcon{}) {}

VectorIcon VectorIcon::fromGlyph(const QRawFont& font, const quint32 glyphIndex) {
    ensureGuiThread();
    if (!font.isValid()) return VectorIcon(nullptr, {});
    return VectorIcon(std::make_shared<Sources>(font, glyphIndex), {});
}

QIcon VectorIcon::icon() const { return m_icon; }
QIcon VectorIcon::themedIcon(const QString& colorName, const QColor& fallback) const {
    return styledIcon({.color = fallback}, colorName);
}
QIcon VectorIcon::icon(const QColor& tint) const {
    return styledIcon({.color = tint});
}
QIcon VectorIcon::styledIcon(const IconStyle& style, const QString& themeColor) const {
    ensureGuiThread();
    if (m_icon.isNull() || (style == IconStyle{} && themeColor.isEmpty())) return m_icon;
    // Styled variants share the parsed SVG or glyph with the original.
    const auto key = QStringLiteral("styled/%1/%2/%3/%4/%5").arg(m_sources->id)
        .arg(style.color.isValid() ? qint64(style.color.rgba()) : -1)
        .arg(style.brightness.value_or(-1.0), 0, 'g', 17).arg(style.opacity.value_or(-1.0), 0, 'g', 17)
        .arg(themeColor);
    if (const auto* cached = icons().object(key)) return cached->m_icon;
    const auto result = VectorIcon(m_sources, style, themeColor);
    icons().insert(key, new VectorIcon(result));
    return result.m_icon;
}

QSize VectorIcon::defaultSize() const { return m_sources && m_sources->off ? m_sources->off->defaultSize() : QSize{}; }
QSize VectorIcon::sourceSize(int glyphExtent) const {
    if (!m_sources) return {};
    if (m_sources->isGlyph()) return fitIconSize(m_sources->glyphBounds.size(), {glyphExtent, glyphExtent});
    return m_sources->svgSize;
}
bool VectorIcon::isValid() const { return !m_icon.isNull(); }

} // namespace TrenchBroom::IO
