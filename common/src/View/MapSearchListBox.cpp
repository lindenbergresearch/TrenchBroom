#include "MapSearchListBox.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QTreeWidget>

#include "Model/EntityNode.h"
#include "Model/EntityNodeIndex.h"
#include "Model/LayerNode.h"
#include "Model/WorldNode.h"
#include "View/MapDocument.h"
#include "View/QtUtils.h"
#include "ViewConstants.h"
#include "StringUtils.h"
#include "TrenchBroomApp.h"

#include <kdl/memory_utils.h>

#include <vm/vec_io.h>

namespace TrenchBroom::View {
/* ---------------------------------------------------------------------------- */

SearchListBoxItemRenderer::SearchListBoxItemRenderer(std::weak_ptr<MapDocument> document, MapView *mapView, Model::EntityNodeBase *node, QWidget *parent)
    : ControlListBoxItemRenderer(parent), m_document(std::move(document)), m_node(node), m_mapView(mapView) {
    installEventFilter(this);

    m_nameText = new QLabel{};
    m_infoText = new QLabel{};
    m_infoText->setObjectName("SearchListBoxItemRenderer_m_infoText");
    const auto panel = new QWidget{this};

    makeMono(m_nameText);

    if (m_node != nullptr) {
        const auto num_properties = m_node->entity().properties().size();

        std::stringstream origin;
        origin << "@(" << m_node->physicalBounds().center() << ") " << num_properties
               << ((num_properties != 1) ? " attributes" : " attribute");
        const auto str_origin = QString::fromStdString(origin.str());

        m_nameText->setText(QString::fromStdString(m_node->entity().classname()));
        m_infoText->setText(str_origin);
        setToolTip(m_nameText->text() + "\n" + m_infoText->text());
    } else {
        m_nameText->setText("Unknown");
        m_infoText->setText("");
    }

    m_nameText->installEventFilter(this);
    m_infoText->installEventFilter(this);
    makeSmall(m_infoText);
    makeEmphasized(m_nameText);
    makeSelected(m_nameText);

    auto *textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(LayoutConstants::MediumHMargin, LayoutConstants::NarrowVMargin, 0, LayoutConstants::NarrowVMargin);
    textLayout->setSpacing(LayoutConstants::NarrowVMargin);
    textLayout->addWidget(m_nameText, 1);
    textLayout->addWidget(m_infoText, 1);

    panel->setLayout(textLayout);

    auto *itemLayout = new QHBoxLayout{};
    itemLayout->setContentsMargins(LayoutConstants::NarrowHMargin, LayoutConstants::NarrowVMargin, LayoutConstants::NarrowHMargin, LayoutConstants::NarrowVMargin);
    itemLayout->addWidget(panel);

    setLayout(itemLayout);
}

void SearchListBoxItemRenderer::updateItem() { ControlListBoxItemRenderer::updateItem(); }

bool SearchListBoxItemRenderer::eventFilter(QObject *target, QEvent *event) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            emit itemDoubleClicked();
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::RightButton) {
            emit itemRightClicked();
            return true;
        }
    }

    return QObject::eventFilter(target, event);
}

QLabel *SearchListBoxItemRenderer::getNameText() const { return m_nameText; }

void SearchListBoxItemRenderer::setNameText(QLabel *MNameText) { m_nameText = MNameText; }

QLabel *SearchListBoxItemRenderer::getInfoText() const { return m_infoText; }

void SearchListBoxItemRenderer::setInfoText(QLabel *MInfoText) { m_infoText = MInfoText; }

void SearchListBoxItemRenderer::itemRightClicked() {}

void SearchListBoxItemRenderer::itemDoubleClicked() const {
    const auto document = kdl::mem_lock(m_document);

    if (m_node) {
        std::vector<Model::Node *> nodes;
        nodes.push_back(m_node);

        document->deselectAll();
        document->selectNodes(nodes);
        m_mapView->focusCameraOnSelection(true);
    }
}

/* ---------------------------------------------------------------------------- */

MapSearchListBox::MapSearchListBox(std::weak_ptr<MapDocument> document, MapView *mapView, Inspector *inspector, QWidget *parent, QLabel *label)
    : ControlListBox("", false, parent), m_document(std::move(document)), m_mapView(mapView), m_inspector(inspector), m_searchResults(label) {
    setEmptyText("No objects found.");
    m_searchTimer = new QTimer(this);
    m_searchTimer->setTimerType(Qt::TimerType::CoarseTimer);
    m_searchTimer->setSingleShot(true);

    updateSearchLabel();
    connect(m_searchTimer, &QTimer::timeout, this, &MapSearchListBox::searchTimerTigger);
    connectObservers();
}

ControlListBoxItemRenderer *MapSearchListBox::createItemRenderer(QWidget *parent, const size_t index) {
    const auto document = kdl::mem_lock(m_document);
    Model::EntityNodeBase *node = m_searchResult.at(index);

    return new SearchListBoxItemRenderer{document, m_mapView, node, parent};
}

void MapSearchListBox::selectedRowChanged(int index) {}

void MapSearchListBox::connectObservers() {
    const auto document = kdl::mem_lock(m_document);
    // Search rows contain node pointers too. Drop them before deletion/replacement.
    const auto clearResults = [this] {
        m_searchTimer->stop();
        m_searchResult.clear();
        updateSearchLabel();
        reload();
    };

    m_notifierConnection += document->documentWillBeClearedNotifier.connect([clearResults](auto *) { clearResults(); });
    m_notifierConnection += document->nodesWillBeRemovedNotifier.connect([clearResults](const auto &) { clearResults(); });
    m_notifierConnection += document->documentWasNewedNotifier.connect(this, &MapSearchListBox::documentDidChange);
    m_notifierConnection += document->documentWasLoadedNotifier.connect(this, &MapSearchListBox::documentDidChange);
    m_notifierConnection += document->documentWasClearedNotifier.connect(this, &MapSearchListBox::documentDidChange);
    m_notifierConnection += document->nodesWereAddedNotifier.connect(this, &MapSearchListBox::nodesDidChange);
    m_notifierConnection += document->nodesWereRemovedNotifier.connect(this, &MapSearchListBox::nodesDidChange);
    m_notifierConnection += document->nodesDidChangeNotifier.connect(this, &MapSearchListBox::nodesDidChange);
    m_notifierConnection += document->nodeVisibilityDidChangeNotifier.connect(this, &MapSearchListBox::nodesDidChange);
    m_notifierConnection += document->nodeLockingDidChangeNotifier.connect(this, &MapSearchListBox::nodesDidChange);
}

void MapSearchListBox::documentDidChange(MapDocument *) const { m_searchTimer->start(0); }

void MapSearchListBox::nodesDidChange(const std::vector<Model::Node *> &) const { if (!m_searchPhrase.isEmpty()) m_searchTimer->start(150); }

size_t MapSearchListBox::itemCount() const { return m_searchResult.size(); }

void MapSearchListBox::updateSearch(const QString &searchPhrase) {
    m_searchPhrase = searchPhrase;
    const auto document = kdl::mem_lock(m_document);
    m_searchResult.clear();
    if (!document->world()) {
        updateSearchLabel();
        reload();
        return;
    }
    const auto nodeIndex = &document->world()->entityNodeIndex();

    QString searchValue = "";
    QString searchClass = "classname";

    if (searchPhrase.contains(':')) {
        auto splitted = searchPhrase.split(':');
        if (splitted.size() == 2 && splitted[0].size() > 2 && splitted[1].size() > 2) {
            searchClass = splitted[0];
            searchValue = splitted[1];
        }
    } else { searchValue = searchPhrase; }

    if (searchValue.size() >
        0) { m_searchResult = nodeIndex->findEntity(searchClass.toStdString(), searchValue.toStdString()); }

    updateSearchLabel();
    reload();
}

void MapSearchListBox::searchTimerTigger() { updateSearch(m_searchPhrase); }

void MapSearchListBox::triggerSearch(const QString &phrase) {
    m_searchTimer->stop();

    if (phrase.isEmpty() || phrase.size() <= 0) {
        m_searchPhrase = phrase;
        m_searchResult.clear();

        updateSearchLabel();
        reload();
        return;
    }

    if (m_searchPhrase != phrase) {
        m_searchPhrase = phrase;
        m_searchTimer->start(150);
    }
}

void MapSearchListBox::contextMenuRequested(const QPoint &pos) {
    auto *popupMenu = new QMenu{this};

    popupMenu->addAction(tr("Select Entity"), this, &MapSearchListBox::selectNodeInDocument);
    popupMenu->addAction(tr("Move Camera to Entity"), this, &MapSearchListBox::focusSelectedNode);
    popupMenu->addSeparator();
    popupMenu->addAction(tr("Show Attributes"), this, &MapSearchListBox::showNodeAttributes);

    popupMenu->popup(this->mapToGlobal(pos));
}

void MapSearchListBox::selectNodeInDocument() const {
    const auto document = kdl::mem_lock(m_document);

    if (const auto node = nodeForCurrentRowIndex()) {
        std::vector<Model::Node *> nodes;
        nodes.push_back(node);

        document->deselectAll();
        document->selectNodes(nodes);
    }
}

Model::EntityNodeBase *MapSearchListBox::nodeForCurrentRowIndex() const {
    const auto index = static_cast<size_t>(currentRow());

    if (m_searchResult.size() > index) { return m_searchResult[index]; }

    return nullptr;
}

void MapSearchListBox::focusSelectedNode() const {
    selectNodeInDocument();
    m_mapView->focusCameraOnSelection(true);
}

void MapSearchListBox::showNodeAttributes() const {
    selectNodeInDocument();
    m_inspector->revealTool(QStringLiteral("properties"));
}

void MapSearchListBox::updateSearchLabel() const {
    if (itemCount() > 0) {
        const auto s = stringf("Found: %d", itemCount());
        m_searchResults->setText(QString::fromStdString(s));
    } else { m_searchResults->setText("Found: -"); }
}
}
