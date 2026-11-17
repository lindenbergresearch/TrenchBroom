#pragma once
#include "NotifierConnection.h"
#include "View/EditorToolbarLayout.h"
#include <QFrame>

class QBoxLayout;
class QToolButton;

namespace TrenchBroom::View {
class MapViewBase;
class MapViewToolBox;

class EditorToolbarPanel : public QFrame {
    Q_OBJECT
    Q_PROPERTY(int panelPadding READ panelPadding WRITE setPanelPadding)
    Q_PROPERTY(int buttonPadding READ buttonPadding WRITE setButtonPadding)
    Q_PROPERTY(int buttonSpacing READ buttonSpacing WRITE setButtonSpacing)
    Q_PROPERTY(int edgeMargin READ edgeMargin WRITE setEdgeMargin)
    Q_PROPERTY(int toolbarGap READ toolbarGap WRITE setToolbarGap)
    EditorToolbarLayoutItem m_item;
    MapViewBase& m_view;
    QBoxLayout* m_layout;
    std::vector<QWidget*> m_items;
    std::vector<QToolButton*> m_buttons;
    int m_padding = 6, m_buttonPadding = 4, m_spacing = 4, m_margin = 12, m_gap = 6, m_iconSize = -1;

  public:
    EditorToolbarPanel(const EditorToolbarDefinition& definition, MapViewBase& view);
    EditorToolbarLayoutItem layoutItem();
    void setIconExtent(int size);
    void refreshActions();
    int panelPadding() const { return m_padding; }
    int buttonPadding() const { return m_buttonPadding; }
    int buttonSpacing() const { return m_spacing; }
    int edgeMargin() const { return m_margin; }
    int toolbarGap() const { return m_gap; }
    void setPanelPadding(int value);
    void setButtonPadding(int value);
    void setButtonSpacing(int value);
    void setEdgeMargin(int value);
    void setToolbarGap(int value);
  signals:
    void geometrySettingsChanged();

  protected:
    bool event(QEvent* event) override;
};

class EditorToolbars : public QObject {
    Q_OBJECT
    MapViewBase& m_view;
    std::vector<EditorToolbarPanel*> m_panels;
    NotifierConnection m_connections;
    bool m_layoutPending = false;
    void rebuild();
    void scheduleLayout();
    void place();

  public:
    EditorToolbars(MapViewToolBox& tools, MapViewBase& view);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};
} // namespace TrenchBroom::View
