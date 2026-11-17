#pragma once

#include <QMargins>
#include <QStyledItemDelegate>
#include <QWidget>

#include <array>
#include <filesystem>
#include <memory>
#include <map>

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
    QMargins margins{9, 6, 4, 6};
    int iconSize = 20;
    int iconSpacing = 6;
    int lineSpacing = 0;
    qreal detailFontReduction = 2;
    qreal detailOpacity = -1;
    QColor detailColor;
    int minimumHeight = 0;
};

// A small, shared style proxy for each type/state combination, never a widget
// per scene node. Qt applies base.qss to these properties and to the standard
// font, color, background and border attributes.
class LayerTreeRowStyle : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int paddingLeft MEMBER paddingLeft)
    Q_PROPERTY(int paddingTop MEMBER paddingTop)
    Q_PROPERTY(int paddingRight MEMBER paddingRight)
    Q_PROPERTY(int paddingBottom MEMBER paddingBottom)
    Q_PROPERTY(int contentIconSize MEMBER contentIconSize)
    Q_PROPERTY(int iconSpacing MEMBER iconSpacing)
    Q_PROPERTY(int lineSpacing MEMBER lineSpacing)
    Q_PROPERTY(qreal detailFontReduction MEMBER detailFontReduction)
    Q_PROPERTY(qreal detailOpacity MEMBER detailOpacity)
    Q_PROPERTY(QColor detailColor MEMBER detailColor)
    Q_PROPERTY(int minimumRowHeight MEMBER minimumRowHeight)
    Q_PROPERTY(int rowInsetX MEMBER rowInsetX)
    Q_PROPERTY(int rowInsetY MEMBER rowInsetY)
public:
    explicit LayerTreeRowStyle(QWidget* parent);
    LayerTreeItemLayout itemLayout() const;
    int paddingLeft = 0, paddingTop = 0, paddingRight = 0, paddingBottom = 0;
    int contentIconSize = 20, iconSpacing = 0, lineSpacing = 0;
    qreal detailFontReduction = 2, detailOpacity = -1;
    QColor detailColor;
    int minimumRowHeight = 0, rowInsetX = 0, rowInsetY = 0;
};

// Each node type owns its content and layout. Types can also replace paint() and
// sizeHint() for a completely different presentation without changing the tree.
// Painting uses cached item data, so only visible rows need to be rendered.
class LayerTreeItemRenderer {
public:
    LayerTreeItemRenderer() = default;

    virtual ~LayerTreeItemRenderer();

    virtual LayerTreeItemContent content(const Model::Node& node, int ordinal, bool active) const = 0;

    virtual void paint(QPainter& painter, const QStyleOptionViewItem& option, const QModelIndex& index, const LayerTreeItemLayout& layout = {}) const;

    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index, const LayerTreeItemLayout& layout = {}) const;
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
    enum Role { DetailRole = Qt::UserRole + 1, RendererRole, TypeGroupRole, EntityClassRole, IconSourceRole, HiddenRole, LockedRole, CurrentLayerRole };
    enum Type { Layer, Group, Entity, Brush, Patch, TypeCount };

    explicit LayerTreeItemDelegate(QObject* parent = nullptr);

    void updateItem(QTreeWidgetItem& item, const Model::Node& node, int ordinal, bool active) const;
    void updateTypeGroup(QTreeWidgetItem& item, Type type, size_t count, const QString& entityClass = {}) const;
    void updateIcon(QTreeWidgetItem& item, const std::filesystem::path& changedPreference = {}) const;
    static bool isIconColorPreference(const std::filesystem::path& path);
    static Type typeForNode(const Model::Node& node);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paintRowBackground(QPainter& painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;

private:

    std::array<std::unique_ptr<LayerTreeItemRenderer>, TypeCount> m_renderers;
    mutable std::map<unsigned, LayerTreeRowStyle*> m_styles;
    LayerTreeRowStyle& rowStyle(const QModelIndex& index, const QStyleOptionViewItem& option) const;

    const LayerTreeItemRenderer& renderer(const QModelIndex& index) const;
};
} // namespace TrenchBroom::View
