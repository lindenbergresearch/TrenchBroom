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
#include <QLabel>
#include <QPainter>
#include <QPixmap>
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

void paintLabel(QPainter& painter, QLabel& label, const QRect& rect, qreal opacity = 1) {
    if (rect.isEmpty() || label.text().isEmpty()) return;
    const auto text = label.text();
    label.setGeometry(rect);
    const auto width = std::max(0, label.contentsRect().width() - 2 * label.margin() - std::max(0, label.indent()));
    label.setText(label.fontMetrics().elidedText(text, Qt::ElideRight, width));
    // Qt 6/macOS can omit opaque QLabel text when render() shares an active
    // viewport's painter. Isolate the widget paint at the target's actual
    // device scale before compositing it, without resizing a rasterized image.
    const auto ratio = painter.device()->devicePixelRatioF();
    QPixmap buffer(QSize(int(std::ceil(rect.width() * ratio)), int(std::ceil(rect.height() * ratio))));
    buffer.setDevicePixelRatio(ratio);
    buffer.fill(Qt::transparent);
    label.render(&buffer, QPoint{}, QRegion{}, QWidget::DrawChildren);
    painter.save();
    painter.setClipRect(rect, Qt::IntersectClip);
    painter.setOpacity(painter.opacity() * opacity);
    painter.drawPixmap(rect.topLeft(), buffer);
    painter.restore();
    label.setText(text);
}

int upperLineHeight(const LayerTreeRowStyle& style) {
    return std::max(style.titleLabel().sizeHint().height(),
        style.addendumLabel().text().isEmpty() ? 0 : style.addendumLabel().sizeHint().height());
}

}

LayerTreeRowStyle::LayerTreeRowStyle(QWidget *parent) : QWidget(parent) {
    setObjectName("LayerTreeRow");
    hide();
    setAttribute(Qt::WA_TransparentForMouseEvents);
    const std::array<const char*, 3> names{"LayerTreeTitle", "LayerTreeDetail", "LayerTreeAddendum"};
    for (size_t i = 0; i < m_labels.size(); ++i) {
        auto* label = m_labels[i] = new QLabel(this);
        label->setObjectName(names[i]);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(false);
        label->setAlignment((i == 2 ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        label->setAttribute(Qt::WA_NoSystemBackground);
    }
}

LayerTreeItemLayout LayerTreeRowStyle::itemLayout() {
    return {{std::max(0, paddingLeft), std::max(0, paddingTop), std::max(0, paddingRight), std::max(0, paddingBottom)},
            std::max(0, contentIconSize), std::max(0, iconSpacing), std::max(0, lineSpacing),
            std::max(qreal(0), detailFontReduction), detailOpacity, detailColor, std::max(0, minimumRowHeight),
            std::max(0, addendumSpacing), this};
}

QLabel& LayerTreeRowStyle::titleLabel() const { return *m_labels[0]; }
QLabel& LayerTreeRowStyle::detailLabel() const { return *m_labels[1]; }
QLabel& LayerTreeRowStyle::addendumLabel() const { return *m_labels[2]; }

void LayerTreeRowStyle::prepareText(const QStyleOptionViewItem& option, const QModelIndex& index, const LayerTreeItemLayout& layout) {
    const auto settings = listFontSettings();
    const std::array<QFont, 3> fonts{
        listTextFont(option.font, settings, true),
        listTextFont(detailFont(option.font, layout.detailFontReduction), settings),
        listTextFont(option.font, settings, true)};
    const std::array<QString, 3> texts{option.text, index.data(LayerTreeItemDelegate::DetailRole).toString(),
        index.data(LayerTreeItemDelegate::AddendumRole).toString()};
    const auto enabled = bool(option.state & QStyle::State_Enabled);
    const auto group = !enabled ? QPalette::Disabled : option.state & QStyle::State_Active ? QPalette::Active : QPalette::Inactive;
    for (size_t i = 0; i < m_labels.size(); ++i) {
        auto& label = *m_labels[i];
        const auto fontChanged = !m_textInitialized || m_baseFonts[i] != fonts[i];
        auto palette = option.palette;
        const auto color = i == 1 && layout.detailColor.isValid() ? layout.detailColor : palette.color(group, QPalette::Text);
        palette.setColor(QPalette::WindowText, color);
        const auto paletteChanged = !m_textInitialized || m_basePalettes[i] != palette;
        label.setEnabled(enabled);
        label.ensurePolished();
        // The row may already have polished its child labels. With Qt's
        // stylesheet propagation enabled, setting defaults afterwards replaces
        // their QSS fonts. Unpolish first (it also restores the old palette),
        // then set defaults and apply the label's rules before measuring it.
        // Unchanged rows do not need a repolish.
        if (fontChanged || paletteChanged) {
            label.style()->unpolish(&label);
            label.setFont(fonts[i]);
            label.setPalette(palette);
            label.style()->polish(&label);
            m_baseFonts[i] = fonts[i];
            m_basePalettes[i] = palette;
        }
        label.setText(texts[i]);
    }
    m_textInitialized = true;
}

LayerTreeItemRenderer::LayerTreeItemRenderer() = default;
LayerTreeItemRenderer::~LayerTreeItemRenderer() = default;

LayerTreeRowStyle& LayerTreeItemRenderer::textStyle(const QStyleOptionViewItem& option, const QModelIndex& index, const LayerTreeItemLayout& layout) const {
    if (!layout.textStyle && !m_defaultTextStyle) m_defaultTextStyle = std::make_unique<LayerTreeRowStyle>(nullptr);
    auto& style = layout.textStyle ? *layout.textStyle : *m_defaultTextStyle;
    style.prepareText(option, index, layout);
    return style;
}

void LayerTreeItemRenderer::paint(QPainter &painter, const QStyleOptionViewItem &option, const QModelIndex &index, const LayerTreeItemLayout &layout) const {
    const auto rect = option.rect.marginsRemoved(layout.margins);
    const auto iconRect = QRect(rect.left(), qRound(
        rect.center().y() - layout.iconSize / 2.0), layout.iconSize, layout.iconSize);
    const auto enabled = bool(option.state & QStyle::State_Enabled);
    const auto selected = bool(option.state & QStyle::State_Selected);
    option.icon.paint(&painter, iconRect, Qt::AlignCenter, !enabled ? QIcon::Disabled : selected ? QIcon::Selected
                                                                                                 : QIcon::Normal);

    auto& style = textStyle(option, index, layout);
    auto& title = style.titleLabel();
    auto& detail = style.detailLabel();
    auto& addendum = style.addendumLabel();
    const auto upperHeight = upperLineHeight(style);
    const auto textHeight = upperHeight + (detail.text().isEmpty() ? 0 : layout.lineSpacing + detail.sizeHint().height());
    const auto textX = rect.left() + layout.iconSize + layout.iconSpacing;

    const auto textWidth = std::max(0, rect.right() + 1 - textX);
    const auto textY = rect.top() + (rect.height() - textHeight) / 2;
    auto titleWidth = textWidth;
    auto gap = 0;
    if (!addendum.text().isEmpty()) {
        gap = std::min(layout.addendumSpacing, textWidth);
        const auto reserve = std::min(addendum.sizeHint().width(), (textWidth - gap) / 2);
        titleWidth = std::min(title.sizeHint().width(), textWidth - gap - reserve);
    }

    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::VerticalSubpixelPositioning, listFontSettings().subpixelPositioning);
    paintLabel(painter, title, QRect(textX, textY, titleWidth, upperHeight));
    if (!addendum.text().isEmpty())
        paintLabel(painter, addendum, QRect(textX + titleWidth + gap, textY, textWidth - titleWidth - gap, upperHeight));
    if (!detail.text().isEmpty()) {
        const auto opacity = std::clamp(
            layout.detailOpacity < 0 ? qreal(pref(Preferences::UIDetailTextOpacity)) : layout.detailOpacity, 0.0, 1.0);
        paintLabel(painter, detail, QRect(textX, textY + upperHeight + layout.lineSpacing, textWidth, detail.sizeHint().height()), opacity);
    }
}

QSize LayerTreeItemRenderer::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index, const LayerTreeItemLayout &layout) const {
    auto& style = textStyle(option, index, layout);
    const auto& title = style.titleLabel();
    const auto& detail = style.detailLabel();
    const auto& addendum = style.addendumLabel();
    const auto textHeight = upperLineHeight(style) + (detail.text().isEmpty() ? 0 : layout.lineSpacing + detail.sizeHint().height());
    const auto upperWidth = title.sizeHint().width() + (addendum.text().isEmpty() ? 0 : layout.addendumSpacing + addendum.sizeHint().width());
    const auto textWidth = std::max(upperWidth, detail.text().isEmpty() ? 0 : detail.sizeHint().width());
    return {
        layout.margins.left() + layout.iconSize + layout.iconSpacing + textWidth + layout.margins.right(),
        std::max(layout.minimumHeight, layout.margins.top() + std::max(layout.iconSize, textHeight) + layout.margins.bottom())
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
    item.setData(0, AddendumRole, data.addendum);
    item.setData(0, RendererRole, int(type));
    item.setData(0, HiddenRole, !node.visible());
    item.setData(0, LockedRole, node.locked());
    item.setData(0, CurrentLayerRole, active);
    item.setData(0, Qt::AccessibleDescriptionRole, data.detail);
    const auto heading = data.title + (data.addendum.isEmpty() ? QString{} : " " + data.addendum);
    item.setData(0, Qt::AccessibleTextRole, heading);
    item.setToolTip(0, data.detail.isEmpty() ? heading : heading + "\n" + data.detail);
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
    item.setText(0, name);
    item.setData(0, AddendumRole, tr("(%1)").arg(count));
    item.setData(0, Qt::AccessibleTextRole, tr("%1 (%2)").arg(name).arg(count));
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
