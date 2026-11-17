#include "Error.h"
#include "IO/GameConfigParser.h"
#include "Logger.h"
#include "Model/GameImpl.h"
#include "View/EntityPropertyEditor.h"
#include "View/GLContextManager.h"
#include "View/Inspector.h"
#include "View/MapDocumentCommandFacade.h"
#include "View/RenderView.h"
#include "View/SwitchableTitledPanel.h"
#include "View/TitleBar.h"
#include <QAction>
#include <QApplication>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QPointer>
#include <QRubberBand>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <catch2/catch.hpp>
#include <kdl/result.h>

namespace TrenchBroom::View {
namespace {
class InspectorSettingsScope {
    const QStringList m_keys{"Inspector/LeftTabs", "Inspector/Tools", "Inspector/DockLayout"};
    QSettings m_settings;
    QVariantMap m_saved;

public:
    InspectorSettingsScope() {
        for (const auto& key : m_keys) {
            m_saved.insert(key, m_settings.value(key));
            m_settings.remove(key);
        }
    }

    ~InspectorSettingsScope() {
        for (const auto& key : m_keys) {
            const auto value = m_saved.value(key);
            if (value.isValid()) m_settings.setValue(key, value);
            else m_settings.remove(key);
        }
    }
};

class InspectorFixture {
public:
    NullLogger logger;
    Model::GameConfig config = IO::GameConfigParser(R"({
        "version": 8, "name": "InspectorTest",
        "fileformats": [{"format": "Standard"}],
        "filesystem": {"searchpath": ".", "packageformat": {"extension": ".pak", "format": "idpak"} },
        "textures": {"root": "textures", "format": {"extensions": [".png"], "format": "image"} },
        "entities": {"definitions": ["Generic.fgd"], "defaultcolor": "0.6 0.6 0.6 1.0"}
    })", std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path()
        / "app/resources/games/Generic/GameConfig.cfg").parse();
    std::shared_ptr<Model::GameImpl> game = std::make_shared<Model::GameImpl>(config, std::filesystem::path{}, logger);
    std::shared_ptr<MapDocument> document = MapDocumentCommandFacade::newMapDocument();
    GLContextManager graphics;
    QWidget host;
    Inspector* inspector;
    QSplitter* left;
    QSplitter* right;

    explicit InspectorFixture(bool showRenderViews = false) {
        REQUIRE(document->newDocument(Model::MapFormat::Standard, vm::bbox3(8192.0), game).is_success());
        auto* layout = new QVBoxLayout(&host);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* outer = new QSplitter(Qt::Horizontal);
        layout->addWidget(outer);
        auto* inner = new QSplitter(Qt::Horizontal);
        inspector = new Inspector(document, graphics, nullptr, inner);
        inner->addWidget(new QWidget);
        inner->addWidget(inspector);
        outer->addWidget(inspector->leftPane());
        outer->addWidget(inner);
        outer->setStretchFactor(1, 1);
        inner->setStretchFactor(0, 1);
        left = inspector->leftPane()->findChild<QSplitter*>("Inspector_LeftStack");
        right = inspector->findChild<QSplitter*>("Inspector_RightStack");
        REQUIRE(left);
        REQUIRE(right);
        inspector->setPanelsVisible(true);
        if (!showRenderViews) {
            for (auto* view : host.findChildren<RenderView*>()) view->hide();
        }
    }

    void showHost() {
        host.resize(1120, 720);
        host.show();
        QCoreApplication::processEvents();
    }

    QWidget* search() const {
        auto* result = host.findChild<QWidget*>("MapInspector_SearchBoxPanel");
        REQUIRE(result);
        return result;
    }

    QWidget* layers() const {
        auto* result = host.findChild<QWidget*>("LayerInspector_LayersPanel");
        REQUIRE(result);
        return result;
    }

    QWidget* mapProperties() const {
        auto* result = host.findChild<QWidget*>("MapInspector_MapPropertiesPanel");
        REQUIRE(result);
        return result;
    }

    QRubberBand* preview() const {
        return host.findChild<QRubberBand*>("InspectorDropPreview");
    }

    static QWidget* groupOf(QWidget* tool) {
        for (auto* parent = tool->parentWidget(); parent; parent = parent->parentWidget()) {
            if (parent->objectName() == "InspectorToolGroup") return parent;
        }
        return nullptr;
    }

    static QTabBar* groupTabs(QWidget* tool) {
        auto* group = groupOf(tool);
        REQUIRE(group);
        auto* bar = group->findChild<QTabBar*>("InspectorToolTabs");
        REQUIRE(bar);
        return bar;
    }

    static int tabIndex(const QTabBar& bar, const QString& title) {
        for (int i = 0; i < bar.count(); ++i) {
            if (bar.tabText(i) == title) return i;
        }
        return -1;
    }

    static QStackedWidget* groupPages(QWidget* tool) {
        auto* group = groupOf(tool);
        REQUIRE(group);
        auto* pages = group->findChild<QStackedWidget*>();
        REQUIRE(pages);
        return pages;
    }

    static TitleBar* titleBar(QWidget* tool) {
        auto* bar = tool->findChild<TitleBar*>(QString(), Qt::FindDirectChildrenOnly);
        REQUIRE(bar);
        return bar;
    }

    static QToolBar* visibleToolbar(QWidget* tool) {
        for (auto* toolbar : tool->findChildren<QToolBar*>("InspectorToolToolbar")) {
            if (toolbar->isVisible()) return toolbar;
        }
        return nullptr;
    }

    static QPoint center(const QWidget& widget) {
        return widget.mapToGlobal(widget.rect().center());
    }

    static QPoint visibleCenter(QWidget& widget) {
        auto bounds = QRect(widget.mapToGlobal(QPoint{}), widget.size());
        for (auto* ancestor = widget.parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
            bounds = bounds.intersected(QRect(ancestor->mapToGlobal(QPoint{}), ancestor->size()));
        }
        CAPTURE(widget.objectName(), widget.x(), widget.y(), widget.width(), widget.height());
        REQUIRE_FALSE(bounds.isEmpty());
        return bounds.center();
    }

    static void press(QWidget& handle, const QPoint& position) {
        mouse(handle, QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton);
    }

    static void move(QWidget& handle, const QPoint& position) {
        mouse(handle, QEvent::MouseMove, position, Qt::NoButton, Qt::LeftButton);
    }

    static void release(QWidget& handle, const QPoint& position) {
        mouse(handle, QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton);
        QCoreApplication::processEvents();
    }

    static void drag(QWidget& handle, const QPoint& start, const QPoint& destination) {
        press(handle, start);
        move(handle, destination);
        release(handle, destination);
    }

    static void drag(QWidget& handle, const QPoint& destination) {
        drag(handle, center(handle), destination);
    }

    static bool hasHandCursor(const QWidget& widget) {
        const auto* override = QApplication::overrideCursor();
        const auto shape = override ? override->shape() : widget.cursor().shape();
        return shape == Qt::OpenHandCursor || shape == Qt::ClosedHandCursor;
    }

private:
    static void mouse(QWidget& handle, const QEvent::Type type, const QPoint& position,
        const Qt::MouseButton button, const Qt::MouseButtons buttons) {
        QMouseEvent event(type, handle.mapFromGlobal(position), handle.window()->mapFromGlobal(position),
            position, button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(&handle, &event);
    }
};
}

TEST_CASE_METHOD(InspectorSettingsScope, "Inspector starts with independent tools and no general tabs", "[inspector]") {
    InspectorFixture f;
    CHECK(f.left->count() == 0);
    CHECK(f.inspector->leftPane()->isHidden());
    CHECK(f.right->count() == 10);
    CHECK(f.host.findChildren<QTabWidget*>().empty());
    for (auto* tabs : f.host.findChildren<QTabBar*>("InspectorToolTabs")) {
        CHECK(tabs->isHidden());
    }

    auto* properties = f.host.findChild<EntityPropertyEditor*>();
    auto* entity = f.host.findChild<QWidget*>("EntityInspector_BrowserPanel");
    REQUIRE(properties);
    REQUIRE(entity);
    CHECK_FALSE(entity->isAncestorOf(properties));
}

TEST_CASE_METHOD(InspectorSettingsScope, "Each tool has a uniform title bar and a toolbar without collapse controls", "[inspector]") {
    InspectorFixture f;
    f.showHost();
    const auto titleHeight = InspectorFixture::titleBar(f.search())->height();
    REQUIRE(titleHeight > 0);
    CHECK(f.host.findChildren<QWidget*>("CollapsibleTitleBar").empty());
    for (int row = 0; row < f.right->count(); ++row) {
        auto* group = f.right->widget(row);
        auto* pages = group->findChild<QStackedWidget*>();
        REQUIRE(pages);
        auto* tool = pages->currentWidget();
        REQUIRE(tool);
        CAPTURE(tool->objectName());
        const auto* title = InspectorFixture::titleBar(tool);
        auto* toolbar = InspectorFixture::visibleToolbar(tool);
        CHECK(title->height() == titleHeight);
        REQUIRE(toolbar);
        CHECK(toolbar->isVisible());
        CHECK(toolbar->mapToGlobal(QPoint{}).y() >= title->mapToGlobal(QPoint{}).y() + title->height());
    }

    f.inspector->revealTool("search");
    auto* field = f.search()->findChild<QLineEdit*>();
    REQUIRE(field);
    REQUIRE(field->isVisible());
    auto* title = InspectorFixture::titleBar(f.search());
    const auto click = title->mapToGlobal(QPoint(title->width() - 8, title->height() / 2));
    InspectorFixture::press(*title, click);
    InspectorFixture::release(*title, click);
    CHECK(field->isVisible());
}

TEST_CASE_METHOD(InspectorSettingsScope, "Inspector tool and tab header sizes follow the stylesheet", "[inspector]") {
    InspectorFixture f;
    f.host.setStyleSheet(
        "QWidget#InspectorToolTitle { min-height: 36px; max-height: 36px; }"
        "QTabBar#InspectorToolTabs { min-height: 42px; max-height: 42px; }");
    f.showHost();
    REQUIRE(InspectorFixture::titleBar(f.layers())->objectName() == "InspectorToolTitle");
    CHECK(InspectorFixture::titleBar(f.layers())->height() == 36);

    f.inspector->tabifyTool("search", "layers");
    QCoreApplication::processEvents();
    CHECK(InspectorFixture::groupTabs(f.layers())->height() == 42);
}

TEST_CASE_METHOD(InspectorSettingsScope, "Dropping onto a title bar creates tabs without replacing either tool", "[inspector]") {
    InspectorFixture f;
    auto* search = f.search();
    auto* layers = f.layers();
    f.inspector->dockToolToSide("layers", false, 0);
    f.inspector->dockToolToSide("search", true, 0);
    f.showHost();

    auto* field = search->findChild<QLineEdit*>();
    REQUIRE(field);
    field->setText("persistent search");
    auto* searchBar = InspectorFixture::titleBar(search);
    auto* layersBar = InspectorFixture::titleBar(layers);
    REQUIRE(searchBar->isVisible());
    REQUIRE(layersBar->isVisible());
    f.inspector->revealTool("layers");
    QCoreApplication::processEvents();
    const auto target = InspectorFixture::visibleCenter(*layersBar);
    InspectorFixture::press(*searchBar, InspectorFixture::center(*searchBar));
    InspectorFixture::move(*searchBar, InspectorFixture::center(f.host));
    auto* floating = search->window();
    REQUIRE(floating != &f.host);
    CHECK(floating->isVisible());
    InspectorFixture::move(*floating, target);
    auto* preview = f.preview();
    REQUIRE(preview);
    CHECK(preview->isVisible());
    CHECK(preview->property("dropKind").toString() == "tab");
    InspectorFixture::release(*floating, target);

    REQUIRE(InspectorFixture::groupOf(search) == InspectorFixture::groupOf(layers));
    auto* tabs = InspectorFixture::groupTabs(search);
    CHECK(tabs->count() == 2);
    CHECK(tabs->isVisible());
    CHECK(InspectorFixture::tabIndex(*tabs, "Layers") >= 0);
    CHECK(InspectorFixture::tabIndex(*tabs, "Search") >= 0);
    CHECK(f.search() == search);
    CHECK(f.layers() == layers);
    CHECK(field->isVisible());
    CHECK(field->text() == "persistent search");
    CHECK(f.left->count() == 0);
    CHECK(f.inspector->leftPane()->isHidden());
    CHECK_FALSE(preview->isVisible());

    f.inspector->revealTool("search");
    CHECK(InspectorFixture::groupPages(search)->currentWidget() == search);
}

TEST_CASE_METHOD(InspectorSettingsScope, "A tool can join an existing tab group and leave it by dragging its tab", "[inspector]") {
    InspectorFixture f;
    auto* search = f.search();
    auto* layers = f.layers();
    auto* map = f.mapProperties();
    f.inspector->dockToolToSide("layers", false, 0);
    f.inspector->tabifyTool("search", "layers");
    f.inspector->dockToolToSide("map-properties", true, 0);
    f.showHost();

    auto* tabs = InspectorFixture::groupTabs(layers);
    REQUIRE(tabs->count() == 2);
    REQUIRE(tabs->isVisible());
    auto* mapBar = InspectorFixture::titleBar(map);
    REQUIRE(mapBar->isVisible());
    f.inspector->revealTool("layers");
    QCoreApplication::processEvents();
    InspectorFixture::drag(*mapBar, InspectorFixture::visibleCenter(*tabs));
    CHECK(tabs->count() == 3);
    CHECK(InspectorFixture::groupOf(map) == InspectorFixture::groupOf(layers));
    CHECK(InspectorFixture::groupOf(search) == InspectorFixture::groupOf(layers));

    // With the left side empty, dragging a tab to the left edge opens a stack there.
    REQUIRE(f.inspector->leftPane()->isHidden());
    const auto searchIndex = InspectorFixture::tabIndex(*tabs, "Search");
    REQUIRE(searchIndex >= 0);
    InspectorFixture::drag(*tabs, tabs->mapToGlobal(tabs->tabRect(searchIndex).center()),
        f.host.mapToGlobal(QPoint(8, f.host.height() / 2)));
    CHECK(f.left->count() == 1);
    CHECK(f.inspector->leftPane()->isAncestorOf(search));
    CHECK(tabs->count() == 2);
    CHECK(InspectorFixture::groupOf(map) == InspectorFixture::groupOf(layers));
    CHECK(InspectorFixture::groupTabs(search)->isHidden());

    // Moving the second tab away restores the remaining tool's ordinary title bar.
    const auto mapIndex = InspectorFixture::tabIndex(*tabs, "Map Properties");
    REQUIRE(mapIndex >= 0);
    InspectorFixture::drag(*tabs, tabs->mapToGlobal(tabs->tabRect(mapIndex).center()),
        f.host.mapToGlobal(QPoint(8, f.host.height() / 2)));
    CHECK(f.left->count() == 2);
    CHECK(f.inspector->leftPane()->isAncestorOf(map));
    CHECK(InspectorFixture::groupTabs(layers)->isHidden());
    CHECK(InspectorFixture::titleBar(layers)->isVisible());
}

TEST_CASE_METHOD(InspectorSettingsScope, "Drag feedback distinguishes stacking from tabs, and the hand is shown only while pressed", "[inspector]") {
    InspectorFixture f;
    f.inspector->dockToolToSide("layers", false, 0);
    f.inspector->dockToolToSide("search", true, 0);
    f.showHost();
    auto* search = f.search();
    auto* searchBar = InspectorFixture::titleBar(search);
    auto* layersBar = InspectorFixture::titleBar(f.layers());
    REQUIRE(searchBar->isVisible());
    REQUIRE(layersBar->isVisible());

    QEvent hover(QEvent::Enter);
    QCoreApplication::sendEvent(searchBar, &hover);
    CHECK_FALSE(InspectorFixture::hasHandCursor(*searchBar));
    const auto start = InspectorFixture::center(*searchBar);
    InspectorFixture::press(*searchBar, start);
    CHECK(InspectorFixture::hasHandCursor(*searchBar));

    f.inspector->revealTool("layers");
    QCoreApplication::processEvents();
    const auto tabTarget = InspectorFixture::visibleCenter(*layersBar);
    InspectorFixture::move(*searchBar, tabTarget);
    auto* preview = f.preview();
    REQUIRE(preview);
    CHECK(preview->isVisible());
    CHECK(preview->property("dropKind").toString() == "tab");
    InspectorFixture::move(*searchBar, f.host.mapToGlobal(QPoint(8, f.host.height() / 2)));
    CHECK(preview->isVisible());
    CHECK(preview->property("dropKind").toString() == "stack");

    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(searchBar, &escape);
    CHECK_FALSE(preview->isVisible());
    CHECK_FALSE(InspectorFixture::hasHandCursor(*searchBar));
    InspectorFixture::release(*searchBar, tabTarget);
    CHECK(f.inspector->leftPane()->isAncestorOf(search));
    CHECK(InspectorFixture::groupOf(search) != InspectorFixture::groupOf(f.layers()));
}

TEST_CASE_METHOD(InspectorSettingsScope, "Dropping below a free tool reorders the stack instead of creating tabs", "[inspector]") {
    InspectorFixture f;
    auto* search = f.search();
    auto* layers = f.layers();
    f.inspector->dockToolToSide("layers", true, 0);
    f.inspector->dockToolToSide("search", true, 1);
    f.showHost();
    QCoreApplication::processEvents();
    auto* searchGroup = InspectorFixture::groupOf(search);
    auto* layersGroup = InspectorFixture::groupOf(layers);
    REQUIRE(f.left->count() == 2);
    REQUIRE(f.left->widget(0) == layersGroup);
    REQUIRE(f.left->widget(1) == searchGroup);
    auto* handle = InspectorFixture::titleBar(layers);
    const auto target = searchGroup->mapToGlobal(QPoint(searchGroup->width() / 2, searchGroup->height() - 2));
    InspectorFixture::press(*handle, InspectorFixture::center(*handle));
    InspectorFixture::move(*handle, target);
    auto* preview = f.preview();
    REQUIRE(preview);
    CHECK(preview->isVisible());
    CHECK(preview->property("dropKind").toString() == "stack");
    InspectorFixture::release(*handle, target);
    CHECK(f.left->count() == 2);
    CHECK(f.left->widget(0) == InspectorFixture::groupOf(search));
    CHECK(f.left->widget(1) == InspectorFixture::groupOf(layers));
    CHECK(InspectorFixture::groupTabs(layers)->isHidden());
}

TEST_CASE_METHOD(InspectorSettingsScope, "A floating tool uses the window title bar and restores its panel title when closed", "[inspector]") {
    InspectorFixture f;
    auto* search = f.search();
    f.inspector->dockToolToSide("search", true, 0);
    f.showHost();
    auto* bar = InspectorFixture::titleBar(search);
    auto* field = search->findChild<QLineEdit*>();
    REQUIRE(field);
    field->setText("floating search");
    InspectorFixture::drag(*bar, InspectorFixture::center(f.host));

    QPointer<QWidget> floating = search->window();
    REQUIRE(floating);
    REQUIRE(floating != &f.host);
    CHECK(floating->objectName() == "InspectorFloating_search");
    CHECK(floating->windowTitle() == "Search");
    CHECK(floating->isVisible());
    CHECK(bar->isHidden());
    auto* toolbar = InspectorFixture::visibleToolbar(search);
    REQUIRE(toolbar);
    CHECK(toolbar->isVisible());
    CHECK(field->isVisible());
    CHECK(field->text() == "floating search");
    CHECK(f.inspector->leftPane()->isHidden());

    f.inspector->setPanelsVisible(false);
    CHECK_FALSE(floating->isVisible());
    f.inspector->setPanelsVisible(true);
    CHECK(floating->isVisible());

    floating->close();
    QCoreApplication::processEvents();
    CHECK(search->window() == &f.host);
    CHECK(f.inspector->leftPane()->isAncestorOf(search));
    CHECK(bar->isVisible());
}

TEST_CASE_METHOD(InspectorSettingsScope, "Inspector restores ordered stacks, tab groups, and floating windows", "[inspector]") {
    {
        InspectorFixture f;
        f.inspector->dockToolToSide("layers", true, 0);
        f.inspector->tabifyTool("search", "layers");
        f.inspector->revealTool("search");
        f.inspector->dockToolToSide("map-properties", true, 0);
        f.inspector->floatTool("mods", QPoint(75, 95));
        f.inspector->saveToolLayout();
    }

    InspectorFixture restored;
    auto* search = restored.search();
    auto* layers = restored.layers();
    auto* map = restored.mapProperties();
    auto* mods = restored.host.findChild<QWidget*>("MapInspector_ModsPanel");
    REQUIRE(mods);
    REQUIRE(restored.left->count() == 2);
    CHECK(restored.left->widget(0) == InspectorFixture::groupOf(map));
    CHECK(restored.left->widget(1) == InspectorFixture::groupOf(search));
    CHECK(InspectorFixture::groupOf(search) == InspectorFixture::groupOf(layers));
    CHECK(InspectorFixture::groupTabs(search)->count() == 2);
    CHECK(InspectorFixture::groupPages(search)->currentWidget() == search);
    CHECK(mods->window()->objectName() == "InspectorFloating_mods");
    CHECK(InspectorFixture::titleBar(mods)->isHidden());

    restored.inspector->resetToolLayout();
    CHECK(restored.left->count() == 0);
    CHECK(restored.right->count() == 10);
    CHECK(restored.inspector->leftPane()->isHidden());
    CHECK(InspectorFixture::groupOf(search) != InspectorFixture::groupOf(layers));
    CHECK(mods->window() == &restored.host);
}

TEST_CASE_METHOD(InspectorSettingsScope, "Dragging a tool floats it immediately and saves the release position", "[inspector]") {
    QPoint savedPosition;
    {
        InspectorFixture f;
        auto* search = f.search();
        f.inspector->dockToolToSide("search", true, 0);
        f.showHost();
        auto* title = InspectorFixture::titleBar(search);
        const auto first = InspectorFixture::center(f.host) - QPoint(32, 24);
        const auto final = first + QPoint(26, 18);
        InspectorFixture::press(*title, InspectorFixture::visibleCenter(*title));
        InspectorFixture::move(*title, first);
        QCoreApplication::processEvents();
        auto* floating = search->window();
        REQUIRE(floating != &f.host);
        REQUIRE(floating->objectName() == "InspectorFloating_search");
        CHECK(floating->isVisible());
        CHECK(title->isHidden());
        const auto firstPosition = floating->pos();

        InspectorFixture::move(*floating, final);
        QCoreApplication::processEvents();
        CHECK(search->window() == floating);
        CHECK(floating->pos() - firstPosition == final - first);
        savedPosition = floating->pos();
        InspectorFixture::release(*floating, final);
        REQUIRE(search->window() == floating);
        CHECK(floating->isVisible());
        CHECK(floating->pos() == savedPosition);
    }

    QSize savedSize;
    {
        InspectorFixture restored;
        restored.showHost();
        auto* floating = restored.search()->window();
        REQUIRE(floating != &restored.host);
        CHECK(floating->objectName() == "InspectorFloating_search");
        CHECK(floating->isVisible());
        const auto available = floating->screen()->availableGeometry();
        CAPTURE(floating->pos().x(), floating->pos().y(), floating->width(), floating->height(),
            savedPosition.x(), savedPosition.y(), available.x(), available.y(), available.width(), available.height());
        CHECK(floating->pos() == savedPosition);
        CHECK(restored.inspector->leftPane()->isHidden());

        floating->move(savedPosition + QPoint(28, 20));
        floating->resize(floating->size() + QSize(40, 24));
        QEventLoop settle;
        QTimer::singleShot(250, &settle, &QEventLoop::quit);
        settle.exec();
        savedPosition = floating->pos();
        savedSize = floating->size();
    }

    InspectorFixture restoredAgain;
    restoredAgain.showHost();
    const auto* floating = restoredAgain.search()->window();
    REQUIRE(floating != &restoredAgain.host);
    CHECK(floating->pos() == savedPosition);
    CHECK(floating->size() == savedSize);
}

TEST_CASE_METHOD(InspectorSettingsScope, "Escape restores the original tab order and selection after live floating", "[inspector]") {
    InspectorFixture f;
    auto* search = f.search();
    auto* layers = f.layers();
    f.inspector->dockToolToSide("layers", true, 0);
    f.inspector->tabifyTool("search", "layers");
    f.showHost();
    f.inspector->revealTool("layers");
    f.inspector->saveToolLayout();
    const auto initialLayout = QSettings{}.value("Inspector/DockLayout");
    auto* tabs = InspectorFixture::groupTabs(layers);
    const auto searchIndex = InspectorFixture::tabIndex(*tabs, "Search");
    REQUIRE(searchIndex >= 0);
    const auto start = tabs->mapToGlobal(tabs->tabRect(searchIndex).center());
    const auto target = InspectorFixture::center(f.host);
    InspectorFixture::press(*tabs, start);
    InspectorFixture::move(*tabs, target);
    QCoreApplication::processEvents();
    auto* floating = search->window();
    REQUIRE(floating != &f.host);
    CHECK(floating->isVisible());
    CHECK(QSettings{}.value("Inspector/DockLayout") == initialLayout);

    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(floating, &escape);
    QCoreApplication::processEvents();
    CHECK(search->window() == &f.host);
    CHECK(f.inspector->leftPane()->isAncestorOf(search));
    REQUIRE(InspectorFixture::groupOf(search) == InspectorFixture::groupOf(layers));
    tabs = InspectorFixture::groupTabs(search);
    CHECK(tabs->count() == 2);
    CHECK(InspectorFixture::tabIndex(*tabs, "Search") == searchIndex);
    CHECK(InspectorFixture::groupPages(search)->currentWidget() == layers);
    CHECK_FALSE(InspectorFixture::hasHandCursor(*tabs));
    InspectorFixture::release(*tabs, target);
    CHECK(search->window() == &f.host);
    CHECK(QSettings{}.value("Inspector/DockLayout") == initialLayout);
}

TEST_CASE_METHOD(InspectorSettingsScope, "The old inspector layout migrates to free stacks without recreating the general tabs", "[inspector]") {
    auto settings = QSettings{};
    settings.setValue("Inspector/LeftTabs", QVariantList{0});
    settings.setValue("Inspector/Tools", QVariantMap{
        {"placements", QVariantMap{
            {"map-properties", QVariantMap{{"host", "left"}, {"index", 0}}},
            {"search", QVariantMap{{"host", "right"}, {"index", 0}}},
        }},
    });

    InspectorFixture f;
    CHECK(f.inspector->leftPane()->isAncestorOf(f.layers()));
    CHECK(f.inspector->leftPane()->isAncestorOf(f.mapProperties()));
    CHECK(f.inspector->isAncestorOf(f.search()));
    CHECK(f.left->count() == 2);
    CHECK(f.right->count() == 8);
    CHECK(f.host.findChildren<QTabWidget*>().empty());
    CHECK(InspectorFixture::groupTabs(f.layers())->isHidden());
}

TEST_CASE_METHOD(InspectorSettingsScope, "A new editor initializes its own shaders after closing an editor", "[inspector-native][.native-gl]") {
    for (int frame = 0; frame < 2; ++frame) {
        CAPTURE(frame);
        InspectorFixture f(true);
        REQUIRE_FALSE(f.graphics.initialized());
        f.inspector->dockToolToSide("texture-browser", true, 0);
        f.showHost();

        auto* browser = f.host.findChild<SwitchableTitledPanel*>("FaceInspector_TextureBrowserPanel");
        REQUIRE(browser);
        auto* view = browser->findChild<RenderView*>();
        REQUIRE(view);
        REQUIRE(view->isValid());
        REQUIRE(f.graphics.initialized());
        CHECK_FALSE(view->grabFramebuffer().isNull());
    }
}

TEST_CASE_METHOD(InspectorSettingsScope, "Floating an OpenGL browser keeps its render context without an internal title bar", "[inspector-native][.native-gl]") {
    InspectorFixture f(true);
    f.inspector->dockToolToSide("texture-browser", true, 0);
    f.showHost();
    f.inspector->revealTool("texture-browser");
    QCoreApplication::processEvents();
    auto* browser = f.host.findChild<SwitchableTitledPanel*>("FaceInspector_TextureBrowserPanel");
    REQUIRE(browser);
    auto* view = browser->findChild<RenderView*>();
    REQUIRE(view);
    REQUIRE(view->isValid());
    QPointer<QOpenGLContext> originalContext = view->context();
    REQUIRE(originalContext);

    f.inspector->floatTool("texture-browser", f.host.pos() + QPoint(90, 90));
    QCoreApplication::processEvents();
    REQUIRE(browser->window()->objectName() == "InspectorFloating_texture-browser");
    CHECK(InspectorFixture::titleBar(browser)->isHidden());
    REQUIRE(view->isValid());
    CHECK(view->context() == originalContext);
    CHECK_FALSE(view->grabFramebuffer().isNull());

    const auto switchMode = [browser] {
        auto* toolbar = InspectorFixture::visibleToolbar(browser);
        REQUIRE(toolbar);
        for (auto* action : toolbar->actions()) {
            if (action->objectName() == "InspectorTextureModeAction") {
                auto* button = qobject_cast<QToolButton*>(toolbar->widgetForAction(action));
                REQUIRE(button);
                REQUIRE(button->isVisible());
                button->click();
                return;
            }
        }
        FAIL("The active texture toolbar is missing its mode action");
    };
    const auto originalIndex = browser->currentIndex();
    switchMode();
    CHECK(browser->currentIndex() != originalIndex);
    switchMode();
    CHECK(browser->currentIndex() == originalIndex);

    browser->window()->close();
    QCoreApplication::processEvents();
    CHECK(view->context() == originalContext);
    CHECK(f.inspector->leftPane()->isAncestorOf(browser));
    CHECK(InspectorFixture::titleBar(browser)->isVisible());
}
} // namespace TrenchBroom::View
