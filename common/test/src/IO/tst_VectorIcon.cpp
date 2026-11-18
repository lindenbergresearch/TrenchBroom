#include "IO/PathQt.h"
#include "IO/ResourceUtils.h"
#include "IO/VectorIcon.h"
#include "View/Theme/QtUtils.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QPainter>
#include <QPixmapCache>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QToolButton>
#include <QScopeGuard>

#include <catch2/catch.hpp>

namespace TrenchBroom::IO {
namespace {
const QByteArray detailSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M1.5 1.5H14.5V14.5H1.5Z M4.5 3V13 M3 5.5H13" fill="none" stroke="#203c81"/><circle cx="10" cy="10" r="2.25" fill="#e07c21"/></svg>)";

QImage paintIcon(const QIcon& icon, const QSize size, const qreal density = 1.0, const QIcon::State state = QIcon::Off) {
    QImage image(size * density, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(density);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    icon.paint(&painter, QRect(QPoint(), size), Qt::AlignCenter, QIcon::Normal, state);
    return image;
}

class VectorIconTest {
protected:
    QTemporaryDir directory;
    const int previousCapacity = IconCache::maxEntries();
    const int previousPixmapLimit = QPixmapCache::cacheLimit();
    const QPalette previousPalette = qApp->palette();
    const bool previousNormalize = IconAppearance::instance().normalizeBrightness();
    const qreal previousBrightness = IconAppearance::instance().brightness();

    VectorIconTest() {
        IconCache::resetCache();
        // Geometry and source-color tests compare against unmodified Qt SVGs.
        IconAppearance::instance().setNormalizeBrightness(false);
    }
    ~VectorIconTest() {
        IconCache::resetCache();
        IconCache::setMaxEntries(previousCapacity);
        QPixmapCache::setCacheLimit(previousPixmapLimit);
        qApp->setPalette(previousPalette);
        IconAppearance::instance().setNormalizeBrightness(previousNormalize);
        IconAppearance::instance().setBrightness(previousBrightness);
    }

    std::filesystem::path path(const QString& name) const { return pathFromQString(directory.filePath(name)); }
    std::filesystem::path write(const QString& name, const QByteArray& contents = detailSvg) {
        QFile file(directory.filePath(name));
        REQUIRE(file.open(QIODevice::WriteOnly));
        REQUIRE(file.write(contents) == contents.size());
        return path(name);
    }

    std::filesystem::path solid(const QString& name, const QByteArray& color) {
        return write(name, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 16 16\"><rect width=\"16\" height=\"16\" fill=\"" + color + "\"/></svg>");
    }
};
}

TEST_CASE_METHOD(VectorIconTest, "VectorIcon rasterizes each size and density directly from SVG", "[VectorIcon]") {
    const auto source = write("detail.svg");
    const auto small = loadSVGIcon(source, 12);
    const auto large = loadSVGIcon(source, 48);
    REQUIRE_FALSE(small.isNull());
    CHECK(small.cacheKey() == large.cacheKey()); // Share vectors, never one fixed raster size.
    for (const auto extent : {12, 16, 24, 48, 96}) {
        for (const auto density : {1.0, 1.25, 1.5, 2.0, 3.0}) {
            CAPTURE(extent, density);
            const auto actual = paintIcon(small, QSize(extent, extent), density);
            QImage reference(actual.size(), QImage::Format_ARGB32_Premultiplied);
            reference.setDevicePixelRatio(density);
            reference.fill(Qt::transparent);
            {
                QPainter painter(&reference);
                painter.setRenderHint(QPainter::Antialiasing);
                QSvgRenderer svg(detailSvg);
                svg.render(&painter, QRectF(0, 0, extent, extent));
            }
            CHECK(actual == reference); // Detect downsampling, halos, or lost pixel density.
            CHECK(actual == paintIcon(large, QSize(extent, extent), density));
        }
    }
    const auto pixmap = small.pixmap(QSize(24, 24));
    CHECK(pixmap.size() == QSize(24, 24) * pixmap.devicePixelRatio());
    CHECK(pixmap.devicePixelRatio() == Approx(qApp->devicePixelRatio()));
    CHECK(pixmap.cacheKey() == small.pixmap(QSize(24, 24)).cacheKey());
    CHECK(pixmap.cacheKey() != small.pixmap(QSize(48, 48)).cacheKey());
    auto clone = small;
    clone.detach();
    CHECK(paintIcon(clone, QSize(64, 64), 2.0) == paintIcon(small, QSize(64, 64), 2.0));
}

TEST_CASE_METHOD(VectorIconTest, "VectorIcon preserves color transparency and aspect ratio", "[VectorIcon]") {
    const auto source = write("wide.svg", R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 16"><rect width="32" height="16" fill="#203c81" fill-opacity="0.5"/></svg>)");
    const auto icon = VectorIcon(source).icon();
    CHECK(VectorIcon(source).defaultSize() == QSize(32, 16));
    const auto resource = loadPixmapResource(source);
    CHECK(resource.size() == QSize(32, 16) * resource.devicePixelRatio());
    CHECK(resource.devicePixelRatio() == Approx(qApp->devicePixelRatio()));
    CHECK(icon.actualSize(QSize(48, 48)) == QSize(48, 24));
    const auto image = paintIcon(icon, QSize(48, 48), 2.0);
    CHECK(image.pixelColor(48, 4).alpha() == 0);
    const auto center = image.pixelColor(48, 48);
    CHECK(center.red() == Approx(32).margin(1));
    CHECK(center.green() == Approx(60).margin(1));
    CHECK(center.blue() == Approx(129).margin(1));
    CHECK(center.alpha() == Approx(128).margin(1));
    // Painter scaling must request more pixels, just like display scaling.
    QImage transformed(QSize(96, 48), QImage::Format_ARGB32_Premultiplied);
    transformed.fill(Qt::transparent);
    {
        QPainter painter(&transformed);
        painter.scale(2, 2);
        icon.paint(&painter, QRect(0, 0, 48, 24));
    }
    CHECK(transformed == paintIcon(icon, QSize(48, 24), 2.0).convertToFormat(transformed.format()));
}

TEST_CASE_METHOD(VectorIconTest, "VectorIcon tints keep alpha and do not mix cached colors", "[VectorIcon]") {
    const auto source = write("tint.svg", R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><rect x="4" y="4" width="8" height="8" fill="#203c81" fill-opacity="0.5"/></svg>)");
    const VectorIcon vector(source);
    const auto green = vector.icon(QColor(0, 255, 0, 128));
    const auto magenta = vector.icon(Qt::magenta);
    REQUIRE_FALSE(green.isNull());
    CHECK(green.cacheKey() == VectorIcon(source).icon(QColor(0, 255, 0, 128)).cacheKey());
    CHECK(green.cacheKey() != magenta.cacheKey());

    const auto rendered = paintIcon(green, {24, 24}, 1.5);
    CHECK(rendered.size() == QSize(36, 36));
    CHECK(rendered.pixelColor(0, 0).alpha() == 0);
    const auto sourcePixel = paintIcon(vector.icon(), {24, 24}, 1.5).pixelColor(18, 18);
    const auto greenPixel = rendered.pixelColor(18, 18);
    const auto magentaPixel = paintIcon(magenta, {24, 24}, 1.5).pixelColor(18, 18);
    CHECK(greenPixel == QColor(0, 255, 0, qRound(sourcePixel.alpha() * 128.0 / 255.0)));
    CHECK(magentaPixel == QColor(255, 0, 255, sourcePixel.alpha()));
    CHECK(green.pixmap({24, 24}, QIcon::Selected).toImage() == green.pixmap({24, 24}).toImage());
    CHECK(sourcePixel.green() == Approx(60).margin(1));
}

TEST_CASE_METHOD(VectorIconTest, "VectorIcon separates checked states and handles partial variants", "[VectorIcon]") {
    solid("switch_on.svg", "#ff0000");
    solid("switch_off.svg", "#0000ff");
    solid("switch.svg", "#00ff00");
    const auto icon = VectorIcon(path("switch.svg")).icon();
    CHECK(paintIcon(icon, {16, 16}, 1, QIcon::On).pixelColor(8, 8) == QColor(Qt::red));
    CHECK(paintIcon(icon, {16, 16}, 1, QIcon::Off).pixelColor(8, 8) == QColor(Qt::blue));
    const auto disabled = icon.pixmap({16, 16}, QIcon::Disabled, QIcon::On);
    CHECK_FALSE(disabled.isNull());
    CHECK(disabled.toImage() != icon.pixmap({16, 16}, QIcon::Normal, QIcon::On).toImage());
    solid("partial_on.svg", "#ff0000");
    solid("partial.svg", "#00ff00");
    const auto partial = VectorIcon(path("partial.svg")).icon();
    CHECK(paintIcon(partial, {16, 16}, 1, QIcon::On).pixelColor(8, 8) == QColor(Qt::red));
    CHECK(paintIcon(partial, {16, 16}, 1, QIcon::Off).pixelColor(8, 8) == QColor(Qt::green));
    solid("alone_on.svg", "#ff0000");
    const auto alone = VectorIcon(path("alone.svg")).icon();
    CHECK(paintIcon(alone, {16, 16}, 1, QIcon::Off).pixelColor(8, 8) == QColor(Qt::red));
    const auto selected = icon.pixmap({16, 16}, QIcon::Selected);
    auto palette = qApp->palette();
    palette.setColor(QPalette::Highlight, Qt::magenta);
    qApp->setPalette(palette);
    CHECK(selected.cacheKey() != icon.pixmap({16, 16}, QIcon::Selected).cacheKey());
}

TEST_CASE_METHOD(VectorIconTest, "VectorIcon cache is bounded and eviction never invalidates live icons", "[VectorIcon]") {
    IconCache::setMaxEntries(2);
    const auto firstPath = solid("first.svg", "#ff0000");
    const auto first = VectorIcon(firstPath).icon();
    const auto second = VectorIcon(write("second.svg")).icon();
    CHECK(IconCache::size() == 2);
    CHECK(VectorIcon(firstPath.parent_path() / "." / "first.svg").icon().cacheKey() == first.cacheKey());
    VectorIcon(write("third.svg")); // Evicts second, not recently used first.
    CHECK(IconCache::size() == 2);
    CHECK(VectorIcon(firstPath).icon().cacheKey() == first.cacheKey());
    CHECK(VectorIcon(path("second.svg")).icon().cacheKey() != second.cacheKey());
    CHECK(QFile::remove(pathAsQString(firstPath)));
    IconCache::resetCache();
    CHECK(IconCache::size() == 0);
    CHECK(paintIcon(first, {72, 72}, 2).pixelColor(72, 72) == QColor(Qt::red)); // Parsed snapshot survives file removal.
    solid("first.svg", "#0000ff");
    CHECK(paintIcon(VectorIcon(firstPath).icon(), {72, 72}, 2).pixelColor(72, 72) == QColor(Qt::blue));
    QPixmapCache::setCacheLimit(1); // Too small for these pixmaps: rendering still works.
    CHECK_FALSE(first.pixmap({128, 128}).isNull());
    IconCache::setMaxEntries(0);
    CHECK(IconCache::size() == 0);
    CHECK(VectorIcon(firstPath).isValid());
    CHECK(IconCache::size() == 0);
}

TEST_CASE_METHOD(VectorIconTest, "Disabled vector icons are darker and QSS changes invalidate rasterizations", "[VectorIcon]") {
    auto& appearance = IconAppearance::instance();
    const auto previous = appearance.styleSheet();
    const auto oldBrightness = appearance.disabledBrightness(), oldOpacity = appearance.disabledOpacity();
    const auto restore = qScopeGuard([&] { appearance.setStyleSheet(previous); appearance.setDisabledBrightness(oldBrightness); appearance.setDisabledOpacity(oldOpacity); });
    appearance.setStyleSheet("QWidget#IconAppearance { qproperty-disabledBrightness: 0.25; qproperty-disabledOpacity: 1.0; }");
    const auto icon = VectorIcon(solid("disabled.svg","#c89664")).icon();
    const auto center = [&](QIcon::Mode mode) { const auto image = icon.pixmap({16,16},mode).toImage(); return image.pixelColor(image.width()/2,image.height()/2); };
    CHECK(center(QIcon::Normal) == QColor(200,150,100));
    const auto dark = center(QIcon::Disabled);
    CHECK(dark.red() == Approx(50).margin(1)); CHECK(dark.green() == Approx(38).margin(1)); CHECK(dark.blue() == Approx(25).margin(1));
    CHECK(dark.alpha() == 255);
    appearance.setStyleSheet("QWidget#IconAppearance { qproperty-disabledBrightness: 0.5; qproperty-disabledOpacity: 1.0; }");
    const auto lighter = center(QIcon::Disabled);
    CHECK(lighter.red() == Approx(100).margin(1)); CHECK(lighter != dark);
    CHECK(center(QIcon::Normal) == QColor(200,150,100));
}

TEST_CASE_METHOD(VectorIconTest, "VectorIcon normalizes dark white and black sources to the configured peak", "[VectorIcon][brightness]") {
    auto& appearance = IconAppearance::instance();
    appearance.setNormalizeBrightness(true);
    appearance.setBrightness(0.75);
    for (const auto* color : {"#404040", "#ffffff", "#000000"}) {
        CAPTURE(color);
        const auto icon = VectorIcon(solid(QString(color) + ".svg", color)).icon();
        const auto center = paintIcon(icon, {24, 24}, 2).pixelColor(24, 24);
        CHECK(center.red() == Approx(191).margin(1));
        CHECK(center.green() == center.red());
        CHECK(center.blue() == center.red());
        CHECK(center.alpha() == 255);
    }
}

TEST_CASE_METHOD(VectorIconTest, "Normalized icons preserve hue opacity and antialiased edges", "[VectorIcon][brightness]") {
    auto& appearance = IconAppearance::instance();
    appearance.setNormalizeBrightness(true);
    appearance.setBrightness(0.75);
    const auto icon = VectorIcon(write("translucent.svg", R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><rect x="4" y="4" width="8" height="8" fill="#204080" fill-opacity="0.5"/></svg>)")).icon();
    const auto image = paintIcon(icon, {24, 24}, 2);
    const auto center = image.pixelColor(24, 24);
    CHECK(center.red() == Approx(48).margin(2));
    CHECK(center.green() == Approx(96).margin(2));
    CHECK(center.blue() == Approx(191).margin(2));
    CHECK(center.alpha() == Approx(128).margin(1));
    CHECK(image.pixelColor(0, 0).alpha() == 0);

    const auto circle = [&](const QString& name, const QByteArray& fill) {
        return VectorIcon(write(name, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 16 16\"><circle cx=\"8.2\" cy=\"8.2\" r=\"6.3\" fill=\"" + fill + "\"/></svg>")).icon();
    };
    const auto dark = paintIcon(circle("dark.svg", "#202020"), {24, 24}, 1.5);
    const auto white = paintIcon(circle("white.svg", "#ffffff"), {24, 24}, 1.5);
    int edgePixels = 0;
    for (auto y = 0; y < dark.height(); ++y) {
        for (auto x = 0; x < dark.width(); ++x) {
            const auto a = dark.pixel(x, y), b = white.pixel(x, y);
            REQUIRE(qAlpha(a) == qAlpha(b));
            REQUIRE(std::abs(qRed(a) - qRed(b)) <= 1);
            if (qAlpha(a) > 0 && qAlpha(a) < 255) ++edgePixels;
        }
    }
    CHECK(edgePixels > 0);
}

TEST_CASE_METHOD(VectorIconTest, "Brightness changes reuse SVGs and update held icons and checked variants", "[VectorIcon][brightness]") {
    auto& appearance = IconAppearance::instance();
    const auto oldStyle = appearance.styleSheet();
    const auto restore = qScopeGuard([&] { appearance.setStyleSheet(oldStyle); });
    solid("vary_on.svg", "#202020");
    solid("vary_off.svg", "#ffffff");
    const auto icon = VectorIcon(path("vary.svg")).icon();
    const auto key = icon.cacheKey();
    appearance.setStyleSheet("QWidget#IconAppearance { qproperty-normalizeBrightness: true; qproperty-brightness: 0.75; }");
    CHECK(paintIcon(icon, {16, 16}, 1, QIcon::On).pixelColor(8, 8).red() == Approx(191).margin(1));
    CHECK(paintIcon(icon, {16, 16}, 1, QIcon::Off).pixelColor(8, 8).red() == Approx(191).margin(1));
    REQUIRE(QFile::remove(pathAsQString(path("vary_on.svg"))));
    REQUIRE(QFile::remove(pathAsQString(path("vary_off.svg"))));
    for (const auto factor : {0.5, 1.0, 0.0}) {
        appearance.setBrightness(factor);
        for (const auto state : {QIcon::On, QIcon::Off}) {
            const auto result = paintIcon(icon, {48, 48}, 2, state);
            CHECK(result.size() == QSize(96, 96));
            CHECK(result.pixelColor(48, 48).red() == Approx(qRound(255 * factor)).margin(1));
            CHECK(result.pixelColor(48, 48).alpha() == 255);
        }
        CHECK(icon.cacheKey() == key);
        CHECK(IconCache::size() == 1);
    }
}

TEST_CASE_METHOD(VectorIconTest, "Tinted and disabled icons apply brightness before disabled dimming", "[VectorIcon][brightness]") {
    auto& appearance = IconAppearance::instance();
    const auto oldDisabled = appearance.disabledBrightness(), oldOpacity = appearance.disabledOpacity();
    const auto restore = qScopeGuard([&] { appearance.setDisabledBrightness(oldDisabled); appearance.setDisabledOpacity(oldOpacity); });
    appearance.setNormalizeBrightness(true);
    appearance.setBrightness(0.75);
    appearance.setDisabledBrightness(0.25);
    appearance.setDisabledOpacity(1);
    const auto icon = VectorIcon(solid("tinted.svg", "#111111")).icon(QColor{32, 64, 128});
    const auto normal = paintIcon(icon, {16, 16}).pixelColor(8, 8);
    CHECK(normal.red() == Approx(48).margin(1));
    CHECK(normal.green() == Approx(96).margin(1));
    CHECK(normal.blue() == Approx(191).margin(1));
    const auto disabled = icon.pixmap({16, 16}, QIcon::Disabled).toImage().pixelColor(8, 8);
    CHECK(disabled.blue() == Approx(48).margin(1));
    CHECK(disabled.alpha() == 255);
}

TEST_CASE("Icon brightness measurement ignores margins and weak edge pixels", "[VectorIcon][brightness]") {
    QImage image{8, 8, QImage::Format_RGBA64_Premultiplied};
    image.fill(Qt::transparent);
    CHECK(measureIconBrightness(image) == 0);
    image.setPixelColor(4, 4, QColor{64, 32, 16});
    image.setPixelColor(0, 0, QColor{255, 255, 255, 1});
    CHECK(measureIconBrightness(image) == Approx(64.0 / 255).margin(0.0001));
    const auto oldAlpha = image.pixelColor(0, 0).alpha();
    normalizeIconBrightness(image, measureIconBrightness(image), 0.75);
    CHECK(image.pixelColor(4, 4).red() == Approx(191).margin(1));
    CHECK(image.pixelColor(0, 0).alpha() == oldAlpha);
    CHECK(image.pixelColor(7, 7).alpha() == 0);
    QImage empty;
    CHECK(measureIconBrightness(empty) == 0);
    normalizeIconBrightness(empty, 0, 1);
    CHECK(empty.isNull());
}

TEST_CASE_METHOD(VectorIconTest, "VectorIcon failures return safe values and do not poison other requests", "[VectorIcon]") {
    CHECK(VectorIcon({}).icon().isNull());
    CHECK(VectorIcon(path("missing.svg")).icon().isNull());
    CHECK(VectorIcon(write("invalid.svg", "not svg")).icon().isNull());
    const auto valid = write("valid.svg");
    CHECK(VectorIcon(valid).isValid());
    CHECK(loadSVGIcon(valid, 0).isNull());
    CHECK(loadSVGIcon(valid, -1).isNull());
    const auto icon = VectorIcon(valid).icon();
    CHECK(icon.pixmap(QSize(0, 16)).isNull());
    const auto button = std::unique_ptr<QToolButton>(View::createBitmapButton(icon, "Vector icon"));
    CHECK(button->iconSize() == QSize(16, 16));
    CHECK_FALSE(button->icon().isNull());
    CHECK(icon.availableSizes().isEmpty()); // Scalable, not a list of cached bitmaps.
    solid("missing.svg", "#00ff00");
    IconCache::resetCache();
    CHECK(VectorIcon(path("missing.svg")).isValid());
}

TEST_CASE_METHOD(VectorIconTest, "VectorIcon loads real toolbar tree and resource graphics", "[VectorIcon]") {
    for (const auto* name : {"GroupByType.svg", "Layer.svg", "Brush.svg", "Lock.svg", "Hidden.svg", "DefaultGameIcon.svg", "document.svg"}) {
        CAPTURE(name);
        const auto icon = loadSVGIcon(name, 16);
        REQUIRE_FALSE(icon.isNull());
        CHECK_FALSE(icon.pixmap(QSize(16, 16)).isNull());
        CHECK_FALSE(icon.pixmap(QSize(48, 48), QIcon::Disabled, QIcon::On).isNull());
    }
    const auto resource = loadPixmapResource("DefaultGameIcon.svg");
    CHECK(resource.size() == QSize(32, 32) * resource.devicePixelRatio());
    CHECK_FALSE(loadPixmapResource("AppIcon.png").isNull());
    CHECK(loadPixmapResource("missing.png").isNull());
    const auto unicodePath = solid(QString::fromUtf8("Licht-ä-光.svg"), "#00ff00");
    CHECK(VectorIcon(unicodePath).isValid());
}

TEST_CASE_METHOD(VectorIconTest, "VectorIcon caching cost comparison", "[.][VectorIconBenchmark]") {
    const auto source = write("bench.svg");
    constexpr int coldCount = 300;
    constexpr int warmCount = 10000;
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < coldCount; ++i) {
        IconCache::resetCache();
        const auto icon = VectorIcon(source).icon();
        (void)paintIcon(icon, {24, 24}, 2);
    }
    const auto cold = double(timer.nsecsElapsed()) / coldCount / 1000;
    const auto held = VectorIcon(source).icon();
    timer.restart();
    for (int i = 0; i < warmCount; ++i) {
        const auto icon = VectorIcon(source).icon();
        (void)paintIcon(icon, {24, 24}, 2);
    }
    const auto warm = double(timer.nsecsElapsed()) / warmCount / 1000;
    qInfo("VectorIcon load + paint, 24 px @2x: cold %.2f us, warm %.2f us (%.1fx)", cold, warm, cold / warm);
    CHECK(IconCache::size() == 1);
    CHECK(VectorIcon(source).icon().cacheKey() == held.cacheKey());
}
} // namespace TrenchBroom::IO
