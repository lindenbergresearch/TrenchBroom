#include "View/Theme/WidgetTextPaint.h"

#include <QApplication>
#include <QEvent>

#ifdef TB_QT_WIDGET_FONTSTASH
#include "FontStashPreferences.h"
#include "View/Theme/FontStashText.h"

#include <QPointer>
#include <QRawFont>
#include <QScopeGuard>
#include <private/qpaintengine_raster_p.h>
#include <private/qpixmap_raster_p.h>
#include <private/qrawfont_p.h>
#include <private/qwidget_p.h>

#include <cstring>
#endif

namespace TrenchBroom::View {
namespace {

thread_local bool useQtText = false;

#ifdef TB_QT_WIDGET_FONTSTASH

class WidgetTextEngine : public QRasterPaintEngine {
    FontStash::SdfSettings m_settings;

public:
    WidgetTextEngine(QPaintDevice& device, const FontStash::SdfSettings& settings)
        : QRasterPaintEngine(&device), m_settings(settings) {}

    bool drawCachedGlyphs(int count, const glyph_t* glyphs, const QFixedPoint* positions, QFontEngine* engine) override {
        const auto& matrix = state()->matrix;
        const auto scale = matrix.m11();
        if (useQtText || engine->isColorFont() || engine->type() == QFontEngine::Multi
            || (engine->fontDef.styleStrategy & QFont::NoAntialias)
            || matrix.type() > QTransform::TxScale || scale <= 0 || !qFuzzyCompare(scale, matrix.m22())
            || engine->fontDef.pixelSize <= 0 || engine->fontDef.pixelSize * scale > 256) {
            return QRasterPaintEngine::drawCachedGlyphs(count, glyphs, positions, engine);
        }

        // Qt has already shaped the run and calculated physical glyph positions.
        // Keep its clipping, pen, opacity and composition in the final blend.
        auto font = QRawFont{};
        QRawFontPrivate::get(font)->setFontEngine(engine);
        for (auto i = 0; i < count; ++i) {
            const auto glyph = fontStashGlyph(font, glyphs[i], positions[i].toPointF(), scale, &m_settings);
            if (glyph.mask.pixels.isNull()) {
                QRasterPaintEngine::drawCachedGlyphs(1, glyphs + i, positions + i, engine);
                continue;
            }
            const auto& pixels = glyph.mask.pixels;
            alphaPenBlt(pixels.constBits(), int(pixels.bytesPerLine()), 8, glyph.mask.origin.x(),
                glyph.mask.origin.y(), pixels.width(), pixels.height(), false);
        }
        return true;
    }
};

// Keep the Qt backing store and its paint engine intact. This image borrows the
// same writable pixels for the duration of the widget's paint event.
class WidgetTextDevice : public QImage {
    mutable WidgetTextEngine m_engine;

public:
    WidgetTextDevice(QImage& source, const FontStash::SdfSettings& settings)
        : QImage(source.bits(), source.width(), source.height(), source.bytesPerLine(), source.format())
        , m_engine(*this, settings) {
        setDevicePixelRatio(source.devicePixelRatio());
        setDotsPerMeterX(source.dotsPerMeterX());
        setDotsPerMeterY(source.dotsPerMeterY());
        setColorSpace(source.colorSpace());
    }

    QPaintEngine* paintEngine() const override { return &m_engine; }
};

QImage* rasterImage(QPaintDevice& device) {
    if (device.devType() == QInternal::Image) return static_cast<QImage*>(&device);
    if (device.devType() == QInternal::Pixmap) {
        auto* pixmap = static_cast<QPixmap&>(device).handle();
        if (pixmap && pixmap->classId() == QPlatformPixmap::RasterClass) return pixmap->buffer();
    }
    return nullptr;
}

#endif

} // namespace

ScopedQtTextPaint::ScopedQtTextPaint() : m_previous(useQtText) { useQtText = true; }
ScopedQtTextPaint::~ScopedQtTextPaint() { useQtText = m_previous; }

bool widgetFontStashAvailable() {
#ifdef TB_QT_WIDGET_FONTSTASH
    return std::strcmp(qVersion(), QT_VERSION_STR) == 0;
#else
    return false;
#endif
}

bool notifyWidgetTextPaint(QApplication& app, QObject* receiver, QEvent* event) {
#ifdef TB_QT_WIDGET_FONTSTASH
    if (event && receiver && event->type() == QEvent::Paint && receiver->isWidgetType()
        && !useQtText && widgetFontStashAvailable()) {
        const auto settings = fontStashPreferences();
        auto* widget = static_cast<QWidget*>(receiver);
        auto* data = QWidgetPrivate::get(widget);
        auto offset = QPoint{};
        auto* target = data->redirected(&offset);
        if (settings.enabled && widget->testAttribute(Qt::WA_WState_InPaintEvent)
            && target && !data->sharedPainter() && !target->paintingActive()) {
            auto* image = rasterImage(*target);
            auto* engine = target->paintEngine();
            if (image && !image->isNull() && image->depth() >= 24 && engine && !engine->isActive()) {
                auto device = WidgetTextDevice(*image, settings.sdf);
                if (!device.isNull()) {
                    // Pin the pixels in case a nested event replaces the backing image.
                    const auto keepPixels = *image;
                    device.paintEngine()->setSystemRect(engine->systemRect());
                    device.paintEngine()->setSystemClip(engine->systemClip());
                    const auto guard = QPointer<QWidget>(widget);
                    data->setRedirected(&device, offset);
                    const auto restore = qScopeGuard([&] {
                        if (guard) data->setRedirected(target, offset);
                    });
                    return app.QApplication::notify(receiver, event);
                }
            }
        }
    }
#endif
    return app.QApplication::notify(receiver, event);
}

} // namespace TrenchBroom::View
