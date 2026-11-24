// FontStash portions adapted from the PlugData NanoVG fork.
// Copyright (c) 2020 Stylus Labs
// Copyright (c) 2013 Mikko Mononen memon@inside.org
// See vendor/LICENSE.txt.

#include "fontstash/GlyphRasterizer.h"

#include <QPainterPath>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>


/* ------------------------------------------------------------------------------------------- */
namespace {

// Adapted from FontStash in the PlugData NanoVG fork. See vendor/LICENSE.txt.
// LRT: Added a little macro for switching between standard trigonometric and
// approx. fast trig. functions for testing purpose

float fontstashCos(float x) {
#ifdef SDF_USE_PRECISE_TRIG
    return std::cos(x);
#else
    const auto x2 = x * x;
    const auto numerator = -(-39251520 + x2 * (18471600 + x2 * (-1075032 + 14615 * x2)));
    const auto denominator = 39251520 + x2 * (1154160 + x2 * (16632 + x2 * 127));
    return numerator / denominator;
#endif
}

float fontstashAcos(float x) {
#ifdef SDF_USE_PRECISE_TRIG
    return std::acos(x);
#else
    x = std::clamp(x, -1.0f, 1.0f);
    if (std::abs(x) <= 0.5f) return 1.57079632679f - x - x * x * x / 6.0f;
    return std::sqrt(1.0f - x) * (1.57079632679f + 0.5f * x);
#endif
}

}

/* ------------------------------------------------------------------------------------------- */

#define STBTT_cos(x) fontstashCos(x)
#define STBTT_acos(x) fontstashAcos(x)
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION

#include "stb_truetype.h"

namespace TrenchBroom::FontStash {
namespace {

stbtt_vertex vertex(int unitsPerPixel, unsigned char type, const QPointF &end,
                    const QPointF &control = {}, const QPointF &second = {}) {
    // Qt outlines are y-down and stb uses y-up coordinates.
    return {
        short(qRound(end.x() * unitsPerPixel)), short(qRound(-end.y() * unitsPerPixel)),
        short(qRound(control.x() * unitsPerPixel)), short(qRound(-control.y() * unitsPerPixel)),
        short(qRound(second.x() * unitsPerPixel)), short(qRound(-second.y() * unitsPerPixel)),
        type,
        0
    };
}

// Qt expresses quadratic segments as cubics. Recover their control point
// exactly and approximate other cubics to subtexel accuracy for the SDF solver.
void appendSdfCurve(std::vector<stbtt_vertex> &vertices, int unitsPerPixel, const QPointF &start,
                    const QPointF &a, const QPointF &b, const QPointF &end) {
    const auto first = (3.f * a - start) / 2.f;
    const auto second = (3.f * b - end) / 2.f;
    if ((first - second).manhattanLength() <= 0.75f) {
        vertices.push_back(vertex(unitsPerPixel, STBTT_vcurve, end, (first + second) / 2));
        return;
    }

    const auto left = (start + a) / 2.f;
    const auto center = (a + b) / 2.f;
    const auto right = (b + end) / 2.f;
    const auto innerLeft = (left + center) / 2.f;
    const auto innerRight = (center + right) / 2.f;
    const auto mid = (innerLeft + innerRight) / 2.f;

    appendSdfCurve(vertices, unitsPerPixel, start, left, innerLeft, mid);
    appendSdfCurve(vertices, unitsPerPixel, mid, innerRight, right, end);
}

std::vector<stbtt_vertex> outlineVertices(const QPainterPath &outline, qreal scale, int unitsPerPixel, bool sdf) {
    auto vertices = std::vector<stbtt_vertex> {};
    vertices.reserve(outline.elementCount());
    auto start = QPointF {};
    auto previous = QPointF {};

    for (auto i = 0; i < outline.elementCount(); ++i) {
        const auto element = outline.elementAt(i);
        const auto point = QPointF(element) * scale;
        if (element.isMoveTo()) {
            if (previous != start) vertices.push_back(vertex(unitsPerPixel, STBTT_vline, start));
            vertices.push_back(vertex(unitsPerPixel, STBTT_vmove, point));
            start = previous = point;
        } else if (element.isLineTo()) {
            vertices.push_back(vertex(unitsPerPixel, STBTT_vline, point));
            previous = point;
        } else if (element.isCurveTo()) {
            const auto control = QPointF(outline.elementAt(++i)) * scale;
            const auto end = QPointF(outline.elementAt(++i)) * scale;
            if (sdf) appendSdfCurve(vertices, unitsPerPixel, previous, point, control, end);
            else vertices.push_back(vertex(unitsPerPixel, STBTT_vcubic, end, point, control));
            previous = end;
        }
    }

    if (previous != start) vertices.push_back(vertex(unitsPerPixel, STBTT_vline, start));
    return vertices;
}

GlyphMask rasterize(const QPainterPath &outline, qreal scale, QPointF phase, const SdfSettings *sdf) {
    const auto bounds = QTransform::fromScale(scale, scale).mapRect(outline.controlPointRect());
    // Bound both allocations and work for unusual glyphs from user fonts.
    const auto unitsPerPixel = sdf ? sdf->unitsPerPixel : SdfSettings {}.unitsPerPixel;
    // The quadratic control point can reach up to twice the cubic's extent.
    constexpr auto limit = 16383.0 / MaximumSdfSettings.unitsPerPixel;
    if (bounds.isEmpty() ||
        !QRectF(-limit, -limit, 2.0 * limit, 2.0 * limit).contains(bounds) ||
        outline.elementCount() > 1024)
        return {};

    const auto rect = bounds.translated(phase).toAlignedRect();
    if (rect.width() > 256 || rect.height() > 256) return {};
    auto vertices = outlineVertices(outline, scale, unitsPerPixel, sdf != nullptr);
    if (vertices.empty() || vertices.size() > 1536) return {};

    const auto padding = sdf ? sdf->padding : 0;
    const auto atlasRect = rect.adjusted(-padding, -padding, padding, padding);
    auto pixels = QImage(atlasRect.size(), QImage::Format_Alpha8);

    if (pixels.isNull()) return {};

    if (sdf) {
        const auto data = std::unique_ptr<unsigned char, decltype(&std::free)> { stbtt__GetShapeSDF(
            vertices.data(), int(vertices.size()), 1.0f / float(unitsPerPixel),
            atlasRect.x(), atlasRect.y(), atlasRect.right() + 1, atlasRect.bottom() + 1,
            127, float(sdf->distancePerTexel), nullptr), &std::free };

        if (!data) return {};

        for (auto y = 0; y < pixels.height(); ++y) {
            std::memcpy(pixels.scanLine(y), data.get() + y * pixels.width(), size_t(pixels.width()));
        }

        return { std::move(pixels), atlasRect.topLeft() };
    }

    pixels.fill(0);
    auto bitmap = stbtt__bitmap { pixels.width(), pixels.height(), int(pixels.bytesPerLine()), pixels.bits() };
    stbtt_Rasterize(
        &bitmap, 0.5f, vertices.data(), int(vertices.size()), 1.0f / float(unitsPerPixel),
        1.0f / float(unitsPerPixel), float(phase.x()), float(phase.y()), rect.x(), rect.y(), 1, nullptr
    );

    return { std::move(pixels), rect.topLeft() };
}

qreal sampleDistance(const QImage &pixels, qreal x, qreal y) {
    // Atlas sample coordinates refer to texel centers.
    const auto left = int(std::floor(x));
    const auto top = int(std::floor(y));
    if (left < 0 || top < 0 || left + 1 >= pixels.width() || top + 1 >= pixels.height()) return 0;
    const auto *row = pixels.constScanLine(top);
    const auto *next = pixels.constScanLine(top + 1);

    return std::lerp(
        std::lerp(qreal(row[left]), qreal(row[left + 1]), x - left),
        std::lerp(qreal(next[left]), qreal(next[left + 1]), x - left), y - top
    );
}

}

GlyphMask glyphBitmap(const QPainterPath &outline, qreal scale, QPointF phase) {
    return rasterize(outline, scale, phase, nullptr);
}

SdfSettings validatedSdfSettings(SdfSettings settings) {
    constexpr auto defaults = SdfSettings {};
    const auto min = MinimumSdfSettings;
    const auto max = MaximumSdfSettings;

    settings.textWeight = std::isfinite(settings.textWeight)
                          ? std::clamp(settings.textWeight, min.textWeight, max.textWeight) : defaults.textWeight;
    settings.unitsPerPixel = std::clamp(settings.unitsPerPixel, min.unitsPerPixel, max.unitsPerPixel);
    settings.distancePerTexel = std::isfinite(settings.distancePerTexel)
                                ? std::clamp(settings.distancePerTexel, min.distancePerTexel, max.distancePerTexel)
                                : defaults.distancePerTexel;

    settings.padding = std::clamp(settings.padding, min.padding, max.padding);

    return settings;
}

GlyphMask glyphSdf(const QPainterPath &outline, qreal scale, const SdfSettings &settings) {
    return rasterize(outline, scale, {}, &settings);
}

qreal sdfAtlasScale(qreal pixelSize, qreal deviceScale, const SdfSettings &settings) {
    // Limit downsampling so the coverage ramp reaches zero at the exterior.
    const auto maxDownsample =
        std::min(
            4.0,
            127.5 / (settings.distancePerTexel * settings.textWeight * settings.textWeight)
        );

    return std::min(64.0 / pixelSize, maxDownsample * deviceScale);
}

GlyphMask sampleGlyphSdf(const GlyphMask &sdf, qreal scale, QPointF phase, const SdfSettings &settings) {
    const auto bounds = QRectF(QPointF(sdf.origin) * scale + phase, QSizeF(sdf.pixels.size()) * scale).toAlignedRect();
    auto pixels = QImage(bounds.size(), QImage::Format_Alpha8);
    if (pixels.isNull()) return {};

    // CPU version of PlugData's superSDF shader. Use the same four samples
    // per device pixel and the same edge weight before compositing in Qt.
    const auto slope = scale / (settings.textWeight * settings.distancePerTexel);
    const auto coverage = [&](qreal x, qreal y) {
        return std::clamp((sampleDistance(sdf.pixels, x, y) - 127.5) * slope + settings.textWeight, 0.0, 1.0);
    };

    const auto quarterPixel = 0.25 / scale;
    for (auto y = 0; y < pixels.height(); ++y) {
        auto *row = pixels.scanLine(y);
        const auto sy = (bounds.y() + y + 0.5 - phase.y()) / scale - sdf.origin.y() - 0.5;
        for (auto x = 0; x < pixels.width(); ++x) {
            const auto sx = (bounds.x() + x + 0.5 - phase.x()) / scale - sdf.origin.x() - 0.5;
            const auto sum =
                coverage(sx - quarterPixel, sy - quarterPixel)
                + coverage(sx + quarterPixel, sy - quarterPixel)
                + coverage(sx - quarterPixel, sy + quarterPixel)
                + coverage(sx + quarterPixel, sy + quarterPixel
                );

            row[x] = uchar(qRound(sum * (255.0 / 4.0)));
        }
    }

    return { std::move(pixels), bounds.topLeft() };
}

} // namespace TrenchBroom::FontStash
