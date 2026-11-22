#include "View/Inspector/Layer/LayerDetailText.h"

#include "PreferenceManager.h"
#include "fontstash/GlyphRasterizer.h"

#include <QCache>
#include <QGlyphRun>
#include <QGuiApplication>
#include <QLabel>
#include <QPainterPath>
#include <QPointer>
#include <QStyle>
#include <QTextLayout>

#include <algorithm>
#include <cmath>

namespace TrenchBroom::View {
namespace {

struct TextKey {
    QString text;
    QFont font;
    QRgb color;
    QRect contents;
    QSize size;
    QSize dpi;
    Qt::Alignment alignment;
    qreal ratio;
    LayerDetailTextRenderer renderer;

    bool operator==(const TextKey&) const = default;

    friend size_t qHash(const TextKey& key, size_t seed) {
        return qHashMulti(seed, key.text, key.font, key.color, key.contents.topLeft(), key.contents.size(),
                          key.size, key.dpi, int(key.alignment), key.ratio, int(key.renderer));
    }
};

struct GlyphKey {
    QRawFont font;
    quint32 index;
    qreal scale;
    QPointF phase;
    bool sdf;

    bool operator==(const GlyphKey&) const = default;

    friend size_t qHash(const GlyphKey& key, size_t seed) {
        return qHashMulti(seed, key.font, key.index, key.scale, key.phase.x(), key.phase.y(), key.sdf);
    }
};

struct CachedGlyph {
    FontStash::GlyphMask mask;
    bool emptyOutline;
};

class DetailTextCache : public QObject {
public:
    DetailTextCache() : QObject(qGuiApp) {
        connect(qGuiApp, &QGuiApplication::fontDatabaseChanged, this, [this] {
            lines.clear();
            m_glyphs.clear();
        });
    }

    CachedGlyph glyph(const GlyphKey& key) {
        if (const auto* cached = m_glyphs.object(key)) return *cached;

        const auto outline = key.font.pathForGlyph(key.index);
        const auto result = CachedGlyph{
            key.sdf ? FontStash::glyphSdf(outline, key.scale) : FontStash::glyphBitmap(outline, key.scale, key.phase),
            outline.isEmpty()};
        m_glyphs.insert(key, new CachedGlyph(result), qsizetype(sizeof(CachedGlyph) + sizeof(GlyphKey)) + result.mask.pixels.sizeInBytes());
        return result;
    }

    QCache<TextKey, QImage> lines{4 * 1024 * 1024};

private:
    QCache<GlyphKey, CachedGlyph> m_glyphs{4 * 1024 * 1024};
};

DetailTextCache& detailTextCache() {
    static QPointer<DetailTextCache> cache;
    if (!cache) cache = new DetailTextCache();
    return *cache;
}

QRect textRect(const QLabel& label, Qt::Alignment alignment) {
    auto rect = label.contentsRect();
    const auto margin = label.margin();
    rect.adjust(margin, margin, -margin, -margin);
    auto indent = label.indent();
    if (indent < 0 && label.frameWidth() > 0)
        indent = label.fontMetrics().horizontalAdvance(QLatin1Char('x')) / 2 - margin;
    if (indent > 0) {
        if (alignment & Qt::AlignLeft) rect.setLeft(rect.left() + indent);
        if (alignment & Qt::AlignRight) rect.setRight(rect.right() - indent);
        if (alignment & Qt::AlignTop) rect.setTop(rect.top() + indent);
        if (alignment & Qt::AlignBottom) rect.setBottom(rect.bottom() - indent);
    }
    return rect;
}

void compositeGlyph(QImage& image, const FontStash::GlyphMask& glyph,
                    QPoint topLeft, const QRect& clip, QRgb color) {
    const auto rect = QRect(topLeft, glyph.pixels.size()).intersected(clip);
    for (auto y = rect.top(); y <= rect.bottom(); ++y) {
        const auto* alpha = glyph.pixels.constScanLine(y - topLeft.y());
        auto* target = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (auto x = rect.left(); x <= rect.right(); ++x) {
            const auto coverage = alpha[x - topLeft.x()] * qAlpha(color) / 255;
            const auto opacity = coverage + qAlpha(target[x]) * (255 - coverage) / 255;
            target[x] = qPremultiply(qRgba(qRed(color), qGreen(color), qBlue(color), opacity));
        }
    }
}

QImage rasterizeLine(const QLabel& label, const TextKey& key, DetailTextCache& cache) {
    if (key.contents.isEmpty()) return {};
    auto layout = QTextLayout(key.text, key.font, &label);
    auto options = QTextOption(key.alignment);
    options.setTextDirection(key.text.isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight);
    options.setWrapMode(QTextOption::NoWrap);
    layout.setTextOption(options);
    layout.beginLayout();
    auto line = layout.createLine();
    if (!line.isValid()) return {};
    line.setLineWidth(key.contents.width());
    layout.endLayout();

    auto top = key.contents.y();
    if (key.alignment & Qt::AlignVCenter) top += (key.contents.height() - qCeil(line.height())) / 2;
    if (key.alignment & Qt::AlignBottom) top += key.contents.height() - qCeil(line.height());
    line.setPosition(QPointF(key.contents.x(), top));

    auto image = QImage(key.size, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) return {};
    image.fill(Qt::transparent);
    const auto clip = QRectF(QPointF(key.contents.topLeft()) * key.ratio, QSizeF(key.contents.size()) * key.ratio)
        .toAlignedRect().intersected(image.rect());
    const auto sdf = key.renderer == LayerDetailTextRenderer::FontStashSdf;
    const auto runs = layout.glyphRuns(0, -1, QTextLayout::RetrieveGlyphIndexes
        | QTextLayout::RetrieveGlyphPositions | QTextLayout::RetrieveStringIndexes);
    for (const auto& run : runs) {
        const auto font = run.rawFont();
        if (!font.isValid() || font.pixelSize() <= 0) return {};
        // PlugData uses a 64 px atlas. Very small text needs a smaller field
        // so the coverage ramp can reach zero outside the glyph.
        const auto scale = sdf ? std::min(64.0 / font.pixelSize(), 4 * key.ratio) : key.ratio;
        const auto indexes = run.glyphIndexes();
        const auto positions = run.positions();
        const auto textIndexes = run.stringIndexes();
        if (positions.size() != indexes.size() || textIndexes.size() != indexes.size()) return {};
        for (auto i = qsizetype{0}; i < indexes.size(); ++i) {
            const auto position = positions[i] * key.ratio;
            const auto devicePosition = QPointF(qRound(position.x() * 4) / 4.0, qRound(position.y() * 4) / 4.0);
            const auto origin = QPoint(qFloor(devicePosition.x()), qFloor(devicePosition.y()));
            const auto phase = devicePosition - QPointF(origin);
            const auto glyph = cache.glyph({font, indexes[i], scale, sdf ? QPointF{} : phase, sdf});
            if (glyph.emptyOutline) {
                // Some fonts (e.g. color emoji) have bitmap glyphs with no
                // vector path. Only skip characters that are actually blank.
                const auto textIndex = textIndexes[i];
                if (textIndex < 0 || textIndex >= key.text.size()) return {};
                const auto source = key.text.at(textIndex);
                if (source.isSpace() || source.category() == QChar::Other_Format) continue;
                return {};
            }
            if (glyph.mask.pixels.isNull()) return {};
            const auto mask = sdf ? FontStash::sampleGlyphSdf(glyph.mask, key.ratio / scale, phase) : glyph.mask;
            if (mask.pixels.isNull()) return {};
            compositeGlyph(image, mask, origin + mask.origin, clip, key.color);
        }
    }
    image.setDevicePixelRatio(key.ratio);
    return image;
}

}

Preference<int>& layerDetailTextRendererPreference() {
    return PreferenceManager::instance().dynamicPreference("Layer tree/Detail text renderer", int(LayerDetailTextRenderer::Qt));
}

LayerDetailTextRenderer layerDetailTextRenderer() {
    switch (pref(layerDetailTextRendererPreference())) {
        case int(LayerDetailTextRenderer::FontStashBitmap): return LayerDetailTextRenderer::FontStashBitmap;
        case int(LayerDetailTextRenderer::FontStashSdf): return LayerDetailTextRenderer::FontStashSdf;
        default: return LayerDetailTextRenderer::Qt;
    }
}

QImage renderLayerDetailText(const QLabel& label, LayerDetailTextRenderer renderer, qreal devicePixelRatio) {
    if (renderer == LayerDetailTextRenderer::Qt) return {};
    const auto text = label.text();
    const auto font = label.font();
    if (text.isEmpty() || text.contains(QLatin1Char('\n'))
        || label.wordWrap() || font.underline() || font.overline() || font.strikeOut()) return {};
    const auto align = QStyle::visualAlignment(text.isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight, label.alignment());
    const auto color = label.palette().color(label.foregroundRole()).rgba();
    const auto key = TextKey{text, font, color, textRect(label, align),
        QSize(qCeil(label.width() * devicePixelRatio), qCeil(label.height() * devicePixelRatio)),
        QSize(label.logicalDpiX(), label.logicalDpiY()), align, devicePixelRatio, renderer};
    auto& cache = detailTextCache();
    if (const auto* image = cache.lines.object(key)) return *image;
    const auto result = rasterizeLine(label, key, cache);
    const auto bytes = qsizetype(sizeof(TextKey) + sizeof(QImage)) + text.size() * qsizetype(sizeof(QChar)) + result.sizeInBytes();
    cache.lines.insert(key, new QImage(result), bytes);
    return result;
}

} // namespace TrenchBroom::View
