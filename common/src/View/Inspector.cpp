/*
 Copyright (C) 2010-2017 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Inspector.h"

#include "View/EntityInspector.h"
#include "View/EntityPropertyEditor.h"
#include "View/FaceInspector.h"
#include "View/FloatingInspectorTool.h"
#include "View/LayerInspector.h"
#include "View/MapInspector.h"
#include "View/PrefabBrowser.h"
#include "View/HistoryEditor.h"
#include "View/TitleBar.h"
#include "View/TitledPanel.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QRubberBand>
#include <QScopedValueRollback>
#include <QScreen>
#include <QScrollBar>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <optional>
#include <utility>
#include <vector>

namespace TrenchBroom::View {
namespace {
constexpr auto layoutKey = "Inspector/DockLayout";
constexpr auto legacyLayoutKey = "Inspector/Tools";
constexpr auto legacyTabsKey = "Inspector/LeftTabs";
constexpr auto headerHeight = 26;

QRect globalRect(const QWidget* widget) {
    return {widget->mapToGlobal(QPoint{}), widget->size()};
}

class ToolGroup : public QWidget {
public:
    ToolGroup(bool onLeft, const QString& savedId, QWidget* parent)
        : QWidget(parent)
        , id(savedId.isEmpty() ? QUuid::createUuid().toString() : savedId)
        , left(onLeft)
        , tabs(new QTabBar(this))
        , contents(new QStackedWidget(this)) {
        setObjectName("InspectorToolGroup");
        tabs->setObjectName("InspectorToolTabs");
        tabs->setExpanding(false);
        tabs->setUsesScrollButtons(true);
        auto* box = new QVBoxLayout(this);
        box->setContentsMargins(0, 0, 0, 0);
        box->setSpacing(0);
        box->addWidget(tabs);
        box->addWidget(contents);
        tabs->hide();
        connect(tabs, &QTabBar::currentChanged, contents, &QStackedWidget::setCurrentIndex);
    }

    int count() const { return contents->count(); }
    void select(int index) {
        tabs->setCurrentIndex(index);
        contents->setCurrentIndex(index);
    }

    const QString id;
    const bool left;
    QTabBar* const tabs;
    QStackedWidget* const contents;
};
}

class Inspector::ToolLayout : public QObject {
    struct Side {
        QPointer<QScrollArea> scroll;
        QPointer<QSplitter> stack;
    };

    struct Tool {
        QString id;
        QString title;
        QPointer<QWidget> widget;
        QPointer<QWidget> handle;
        int stretch;
        QString legacyHost;
        QPointer<ToolGroup> group{};
        QPointer<FloatingInspectorTool> window{};
        QByteArray geometry{};
        std::optional<QPoint> restorePosition{};
        bool returnLeft = false;
        QString returnGroup{};
        int returnRow = -1;
        int returnIndex = -1;
    };

    enum class DropKind { Float, Stack, Tab, Stay };
    struct Drop {
        DropKind kind = DropKind::Float;
        bool left = false;
        int index = -1;
        QPointer<ToolGroup> group;
        QRect preview;
    };

    struct DragOrigin {
        int activeTab;
        QList<int> sizes;
        int scrollPosition;
        QByteArray geometry;
    };

    Inspector& m_owner;
    Side m_left;
    Side m_right;
    std::vector<Tool> m_tools;
    QPointer<QRubberBand> m_preview;
    Tool* m_pressed = nullptr;
    QPointer<QWidget> m_pressedHandle;
    QPoint m_pressPosition;
    bool m_dockedGesture = false;
    bool m_dragging = false;
    bool m_restoring = true;
    bool m_updatingVisibility = false;
    std::optional<DragOrigin> m_dragOrigin;
    QPointer<QWidget> m_mouseGrabber;
    std::function<void()> m_removeNativeDrag;
    QTimer m_geometryTimer;
    QPoint m_dragPosition;
    QTimer m_scrollTimer;

public:
    explicit ToolLayout(Inspector& owner)
        : m_owner(owner), m_left(makeSide(owner.m_leftPane, true)), m_right(makeSide(&owner, false)) {
        m_scrollTimer.setInterval(32);
        connect(&m_scrollTimer, &QTimer::timeout, this, &ToolLayout::scrollDuringDrag);
        m_geometryTimer.setSingleShot(true);
        m_geometryTimer.setInterval(150);
        connect(&m_geometryTimer, &QTimer::timeout, this, [this] {
            if (!m_pressed) save();
        });
    }

    ~ToolLayout() override {
        clearDrag();
        delete m_preview.data();
        for (auto& tool : m_tools) delete tool.window.data();
    }

    void addTools(const QString& legacyHost, const std::vector<InspectorToolSection>& sections) {
        for (const auto& section : sections) {
            if (section.widget->objectName().isEmpty())
                section.widget->setObjectName("InspectorTool_" + section.id);
            section.dragHandle->setToolTip(tr("Drag to dock, group or float this tool. Right-click for placement options."));
            section.dragHandle->installEventFilter(this);
            section.dragHandle->setObjectName("InspectorToolTitle");
            m_tools.push_back({section.id, section.title, section.widget, section.dragHandle, section.stretch, legacyHost});
        }
    }

    void restore() {
        const auto state = QSettings{}.value(layoutKey).toMap();
        if (state.isEmpty()) {
            restoreLegacy();
        } else {
            const auto tools = state.value("tools").toMap();
            for (auto& tool : m_tools) readToolState(tool, tools.value(tool.id).toMap());
            restoreSide(m_left, true, state.value("left").toMap());
            restoreSide(m_right, false, state.value("right").toMap());
            for (auto& tool : m_tools) {
                if (!tool.group && tools.value(tool.id).toMap().value("floating").toBool())
                    makeFloating(tool, m_owner.window()->pos() + QPoint(80, 80), true);
            }
        }
        for (auto& tool : m_tools) {
            if (!tool.group && !tool.window) insertGroup(tool, false, -1);
        }
        updateVisibility();
        // The central widget and frame geometry are installed after Inspector construction.
        QTimer::singleShot(0, this, [this] {
            m_restoring = false;
            updateVisibility();
        });
    }

    void save() const {
        QVariantMap tools;
        for (const auto& tool : m_tools) {
            auto state = QVariantMap{
                {"floating", bool(tool.window)},
                {"geometry", tool.window ? tool.window->saveGeometry() : tool.geometry},
                {"returnLeft", tool.returnLeft}, {"returnGroup", tool.returnGroup},
                {"returnRow", tool.returnRow}, {"returnIndex", tool.returnIndex}};
            if (tool.window) state["position"] = tool.restorePosition.value_or(tool.window->pos());
            tools[tool.id] = state;
        }
        QSettings{}.setValue(layoutKey,
            QVariantMap{{"left", saveSide(m_left)}, {"right", saveSide(m_right)}, {"tools", tools}});
    }

    void dock(const QString& id, bool left, int row) {
        if (auto* tool = find(id)) {
            if (!tool->group || tool->group->count() != 1 || tool->group->left != left
                || side(left).stack->indexOf(tool->group) != row) {
                take(*tool);
                insertGroup(*tool, left, row);
            }
            placed(*tool);
        }
    }

    void tabify(const QString& id, const QString& targetId, int index) {
        auto* tool = find(id);
        const auto* target = find(targetId);
        if (!tool || !target || tool == target || !target->group) return;
        moveToTab(*tool, *target->group, index);
    }

    void floatTool(const QString& id, const QPoint& position) {
        if (auto* tool = find(id)) {
            makeFloating(*tool, position, false);
            placed(*tool);
        }
    }

    void reveal(const QString& id) {
        if (auto* tool = find(id)) reveal(*tool);
    }

    void reset() {
        cancelDrag();
        for (auto& tool : m_tools) take(tool);
        for (auto& tool : m_tools) {
            tool.geometry.clear();
            tool.restorePosition.reset();
            tool.returnGroup.clear();
            tool.returnLeft = false;
            tool.returnRow = tool.returnIndex = -1;
            insertGroup(tool, false, -1);
        }
        updateVisibility();
        QSettings settings;
        settings.remove(legacyLayoutKey);
        settings.remove(legacyTabsKey);
        save();
    }

    void updateVisibility() {
        const QScopedValueRollback updating(m_updatingVisibility, true);
        const auto showSide = [this](QWidget* pane, const Side& dockSide) {
            pane->setVisible(m_owner.panelsVisible() && dockSide.stack && dockSide.stack->count() > 0);
            auto* splitter = qobject_cast<QSplitter*>(pane->parentWidget());
            if (!pane->isVisible() || !splitter || splitter->orientation() != Qt::Horizontal) return;
            auto sizes = splitter->sizes();
            const auto index = splitter->indexOf(pane);
            if (index >= 0 && sizes[index] == 0) {
                sizes[index] = pane->minimumWidth();
                splitter->setSizes(sizes);
            }
        };
        showSide(&m_owner, m_right);
        showSide(m_owner.m_leftPane, m_left);
        for (auto& tool : m_tools) {
            if (!tool.window) continue;
            const auto show = !m_restoring && m_owner.panelsVisible();
            tool.window->setVisible(show);
            if (show && tool.restorePosition) {
                const auto position = *std::exchange(tool.restorePosition, std::nullopt);
                auto* screen = QGuiApplication::screenAt(position);
                // Qt may move the entire window onto the screen on restore.
                // Keep its saved position when the title remains reachable.
                if (screen && screen->availableGeometry().contains(QRect(position, QSize(80, 18))))
                    tool.window->move(position);
            }
        }
    }

    bool cancelDrag() {
        if (!m_pressed) return false;
        auto* tool = m_pressed;
        const auto origin = m_dragOrigin;
        if (!m_dockedGesture && tool->window) tool->window->cancelTitleDrag();
        clearDrag();
        if (origin) {
            auto& group = returnToDock(*tool);
            tool->geometry = origin->geometry;
            group.select(origin->activeTab);
            const auto& dockSide = side(group.left);
            dockSide.stack->setSizes(origin->sizes);
            updateVisibility();
            dockSide.scroll->verticalScrollBar()->setValue(origin->scrollPosition);
        }
        return true;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if ((event->type() == QEvent::Move || event->type() == QEvent::Resize)
            && !m_pressed && !m_restoring && !m_updatingVisibility) {
            const auto* widget = qobject_cast<QWidget*>(watched);
            if (widget && widget->isVisible() && std::any_of(m_tools.begin(), m_tools.end(),
                    [widget](const Tool& tool) { return tool.window == widget; }))
                m_geometryTimer.start();
        }
        if (m_pressed && m_dockedGesture) {
            switch (event->type()) {
                case QEvent::ApplicationDeactivate:
                    cancelDrag();
                    break;
                case QEvent::Hide:
                    if (watched == m_pressedHandle && !m_dragOrigin) cancelDrag();
                    break;
                case QEvent::WindowDeactivate:
                    if (watched == m_owner.window() && !m_dragOrigin) cancelDrag();
                    break;
                case QEvent::KeyPress:
                    if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
                        cancelDrag();
                        return true;
                    }
                    break;
                case QEvent::MouseMove: {
                    const auto* mouse = static_cast<QMouseEvent*>(event);
                    if (!mouse->buttons().testFlag(Qt::LeftButton)) {
                        cancelDrag();
                        break;
                    }
                    const auto pos = mouse->globalPos();
                    m_dragging = m_dragging || (pos - m_pressPosition).manhattanLength() >= QApplication::startDragDistance();
                    if (m_dragging) previewAt(pos);
                    return true;
                }
                case QEvent::MouseButtonRelease: {
                    const auto* mouse = static_cast<QMouseEvent*>(event);
                    if (mouse->button() != Qt::LeftButton) break;
                    auto* tool = m_pressed;
                    const auto handle = m_pressedHandle;
                    const auto pos = mouse->globalPos();
                    if (m_dragging) finishDrag(pos);
                    else {
                        clearDrag();
                        if (handle && globalRect(handle).contains(pos)) click(*tool, handle, pos);
                    }
                    return true;
                }
                default: break;
            }
        }

        if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::ContextMenu)
            return QObject::eventFilter(watched, event);
        const auto pos = event->type() == QEvent::MouseButtonPress
            ? static_cast<QMouseEvent*>(event)->globalPos()
            : static_cast<QContextMenuEvent*>(event)->globalPos();
        auto* tool = toolAt(watched, pos);
        if (!tool) return QObject::eventFilter(watched, event);
        if (event->type() == QEvent::ContextMenu) {
            showMenu(*tool, pos);
            return true;
        }
        if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
            beginDrag(*tool, static_cast<QWidget*>(watched), pos);
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    static Side makeSide(QWidget* pane, bool left) {
        pane->setMinimumWidth(300);
        auto* scroll = new QScrollArea(pane);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* stack = new QSplitter(Qt::Vertical);
        stack->setObjectName(left ? "Inspector_LeftStack" : "Inspector_RightStack");
        stack->setChildrenCollapsible(false);
        stack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
        scroll->setWidget(stack);
        auto* box = new QVBoxLayout(pane);
        box->setContentsMargins(0, 0, 0, 0);
        box->addWidget(scroll);
        return {scroll, stack};
    }

    const Side& side(bool left) const { return left ? m_left : m_right; }

    Tool* find(const QString& id) {
        const auto it = std::find_if(m_tools.begin(), m_tools.end(), [&](const Tool& tool) { return tool.id == id; });
        return it == m_tools.end() ? nullptr : &*it;
    }

    Tool* toolAt(QObject* handle, const QPoint& pos) {
        if (const auto* tabs = qobject_cast<QTabBar*>(handle)) {
            auto* tool = find(tabs->tabData(tabs->tabAt(tabs->mapFromGlobal(pos))).toString());
            return tool && tool->group && tool->group->tabs == tabs ? tool : nullptr;
        }
        const auto it = std::find_if(m_tools.begin(), m_tools.end(), [&](const Tool& tool) {
            return tool.handle == handle;
        });
        return it == m_tools.end() ? nullptr : &*it;
    }

    void syncGroup(ToolGroup& group) {
        const auto grouped = group.count() > 1;
        group.tabs->setVisible(grouped);
        auto* splitter = side(group.left).stack.data();
        int stretch = 0;
        for (int i = 0; i < group.count(); ++i) {
            if (auto* tool = find(group.tabs->tabData(i).toString())) {
                tool->handle->setVisible(!grouped);
                stretch = std::max(stretch, tool->stretch);
            }
        }
        splitter->setStretchFactor(splitter->indexOf(&group), stretch);
    }

    void take(Tool& tool) {
        if (auto* group = tool.group.data()) {
            const auto index = group->contents->indexOf(tool.widget);
            const QSignalBlocker blocker(group->tabs);
            group->contents->removeWidget(tool.widget);
            group->tabs->removeTab(index);
            tool.group.clear();
            if (group->count() > 0) {
                group->select(group->tabs->currentIndex());
                syncGroup(*group);
            } else {
                group->hide();
                group->setParent(&m_owner);
                group->deleteLater();
            }
        }
        tool.widget->hide();
        tool.widget->setParent(&m_owner);
        if (auto* window = tool.window.data()) {
            tool.geometry = window->saveGeometry();
            tool.restorePosition.reset();
            tool.window.clear();
            window->hide();
            window->deleteLater();
        }
    }

    ToolGroup& insertGroup(Tool& tool, bool left, int row, const QString& id = {}) {
        auto* stack = side(left).stack.data();
        auto* group = new ToolGroup(left, id, stack);
        group->tabs->installEventFilter(this);
        stack->insertWidget(row < 0 ? stack->count() : row, group);
        insertTab(tool, *group, -1);
        group->show();
        return *group;
    }

    void insertTab(Tool& tool, ToolGroup& group, int index) {
        tool.group = &group;
        const QSignalBlocker blocker(group.tabs);
        const auto i = group.contents->insertWidget(index, tool.widget);
        group.tabs->insertTab(i, tool.title);
        group.tabs->setTabData(i, tool.id);
        group.select(i);
        syncGroup(group);
    }

    void moveToTab(Tool& tool, ToolGroup& group, int index) {
        take(tool);
        insertTab(tool, group, index);
        placed(tool);
    }

    void makeFloating(Tool& tool, const QPoint& position, bool restoring) {
        bool restoredGeometry = false;
        if (!tool.window) {
            if (tool.group) {
                tool.returnLeft = tool.group->left;
                tool.returnGroup = tool.group->id;
                tool.returnRow = side(tool.returnLeft).stack->indexOf(tool.group);
                tool.returnIndex = tool.group->contents->indexOf(tool.widget);
            }
            const auto size = tool.widget->size();
            take(tool);
            auto* window = new FloatingInspectorTool(&m_owner, [this, &tool] {
                if (m_pressed == &tool) cancelDrag();
                else {
                    returnToDock(tool);
                    placed(tool);
                }
            }, {
                [this, &tool] { beginDrag(tool, nullptr, {}); },
                [this](const QPoint& pos) {
                    m_dragging = true;
                    previewAt(pos);
                },
                [this](const QPoint& pos) { finishDrag(pos); },
                [this] { clearDrag(); }
            });
            tool.window = window;
            window->setObjectName("InspectorFloating_" + tool.id);
            window->setProperty("inspectorDropHighlight", false);
            window->setToolTitle(tool.title);
            window->setAttribute(Qt::WA_ShowWithoutActivating);
            window->installEventFilter(this);
            tool.handle->hide();
            window->layout()->addWidget(tool.widget);
            tool.widget->show();
            window->resize(size.boundedTo(QSize(560, 520)).expandedTo(QSize(300, 200)));
            restoredGeometry = restoring && !tool.geometry.isEmpty() && window->restoreGeometry(tool.geometry);
        }
        if (!restoring) tool.restorePosition.reset();
        if (!restoredGeometry) {
            auto pos = position;
            auto* screen = QGuiApplication::screenAt(pos);
            if (!screen) screen = m_owner.screen();
            if (screen) {
                const auto area = screen->availableGeometry();
                pos.setX(std::clamp(pos.x(), area.left(), std::max(area.left(), area.right() - 80)));
                pos.setY(std::clamp(pos.y(), area.top(), std::max(area.top(), area.bottom() - 40)));
            }
            tool.window->move(pos);
        }
    }

    ToolGroup& returnToDock(Tool& tool) {
        take(tool);
        for (const auto left : {tool.returnLeft, !tool.returnLeft}) {
            auto* stack = side(left).stack.data();
            for (int i = 0; i < stack->count(); ++i) {
                auto* group = static_cast<ToolGroup*>(stack->widget(i));
                if (group->id == tool.returnGroup) {
                    insertTab(tool, *group, tool.returnIndex);
                    return *group;
                }
            }
        }
        return insertGroup(tool, tool.returnLeft, tool.returnRow, tool.returnGroup);
    }

    void reveal(Tool& tool) {
        m_owner.setPanelsVisible(true);
        if (tool.window) {
            tool.window->raise();
            tool.window->activateWindow();
        } else if (tool.group) {
            auto* group = tool.group.data();
            group->select(group->contents->indexOf(tool.widget));
            auto* header = group->count() == 1 ? tool.handle.data() : group->tabs;
            side(group->left).scroll->ensureWidgetVisible(header, 0, 8);
        }
    }

    void placed(Tool& tool) {
        reveal(tool);
        save();
    }

    static QVariantMap saveSide(const Side& dockSide) {
        if (!dockSide.stack) return {};
        QVariantList groups;
        QVariantList sizes;
        for (int i = 0; i < dockSide.stack->count(); ++i) {
            const auto* group = static_cast<ToolGroup*>(dockSide.stack->widget(i));
            QStringList ids;
            for (int j = 0; j < group->count(); ++j) ids.push_back(group->tabs->tabData(j).toString());
            groups.push_back(QVariantMap{{"id", group->id}, {"tools", ids},
                {"active", group->tabs->tabData(group->tabs->currentIndex())}});
        }
        for (const auto size : dockSide.stack->sizes()) sizes.push_back(size);
        return {{"groups", groups}, {"sizes", sizes}};
    }

    static void readToolState(Tool& tool, const QVariantMap& state) {
        tool.geometry = state.value("geometry").toByteArray();
        if (const auto position = state.value("position"); position.canConvert<QPoint>())
            tool.restorePosition = position.toPoint();
        tool.returnLeft = state.value("returnLeft").toBool();
        tool.returnGroup = state.value("returnGroup").toString();
        tool.returnRow = state.value("returnRow", -1).toInt();
        tool.returnIndex = state.value("returnIndex", -1).toInt();
    }

    void restoreSide(const Side& dockSide, bool left, const QVariantMap& state) {
        for (const auto& row : state.value("groups").toList()) {
            const auto groupState = row.toMap();
            ToolGroup* group = nullptr;
            for (const auto& id : groupState.value("tools").toStringList()) {
                auto* tool = find(id);
                if (!tool || tool->group) continue;
                if (!group) group = &insertGroup(*tool, left, -1, groupState.value("id").toString());
                else insertTab(*tool, *group, -1);
            }
            if (auto* active = find(groupState.value("active").toString()); group && active && active->group == group)
                group->select(group->contents->indexOf(active->widget));
        }
        const auto savedSizes = state.value("sizes").toList();
        if (savedSizes.size() == dockSide.stack->count()) {
            QList<int> sizes;
            for (const auto& size : savedSizes) sizes.push_back(std::clamp(size.toInt(), 0, 100000));
            dockSide.stack->setSizes(sizes);
        }
    }

    void restoreLegacy() {
        const QSettings settings;
        const auto placements = settings.value(legacyLayoutKey).toMap().value("placements").toMap();
        const auto leftTabs = settings.value(legacyTabsKey).toList();
        if (placements.isEmpty() && leftTabs.isEmpty()) return;
        const QStringList hosts{"layer", "map", "face", "entity", "properties", "left", "right"};
        const auto onLeft = [&](const QString& host) {
            const auto index = hosts.indexOf(host);
            return host == "left" || (index >= 0 && leftTabs.contains(QVariant(index)));
        };
        struct Placement {
            Tool* tool;
            qsizetype hostOrder;
            int order;
            bool left;
        };
        std::vector<Placement> ordered;
        for (auto& tool : m_tools) {
            const auto state = placements.value(tool.id).toMap();
            readToolState(tool, state);
            const auto host = state.value("host", tool.legacyHost).toString();
            if (host == "floating") {
                tool.returnLeft = onLeft(state.value("returnHost").toString());
                tool.returnRow = state.value("returnIndex", -1).toInt();
                makeFloating(tool, m_owner.window()->pos() + QPoint(80, 80), true);
            } else {
                ordered.push_back({&tool, hosts.indexOf(host), state.value("index", 0).toInt(), onLeft(host)});
            }
        }
        std::stable_sort(ordered.begin(), ordered.end(), [](const Placement& a, const Placement& b) {
            if (a.hostOrder != b.hostOrder) return a.hostOrder < b.hostOrder;
            return a.order < b.order;
        });
        for (const auto& placement : ordered) insertGroup(*placement.tool, placement.left, -1);
    }

    QWidget* dockArea() const {
        auto* window = m_owner.window();
        if (auto* frame = qobject_cast<QMainWindow*>(window); frame && frame->centralWidget())
            return frame->centralWidget();
        return window;
    }

    Drop dropAt(const QPoint& pos) {
        auto* top = QApplication::topLevelAt(pos);
        if (top && top != m_owner.window() && top != m_pressed->window) return {};
        const auto area = globalRect(dockArea());
        if (!area.contains(pos)) return {};
        constexpr int edgeWidth = 28;
        constexpr int stackGutter = 7;
        for (const auto left : {true, false}) {
            const auto& dockSide = side(left);
            const auto visible = globalRect(dockSide.scroll->viewport());
            if (dockSide.stack->count() == 0 || !dockSide.scroll->isVisible() || visible.width() <= 0) {
                const auto edge = left ? area.left() : area.right() - edgeWidth + 1;
                if (QRect(edge, area.top(), edgeWidth, area.height()).contains(pos)) {
                    const auto width = std::min(300, area.width() / 2);
                    const auto x = left ? area.left() : area.right() - width + 1;
                    return {DropKind::Stack, left, 0, {}, QRect(x, area.top(), width, area.height())};
                }
                continue;
            }
            if (!visible.contains(pos)) continue;
            int index = 0;
            int markerY = visible.top();
            for (int row = 0; row < dockSide.stack->count(); ++row) {
                auto* group = static_cast<ToolGroup*>(dockSide.stack->widget(row));
                const auto bounds = globalRect(group);
                const auto grouped = group->count() > 1;
                auto* first = find(group->tabs->tabData(0).toString());
                const auto header = globalRect(grouped ? group->tabs : first->handle.data());
                const auto tabTarget = header.contains(pos)
                    || (grouped && bounds.adjusted(0, 0, 0, -stackGutter).contains(pos));
                if (tabTarget) {
                    if (!grouped && first == m_pressed) return {DropKind::Stay, left, -1, {}, {}};
                    auto tabIndex = -1;
                    if (grouped && header.contains(pos)) {
                        tabIndex = 0;
                        const auto x = group->tabs->mapFromGlobal(pos).x();
                        for (int i = 0; i < group->count(); ++i) {
                            if (group->tabs->tabData(i).toString() == m_pressed->id) continue;
                            if (x < group->tabs->tabRect(i).center().x()) break;
                            ++tabIndex;
                        }
                    }
                    return {DropKind::Tab, left, tabIndex, group, bounds.intersected(visible)};
                }
                if (group == m_pressed->group && !grouped) continue;
                if (pos.y() < bounds.center().y()) {
                    markerY = bounds.top();
                    break;
                }
                markerY = bounds.bottom();
                ++index;
            }
            const auto y = std::clamp(markerY, visible.top(), std::max(visible.top(), visible.bottom() - 3));
            return {DropKind::Stack, left, index, {}, QRect(visible.left(), y, visible.width(), 4)};
        }
        return {};
    }

    static void setDropHighlight(QWidget* window, bool highlight) {
        if (!window || window->property("inspectorDropHighlight").toBool() == highlight) return;
        window->setProperty("inspectorDropHighlight", highlight);
        window->style()->unpolish(window);
        window->style()->polish(window);
    }

    void showPreview(const Drop& target) {
        const auto highlighting = !target.preview.isEmpty();
        setDropHighlight(m_pressed->window, highlighting);
        if (!highlighting) {
            if (m_preview) m_preview->hide();
            return;
        }
        auto* area = dockArea();
        if (!m_preview) {
            m_preview = new QRubberBand(QRubberBand::Rectangle, area);
            m_preview->setObjectName("InspectorDropPreview");
            m_preview->setAttribute(Qt::WA_TransparentForMouseEvents);
        }
        m_preview->setProperty("dropKind", target.kind == DropKind::Tab ? "tab" : "stack");
        m_preview->setGeometry(target.preview.translated(-area->mapToGlobal(QPoint{})));
        m_preview->raise();
        m_preview->show();
    }

    void previewAt(const QPoint& pos) {
        m_dragPosition = pos;
        const auto target = dropAt(pos);
        if (target.kind == DropKind::Float && m_dockedGesture && !m_dragOrigin)
            beginLiveFloat(pos);
        if (m_dragOrigin) m_pressed->window->move(pos - QPoint(24, 16));
        showPreview(target);
        if (!m_scrollTimer.isActive()) m_scrollTimer.start();
    }

    void beginLiveFloat(const QPoint& pos) {
        auto& tool = *m_pressed;
        const auto& dockSide = side(tool.group->left);
        m_dragOrigin.emplace(DragOrigin{tool.group->tabs->currentIndex(), dockSide.stack->sizes(),
            dockSide.scroll->verticalScrollBar()->value(), tool.geometry});
#ifdef __APPLE__
        if (QGuiApplication::platformName() == "cocoa") {
            // Reparenting the dragged panel invalidates Qt's implicit mouse grab.
            m_removeNativeDrag = installMacToolTitleHandler(*m_owner.window(),
                [this](ToolTitleEvent event, const QPoint& point) {
                    if (!m_dockedGesture || !m_dragOrigin) return false;
                    if (event == ToolTitleEvent::Move) previewAt(point);
                    else if (event == ToolTitleEvent::Release) finishDrag(point);
                    else if (event == ToolTitleEvent::Cancel) cancelDrag();
                    return true;
                }, true);
        }
#endif
        makeFloating(tool, pos - QPoint(24, 16), false);
        updateVisibility();
    }

    void scrollDuringDrag() {
        constexpr int margin = 20;
        constexpr int step = 12;
        for (const auto* dockSide : {&m_left, &m_right}) {
            auto* scroll = dockSide->scroll.data();
            if (!scroll || !scroll->isVisible()) continue;
            const auto viewport = globalRect(scroll->viewport());
            if (!viewport.contains(m_dragPosition)) continue;
            auto* bar = scroll->verticalScrollBar();
            const auto before = bar->value();
            if (m_dragPosition.y() < viewport.top() + margin) bar->setValue(before - step);
            else if (m_dragPosition.y() > viewport.bottom() - margin) bar->setValue(before + step);
            if (bar->value() != before) showPreview(dropAt(m_dragPosition));
            return;
        }
    }

    void beginDrag(Tool& tool, QWidget* handle, const QPoint& pos) {
        if (m_pressed) cancelDrag();
        m_pressed = &tool;
        m_pressedHandle = handle;
        m_dockedGesture = handle != nullptr;
        m_pressPosition = pos;
        if (m_dockedGesture) {
            qApp->installEventFilter(this);
#ifndef __APPLE__
            if (QGuiApplication::platformName() != "offscreen") {
                m_mouseGrabber = m_owner.window();
                m_mouseGrabber->grabMouse();
            }
#endif
        }
        QApplication::setOverrideCursor(Qt::ClosedHandCursor);
    }

    void clearDrag() {
        if (!m_pressed) return;
        auto* tool = m_pressed;
        m_pressed = nullptr;
        m_pressedHandle.clear();
        m_dockedGesture = false;
        m_dragOrigin.reset();
        if (m_removeNativeDrag) std::exchange(m_removeNativeDrag, {})();
        if (m_mouseGrabber) {
            if (QWidget::mouseGrabber() == m_mouseGrabber) m_mouseGrabber->releaseMouse();
            m_mouseGrabber.clear();
        }
        m_dragging = false;
        m_scrollTimer.stop();
        if (qApp) {
            qApp->removeEventFilter(this);
            QApplication::restoreOverrideCursor();
        }
        if (m_preview) m_preview->hide();
        setDropHighlight(tool->window, false);
    }

    void finishDrag(const QPoint& pos) {
        auto* tool = m_pressed;
        if (!tool) return;
        const auto target = dropAt(pos);
        const auto liveFloat = m_dragOrigin.has_value();
        if (liveFloat) tool->window->move(pos - QPoint(24, 16));
        clearDrag();
        switch (target.kind) {
            case DropKind::Stack:
                dock(tool->id, target.left, target.index);
                break;
            case DropKind::Tab:
                moveToTab(*tool, *target.group, target.index);
                break;
            case DropKind::Float:
                if (liveFloat) placed(*tool);
                else if (tool->window) save();
                else floatTool(tool->id, pos - QPoint(24, 16));
                break;
            case DropKind::Stay: break;
        }
    }

    void click(Tool& tool, QWidget* handle, const QPoint& pos) {
        if (qobject_cast<QTabBar*>(handle) && toolAt(handle, pos) == &tool && tool.group) {
            handle->setFocus(Qt::MouseFocusReason);
            tool.group->select(tool.group->contents->indexOf(tool.widget));
        }
    }

    void showMenu(Tool& tool, const QPoint& pos) {
        QMenu menu(&m_owner);
        auto* left = menu.addAction(tr("Dock left"));
        auto* right = menu.addAction(tr("Dock right"));
        auto* floating = menu.addAction(tr("Float tool"));
        menu.addSeparator();
        auto* resetAction = menu.addAction(tr("Reset tool layout"));
        const auto* action = menu.exec(pos);
        if (action == left) dock(tool.id, true, -1);
        else if (action == right) dock(tool.id, false, -1);
        else if (action == floating) floatTool(tool.id, pos);
        else if (action == resetAction) reset();
    }
};

Inspector::Inspector(const std::weak_ptr<MapDocument>& document, GLContextManager& contextManager,
    MapView* mapView, QWidget* parent)
    : QWidget(parent), m_leftPane(new QWidget(this)), m_faceInspector(new FaceInspector(document, contextManager, this)) {
    setObjectName("Inspector_Widget");
    m_leftPane->setObjectName("Inspector_LeftPane");
    m_leftPane->setMinimumHeight(100);

    auto* layers = new LayerInspector(document, mapView, this, this);
    auto* map = new MapInspector(document, this);
    auto* entities = new EntityInspector(document, contextManager, this);
    for (auto* page : std::array<QWidget*, 4>{layers, map, m_faceInspector, entities}) page->hide();

    auto* properties = new TitledPanel(tr("Properties"), this);
    auto* editor = new EntityPropertyEditor(document);
    editor->setObjectName("PropertiesInspector_Widget");
    auto* layout = new QVBoxLayout(properties->getPanel());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(editor);

    auto* prefabs = new TitledPanel(tr("Prefab"), this);
    auto* prefabLayout = new QVBoxLayout(prefabs->getPanel());
    prefabLayout->setContentsMargins(0, 0, 0, 0);
    prefabLayout->addWidget(new PrefabBrowser(document, mapView));

    auto* history = new TitledPanel(tr("History"), this);
    auto* historyLayout = new QVBoxLayout(history->getPanel());
    historyLayout->setContentsMargins(0, 0, 0, 0);
    historyLayout->addWidget(new HistoryEditor(document));

    m_toolLayout = std::make_unique<ToolLayout>(*this);
    m_toolLayout->addTools("layer", layers->toolSections());
    m_toolLayout->addTools("map", map->toolSections());
    m_toolLayout->addTools("face", m_faceInspector->toolSections());
    m_toolLayout->addTools("entity", entities->toolSections());
    m_toolLayout->addTools("properties", {{"properties", tr("Properties"), properties, properties->getTitleBar()}});
    m_toolLayout->addTools("entity", {{"prefabs", tr("Prefab"), prefabs, prefabs->getTitleBar(), 2}});
    m_toolLayout->addTools("map", {{"history", tr("History"), history, history->getTitleBar(), 2}});
    m_toolLayout->restore();
}

Inspector::~Inspector() = default;

void Inspector::dockToolToSide(const QString& id, bool left, int row) { m_toolLayout->dock(id, left, row); }
void Inspector::tabifyTool(const QString& id, const QString& targetId, int index) { m_toolLayout->tabify(id, targetId, index); }
void Inspector::floatTool(const QString& id, const QPoint& pos) { m_toolLayout->floatTool(id, pos); }
void Inspector::revealTool(const QString& id) { m_toolLayout->reveal(id); }
void Inspector::resetToolLayout() { m_toolLayout->reset(); }
void Inspector::saveToolLayout() const { m_toolLayout->save(); }

QWidget* Inspector::leftPane() const { return m_leftPane; }

void Inspector::setPanelsVisible(bool visible) {
    if (!visible) m_toolLayout->cancelDrag();
    m_panelsVisible = visible;
    m_toolLayout->updateVisibility();
}

bool Inspector::panelsVisible() const { return m_panelsVisible; }
bool Inspector::cancelMouseDrag() const { return m_toolLayout->cancelDrag() || m_faceInspector->cancelMouseDrag(); }
FaceInspector* Inspector::faceInspector() const { return m_faceInspector; }
}
