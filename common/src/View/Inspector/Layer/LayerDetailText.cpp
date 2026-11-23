#include "View/Inspector/Layer/LayerDetailText.h"

#include "FontStashPreferences.h"
#include "PreferenceManager.h"
#include "View/Theme/FontStashText.h"

#include <QCache>
#include <QGuiApplication>
#include <QLabel>
#include <QPointer>
#include <QStyle>

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
    FontStash::SdfSettings sdf;

    bool operator==(const TextKey&) const = default;

    friend size_t qHash(const TextKey& key, size_t seed) {
        return qHashMulti(seed, key.text, key.font, key.color, key.contents.topLeft(), key.contents.size(),
                          key.size, key.dpi, int(key.alignment), key.ratio, int(key.renderer),
                          key.sdf.textWeight, key.sdf.unitsPerPixel, key.sdf.distancePerTexel, key.sdf.padding);
    }
};

class DetailTextCache : public QObject {
public:
    DetailTextCache() : QObject(qGuiApp) {
        connect(qGuiApp, &QGuiApplication::fontDatabaseChanged, this, [this] { lines.clear(); });
    }

    QCache<TextKey, QImage> lines{4 * 1024 * 1024};
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

}

Preference<int>& layerDetailTextRendererPreference() {
    return PreferenceManager::instance().dynamicPreference("Layer tree/Detail text renderer", int(LayerDetailTextRenderer::Application));
}

LayerDetailTextRenderer layerDetailTextRenderer() {
    switch (pref(layerDetailTextRendererPreference())) {
        case int(LayerDetailTextRenderer::FontStashBitmap): return LayerDetailTextRenderer::FontStashBitmap;
        case int(LayerDetailTextRenderer::FontStashSdf): return LayerDetailTextRenderer::FontStashSdf;
        case int(LayerDetailTextRenderer::Qt): return LayerDetailTextRenderer::Qt;
        default: return LayerDetailTextRenderer::Application;
    }
}

QImage renderLayerDetailText(const QLabel& label, LayerDetailTextRenderer renderer, qreal devicePixelRatio) {
    if (renderer == LayerDetailTextRenderer::Application || renderer == LayerDetailTextRenderer::Qt) return {};
    const auto text = label.text();
    const auto font = label.font();
    if (text.isEmpty() || text.contains(QLatin1Char('\n'))
        || label.wordWrap() || font.underline() || font.overline() || font.strikeOut()) return {};
    const auto align = QStyle::visualAlignment(text.isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight, label.alignment());
    const auto color = label.palette().color(label.foregroundRole()).rgba();
    const auto sdf = renderer == LayerDetailTextRenderer::FontStashSdf
        ? fontStashPreferences().sdf : FontStash::SdfSettings{};
    const auto key = TextKey{text, font, color, textRect(label, align),
        QSize(qCeil(label.width() * devicePixelRatio), qCeil(label.height() * devicePixelRatio)),
        QSize(label.logicalDpiX(), label.logicalDpiY()), align, devicePixelRatio, renderer, sdf};
    auto& cache = detailTextCache();
    if (const auto* image = cache.lines.object(key)) return *image;
    const auto result = fontStashTextLine(label, text, font, QColor::fromRgba(color), key.contents,
        key.size, key.ratio, align, renderer == LayerDetailTextRenderer::FontStashSdf ? &sdf : nullptr);
    const auto bytes = qsizetype(sizeof(TextKey) + sizeof(QImage)) + text.size() * qsizetype(sizeof(QChar)) + result.sizeInBytes();
    cache.lines.insert(key, new QImage(result), bytes);
    return result;
}

} // namespace TrenchBroom::View
