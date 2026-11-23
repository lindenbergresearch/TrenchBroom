#include "View/Theme/FontStashText.h"

#include <QCache>
#include <QGlyphRun>
#include <QGuiApplication>
#include <QPainterPath>
#include <QPointer>
#include <QTextLayout>

#include <optional>

namespace TrenchBroom::View {
namespace {

struct GlyphKey {
    QRawFont font;
    quint32 index;
    qreal scale;
    QPointF phase;
    std::optional<FontStash::SdfSettings> sdf;

    bool operator==(const GlyphKey&) const = default;

    friend size_t qHash(const GlyphKey& key, size_t seed) {
        auto hash = qHashMulti(seed, key.font, key.index, key.scale, key.phase.x(), key.phase.y());
        if (const auto& sdf = key.sdf)
            hash = qHashMulti(hash, sdf->textWeight, sdf->unitsPerPixel, sdf->distancePerTexel, sdf->padding);
        return hash;
    }
};

class GlyphCache : public QObject {
    QCache<GlyphKey, FontStashGlyph> m_masks{4 * 1024 * 1024};
    QCache<GlyphKey, FontStashGlyph> m_atlas{8 * 1024 * 1024};

    static void insert(QCache<GlyphKey, FontStashGlyph>& cache, const GlyphKey& key, const FontStashGlyph& glyph) {
        const auto bytes = qsizetype(sizeof(GlyphKey) + sizeof(FontStashGlyph)) + glyph.mask.pixels.sizeInBytes();
        cache.insert(key, new FontStashGlyph(glyph), bytes);
    }

    FontStashGlyph field(const GlyphKey& key) {
        auto atlasSettings = *key.sdf;
        const auto atlasScale = FontStash::sdfAtlasScale(key.font.pixelSize(), key.scale, atlasSettings);
        // Weight affects sampling. Distance, padding and precision affect the field itself.
        atlasSettings.textWeight = FontStash::SdfSettings{}.textWeight;
        const auto atlasKey = GlyphKey{key.font, key.index, atlasScale, {}, atlasSettings};
        if (const auto* glyph = m_atlas.object(atlasKey)) return *glyph;
        const auto outline = key.font.pathForGlyph(key.index);
        const auto glyph = FontStashGlyph{FontStash::glyphSdf(outline, atlasScale, atlasSettings), outline.isEmpty()};
        insert(m_atlas, atlasKey, glyph);
        return glyph;
    }

public:
    GlyphCache() : QObject(qGuiApp) {
        connect(qGuiApp, &QGuiApplication::fontDatabaseChanged, this, [this] {
            m_masks.clear();
            m_atlas.clear();
        });
    }

    FontStashGlyph mask(const GlyphKey& key) {
        if (const auto* cached = m_masks.object(key)) return *cached;
        auto glyph = FontStashGlyph{};
        if (key.sdf) {
            glyph = field(key);
            if (!glyph.mask.pixels.isNull()) {
                const auto scale = FontStash::sdfAtlasScale(key.font.pixelSize(), key.scale, *key.sdf);
                glyph.mask = FontStash::sampleGlyphSdf(glyph.mask, key.scale / scale, key.phase, *key.sdf);
            }
        } else {
            const auto outline = key.font.pathForGlyph(key.index);
            glyph = {FontStash::glyphBitmap(outline, key.scale, key.phase), outline.isEmpty()};
        }
        insert(m_masks, key, glyph);
        return glyph;
    }
};

GlyphCache& glyphCache() {
    static QPointer<GlyphCache> cache;
    if (!cache) cache = new GlyphCache();
    return *cache;
}

void compositeGlyph(QImage& image, const FontStash::GlyphMask& glyph, const QRect& clip, QRgb color) {
    const auto rect = QRect(glyph.origin, glyph.pixels.size()).intersected(clip);
    for (auto y = rect.top(); y <= rect.bottom(); ++y) {
        const auto* alpha = glyph.pixels.constScanLine(y - glyph.origin.y());
        auto* target = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (auto x = rect.left(); x <= rect.right(); ++x) {
            const auto coverage = alpha[x - glyph.origin.x()] * qAlpha(color) / 255;
            const auto opacity = coverage + qAlpha(target[x]) * (255 - coverage) / 255;
            target[x] = qPremultiply(qRgba(qRed(color), qGreen(color), qBlue(color), opacity));
        }
    }
}

} // namespace

FontStashGlyph fontStashGlyph(const QRawFont& font, quint32 glyph, QPointF baseline,
                            qreal deviceScale, const FontStash::SdfSettings* sdf) {
    const auto position = QPointF(qRound(baseline.x() * 4) / 4.0, qRound(baseline.y() * 4) / 4.0);
    const auto origin = QPoint(qFloor(position.x()), qFloor(position.y()));
    const auto phase = position - QPointF(origin);
    auto result = glyphCache().mask({font, glyph, deviceScale, phase, sdf ? std::optional{*sdf} : std::nullopt});
    result.mask.origin += origin;
    return result;
}

QImage fontStashTextLine(const QPaintDevice& fontDevice, const QString& text, const QFont& font,
                         QColor color, const QRectF& content, QSize imageSize, qreal deviceScale,
                         Qt::Alignment alignment, const FontStash::SdfSettings* sdf) {
    if (text.isEmpty() || content.isEmpty()) return {};
    auto layout = QTextLayout(text, font, &fontDevice);
    auto options = QTextOption(alignment);
    options.setTextDirection(text.isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight);
    options.setWrapMode(QTextOption::NoWrap);
    layout.setTextOption(options);
    layout.beginLayout();
    auto line = layout.createLine();
    if (!line.isValid()) return {};
    line.setLineWidth(content.width());
    layout.endLayout();

    auto top = content.y();
    if (alignment & Qt::AlignVCenter) top += qFloor((content.height() - qCeil(line.height())) / 2);
    if (alignment & Qt::AlignBottom) top += content.height() - qCeil(line.height());
    line.setPosition(QPointF(content.x(), top));

    auto image = QImage(imageSize, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) return {};
    image.fill(Qt::transparent);
    const auto clip = QRectF(content.topLeft() * deviceScale, content.size() * deviceScale)
        .toAlignedRect().intersected(image.rect());
    const auto runs = layout.glyphRuns(0, -1, QTextLayout::RetrieveGlyphIndexes
        | QTextLayout::RetrieveGlyphPositions | QTextLayout::RetrieveStringIndexes);
    for (const auto& run : runs) {
        const auto raw = run.rawFont();
        if (!raw.isValid() || raw.pixelSize() <= 0) return {};
        const auto indexes = run.glyphIndexes();
        const auto positions = run.positions();
        const auto textIndexes = run.stringIndexes();
        if (positions.size() != indexes.size() || textIndexes.size() != indexes.size()) return {};
        for (auto i = qsizetype{0}; i < indexes.size(); ++i) {
            const auto glyph = fontStashGlyph(raw, indexes[i], positions[i] * deviceScale, deviceScale, sdf);
            if (glyph.emptyOutline) {
                const auto index = textIndexes[i];
                if (index < 0 || index >= text.size()) return {};
                const auto source = text.at(index);
                if (source.isSpace() || source.category() == QChar::Other_Format) continue;
                return {};
            }
            if (glyph.mask.pixels.isNull()) return {};
            compositeGlyph(image, glyph.mask, clip, color.rgba());
        }
    }
    image.setDevicePixelRatio(deviceScale);
    return image;
}

} // namespace TrenchBroom::View
