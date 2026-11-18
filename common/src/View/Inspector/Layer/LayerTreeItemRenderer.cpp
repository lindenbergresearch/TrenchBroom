#include "View/Inspector/Layer/LayerTreeItemRenderer.h"

#include "IO/PathQt.h"
#include "IO/VectorIcon.h"
#include "Model/BrushNode.h"
#include "Model/EntityNode.h"
#include "Model/GroupNode.h"
#include "Model/LayerNode.h"
#include "PreferenceManager.h"
#include "View/Theme/ThemeManager.h"
#include "TrenchBroomApp.h"
#include "Preferences.h"
#include "View/Preferences/FontUtils.h"
#include "View/Theme/QtUtils.h"

#include <QApplication>
#include <QFontMetricsF>
#include <QPainter>
#include <QStyle>
#include <QTreeWidgetItem>

#include <algorithm>
#include <cmath>

namespace TrenchBroom::View {
namespace {
const std::array<Preference<Color> *, LayerTreeItemDelegate::TypeCount> iconColors{
    &Preferences::LayerTreeLayerIconColor,
    &Preferences::LayerTreeGroupIconColor,
    &Preferences::LayerTreeEntityIconColor,
    &Preferences::LayerTreeBrushIconColor,
    &Preferences::LayerTreePatchIconColor,
};
const std::array<const char*, LayerTreeItemDelegate::TypeCount> iconThemeNames{
    "color.icon.layer", "color.icon.group", "color.icon.entity", "color.icon.brush", "color.icon.patch"
};

}

LayerTreeRowStyle::LayerTreeRowStyle(QWidget *parent) : QWidget(parent) {
    setObjectName("LayerTreeRow");
    hide();
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

LayerTreeItemLayout LayerTreeRowStyle::itemLayout() const {
    return {{std::max(0, paddingLeft), std::max(0, paddingTop), std::max(0, paddingRight), std::max(0, paddingBottom)},
            std::max(0, contentIconSize), std::max(0, iconSpacing), std::max(0, lineSpacing),
            std::max(qreal(0), detailFontReduction), detailOpacity, detailColor, std::max(0, minimumRowHeight)};
}

LayerTreeItemRenderer::~LayerTreeItemRenderer() = default;

void LayerTreeItemRenderer::paint(QPainter &painter, const QStyleOptionViewItem &option, const QModelIndex &index, const LayerTreeItemLayout &layout) const {
    const auto rect = QRectF(option.rect).marginsRemoved(QMarginsF(layout.margins));
    const auto iconRect = QRect(qRound(rect.left()), qRound(
        rect.center().y() - layout.iconSize / 2.0), layout.iconSize, layout.iconSize);
    const auto enabled = bool(option.state & QStyle::State_Enabled);
    const auto selected = bool(option.state & QStyle::State_Selected);
    const auto group = !enabled ? QPalette::Disabled : (option.state & QStyle::State_Active ? QPalette::Active
                                                                                            : QPalette::Inactive);
    const auto textColor = option.palette.color(group, QPalette::Text);
    option.icon.paint(&painter, iconRect, Qt::AlignCenter, !enabled ? QIcon::Disabled : selected ? QIcon::Selected
                                                                                                 : QIcon::Normal);

    const auto infoFont = detailFont(option.font, layout.detailFontReduction);
    const auto titleMetrics = QFontMetricsF(option.font);
    const auto infoMetrics = QFontMetricsF(infoFont);

    const auto info = index.data(LayerTreeItemDelegate::DetailRole).toString();
    const auto textHeight = titleMetrics.height() + (info.isEmpty() ? 0 : layout.lineSpacing + infoMetrics.height());
    const auto textX = rect.left() + layout.iconSize + layout.iconSpacing;

    const auto textWidth = std::max(0.0, rect.right() - textX);
    const auto titleRect = QRectF(textX,
                                  rect.top() + (rect.height() - textHeight) / 2.0, textWidth, titleMetrics.height());

    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(option.font);
    painter.setPen(textColor);
    painter.drawText(titleRect,
                     Qt::AlignLeft | Qt::AlignVCenter, titleMetrics.elidedText(option.text, Qt::ElideRight, textWidth));

    if (!info.isEmpty()) {
        auto infoColor = layout.detailColor.isValid() ? layout.detailColor : textColor;
        const auto opacity = std::clamp(
            layout.detailOpacity < 0 ? qreal(pref(Preferences::UIDetailTextOpacity)) : layout.detailOpacity, 0.0, 1.0);
        infoColor.setAlphaF(infoColor.alphaF() * opacity);
        painter.setFont(infoFont);
        painter.setPen(infoColor);
        const auto infoRect = QRectF(textX, titleRect.bottom() + layout.lineSpacing, textWidth, infoMetrics.height());
        painter.drawText(infoRect,
                         Qt::AlignLeft | Qt::AlignVCenter, infoMetrics.elidedText(info, Qt::ElideRight, textWidth));
    }
}

QSize LayerTreeItemRenderer::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index, const LayerTreeItemLayout &layout) const {
    const auto titleMetrics = QFontMetricsF(option.font);
    const auto infoMetrics = QFontMetricsF(detailFont(option.font, layout.detailFontReduction));
    const auto info = index.data(LayerTreeItemDelegate::DetailRole).toString();
    const auto textHeight = titleMetrics.height() + (info.isEmpty() ? 0 : layout.lineSpacing + infoMetrics.height());
    const auto textWidth = std::max(titleMetrics.horizontalAdvance(option.text), infoMetrics.horizontalAdvance(info));
    return {
        int(std::ceil(
            layout.margins.left() + layout.iconSize + layout.iconSpacing + textWidth + layout.margins.right())),
        std::max(layout.minimumHeight, int(std::ceil(
            layout.margins.top() + std::max(qreal(layout.iconSize), textHeight) + layout.margins.bottom())))
    };
}

LayerTreeItemDelegate::LayerTreeItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {
    m_renderers[Layer] = std::make_unique<LayerTreeLayerRenderer>();
    m_renderers[Group] = std::make_unique<LayerTreeGroupRenderer>();
    m_renderers[Entity] = std::make_unique<LayerTreeEntityRenderer>();
    m_renderers[Brush] = std::make_unique<LayerTreeBrushRenderer>();
    m_renderers[Patch] = std::make_unique<LayerTreePatchRenderer>();
}

void LayerTreeItemDelegate::updateItem(QTreeWidgetItem &item, const Model::Node &node, const int ordinal, const bool active) const {
    const auto type = typeForNode(node);
    const auto data = m_renderers[type]->content(node, ordinal, active);

    item.setText(0, data.title);
    item.setData(0, DetailRole, data.detail);
    item.setData(0, RendererRole, int(type));
    item.setData(0, HiddenRole, !node.visible());
    item.setData(0, LockedRole, node.locked());
    item.setData(0, CurrentLayerRole, active);
    item.setData(0, Qt::AccessibleDescriptionRole, data.detail);
    item.setToolTip(0, data.detail.isEmpty() ? data.title : data.title + "\n" + data.detail);
    item.setData(0, IconSourceRole, QString::fromLatin1(data.icon));
    updateIcon(item);
}

bool LayerTreeItemDelegate::isIconColorPreference(const std::filesystem::path &path) {
    return path == Preferences::LayerTreeTypeGroupIconColor.path()
           || std::ranges::any_of(iconColors, [&](const auto *color) { return path == color->path(); });
}

void LayerTreeItemDelegate::updateIcon(QTreeWidgetItem &item, const std::filesystem::path &changedPreference) const {
    const auto type = size_t(std::clamp(item.data(0, RendererRole).toInt(), 0, int(TypeCount) - 1));
    auto &color = item.data(0, TypeGroupRole).isValid()
                  ? Preferences::LayerTreeTypeGroupIconColor : *iconColors[type];
    if (!changedPreference.empty() && changedPreference != color.path()) return;

    const auto source = IO::pathFromQString(item.data(0, IconSourceRole).toString());
    const auto name = item.data(0, TypeGroupRole).isValid() ? "color.icon.typeGroup" : iconThemeNames[type];
    item.setIcon(0, IO::VectorIcon(source).themedIcon(QString::fromLatin1(name), toQColor(pref(color))));
}

void LayerTreeItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    auto contentOption = option;
    initStyleOption(&contentOption, index);
    painter->save();
    painter->setClipRect(option.rect);
    // The tree paints one background for the complete row. Cells only draw
    // their contents, including the two attribute columns.
    if (index.column() == 0) {
        auto &style = rowStyle(index, contentOption);
        contentOption.font = style.font();
        contentOption.palette.setColor(QPalette::Text, style.palette().color(QPalette::WindowText));
        renderer(index).paint(*painter, contentOption, index, style.itemLayout());
    } else {
        const auto size = option.decorationSize.boundedTo(option.rect.size());
        const auto iconRect = QRect(option.rect.center() - QPoint(size.width() / 2, size.height() / 2), size);
        contentOption.icon.paint(painter, iconRect, Qt::AlignCenter,
                                 option.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled);
    }
    painter->restore();
}

void LayerTreeItemDelegate::updateTypeGroup(QTreeWidgetItem &item, const Type type, const size_t count, const QString &entityClass) const {
    const std::array<QString, TypeCount> names{
        tr("Layers"),
        tr("Groups"),
        tr("Entities"),
        tr("Brushes"),
        tr("Patches")
    };

    const auto name = entityClass.isEmpty() ? names[type] : entityClass == "light" ? tr("Lights") : entityClass;
    item.setText(0, tr("%1 (%2)").arg(name).arg(count));
    item.setData(0, DetailRole, tr("Type group · select all members"));
    item.setData(0, RendererRole, int(Group));
    item.setData(0, TypeGroupRole, int(type));
    item.setData(0, EntityClassRole, entityClass);
    item.setData(0, IconSourceRole, QStringLiteral("circles_ext.svg"));
    updateIcon(item);
    item.setToolTip(0, tr("Display folder for %1 objects. The map hierarchy is unchanged.").arg(count));
    item.setData(0, Qt::AccessibleDescriptionRole, item.toolTip(0));
}

QSize LayerTreeItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    if (index.column() != 0) return {20, 20};
    auto contentOption = option;
    initStyleOption(&contentOption, index);
    auto &style = rowStyle(index, contentOption);
    contentOption.font = style.font();
    return renderer(index).sizeHint(contentOption, index, style.itemLayout());
}

LayerTreeItemDelegate::Type LayerTreeItemDelegate::typeForNode(const Model::Node &node) {
    if (dynamic_cast<const Model::LayerNode *>(&node)) return Layer;
    if (dynamic_cast<const Model::GroupNode *>(&node)) return Group;
    if (dynamic_cast<const Model::EntityNode *>(&node)) return Entity;
    if (dynamic_cast<const Model::BrushNode *>(&node)) return Brush;
    return Patch;
}


LayerTreeRowStyle &LayerTreeItemDelegate::rowStyle(const QModelIndex &index, const QStyleOptionViewItem &option) const {
    const auto name = index.sibling(index.row(), 0);
    const auto type = unsigned(std::clamp(name.data(RendererRole).toInt(), 0, int(TypeCount) - 1));
    const auto hidden = name.data(HiddenRole).toBool(), locked = name.data(LockedRole).toBool();
    const auto selected = bool(option.state & QStyle::State_Selected);
    const auto current = name.data(CurrentLayerRole).toBool(), folder = name.data(TypeGroupRole).isValid();
    const auto key =
        type |
        (unsigned(hidden) << 3) |
        (unsigned(locked) << 4) |
        (unsigned(selected) << 5) |
        (unsigned(current) << 6) |
        (unsigned(folder) << 7);

    if (const auto it = m_styles.find(key); it != m_styles.end()) return *it->second;
    auto *parent = qobject_cast<QWidget *>(this->parent());
    auto *style = new LayerTreeRowStyle(parent);
    static const std::array<const char *, TypeCount> names{"layer", "group", "entity", "brush", "patch"};

    style->setProperty("nodeType", folder ? "typeGroup" : names[type]);
    style->setProperty("hidden", hidden);
    style->setProperty("locked", locked);
    style->setProperty("selected", selected);
    style->setProperty("currentLayer", current);
    style->ensurePolished();
    m_styles.emplace(key, style);

    return *style;
}

void LayerTreeItemDelegate::paintRowBackground(QPainter &painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    auto &style = rowStyle(index, option);
    QStyleOption background;
    background.initFrom(&style);
    background.state = option.state;
    background.rect = option.rect.adjusted(std::max(0, style.rowInsetX), std::max(0, style.rowInsetY),
                                           -std::max(0, style.rowInsetX), -std::max(0, style.rowInsetY));
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    style.style()->drawPrimitive(QStyle::PE_Widget, &background, &painter, &style);
    painter.restore();
}

const LayerTreeItemRenderer &LayerTreeItemDelegate::renderer(const QModelIndex &index) const {
    return *m_renderers[size_t(std::clamp(index.data(RendererRole).toInt(), 0, int(TypeCount) - 1))];
}
} // namespace TrenchBroom::View
