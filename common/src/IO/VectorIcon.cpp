#include "VectorIcon.h"

#include "Ensure.h"
#include "ThemeManager.h"
#include "IO/PathQt.h"
#include "IO/SystemPaths.h"

#include <QApplication>
#include <QCache>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QIconEngine>
#include <QImage>
#include <QPainter>
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

    explicit Sources(const std::filesystem::path& path) {
        static quint64 nextId = 0; // GUI thread only; never recycled on cache reset.
        id = ++nextId;
        auto onPath = path.parent_path() / path.stem();
        auto offPath = onPath;
        onPath += "_on.svg";
        offPath += "_off.svg";
        on = loadSvg(onPath);
        off = loadSvg(offPath);
        if (!on || !off) {
            const auto base = loadSvg(path);
            if (!on) on = base ? base : off;
            if (!off) off = base ? base : on;
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
};

class VectorIcon::Engine : public QIconEngine {
    std::shared_ptr<Sources> m_sources;
    QColor m_tint;
    QString m_colorName;

    QPixmap rasterize(const QSize& requestedPixels, const qreal devicePixelRatio, const QIcon::Mode mode, const QIcon::State state) {
        ensureGuiThread();
        const auto pixels = actualSize(requestedPixels, mode, state);
        if (pixels.isEmpty()) return {};

        // Include density even for identical pixel dimensions: the pixmap's
        // logical size differs. Generated states also depend on theme/style.
        const auto& settings = IconAppearance::instance();
        const auto tint = m_colorName.isEmpty() ? m_tint : ThemeManager::instance().color(m_colorName, m_tint);
        const auto key = QString("tb/vector/%1/%2x%3/%4/%5/%6/%7/%8/%9/%10/%11/%12/%13")
            .arg(m_sources->id).arg(pixels.width()).arg(pixels.height())
            .arg(devicePixelRatio, 0, 'g', 17).arg(int(mode)).arg(int(state))
            .arg(qApp->palette().cacheKey()).arg(quintptr(qApp->style()))
            .arg(tint.isValid() ? qint64(tint.rgba()) : -1)
            .arg(settings.disabledBrightness(),0,'g',17).arg(settings.disabledOpacity(),0,'g',17)
            .arg(settings.normalizeBrightness()).arg(settings.brightness(),0,'g',17);
        QPixmap pixmap;
        if (QPixmapCache::find(key, &pixmap)) return pixmap;

        // Paint directly at the output pixel resolution. No downscaling to
        // logical pixels, sharpening or fixed-size per-state buffers.
        // Keep extra precision until brightness is applied: boosting a dark
        // icon must not turn low-alpha antialiasing pixels into dark fringes.
        QImage image(pixels, settings.normalizeBrightness() ? QImage::Format_RGBA64_Premultiplied : QImage::Format_ARGB32_Premultiplied);
        if (image.isNull()) return {};
        image.fill(Qt::transparent);
        {
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            m_sources->renderer(state)->render(&painter, QRectF(QPointF(), QSizeF(pixels)));
            if (tint.isValid()) {
                painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
                painter.fillRect(image.rect(), tint);
            }
        }
        if (settings.normalizeBrightness()) {
            const auto peak = tint.isValid() ? tint.valueF() : m_sources->brightness(state);
            normalizeIconBrightness(image, peak, settings.brightness());
        }
        pixmap = QPixmap::fromImage(image);
        pixmap.setDevicePixelRatio(devicePixelRatio);
        // Keep explicitly configured colors distinguishable on selected rows.
        if (mode == QIcon::Disabled) pixmap = disabledIconPixmap(pixmap);
        else if (mode != QIcon::Normal && !tint.isValid()) {
            QStyleOption option;
            option.palette = qApp->palette();
            pixmap = qApp->style()->generatedIconPixmap(mode, pixmap, &option);
            pixmap.setDevicePixelRatio(devicePixelRatio);
        }
        QPixmapCache::insert(key, pixmap);
        return pixmap;
    }

public:
    Engine(std::shared_ptr<Sources> sources, const QColor& tint, const QString& colorName = {})
        : m_sources(std::move(sources)), m_tint(tint), m_colorName(colorName) {}

    QIconEngine* clone() const override { return new Engine(m_sources, m_tint, m_colorName); }
    QString key() const override { return "TrenchBroomVectorIcon"; }

    QSize actualSize(const QSize& size, QIcon::Mode, const QIcon::State state) override {
        const auto* renderer = m_sources->renderer(state);
        if (!renderer || size.isEmpty()) return {};
        auto aspect = QSizeF(renderer->defaultSize());
        if (aspect.isEmpty()) aspect = renderer->viewBoxF().size();
        if (aspect.isEmpty()) return size;
        aspect.scale(QSizeF(size), Qt::KeepAspectRatio);
        return QSize(std::clamp(qRound(aspect.width()), 1, size.width()),
            std::clamp(qRound(aspect.height()), 1, size.height()));
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

    void virtual_hook(const int id, void* data) override {
        if (id == IsNullHook) {
            *static_cast<bool*>(data) = !m_sources->off && !m_sources->on;
        } else if (id == ScaledPixmapHook) {
            // In Qt 5 the size passed to this hook is already in physical pixels.
            auto& request = *static_cast<ScaledPixmapArgument*>(data);
            request.pixmap = rasterize(request.size, request.scale, request.mode, request.state);
        } else {
            QIconEngine::virtual_hook(id, data);
        }
    }
};

VectorIcon IconCache::load(const std::filesystem::path& path) {
    ensureGuiThread();
    if (path.empty()) return VectorIcon(nullptr, QColor{});
    // Cache resource lookups as well as parsed SVGs. No filesystem access on
    // cache hits; lexical normalization merges equivalent request paths.
    const auto cleanPath = QDir::cleanPath(pathAsQString(path));
    const auto key = QStringLiteral("source/") + cleanPath;
    if (const auto* icon = icons().object(key)) return *icon;
    const auto result = VectorIcon(std::make_shared<VectorIcon::Sources>(pathFromQString(cleanPath)), QColor{});
    // Null results are bounded, too, and can be retried after resetCache().
    icons().insert(key, new VectorIcon(result));
    return result;
}

int IconCache::size() { ensureGuiThread(); return icons().size(); }
int IconCache::maxEntries() { ensureGuiThread(); return icons().maxCost(); }
void IconCache::setMaxEntries(const int count) { ensureGuiThread(); icons().setMaxCost(std::max(0, count)); }
void IconCache::resetCache() { ensureGuiThread(); icons().clear(); }

VectorIcon::VectorIcon(const std::filesystem::path& path) : VectorIcon(IconCache::load(path)) {}
VectorIcon::VectorIcon(std::shared_ptr<Sources> sources, const QColor& tint, const QString& colorName)
    : m_sources(std::move(sources)),
      m_icon(m_sources && (m_sources->on || m_sources->off) ? QIcon(new Engine(m_sources, tint, colorName)) : QIcon{}) {}

QIcon VectorIcon::icon() const { return m_icon; }
QIcon VectorIcon::themedIcon(const QString& colorName, const QColor& fallback) const {
    ensureGuiThread();
    if (colorName.isEmpty() || m_icon.isNull()) return icon(fallback);
    const auto key = QStringLiteral("theme/%1/%2/%3").arg(m_sources->id).arg(colorName).arg(fallback.isValid() ? qint64(fallback.rgba()) : -1);
    if (const auto* cached = icons().object(key)) return cached->m_icon;
    auto result = VectorIcon{m_sources, fallback, colorName};
    icons().insert(key, new VectorIcon(result));
    return result.m_icon;
}
QIcon VectorIcon::icon(const QColor& tint) const {
    ensureGuiThread();
    if (!tint.isValid() || m_icon.isNull()) return m_icon;
    // Tinted variants share the SVG sources, with the same bounded LRU as
    // untinted icons. Each variant also has a distinct raster cache key.
    const auto rgba = tint.rgba();
    const auto key = QStringLiteral("tint/%1/%2").arg(m_sources->id).arg(rgba);
    if (const auto* cached = icons().object(key)) return cached->m_icon;
    const auto result = VectorIcon(m_sources, QColor::fromRgba(rgba));
    icons().insert(key, new VectorIcon(result));
    return result.m_icon;
}

QSize VectorIcon::defaultSize() const { return m_sources && m_sources->off ? m_sources->off->defaultSize() : QSize{}; }
bool VectorIcon::isValid() const { return !m_icon.isNull(); }

} // namespace TrenchBroom::IO
