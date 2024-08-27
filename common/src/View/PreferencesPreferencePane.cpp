#include "PreferencesPreferencePane.h"

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QTableView>

#include "View/BorderLine.h"
#include "View/PreferenceEditDialog.h"
#include "View/QtUtils.h"

namespace TrenchBroom::View
{
PreferencesPreferencePane::PreferencesPreferencePane(QWidget* parent)
  : PreferencePane(parent)
  , m_table(nullptr)
  , m_model(nullptr)
  , m_proxy(nullptr)
{
  m_model = new PreferenceModel(this);
  m_proxy = new QSortFilterProxyModel(this);
  m_proxy->setSourceModel(m_model);
  m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_proxy->setFilterKeyColumn(-1); // Filter based on all columns

  m_table = new QTableView();
  m_table->setObjectName("QTableView_PreferencesPreferencePane");
  m_table->setCornerButtonEnabled(false);
  m_table->setModel(m_proxy);

  m_table->setHorizontalHeader(new QHeaderView(Qt::Horizontal));
  m_table->horizontalHeader()->resizeSection(0, 100);
  m_table->horizontalHeader()->setSectionResizeMode(
    PreferenceModel::Columns::Index, QHeaderView::ResizeMode::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
    PreferenceModel::Columns::Context, QHeaderView::ResizeMode::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
    PreferenceModel::Columns::Path, QHeaderView::ResizeMode::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(
    PreferenceModel::Columns::Type, QHeaderView::ResizeMode::Stretch);
  m_table->horizontalHeader()->setSectionResizeMode(
    PreferenceModel::Columns::Default, QHeaderView::ResizeMode::Stretch);
  m_table->horizontalHeader()->setSectionResizeMode(
    PreferenceModel::Columns::Value, QHeaderView::ResizeMode::Stretch);

  m_table->verticalHeader()->setDefaultSectionSize(
    m_table->fontMetrics().lineSpacing() + LayoutConstants::MediumHMargin);

  m_table->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
  m_table->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);

  m_table->setSortingEnabled(true);
  m_table->sortByColumn(PreferenceModel::Columns::Type, Qt::AscendingOrder);

  m_table->hideColumn(PreferenceModel::Columns::Index);

  QLineEdit* searchBox = createSearchBox();
  makeSmall(searchBox);

  infoLabel = new QLabel();
  makeInfo(infoLabel);
  updateLabel();

  auto* infoAndSearchLayout = new QHBoxLayout();
  infoAndSearchLayout->setContentsMargins(
    0, LayoutConstants::MediumHMargin, 0, LayoutConstants::WideHMargin);
  infoAndSearchLayout->setSpacing(LayoutConstants::WideHMargin);
  infoAndSearchLayout->addWidget(infoLabel, 1);
  infoAndSearchLayout->addWidget(searchBox);

  auto* layout = new QVBoxLayout();
  layout->setContentsMargins(
    LayoutConstants::WideHMargin,
    LayoutConstants::WideHMargin,
    LayoutConstants::WideHMargin,
    LayoutConstants::WideHMargin);
  layout->setSpacing(0);
  layout->addLayout(infoAndSearchLayout);
  layout->addWidget(m_table, 1);

  layout->addLayout(getControlButtonLayout());
  setLayout(layout);

  setMinimumSize(900, 550);

  setValueButton->setEnabled(m_table->selectionModel()->hasSelection());
  restoreDefaultButton->setEnabled(m_table->selectionModel()->hasSelection());

  connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, [&]() {
    setValueButton->setEnabled(m_table->selectionModel()->hasSelection());

    auto selectionModel = m_table->selectionModel();
    if (selectionModel->hasSelection())
    {
      auto index = selectionModel->currentIndex();
      auto id = size_t(m_table->model()->index(index.row(), 0).data().toInt());
      auto preference = m_model->dataAt(id);
      restoreDefaultButton->setEnabled(
        m_table->selectionModel()->hasSelection() && !preference->isDefault());
    }
    else
    {
      restoreDefaultButton->setEnabled(false);
    }
  });

  connect(searchBox, &QLineEdit::textChanged, this, [&](const QString& newText) {
    m_proxy->setFilterFixedString(newText);
    updateLabel();
  });

  connect(m_table, &QTableView::doubleClicked, this, [&](const QModelIndex& index) {
    auto id = size_t(m_table->model()->index(index.row(), 0).data().toInt());
    auto editDialog = new PreferenceEditDialog(m_model->dataAt(id), this);
    editDialog->exec();
  });

  connect(restoreDefaultButton, &QPushButton::clicked, this, [&]() {
    auto selectionModel = m_table->selectionModel();

    if (selectionModel->hasSelection())
    {
      QMessageBox::StandardButton reply;
      reply = QMessageBox::question(
        this,
        "Advanced Preferences",
        "Reset selected preference to default value?",
        QMessageBox::Yes | QMessageBox::No);

      if (reply == QMessageBox::No)
        return;

      auto index = selectionModel->currentIndex();
      auto id = size_t(m_table->model()->index(index.row(), 0).data().toInt());

      auto preference = m_model->dataAt(id);
      preference->resetToDefault();
      restoreDefaultButton->setEnabled(false);
    }
  });

  connect(setValueButton, &QPushButton::clicked, this, [&]() {
    auto selectionModel = m_table->selectionModel();

    if (selectionModel->hasSelection())
    {
      auto index = selectionModel->currentIndex();
      auto id = size_t(m_table->model()->index(index.row(), 0).data().toInt());
      auto editDialog = new PreferenceEditDialog(m_model->dataAt(id), this);
      editDialog->exec();
    }
  });
}

QHBoxLayout* PreferencesPreferencePane::getControlButtonLayout()
{
  auto buttonLayout = new QHBoxLayout();
  buttonLayout->setContentsMargins(
    0, LayoutConstants::WideHMargin, 0, LayoutConstants::WideHMargin);

  auto bl = new BorderLine(BorderLine::Direction::Horizontal, 13);
  // bl->setContentsMargins(0, 30, 0, 30);
  buttonLayout->addWidget(bl);

  buttonLayout->setSpacing(LayoutConstants::NarrowHMargin);

  restoreDefaultButton = new QPushButton(tr("Default"));
  restoreDefaultButton->setContentsMargins(
    LayoutConstants::NarrowHMargin,
    LayoutConstants::NarrowHMargin,
    LayoutConstants::NarrowHMargin,
    LayoutConstants::NarrowHMargin);

  restoreDefaultButton->setObjectName("PreferencesPreferencePane_smallPushButton");
  makeSmall(restoreDefaultButton);

  setValueButton = new QPushButton(tr("Set..."));
  setValueButton->setContentsMargins(
    LayoutConstants::NarrowHMargin,
    LayoutConstants::NarrowHMargin,
    LayoutConstants::NarrowHMargin,
    LayoutConstants::NarrowHMargin);

  setValueButton->setObjectName("PreferencesPreferencePane_smallPushButton");
  makeSmall(setValueButton);

  auto label =
    new QLabel("To enable changed preferences you may have to restart Trenchbroom.");
  makeInfo(label);

  buttonLayout->addWidget(label, 1);
  buttonLayout->addWidget(setValueButton);
  buttonLayout->addWidget(restoreDefaultButton);

  return buttonLayout;
}

bool PreferencesPreferencePane::doCanResetToDefaults()
{
  return false;
}

void PreferencesPreferencePane::doResetToDefaults() {}

void PreferencesPreferencePane::doUpdateControls()
{
  m_table->update();
}

bool PreferencesPreferencePane::doValidate()
{
  return true;
}

void PreferencesPreferencePane::updateLabel()
{
  infoLabel->setText(
    tr("Number of preferences found: ")
    + QString{}.sprintf("%d", m_proxy->rowCount(m_proxy->index(0, 0))));
}
} // namespace TrenchBroom::View
