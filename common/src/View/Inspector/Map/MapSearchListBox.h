#pragma once

#include <QTimer>

#include "Model/EntityNodeBase.h"
#include "NotifierConnection.h"
#include "View/Widgets/Lists/ControlListBox.h"
#include "View/Inspector/Inspector.h"
#include "View/MapView/MapView.h"

#include <memory>
#include <vector>

namespace TrenchBroom::View {
class MapDocument;

/* ----------------------------------------------------------------------------------------- */

class SearchListBoxItemRenderer : public ControlListBoxItemRenderer {
    Q_OBJECT
    std::weak_ptr<MapDocument> m_document;
    Model::EntityNodeBase* m_node;
    MapView* m_mapView;

    QLabel* m_nameText;
    QLabel* m_infoText;

public:
    SearchListBoxItemRenderer(std::weak_ptr<MapDocument> document, MapView* mapView, Model::EntityNodeBase* node, QWidget* parent = nullptr);

    void updateItem() override;

    QLabel* getNameText() const;

    void setNameText(QLabel* MNameText);

    QLabel* getInfoText() const;

    void setInfoText(QLabel* MInfoText);

    bool eventFilter(QObject* target, QEvent* event) override;

    void itemDoubleClicked() const;

    static void itemRightClicked();
};

/* -----------------------------------------------------------------------------------------
     */

class MapSearchListBox : public ControlListBox {
    Q_OBJECT

private:
    std::weak_ptr<MapDocument> m_document;
    NotifierConnection m_notifierConnection;
    MapView* m_mapView;
    Inspector* m_inspector;

    QLabel* m_searchResults;
    QString m_searchPhrase{};
    std::vector<Model::EntityNodeBase *> m_searchResult;

    QTimer* m_searchTimer;

public:
    explicit MapSearchListBox(std::weak_ptr<MapDocument> document, MapView* mapView, Inspector* inspector, QWidget* parent = nullptr, QLabel* label = nullptr);

    void updateSearch(const QString& searchPhrase);

    void triggerSearch(const QString& phrase);

private:
    size_t itemCount() const override;

    void updateSearchLabel() const;

    ControlListBoxItemRenderer* createItemRenderer(QWidget* parent, size_t index) override;

    void selectedRowChanged(int index) override;

    void connectObservers();

    Model::EntityNodeBase* nodeForCurrentRowIndex() const;

void contextMenuRequested(const QPoint& pos) override;

    void searchTimerTigger();

    void nodesDidChange(const std::vector<Model::Node *>&) const;

    void documentDidChange(MapDocument* document) const;

    void selectNodeInDocument() const;

    void focusSelectedNode() const;

    void showNodeAttributes() const;
};
}
