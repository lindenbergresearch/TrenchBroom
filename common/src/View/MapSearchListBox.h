#pragma once

#include "NotifierConnection.h"
#include "View/ControlListBox.h"

#include <memory>
#include <vector>

namespace TrenchBroom {
namespace View {

//namespace Model {
//class LayerNode;
//class Node;
//}

class MapDocument;


/* ---------------------------------------------------------------------------- */

class SearchListBoxItemRenderer : public ControlListBoxItemRenderer {
Q_OBJECT
  std::weak_ptr<MapDocument> m_document;
  QLabel *m_nameText;
  QLabel *m_infoText;

public:
  SearchListBoxItemRenderer(std::weak_ptr<MapDocument> document, QLabel *nameText, QLabel *infoText, QWidget *parent = nullptr);

  void updateItem() override;

  QLabel *getNameText() const;

  void setNameText(QLabel *MNameText);

  QLabel *getInfoText() const;

  void setInfoText(QLabel *MInfoText);

private:
  bool eventFilter(QObject *target, QEvent *event) override;

signals:

};


/* ---------------------------------------------------------------------------- */

class MapSearchListBox : public ControlListBox {
Q_OBJECT
private:
  std::weak_ptr<MapDocument> m_document;
public:
  explicit MapSearchListBox(std::weak_ptr<MapDocument> document, QWidget *parent = nullptr);

private:
  size_t itemCount() const override;

  ControlListBoxItemRenderer *createItemRenderer(QWidget *parent, size_t index) override;

  void selectedRowChanged(int index) override;

private:
  void connectObservers();

  void documentDidChange(MapDocument *document);
};

}
}
