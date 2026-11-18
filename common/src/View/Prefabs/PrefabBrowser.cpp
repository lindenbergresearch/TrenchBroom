#include "View/Prefabs/PrefabBrowser.h"

#include "IO/ResourceUtils.h"
#include "Model/Game.h"
#include "View/Document/MapDocument.h"
#include "View/Widgets/ElidedLabel.h"
#include "View/Prefabs/PrefabUtils.h"
#include "View/Prefabs/PrefabPreview.h"
#include "View/Theme/QtUtils.h"

#include <QAction>
#include <QActionGroup>
#include <QEvent>
#include <QDrag>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMenu>
#include <QMimeData>
#include <QSettings>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QSpinBox>
#include <QSlider>
#include <QStackedWidget>
#include <QShortcut>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <algorithm>
#include <map>

namespace TrenchBroom::View {
namespace {
constexpr int PathRole = Qt::UserRole;
constexpr int FolderRole = Qt::UserRole + 1;

QSize cardItemSize(QWidget* view, const QSize& contentSize) {
    QStyleOptionViewItem option;
    option.initFrom(view); option.widget = view;
    option.showDecorationSelected = true; // Matches QStyledItemDelegate::updateEditorGeometry.
    // Qt places index widgets inside the item's stylesheet border/padding.
    // Reserve that space in addition to the card's own layout dimensions.
    option.rect = QRect(QPoint{},contentSize);
    const auto content = view->style()->subElementRect(QStyle::SE_ItemViewItemText, &option, view);
    return contentSize + QSize(std::max(0,contentSize.width()-content.width()), std::max(0,contentSize.height()-content.height()));
}

void startPrefabDrag(QWidget* source, const QString& path, bool linked, QWidget* card) {
    if (path.isEmpty()) return;
    auto* mime = new QMimeData;
    mime->setData(PrefabMimeType, QJsonDocument(QJsonObject{{"path", path}, {"linked", linked}}).toJson());
    QDrag drag(source);
    drag.setMimeData(mime);
    if (card) drag.setPixmap(card->grab());
    drag.exec(Qt::CopyAction, Qt::CopyAction);
}

QString parentFolder(const QString& path) {
    const auto separator = path.lastIndexOf('/');
    return separator < 0 ? QString{} : path.left(separator);
}

class PrefabTree : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;
    bool linked = false;

protected:
    void startDrag(Qt::DropActions) override {
        auto* item = currentItem();
        if (item) startPrefabDrag(this, item->data(0, PathRole).toString(), linked, itemWidget(item, 0));
    }
};

class PrefabTiles : public QListWidget {
public:
    using QListWidget::QListWidget;
    bool linked = false;

protected:
    void startDrag(Qt::DropActions) override {
        auto* item = currentItem();
        if (item) startPrefabDrag(this, item->data(PathRole).toString(), linked, itemWidget(item));
    }
};

class PrefabCard : public QWidget {
    QLabel* m_image;
    QPixmap m_preview;
    QIcon m_icon;
    int m_brightness = -1;
    QSize m_pixels;
    qreal m_ratio = 0;
    bool m_tile;

public:
    PrefabCard(const IO::PrefabInfo& info, const QString& file, QWidget* parent, bool tile = false, bool folder = false)
        : QWidget(parent), m_tile(tile) {
        setObjectName("PrefabCard");
        setProperty("layout", tile ? "tiles" : "tree");
        setProperty("folder", folder);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        m_image = new QLabel;
        m_image->setObjectName("PrefabThumbnail");
        m_image->setAlignment(Qt::AlignCenter);
        if (folder) m_icon = IO::loadSVGIcon("Folder.svg", 16);
        else m_preview.load(file);
        if (m_preview.isNull() && m_icon.isNull()) m_icon = IO::loadSVGIcon("Prefab.svg", 16);
        auto* title = new ElidedLabel(info.label, Qt::ElideMiddle);
        title->setObjectName("PrefabName");
        title->findChild<QLabel*>()->setTextFormat(Qt::PlainText);
        auto* dimensions = new ElidedLabel(folder ? QStringLiteral(" ") : QString("%1 × %2 × %3").arg(info.size.x(), 0, 'g', 6).arg(info.size.y(), 0, 'g', 6).arg(info.size.z(), 0, 'g', 6), Qt::ElideRight);
        dimensions->setObjectName("PrefabDimensions");
        auto* text = new QVBoxLayout;
        text->setContentsMargins(0, 0, 0, 0);
        text->addWidget(title); text->addWidget(dimensions);
        if (!tile) text->addStretch();
        auto* layout = new QBoxLayout(tile ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight, this);
        layout->setContentsMargins(LayoutConstants::MediumHMargin, LayoutConstants::NarrowVMargin,
            LayoutConstants::MediumHMargin, LayoutConstants::NarrowVMargin);
        layout->addWidget(m_image, 0, tile ? Qt::AlignHCenter : Qt::Alignment{});
        layout->addLayout(text, 1);
        if (tile) {
            title->findChild<QLabel*>()->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            dimensions->findChild<QLabel*>()->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        }
    }

    void setAppearance(int percent, int brightness) {
        const auto size = (m_tile ? QSize(128, 96) : QSize(96, 72)) * (percent / 100.0);
        m_image->setFixedSize(size);
        const auto ratio = devicePixelRatioF();
        const auto pixels = size * ratio;
        if (m_brightness != brightness || m_pixels != pixels || m_ratio != ratio) {
            auto pixmap = m_icon.isNull() ? m_preview.scaled(pixels, Qt::KeepAspectRatio, Qt::SmoothTransformation) : m_icon.pixmap(pixels);
            // Adjust only the displayed resolution, retaining the original for
            // subsequent zooms. Resizing the pane reuses unchanged pixmaps.
            if (m_icon.isNull() && brightness != 100) pixmap = QPixmap::fromImage(prefabPreviewBrightness(pixmap.toImage(), brightness/100.0));
            pixmap.setDevicePixelRatio(ratio); m_image->setPixmap(pixmap);
            m_brightness = brightness;
            m_pixels = pixels; m_ratio = ratio;
        }
        if (m_tile) setFixedWidth(std::max(96, size.width()) + layout()->contentsMargins().left() + layout()->contentsMargins().right());
        // Item views cache both row sizes and index-widget geometries. Resolve
        // the card layout before publishing its new hint to the view.
        layout()->invalidate(); layout()->activate();
        updateGeometry();
    }

    QSize sizeHint() const override {
        auto size = QWidget::sizeHint();
        if (m_tile) size.setWidth(width());
        return size;
    }
};
}

PrefabBrowser::PrefabBrowser(std::weak_ptr<MapDocument> document, MapView* mapView, QWidget* parent, QString libraryRoot)
    : QWidget(parent), m_document(std::move(document)), m_mapView(mapView), m_library(std::move(libraryRoot)) {
    setObjectName("PrefabBrowser");
    m_cardRefresh = new QTimer(this);
    m_cardRefresh->setSingleShot(true);
    connect(m_cardRefresh, &QTimer::timeout, this, &PrefabBrowser::updateCards);
    auto* toolbar = createInspectorToolBar(this);
    m_add = toolbar->addAction(IO::loadSVGIcon("Add.svg", 16), tr("Add Selection as Prefab"), this, &PrefabBrowser::addPrefab);
    m_add->setObjectName("PrefabAdd");
    m_remove = toolbar->addAction(IO::loadSVGIcon("Remove.svg", 16), tr("Remove Prefab"), this, &PrefabBrowser::removePrefab);
    m_remove->setObjectName("PrefabRemove");
    m_insert = toolbar->addAction(IO::loadSVGIcon("Prefab.svg", 16), tr("Insert Prefab"), this, &PrefabBrowser::insertSelected);
    m_insert->setObjectName("PrefabInsert");
    toolbar->addSeparator();
    auto* viewActions = new QActionGroup(this);
    m_treeView = toolbar->addAction(IO::loadSVGIcon("PrefabViewTree.svg", 16), tr("Tree view"));
    m_treeView->setObjectName("PrefabTreeView"); m_treeView->setCheckable(true); viewActions->addAction(m_treeView);
    m_tileView = toolbar->addAction(IO::loadSVGIcon("PrefabViewTiles.svg", 16), tr("Folder and tile view"));
    m_tileView->setObjectName("PrefabTileView"); m_tileView->setCheckable(true); viewActions->addAction(m_tileView);
    toolbar->addSeparator();
    m_linked = toolbar->addAction(IO::loadSVGIcon("LinkedGroups.svg", 16), tr("Use linked groups on insert"));
    m_linked->setObjectName("PrefabLinkedInsert"); m_linked->setCheckable(true);
    m_linked->setChecked(QSettings{}.value("PrefabBrowser/Linked", false).toBool());
    m_linked->setToolTip(tr("Use linked groups on insert"));
    auto* brightnessButton = new QToolButton;
    brightnessButton->setObjectName("PrefabBrightnessButton");
    brightnessButton->setIcon(IO::loadSVGIcon("light_mode.svg", 16));
    brightnessButton->setToolTip(tr("Preview brightness"));
    brightnessButton->setPopupMode(QToolButton::InstantPopup);
    auto* brightnessMenu = new QMenu(brightnessButton);
    auto* brightnessAction = new QWidgetAction(brightnessMenu);
    auto* brightnessPanel = new QWidget;
    brightnessPanel->setObjectName("PrefabBrightnessPanel");
    auto* brightnessLayout = new QHBoxLayout(brightnessPanel);
    auto* brightnessLabel = new QLabel(tr("Brightness"));
    brightnessLabel->setObjectName("PrefabBrightnessLabel");
    auto* brightnessSlider = new QSlider(Qt::Horizontal);
    brightnessSlider->setObjectName("PrefabBrightnessSlider");
    brightnessSlider->setRange(50,150); brightnessSlider->setMinimumWidth(100);
    brightnessSlider->setAccessibleName(tr("Preview brightness"));
    m_brightness = new QSpinBox;
    m_brightness->setObjectName("PrefabBrightness");
    m_brightness->setRange(50,150); m_brightness->setSingleStep(5); m_brightness->setSuffix(" %");
    m_brightness->setValue(QSettings{}.value("PrefabBrowser/Brightness", 100).toInt());
    brightnessSlider->setValue(m_brightness->value());
    connect(brightnessSlider, &QSlider::valueChanged, m_brightness, &QSpinBox::setValue);
    connect(m_brightness, qOverload<int>(&QSpinBox::valueChanged), brightnessSlider, &QSlider::setValue);
    brightnessLayout->addWidget(brightnessLabel); brightnessLayout->addWidget(brightnessSlider,1); brightnessLayout->addWidget(m_brightness);
    brightnessAction->setDefaultWidget(brightnessPanel); brightnessMenu->addAction(brightnessAction);
    brightnessButton->setMenu(brightnessMenu); toolbar->addWidget(brightnessButton);
    auto* zoomOut = toolbar->addAction(QStringLiteral("−"));
    zoomOut->setToolTip(tr("Smaller previews"));
    m_zoom = new QSpinBox;
    m_zoom->setObjectName("PrefabZoom");
    m_zoom->setRange(50, 300); m_zoom->setSingleStep(25); m_zoom->setSuffix(" %");
    m_zoom->setValue(QSettings{}.value("PrefabBrowser/Zoom", 100).toInt());
    m_zoom->setKeyboardTracking(false);
    toolbar->addWidget(m_zoom);
    auto* zoomIn = toolbar->addAction(QStringLiteral("+"));
    zoomIn->setToolTip(tr("Larger previews"));
    auto* refresh = toolbar->addAction(IO::loadSVGIcon("Refresh.svg", 16), tr("Refresh Prefabs"), this, &PrefabBrowser::reload);
    refresh->setObjectName("PrefabRefresh");
    m_filter = new QLineEdit;
    m_filter->setObjectName("PrefabFilter");
    m_filter->setPlaceholderText(tr("Search prefabs"));
    m_filter->setClearButtonEnabled(true);
    toolbar->addWidget(m_filter);

    auto* tree = new PrefabTree(this);
    m_tree = tree;
    tree->linked = m_linked->isChecked();
    tree->setObjectName("PrefabTree");
    tree->setHeaderHidden(true);
    tree->setColumnCount(1);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->setDragEnabled(true);
    tree->setDragDropMode(QAbstractItemView::DragOnly);
    tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tree->header()->setStretchLastSection(true);
    auto* tiles = new PrefabTiles(this);
    m_tiles = tiles;
    tiles->linked = tree->linked;
    tiles->setObjectName("PrefabTiles");
    tiles->setViewMode(QListView::IconMode);
    tiles->setFlow(QListView::LeftToRight);
    tiles->setWrapping(true);
    tiles->setResizeMode(QListView::Adjust);
    tiles->setMovement(QListView::Static);
    tiles->setUniformItemSizes(true);
    tiles->setSelectionMode(QAbstractItemView::SingleSelection);
    tiles->setDragEnabled(true); tiles->setDragDropMode(QAbstractItemView::DragOnly);
    tiles->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_views = new QStackedWidget;
    m_views->addWidget(tree); m_views->addWidget(tiles);
    m_navigation = new QWidget;
    m_navigation->setObjectName("PrefabNavigation");
    auto* navigation = new QHBoxLayout(m_navigation);
    navigation->setContentsMargins(LayoutConstants::MediumHMargin, LayoutConstants::NarrowVMargin,
        LayoutConstants::MediumHMargin, LayoutConstants::NarrowVMargin);
    auto* up = createBitmapButton("Up.svg", tr("Enclosing folder"));
    up->setObjectName("PrefabUp");
    m_up = new QAction(tr("Enclosing folder"), this);
    connect(up, &QAbstractButton::clicked, m_up, &QAction::trigger);
    connect(m_up, &QAction::changed, up, [this, up] { up->setEnabled(m_up->isEnabled()); });
    connect(m_up, &QAction::triggered, this, [this] { setFolder(parentFolder(m_folder)); });
    m_location = new QLabel;
    m_location->setObjectName("PrefabLocation");
    m_location->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    m_location->setWordWrap(true);
    connect(m_location, &QLabel::linkActivated, this, [this](const QString& link) {
        setFolder(QUrl::fromPercentEncoding(link.mid(1).toUtf8()));
    });
    navigation->addWidget(up); navigation->addWidget(m_location, 1);
    auto* goUp = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Up), tiles);
    connect(goUp, &QShortcut::activated, m_up, &QAction::trigger);
    m_status = new QLabel;
    m_status->setObjectName("PrefabStatus");
    m_status->setWordWrap(true);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
    layout->addWidget(toolbar); layout->addWidget(m_navigation); layout->addWidget(m_views, 1); layout->addWidget(m_status);

    m_watcher = new QFileSystemWatcher(this);
    m_reload = new QTimer(this);
    m_reload->setSingleShot(true); m_reload->setInterval(150);
    connect(m_reload, &QTimer::timeout, this, &PrefabBrowser::reload);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_reload->start(); });
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &PrefabBrowser::updateSelection);
    connect(m_tiles, &QListWidget::itemSelectionChanged, this, &PrefabBrowser::updateSelection);
    connect(m_tiles, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        const auto folder = item->data(FolderRole).toString();
        if (!folder.isEmpty()) setFolder(folder);
        else insertSelected();
    });
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](auto* item, int) {
        if (!item->data(0, PathRole).toString().isEmpty()) insertSelected();
    });
    connect(m_linked, &QAction::toggled, this, [tree, tiles](bool linked) {
        tree->linked = tiles->linked = linked; QSettings{}.setValue("PrefabBrowser/Linked", linked);
    });
    connect(m_treeView, &QAction::triggered, this, [this] { setTileView(false); });
    connect(m_tileView, &QAction::triggered, this, [this] { setTileView(true); });
    connect(m_zoom, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        QSettings{}.setValue("PrefabBrowser/Zoom", value); updateCards();
    });
    connect(m_brightness, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        QSettings{}.setValue("PrefabBrowser/Brightness", value); m_cardRefresh->start();
    });
    connect(zoomOut, &QAction::triggered, this, [this] { m_zoom->stepDown(); });
    connect(zoomIn, &QAction::triggered, this, [this] { m_zoom->stepUp(); });
    connect(m_filter, &QLineEdit::textChanged, this, &PrefabBrowser::applyFilter);
    if (const auto doc = m_document.lock()) {
        m_connections += doc->selectionDidChangeNotifier.connect([this](const auto&) { updateButtons(); });
        m_connections += doc->documentWasLoadedNotifier.connect([this](auto*) { updateButtons(); });
        m_connections += doc->documentWasNewedNotifier.connect([this](auto*) { updateButtons(); });
        m_connections += doc->documentWasClearedNotifier.connect([this](auto*) { updateButtons(); });
    }
    m_folder = QSettings{}.value("PrefabBrowser/Folder").toString();
    reload();
    setTileView(QSettings{}.value("PrefabBrowser/View", "tree").toString() == "tiles");
}

QString PrefabBrowser::selectedPath() const {
    if (m_usingTiles) {
        const auto* item = m_tiles->currentItem();
        return item && item->isSelected() && !item->isHidden() ? item->data(PathRole).toString() : QString{};
    }
    return m_tree->currentItem() && m_tree->currentItem()->isSelected() && !m_tree->currentItem()->isHidden()
        ? m_tree->currentItem()->data(0, PathRole).toString() : QString{};
}

void PrefabBrowser::reload() {
    const auto selected = selectedPath();
    QStringList expanded;
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) if ((*it)->isExpanded()) expanded << (*it)->data(0, Qt::UserRole + 1).toString();
    const auto scroll = m_tree->verticalScrollBar()->value();
    const QSignalBlocker blocker(m_tree);
    m_tree->clear();
    QStringList errors;
    try { m_library.migrateLegacyImports(); }
    catch (const std::exception& e) { errors << QString::fromUtf8(e.what()); }
    m_entries = m_library.entries(&errors);
    while (!m_folder.isEmpty() && std::none_of(m_entries.begin(), m_entries.end(), [this](const auto& info) { return info.path.startsWith(m_folder + '/'); }))
        m_folder = parentFolder(m_folder);
    std::map<QString, QTreeWidgetItem*> folders;
    QStringList watches;
    if (QFileInfo::exists(m_library.root())) watches << m_library.root();
    else watches << QFileInfo(m_library.root()).absolutePath();
    for (const auto& info : m_entries) {
        const auto parts = info.path.split('/');
        QTreeWidgetItem* parent = nullptr;
        QString path;
        for (auto i = 0; i + 1 < parts.size(); ++i) {
            path += (path.isEmpty() ? "" : "/") + parts[i];
            if (!folders.count(path)) {
                auto* folder = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
                folder->setText(0, parts[i]);
                folder->setIcon(0, IO::loadSVGIcon("Group.svg", 16));
                folder->setData(0, Qt::UserRole + 1, path);
                folder->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                folder->setExpanded(expanded.empty() || expanded.contains(path));
                folders[path] = folder;
                watches << m_library.root() + '/' + path;
            }
            parent = folders[path];
        }
        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
        item->setData(0, PathRole, info.path);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        item->setData(0, Qt::AccessibleTextRole, info.label);
        item->setToolTip(0, info.path + "\n" + info.game + " · " + info.mapFormat + "\n" + info.description
            + (info.missingAssets.empty() ? "" : tr("\nUnavailable resources:\n%1").arg(info.missingAssets.join('\n'))));
        auto* card = new PrefabCard(info, m_library.directory(info.path) + "/preview.png", m_tree);
        m_tree->setItemWidget(item, 0, card);
        if (info.path == selected) m_tree->setCurrentItem(item);
        watches << m_library.directory(info.path);
    }
    watches.removeDuplicates();
    if (!m_watcher->directories().empty()) m_watcher->removePaths(m_watcher->directories());
    if (!watches.empty()) m_watcher->addPaths(watches);
    m_status->setText(!errors.empty() ? tr("%1 invalid prefab(s). See details.").arg(errors.size())
        : m_entries.empty() ? tr("Select objects in the map and use Add to create a prefab.") : tr("%1 prefab(s)").arg(m_entries.size()));
    m_status->setToolTip(errors.join('\n'));
    rebuildTiles(selected);
    updateCards(); applyFilter(); updateButtons(); updateSelection();
    m_tree->verticalScrollBar()->setValue(scroll);
}

void PrefabBrowser::rebuildTiles(const QString& selection) {
    const QSignalBlocker blocker(m_tiles);
    m_tiles->clear();
    const auto prefix = m_folder.isEmpty() ? QString{} : m_folder + '/';
    std::map<QString, QString> folders;
    std::vector<const IO::PrefabInfo*> prefabs;
    for (const auto& info : m_entries) {
        if (!info.path.startsWith(prefix)) continue;
        const auto remainder = info.path.mid(prefix.size());
        const auto separator = remainder.indexOf('/');
        if (separator < 0) prefabs.push_back(&info);
        else folders.emplace(remainder.left(separator), prefix + remainder.left(separator));
    }
    for (const auto& [label, folder] : folders) {
        auto* item = new QListWidgetItem(m_tiles);
        item->setData(FolderRole, folder);
        item->setData(Qt::AccessibleTextRole, label);
        item->setToolTip(folder);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        IO::PrefabInfo info; info.label = label;
        m_tiles->setItemWidget(item, new PrefabCard(info, {}, m_tiles, true, true));
    }
    for (const auto* info : prefabs) {
        auto* item = new QListWidgetItem(m_tiles);
        item->setData(PathRole, info->path);
        item->setData(Qt::AccessibleTextRole, info->label);
        item->setToolTip(info->path + '\n' + info->description);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        m_tiles->setItemWidget(item, new PrefabCard(*info, m_library.directory(info->path) + "/preview.png", m_tiles, true));
        if (info->path == selection) m_tiles->setCurrentItem(item);
    }
    updateCards(); applyFilter(); updateNavigation();
}

void PrefabBrowser::setFolder(const QString& folder) {
    if (!folder.isEmpty() && std::none_of(m_entries.begin(), m_entries.end(), [&](const auto& info) { return info.path.startsWith(folder + '/'); })) return;
    m_folder = folder;
    QSettings{}.setValue("PrefabBrowser/Folder", folder);
    rebuildTiles();
    m_tiles->verticalScrollBar()->setValue(0);
    updateSelection();
}

void PrefabBrowser::setTileView(bool tiles) {
    const auto selection = selectedPath();
    const auto folderSelection = m_usingTiles && m_tiles->currentItem() && m_tiles->currentItem()->isSelected()
        ? m_tiles->currentItem()->data(FolderRole).toString() : QString{};
    if (tiles && !m_usingTiles) {
        if (!selection.isEmpty()) m_folder = parentFolder(selection);
        else if (const auto* item = m_tree->currentItem(); item && item->isSelected()) m_folder = item->data(0, FolderRole).toString();
    }
    m_usingTiles = tiles;
    m_views->setCurrentWidget(tiles ? static_cast<QWidget*>(m_tiles) : m_tree);
    m_navigation->setVisible(tiles);
    m_treeView->setChecked(!tiles); m_tileView->setChecked(tiles);
    m_filter->setPlaceholderText(tiles ? tr("Search this folder") : tr("Search prefabs"));
    QSettings{}.setValue("PrefabBrowser/View", tiles ? "tiles" : "tree");
    QSettings{}.setValue("PrefabBrowser/Folder", m_folder);
    if (tiles) rebuildTiles(selection);
    else {
        const QSignalBlocker blocker(m_tree);
        m_tree->clearSelection(); m_tree->setCurrentItem(nullptr);
        const auto folder = folderSelection.isEmpty() ? m_folder : folderSelection;
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
            if ((!selection.isEmpty() && (*it)->data(0, PathRole).toString() == selection)
                || (selection.isEmpty() && !folder.isEmpty() && (*it)->data(0, FolderRole).toString() == folder)) {
                m_tree->setCurrentItem(*it);
                for (auto* parent = (*it)->parent(); parent; parent = parent->parent()) parent->setExpanded(true);
                m_tree->scrollToItem(*it);
                break;
            }
        }
    }
    applyFilter(); updateSelection();
}

void PrefabBrowser::updateNavigation() {
    m_up->setEnabled(!m_folder.isEmpty());
    QString breadcrumb = "<a href=\"#\">" + tr("Prefabs").toHtmlEscaped() + "</a>";
    QString path;
    for (const auto& component : m_folder.split('/', Qt::SkipEmptyParts)) {
        path += (path.isEmpty() ? "" : "/") + component;
        breadcrumb += " / <a href=\"#" + QString::fromLatin1(QUrl::toPercentEncoding(path)) + "\">" + component.toHtmlEscaped() + "</a>";
    }
    m_location->setText(breadcrumb);
    m_location->setToolTip(m_folder.isEmpty() ? tr("Prefab library") : m_folder);
}

void PrefabBrowser::updateCards() {
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
        if (auto* card = dynamic_cast<PrefabCard*>(m_tree->itemWidget(*it, 0))) {
            card->setAppearance(m_zoom->value(), m_brightness->value());
            (*it)->setSizeHint(0, cardItemSize(m_tree,card->sizeHint()));
        }
    }
    for (int i = 0; i < m_tiles->count(); ++i) {
        auto* item = m_tiles->item(i);
        if (auto* card = dynamic_cast<PrefabCard*>(m_tiles->itemWidget(item))) {
            card->setAppearance(m_zoom->value(), m_brightness->value()); item->setSizeHint(cardItemSize(m_tiles,card->sizeHint()));
        }
    }
    m_tree->doItemsLayout(); m_tiles->doItemsLayout();
    m_tree->viewport()->update(); m_tiles->viewport()->update();
}

void PrefabBrowser::updateSelection() {
    const auto update = [](QWidget* card, bool selected) {
        if (card && card->property("selected").toBool() != selected) {
            card->setProperty("selected", selected);
            for (auto* label : card->findChildren<QWidget*>()) { label->style()->unpolish(label); label->style()->polish(label); }
            card->update();
        }
    };
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) if (auto* card = m_tree->itemWidget(*it, 0)) {
        update(card, (*it)->isSelected());
    }
    for (int i = 0; i < m_tiles->count(); ++i) update(m_tiles->itemWidget(m_tiles->item(i)), m_tiles->item(i)->isSelected());
    updateButtons();
}

void PrefabBrowser::updateButtons() {
    const auto doc = m_document.lock();
    const auto loaded = doc && doc->world();
    m_add->setEnabled(loaded && (doc->hasSelectedNodes() || doc->hasSelectedBrushFaces()));
    const auto path = selectedPath();
    const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto& entry) { return entry.path == path; });
    m_remove->setEnabled(it != m_entries.end());
    m_insert->setEnabled(loaded && it != m_entries.end() && it->game == QString::fromStdString(doc->game()->gameName()));
}

void PrefabBrowser::applyFilter() {
    const auto text = m_filter->text().trimmed();
    const auto filter = [&](auto&& self, QTreeWidgetItem* item) -> bool {
        auto visible = false;
        const auto path = item->data(0, PathRole).toString();
        if (!path.isEmpty()) visible = path.contains(text, Qt::CaseInsensitive) || item->toolTip(0).contains(text, Qt::CaseInsensitive);
        for (int i = 0; i < item->childCount(); ++i) visible = self(self, item->child(i)) || visible;
        item->setHidden(!visible);
        if (!text.isEmpty() && visible && item->childCount()) item->setExpanded(true);
        return visible;
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) filter(filter, m_tree->topLevelItem(i));
    for (int i = 0; i < m_tiles->count(); ++i) {
        auto* item = m_tiles->item(i);
        item->setHidden(!item->data(Qt::AccessibleTextRole).toString().contains(text, Qt::CaseInsensitive));
    }
    if (m_status->toolTip().isEmpty() && !m_entries.empty()) {
        int prefabs = 0, folders = 0;
        for (int i = 0; i < m_tiles->count(); ++i) if (!m_tiles->item(i)->isHidden()) {
            if (m_tiles->item(i)->data(FolderRole).toString().isEmpty()) ++prefabs; else ++folders;
        }
        m_status->setText(m_usingTiles ? tr("%1 prefab(s) · %2 folder(s)").arg(prefabs).arg(folders) : tr("%1 prefab(s)").arg(m_entries.size()));
    }
    updateButtons();
}

void PrefabBrowser::addPrefab() {
    if (const auto doc = m_document.lock(); doc && doc->world()) {
        QString folder = m_usingTiles ? m_folder : QString{};
        if (const auto* item = m_tree->currentItem(); item && !m_usingTiles) {
            folder = item->data(0, Qt::UserRole + 1).toString();
            if (folder.isEmpty()) { const auto path = selectedPath(); const auto separator = path.lastIndexOf('/'); if (separator >= 0) folder = path.left(separator); }
        }
        if (!folder.isEmpty()) folder += '/';
        if (savePrefabDialog(doc, prefabSelection(*doc), this, folder, m_library)) reload();
    }
}

void PrefabBrowser::removePrefab() {
    const auto path = selectedPath();
    if (path.isEmpty()) return;
    if (QMessageBox::question(this, tr("Remove Prefab"), tr("Remove \"%1\" from the prefab library? Objects already inserted in maps remain available.").arg(path)) != QMessageBox::Yes) return;
    try { m_library.remove(path); reload(); }
    catch (const std::exception& e) { QMessageBox::warning(this, tr("Cannot Remove Prefab"), QString::fromUtf8(e.what())); }
}

void PrefabBrowser::insertSelected() {
    if (m_insert->isEnabled()) insertPrefab(m_document.lock(), m_mapView, selectedPath(), m_linked->isChecked(), this, m_library);
}

void PrefabBrowser::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (m_cardRefresh && (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange || event->type() == QEvent::ApplicationFontChange))
        m_cardRefresh->start();
}

void PrefabBrowser::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_cardRefresh) m_cardRefresh->start();
}

bool PrefabBrowser::event(QEvent* event) {
    if (m_cardRefresh && event->type() == QEvent::ScreenChangeInternal) m_cardRefresh->start();
    return QWidget::event(event);
}
}
