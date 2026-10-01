#include "LayerTreeItemRenderer.h"

#include "IO/ResourceUtils.h"
#include "Model/BrushNode.h"
#include "Model/EntityNode.h"
#include "Model/GroupNode.h"
#include "Model/LayerNode.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <QTreeWidgetItem>

#include <algorithm>

namespace TrenchBroom::View {namespace {
    QFont detailFont(const QFont& titleFont, const qreal reduction) {
        auto font = titleFont;
        font.setBold(false);
        if (font.pointSizeF() > 0) font.setPointSizeF(std::max(1.0, font.pointSizeF() - reduction));
        else font.setPixelSize(std::max(1, font.pixelSize() - qRound(reduction)));
        return font;
    }
    }

    LayerTreeItemRenderer::LayerTreeItemRenderer(LayerTreeItemLayout layout) : m_layout(std::move(layout)) {}

    LayerTreeItemRenderer::~LayerTreeItemRenderer() = default;

    void LayerTreeItemRenderer::paint(QPainter& painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
        const auto rect = option.rect.marginsRemoved(m_layout.margins);
        const auto iconRect = QRect(rect.left(), rect.center().y() - m_layout.iconSize / 2, m_layout.iconSize, m_layout.iconSize);
        const auto enabled = bool(option.state & QStyle::State_Enabled);
        const auto selected = bool(option.state & QStyle::State_Selected);
        const auto group = !enabled ? QPalette::Disabled : (option.state & QStyle::State_Active ? QPalette::Active : QPalette::Inactive);
        const auto textColor = option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
        option.icon.paint(&painter, iconRect, Qt::AlignCenter, !enabled ? QIcon::Disabled : selected ? QIcon::Selected : QIcon::Normal);

        const auto infoFont = detailFont(option.font, m_layout.detailFontReduction);
        const auto titleMetrics = QFontMetrics(option.font);
        const auto infoMetrics = QFontMetrics(infoFont);
        const auto info = index.data(LayerTreeItemDelegate::DetailRole).toString();
        const auto textHeight = titleMetrics.height() + (info.isEmpty() ? 0 : m_layout.lineSpacing + infoMetrics.height());
        const auto textX = rect.left() + m_layout.iconSize + m_layout.iconSpacing;
        const auto textWidth = std::max(0, rect.right() - textX + 1);
        const auto titleRect = QRect(textX, rect.top() + (rect.height() - textHeight) / 2, textWidth, titleMetrics.height());

        painter.setFont(option.font);
        painter.setPen(textColor);
        painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, titleMetrics.elidedText(option.text, Qt::ElideRight, textWidth));

        if (!info.isEmpty()) {
            auto infoColor = textColor;
            infoColor.setAlphaF(infoColor.alphaF() * (selected ? 0.9 : 0.7));
            painter.setFont(infoFont);
            painter.setPen(infoColor);
            const auto infoRect = QRect(textX, titleRect.bottom() + 1 + m_layout.lineSpacing, textWidth, infoMetrics.height());
            painter.drawText(infoRect, Qt::AlignLeft | Qt::AlignVCenter, infoMetrics.elidedText(info, Qt::ElideRight, textWidth));
        }
    }

    QSize LayerTreeItemRenderer::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
        const auto titleMetrics = QFontMetrics(option.font);
        const auto infoMetrics = QFontMetrics(detailFont(option.font, m_layout.detailFontReduction));
        const auto info = index.data(LayerTreeItemDelegate::DetailRole).toString();
        const auto textHeight = titleMetrics.height() + (info.isEmpty() ? 0 : m_layout.lineSpacing + infoMetrics.height());
        const auto textWidth = std::max(titleMetrics.horizontalAdvance(option.text), infoMetrics.horizontalAdvance(info));
        return {
            m_layout.margins.left() + m_layout.iconSize + m_layout.iconSpacing + textWidth + m_layout.margins.right(),
            m_layout.margins.top() + std::max(m_layout.iconSize, textHeight) + m_layout.margins.bottom()
        };
    }

    LayerTreeItemDelegate::LayerTreeItemDelegate(QObject* parent) : QStyledItemDelegate(parent) {
        m_renderers[Layer] = std::make_unique<LayerTreeLayerRenderer>();
        m_renderers[Group] = std::make_unique<LayerTreeGroupRenderer>();
        m_renderers[Entity] = std::make_unique<LayerTreeEntityRenderer>();
        m_renderers[Brush] = std::make_unique<LayerTreeBrushRenderer>();
        m_renderers[Patch] = std::make_unique<LayerTreePatchRenderer>();
    }

    void LayerTreeItemDelegate::updateItem(QTreeWidgetItem& item, const Model::Node& node, const int ordinal, const bool active) const {
        const auto type = typeForNode(node);
        const auto data = m_renderers[type]->content(node, ordinal, active);

        item.setText(0, data.title);
        item.setData(0, DetailRole, data.detail);
        item.setData(0, RendererRole, int(type));
        item.setData(0, Qt::AccessibleDescriptionRole, data.detail);
        item.setToolTip(0, data.detail.isEmpty() ? data.title : data.title + "\n" + data.detail);
        item.setIcon(0, IO::loadSVGIcon(data.icon, 16));
    }

    void LayerTreeItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
        auto contentOption = option;
        initStyleOption(&contentOption, index);
        auto background = contentOption;
        background.text.clear();
        background.icon = {};
        background.features &= ~(QStyleOptionViewItem::HasDisplay | QStyleOptionViewItem::HasDecoration);
        const auto* style = option.widget ? option.widget->style() : QApplication::style();
        painter->save();
        painter->setClipRect(option.rect);
        style->drawControl(QStyle::CE_ItemViewItem, &background, painter, option.widget);
        renderer(index).paint(*painter, contentOption, index);
        painter->restore();
    }

    QSize LayerTreeItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
        auto contentOption = option;
        initStyleOption(&contentOption, index);
        return renderer(index).sizeHint(contentOption, index);
    }

    LayerTreeItemDelegate::Type LayerTreeItemDelegate::typeForNode(const Model::Node& node) {
        if (dynamic_cast<const Model::LayerNode *>(&node)) return Layer;
        if (dynamic_cast<const Model::GroupNode *>(&node)) return Group;
        if (dynamic_cast<const Model::EntityNode *>(&node)) return Entity;
        if (dynamic_cast<const Model::BrushNode *>(&node)) return Brush;
        return Patch;
    }

    const LayerTreeItemRenderer& LayerTreeItemDelegate::renderer(const QModelIndex& index) const {
        return *m_renderers[size_t(std::clamp(index.data(RendererRole).toInt(), 0, int(TypeCount) - 1))];
    }
} // namespace TrenchBroom::View
