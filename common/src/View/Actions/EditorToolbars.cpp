#include "View/Actions/EditorToolbars.h"
#include "IO/ResourceUtils.h"
#include "View/Actions/Actions.h"
#include "View/Tools/Brush/DrawShapeTool.h"
#include "View/Document/MapDocument.h"
#include "View/App/MapFrame.h"
#include "View/MapView/MapViewBase.h"
#include "View/MapView/MapViewToolBox.h"
#include "View/Theme/QtUtils.h"

#include <QBoxLayout>
#include <QEvent>
#include <QPointer>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <algorithm>

namespace TrenchBroom::View {
namespace {
    bool isVertical(EditorToolbarEdge edge) { return edge == EditorToolbarEdge::Left || edge == EditorToolbarEdge::Right; }
    QString tooltip(const Action& action) {
        auto text = action.label();
        if (!action.keySequence().isEmpty()) text += " (" + action.keySequence().toString(QKeySequence::NativeText) + ')';
        if (!action.statusTip().isEmpty()) text += '\n' + action.statusTip();
        return text;
    }
} // namespace

EditorToolbarPanel::EditorToolbarPanel(const EditorToolbarDefinition& definition, MapViewBase& view)
    : QFrame{&view}, m_view{view}, m_layout{new QBoxLayout(isVertical(definition.edge) ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight, this)} {
    m_item.definition = definition;
    m_layout->setSizeConstraint(QLayout::SetNoConstraint);
    setObjectName("EditorToolbar");
    setProperty("toolbarId", definition.id);
    setProperty("position", editorToolbarEdgeName(definition.edge));
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_NoMousePropagation);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
    setAccessibleName(definition.label);
    auto& registry = ActionManager::instance();
    for (const auto& id : definition.actions) {
        if (id.isEmpty()) {
            if (m_items.empty() || m_items.back()->property("separator").toBool()) continue;
            auto* line = new QFrame{this};
            line->setObjectName("EditorToolbarSeparator");
            line->setProperty("separator", true);
            line->setFrameShape(isVertical(definition.edge) ? QFrame::HLine : QFrame::VLine);
            m_layout->addWidget(line);
            m_items.push_back(line);
            ++m_item.separators;
            continue;
        }
        const auto* action = registry.action(id);
        if (!action || !(action->targets() & EditorToolbarTarget)) continue;
        auto* button = new QToolButton{this};
        button->setObjectName("EditorToolbarButton");
        button->setProperty("editorActionId", id);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setAccessibleName(action->label());
        button->setIcon(action->hasIcon() ? IO::loadSVGIcon(action->iconPath(), definition.iconSize) : IO::loadSVGIcon("document.svg", definition.iconSize));
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setText(action->label());
        button->setToolTip(tooltip(*action));
        button->setStatusTip(action->statusTip());
        button->setCheckable(action->checkable());
        connect(button, &QToolButton::clicked, this, [this, action] {
            const QPointer<MapViewBase> viewport{&m_view};
            const QPointer<EditorToolbarPanel> panel{this};
            viewport->setFocus(Qt::MouseFocusReason);
            if (!viewport) return;
            ActionExecutionContext context{findMapFrame(viewport), viewport};
            action->execute(context);
            // Commands may replace a view/layout or open Preferences, where
            // this very toolbar can be deleted while the command is running.
            if (viewport) viewport->update();
            if (panel) panel->refreshActions();
        });
        m_buttons.push_back(button);
        m_items.push_back(button);
        m_layout->addWidget(button);
        ++m_item.buttons;
    }
    if (!m_items.empty() && m_items.back()->property("separator").toBool()) {
        delete m_items.back();
        m_items.pop_back();
        --m_item.separators;
    }
    refreshActions();
}

EditorToolbarLayoutItem EditorToolbarPanel::layoutItem() {
    ensurePolished();
    m_item.padding = m_padding + frameWidth();
    m_item.buttonPadding = m_buttonPadding;
    m_item.spacing = m_spacing;
    m_item.gap = m_gap;
    return m_item;
}
void EditorToolbarPanel::setIconExtent(const int size) {
    const auto margin = m_padding;
    m_layout->setContentsMargins(margin, margin, margin, margin);
    m_layout->setSpacing(m_spacing);
    if (m_iconSize == size && property("effectiveButtonPadding").toInt() == m_buttonPadding && property("effectiveSpacing").toInt() == m_spacing) return;
    m_iconSize = size;
    setProperty("effectiveIconSize", size);
    setProperty("effectiveButtonPadding", m_buttonPadding);
    setProperty("effectiveSpacing", m_spacing);
    const auto button = size + 2 * m_buttonPadding;
    for (auto* widget : m_items) {
        if (auto* tool = qobject_cast<QToolButton*>(widget)) {
            tool->setIconSize({size, size});
            tool->setFixedSize(button, button);
        } else
            widget->setFixedSize(isVertical(m_item.definition.edge) ? QSize{button, m_spacing * 2 + 1} : QSize{m_spacing * 2 + 1, button});
    }
}
void EditorToolbarPanel::refreshActions() {
    if (!m_view.property("editorActionsReady").toBool()) {
        for (auto* button : m_buttons)
            button->setEnabled(false);
        return;
    }
    auto* frame = findMapFrame(&m_view);
    if (frame && !frame->document()->world()) frame = nullptr;
    ActionExecutionContext context{frame, &m_view};
    for (auto* button : m_buttons) {
        const auto* action = ActionManager::instance().action(button->property("editorActionId").toString());
        const QSignalBlocker block{button};
        button->setEnabled(frame && action && action->enabled(context));
        button->setChecked(frame && action && action->checkable() && action->checked(context));
    }
}
void EditorToolbarPanel::setPanelPadding(int v) {
    m_padding = std::clamp(v, 0, 64);
    emit geometrySettingsChanged();
}
void EditorToolbarPanel::setButtonPadding(int v) {
    m_buttonPadding = std::clamp(v, 0, 64);
    emit geometrySettingsChanged();
}
void EditorToolbarPanel::setButtonSpacing(int v) {
    m_spacing = std::clamp(v, 0, 64);
    emit geometrySettingsChanged();
}
void EditorToolbarPanel::setEdgeMargin(int v) {
    m_margin = std::clamp(v, 0, 128);
    emit geometrySettingsChanged();
}
void EditorToolbarPanel::setToolbarGap(int v) {
    m_gap = std::clamp(v, 0, 64);
    emit geometrySettingsChanged();
}
bool EditorToolbarPanel::event(QEvent* event) {
    const auto handled = QFrame::event(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::Polish || event->type() == QEvent::FontChange) emit geometrySettingsChanged();
    return handled;
}

EditorToolbars::EditorToolbars(MapViewToolBox& tools, MapViewBase& view) : QObject{&view}, m_view{view} {
    m_view.installEventFilter(this);
    m_connections += ActionManager::instance().configurationDidChangeNotifier.connect([this] { rebuild(); });
    const auto refresh = [this](auto&&...) {
        for (auto* panel : m_panels)
            if (panel->isVisible()) panel->refreshActions();
    };
    m_connections += tools.toolActivatedNotifier.connect(refresh);
    m_connections += tools.toolDeactivatedNotifier.connect(refresh);
    m_connections += tools.drawShapeTool().extensionManager().currentExtensionDidChangeNotifier.connect(refresh);
    const auto document = tools.drawShapeTool().extensionManager().document();
    m_connections += document->documentWasNewedNotifier.connect(refresh);
    m_connections += document->documentWasLoadedNotifier.connect(refresh);
    m_connections += document->documentWasClearedNotifier.connect(refresh);
    m_connections += document->selectionDidChangeNotifier.connect(refresh);
    m_connections += document->nodesDidChangeNotifier.connect(refresh);
    m_connections += document->editorContextDidChangeNotifier.connect(refresh);
    auto* timer = new QTimer{this};
    timer->setInterval(150);
    connect(timer, &QTimer::timeout, this, [this] {
        if (m_view.isVisible())
            for (auto* panel : m_panels)
                if (panel->isVisible()) panel->refreshActions();
    });
    timer->start();
    rebuild();
    QTimer::singleShot(0, this, [this] {
        m_view.setProperty("editorActionsReady", true);
        for (auto* panel : m_panels)
            panel->refreshActions();
    });
}
void EditorToolbars::rebuild() {
    for (auto* panel : m_panels) {
        panel->hide();
        panel->deleteLater();
    }
    m_panels.clear();
    for (const auto& definition : ActionManager::instance().configuration().toolbars) {
        auto* panel = new EditorToolbarPanel{definition, m_view};
        connect(panel, &EditorToolbarPanel::geometrySettingsChanged, this, &EditorToolbars::scheduleLayout);
        m_panels.push_back(panel);
    }
    scheduleLayout();
}
void EditorToolbars::scheduleLayout() {
    if (m_layoutPending) return;
    m_layoutPending = true;
    QTimer::singleShot(0, this, [this] {
        m_layoutPending = false;
        place();
    });
}
void EditorToolbars::place() {
    std::vector<EditorToolbarLayoutItem> items;
    std::array<int, 4> margins{12, 12, 12, 12};
    for (auto* panel : m_panels) {
        items.push_back(panel->layoutItem());
        margins[size_t(items.back().definition.edge)] = panel->edgeMargin();
    }
    const auto rect = m_view.rect().adjusted(margins[0], margins[1], -margins[2], -margins[3]);
    const auto placements = layoutEditorToolbars(rect, items, ActionManager::instance().configuration().arrangements);
    std::array<int, 4> insets{};
    for (size_t i = 0; i < m_panels.size(); ++i) {
        auto* panel = m_panels[i];
        const auto& place = placements[i];
        if (!place.visible) {
            panel->hide();
            continue;
        }
        const auto newlyVisible = !panel->isVisible();
        panel->setIconExtent(place.iconSize);
        panel->setGeometry(place.geometry);
        panel->show();
        panel->raise();
        if (newlyVisible) panel->refreshActions();
        const auto edge = items[i].definition.edge;
        const auto inset = edge == EditorToolbarEdge::Left ? place.geometry.right() + 1 + items[i].gap
                                                           : edge == EditorToolbarEdge::Right ? m_view.width() - place.geometry.left() + items[i].gap
                                                                                              : edge == EditorToolbarEdge::Top ? place.geometry.bottom() + 1 + items[i].gap
                                                                                                                               : m_view.height() - place.geometry.top() + items[i].gap;
        insets[size_t(edge)] = std::max(insets[size_t(edge)], inset);
    }
    const QVariantList reserved{insets[0], insets[1], insets[2], insets[3]};
    if (m_view.property("editorToolbarInsets").toList() != reserved) m_view.setProperty("editorToolbarInsets", reserved);
}
bool EditorToolbars::eventFilter(QObject* watched, QEvent* event) {
    if (watched == &m_view && (event->type() == QEvent::Resize || event->type() == QEvent::Show || event->type() == QEvent::StyleChange)) scheduleLayout();
    return QObject::eventFilter(watched, event);
}
} // namespace TrenchBroom::View
