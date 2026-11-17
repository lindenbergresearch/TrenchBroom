#include "VectorIcon.h"

#include "Ensure.h"
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
    }

    QSvgRenderer* renderer(const QIcon::State state) const { return (state == QIcon::On ? on : off).get(); }
};

class VectorIcon::Engine : public QIconEngine {
    std::shared_ptr<Sources> m_sources;
    QColor m_tint;

    QPixmap rasterize(const QSize& requestedPixels, const qreal devicePixelRatio, const QIcon::Mode mode, const QIcon::State state) {
        ensureGuiThread();
        const auto pixels = actualSize(requestedPixels, mode, state);
        if (pixels.isEmpty()) return {};

        // Include density even for identical pixel dimensions: the pixmap's
        // logical size differs. Generated states also depend on theme/style.
        const auto& settings = IconAppearance::instance();
        const auto key = QString("tb/vector/%1/%2x%3/%4/%5/%6/%7/%8/%9/%10/%11")
            .arg(m_sources->id).arg(pixels.width()).arg(pixels.height())
            .arg(devicePixelRatio, 0, 'g', 17).arg(int(mode)).arg(int(state))
            .arg(qApp->palette().cacheKey()).arg(quintptr(qApp->style()))
            .arg(m_tint.isValid() ? qint64(m_tint.rgba()) : -1)
            .arg(settings.disabledBrightness(),0,'g',17).arg(settings.disabledOpacity(),0,'g',17);
        QPixmap pixmap;
        if (QPixmapCache::find(key, &pixmap)) return pixmap;

        // Paint directly at the output pixel resolution. No downscaling to
        // logical pixels, sharpening, brightness changes or per-state buffers.
        QImage image(pixels, QImage::Format_ARGB32_Premultiplied);
        if (image.isNull()) return {};
        image.fill(Qt::transparent);
        {
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            m_sources->renderer(state)->render(&painter, QRectF(QPointF(), QSizeF(pixels)));
            if (m_tint.isValid()) {
                painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
                painter.fillRect(image.rect(), m_tint);
            }
        }
        pixmap = QPixmap::fromImage(image);
        pixmap.setDevicePixelRatio(devicePixelRatio);
        // Keep explicitly configured colors distinguishable on selected rows.
        if (mode == QIcon::Disabled) pixmap = disabledIconPixmap(pixmap);
        else if (mode != QIcon::Normal && !m_tint.isValid()) {
            QStyleOption option;
            option.palette = qApp->palette();
            pixmap = qApp->style()->generatedIconPixmap(mode, pixmap, &option);
            pixmap.setDevicePixelRatio(devicePixelRatio);
        }
        QPixmapCache::insert(key, pixmap);
        return pixmap;
    }

public:
    Engine(std::shared_ptr<Sources> sources, const QColor& tint)
        : m_sources(std::move(sources)), m_tint(tint) {}

    QIconEngine* clone() const override { return new Engine(m_sources, m_tint); }
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
VectorIcon::VectorIcon(std::shared_ptr<Sources> sources, const QColor& tint)
    : m_sources(std::move(sources)),
      m_icon(m_sources && (m_sources->on || m_sources->off) ? QIcon(new Engine(m_sources, tint)) : QIcon{}) {}

QIcon VectorIcon::icon() const { return m_icon; }
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
