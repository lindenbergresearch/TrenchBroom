#include "MapSearchListBox.h"
#include "ViewConstants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QRadioButton>
#include <QToolButton>
#include <QtGlobal>

#include "Model/LayerNode.h"
#include "Model/WorldNode.h"
#include "View/MapDocument.h"
#include "View/QtUtils.h"
#include "View/ViewConstants.h"

#include <kdl/memory_utils.h>

namespace TrenchBroom {
namespace View {

/* ---------------------------------------------------------------------------- */

SearchListBoxItemRenderer::SearchListBoxItemRenderer(std::weak_ptr<MapDocument> document, QLabel *nameText, QLabel *infoText, QWidget *parent) :
    ControlListBoxItemRenderer(parent), m_document(std::move(document)), m_nameText(nameText), m_infoText(infoText) {
  installEventFilter(this);

  m_nameText->installEventFilter(this);
  m_infoText->installEventFilter(this);

  auto *textLayout = new QVBoxLayout();
  textLayout->setContentsMargins(0, LayoutConstants::NarrowVMargin, 0, LayoutConstants::NarrowVMargin);
  textLayout->setSpacing(LayoutConstants::NarrowVMargin);
  textLayout->addWidget(m_nameText, 1);
  textLayout->addWidget(m_infoText, 1);

  setLayout(textLayout);
}

void SearchListBoxItemRenderer::updateItem() {
  ControlListBoxItemRenderer::updateItem();
}

bool SearchListBoxItemRenderer::eventFilter(QObject *target, QEvent *event) {
  if (event->type() == QEvent::MouseButtonDblClick) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      //   emit layerDoubleClicked(m_layer);
      return true;
    }
  } else if (event->type() == QEvent::MouseButtonRelease) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::RightButton) {
      //  emit layerRightClicked(m_layer);
      return true;
    }
  }

  return QObject::eventFilter(target, event);
}

QLabel *SearchListBoxItemRenderer::getNameText() const {
  return m_nameText;
}

void SearchListBoxItemRenderer::setNameText(QLabel *MNameText) {
  m_nameText = MNameText;
}

QLabel *SearchListBoxItemRenderer::getInfoText() const {
  return m_infoText;
}

void SearchListBoxItemRenderer::setInfoText(QLabel *MInfoText) {
  m_infoText = MInfoText;
}

/* ---------------------------------------------------------------------------- */

MapSearchListBox::MapSearchListBox(std::weak_ptr<MapDocument> document, QWidget *parent) : ControlListBox("", true, parent), m_document(std::move(document)) {

}

ControlListBoxItemRenderer *MapSearchListBox::createItemRenderer(QWidget *parent, size_t index) {
  auto document = kdl::mem_lock(m_document);

  auto renderer = new SearchListBoxItemRenderer{document, new QLabel{tr("Test Item _ bla")}, new QLabel{tr("This is at (123 12 42)")}, parent};

  return renderer;
}

void MapSearchListBox::selectedRowChanged(int index) {

}

void MapSearchListBox::connectObservers() {

}

void MapSearchListBox::documentDidChange(MapDocument *document) {

}

size_t MapSearchListBox::itemCount() const {
  return 0;
}
}
}
