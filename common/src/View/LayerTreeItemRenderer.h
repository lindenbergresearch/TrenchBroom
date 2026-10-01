#pragma once

#include <QMargins>
#include <QStyledItemDelegate>

#include <array>
#include <memory>

class QTreeWidgetItem;

namespace TrenchBroom::Model {
class Node;
}

namespace TrenchBroom::View {
struct LayerTreeItemContent {
    QString title;
    QString detail;
    const char* icon;
};

struct LayerTreeItemLayout {
    QMargins margins{4, 6, 4, 6};
    int iconSize = 18;
    int iconSpacing = 4;
    int lineSpacing = 0;
    qreal detailFontReduction = 3;
};

// Each node type owns its content and layout. Types can also replace paint() and
// sizeHint() for a completely different presentation without changing the tree.
// Painting uses cached item data, so only visible rows need to be rendered.
class LayerTreeItemRenderer {
protected:
    LayerTreeItemLayout m_layout;

public:
    explicit LayerTreeItemRenderer(LayerTreeItemLayout layout = {});

    virtual ~LayerTreeItemRenderer();

    virtual LayerTreeItemContent content(const Model::Node& node, int ordinal, bool active) const = 0;

    virtual void paint(QPainter& painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;

    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const;
};

/* ------------------------------------------------------------------------------------------- */


class LayerTreeLayerRenderer : public LayerTreeItemRenderer {
public:
    LayerTreeLayerRenderer();

    LayerTreeItemContent content(const Model::Node& node, int ordinal, bool active) const override;
};

class LayerTreeGroupRenderer : public LayerTreeItemRenderer {
public:
    LayerTreeGroupRenderer();

    LayerTreeItemContent content(const Model::Node& node, int ordinal, bool active) const override;
};

class LayerTreeEntityRenderer : public LayerTreeItemRenderer {
public:
    LayerTreeEntityRenderer();

    LayerTreeItemContent content(const Model::Node& node, int ordinal, bool active) const override;
};

class LayerTreeBrushRenderer : public LayerTreeItemRenderer {
public:
    LayerTreeBrushRenderer();

    LayerTreeItemContent content(const Model::Node& node, int ordinal, bool active) const override;
};

class LayerTreePatchRenderer : public LayerTreeItemRenderer {
public:
    LayerTreePatchRenderer();

    LayerTreeItemContent content(const Model::Node& node, int ordinal, bool active) const override;
};

/* ------------------------------------------------------------------------------------------- */


class LayerTreeItemDelegate : public QStyledItemDelegate {
public:
    // Qt::UserRole remains reserved for the tree's node reference.
    enum Role { DetailRole = Qt::UserRole + 1, RendererRole };

    explicit LayerTreeItemDelegate(QObject* parent = nullptr);

    void updateItem(QTreeWidgetItem& item, const Model::Node& node, int ordinal, bool active) const;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    enum Type { Layer, Group, Entity, Brush, Patch, TypeCount };

    std::array<std::unique_ptr<LayerTreeItemRenderer>, TypeCount> m_renderers;

    static Type typeForNode(const Model::Node& node);

    const LayerTreeItemRenderer& renderer(const QModelIndex& index) const;
};
} // namespace TrenchBroom::View
