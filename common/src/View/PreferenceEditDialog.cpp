#include "PreferenceEditDialog.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

#include "FormWithSectionsLayout.h"
#include "View/AppInfoPanel.h"
#include "View/BorderLine.h"
#include "View/QtUtils.h"

#define IS_TYPE(TNAME, TVAR, TBASE)                                                      \
  Preference<TNAME>* TVAR = dynamic_cast<Preference<TNAME>*>(TBASE)

namespace TrenchBroom {
namespace View {

using namespace LayoutConstants;

PreferenceEditDialog::~PreferenceEditDialog() {}

PreferenceEditDialog::PreferenceEditDialog(
    PreferenceBase *preference, QWidget *parent, const Qt::WindowFlags &f)
    : QDialog(parent, f), m_preference(preference) {
  createGui();
}

void PreferenceEditDialog::createGui() {
  setWindowIconTB(this);
  setWindowTitle(
      "Advanced Preference Editor"
          + QString::fromStdString(m_preference->path().root_name()));

  auto title = new QLabel(tr("Edit preference value"));
  makeTitle(title);

  auto layout = new QVBoxLayout;
  layout->addWidget(title);
  layout->addWidget(new BorderLine);

  lineHeight = QFont{}.pointSize() + LayoutConstants::MediumVMargin*2;

  // --- components -------------------------------------------
  auto pathComp = createCompound<QLabel>(tr("Path/Context"), 110, 220, lineHeight);
  m_pathLabel = std::get<0>(pathComp);

  auto typeComp = createCompound<QLabel>(tr("Datatype"), 110, 220, lineHeight);
  m_typeLabel = std::get<0>(typeComp);

  auto isDefaultComp = createCompound<QLabel>(tr("Set to default"), 110, 220, lineHeight);
  m_defaultIndicatorLabel = std::get<0>(isDefaultComp);

  // --- layout -----------------------------------------------
  layout->addLayout(std::get<1>(pathComp));
  layout->addLayout(std::get<1>(typeComp));
  layout->addLayout(std::get<1>(isDefaultComp));

  // -- type dependent widgets --------------------------------
  loadPreference(layout);

  layout->addWidget(new BorderLine);

  // --- button -----------------------------------------------
  auto buttonLayout = new QHBoxLayout;
  buttonLayout->addWidget(new QLabel(""), 1);
  m_cancelButton = new QPushButton("Cancel", this);
  m_cancelButton->setObjectName("PreferenceEditDialog_toolButton");
  makeSmall(m_cancelButton);
  buttonLayout->addWidget(m_cancelButton);

  m_okButton = new QPushButton("Save", this);
  m_okButton->setObjectName("PreferenceEditDialog_toolButton");
  m_okButton->setFocus();
  makeSmall(m_okButton);
  buttonLayout->addWidget(m_okButton);
  m_okButton->setDefault(true);

  layout->addLayout(buttonLayout);

  setLayout(layout);

  connect(m_okButton, &QPushButton::clicked, this, &PreferenceEditDialog::updateValue);
  connect(m_cancelButton, &QPushButton::clicked, this, &PreferenceEditDialog::cancel);

  m_valueWidget->setFocus();
}

void PreferenceEditDialog::loadPreference(QVBoxLayout *layout) {
  m_pathLabel->setText(QString::fromStdString(m_preference->path()));
  m_defaultIndicatorLabel->setText(m_preference->isDefault() ? "yes" : "no");

  auto innerLayout = new QVBoxLayout;
  auto defaultComp = createCompound<QLabel>(tr("Default value"), 110, 220, lineHeight);
  m_defaultValueWidget = std::get<0>(defaultComp);

  /**
   * QString type
   */
  if (IS_TYPE(QString, preference, m_preference)) {
    m_typeLabel->setText("QString");

    // set label value (default value)
    ((QLabel *) m_defaultValueWidget)->setText(preference->defaultValue());

    auto valueComp = createCompound<QLineEdit>(tr("Current value"), 105, 220, lineHeight);
    m_valueWidget = std::get<0>(valueComp);
    ((QLineEdit *) m_valueWidget)->setText(preference->value());

    innerLayout->addLayout(std::get<1>(defaultComp));
    innerLayout->addWidget(new BorderLine);
    innerLayout->addLayout(std::get<1>(valueComp));
  }

  /**
   * float type
   */
  if (IS_TYPE(float, preference, m_preference)) {
    m_typeLabel->setText("float");

    // set label value (default value)
    ((QLabel *) m_defaultValueWidget)
        ->setText(QString::asprintf("%f", preference->defaultValue()));

    auto valueComp = createCompound<QLineEdit>(tr("Current value"), 105, 220, lineHeight);
    m_valueWidget = std::get<0>(valueComp);
    ((QLineEdit *) m_valueWidget)->setText(QString::asprintf("%f", preference->value()));

    innerLayout->addLayout(std::get<1>(defaultComp));
    innerLayout->addWidget(new BorderLine);
    innerLayout->addLayout(std::get<1>(valueComp));
  }

  /**
   * int type
   */
  if (IS_TYPE(int, preference, m_preference)) {
    m_typeLabel->setText("int");

    // set label value (default value)
    ((QLabel *) m_defaultValueWidget)
        ->setText(QString::asprintf("%d", preference->defaultValue()));

    auto valueComp = createCompound<QLineEdit>(tr("Current value"), 105, 220, lineHeight);
    m_valueWidget = std::get<0>(valueComp);
    ((QLineEdit *) m_valueWidget)->setText(QString::asprintf("%d", preference->value()));

    innerLayout->addLayout(std::get<1>(defaultComp));
    innerLayout->addWidget(new BorderLine);
    innerLayout->addLayout(std::get<1>(valueComp));
  }

  /**
   * bool type
   */
  if (IS_TYPE(bool, preference, m_preference)) {
    m_typeLabel->setText("bool");

    // set label value (default value)
    ((QLabel *) m_defaultValueWidget)
        ->setText(preference->defaultValue() ? "true" : "false");

    auto valueComp = createCompound<QCheckBox>(tr("Flag value"), 110, 220, lineHeight);
    m_valueWidget = std::get<0>(valueComp);
    ((QCheckBox *) m_valueWidget)->setChecked(preference->value());

    innerLayout->addLayout(std::get<1>(defaultComp));
    innerLayout->addWidget(new BorderLine);
    innerLayout->addLayout(std::get<1>(valueComp));
  }

  /**
   * path type
   */
  if (IS_TYPE(std::filesystem::path, preference, m_preference)) {
    m_typeLabel->setText("path");

    // set label value (default value)
    ((QLabel *) m_defaultValueWidget)->setText(preference->defaultValue().string().c_str());

    auto valueComp = createCompound<QLineEdit>(tr("Current value"), 110, 220, lineHeight);
    m_valueWidget = std::get<0>(valueComp);
    ((QLineEdit *) m_valueWidget)->setText(preference->value().string().c_str());

    auto *browseButton = new QPushButton("...");
    browseButton->setObjectName("PreferenceEditDialog_toolButton");

    connect(browseButton, &QPushButton::clicked, this, [=]() {
      const QString pathStr = QFileDialog::getOpenFileName(
          this,
          tr("%1 Path").arg(
              QString::fromStdString(preference->defaultValue().string().c_str())),
          fileDialogDefaultDirectory(FileDialogDir::Resources));

      if (pathStr.isEmpty()) {
        return;
      }

      ((QLineEdit *) m_valueWidget)->setText(pathStr);
    });

    std::get<1>(valueComp)->addWidget(browseButton);

    innerLayout->addLayout(std::get<1>(defaultComp));
    innerLayout->addWidget(new BorderLine);
    innerLayout->addLayout(std::get<1>(valueComp));
  }

  layout->addLayout(innerLayout);
}

void PreferenceEditDialog::updateValue() {
  auto &prefs = PreferenceManager::instance();
  QString valueStr;
  bool valueBool;
  auto failed = false;
  QString msg = "";

  if (dynamic_cast<QLineEdit *>(m_valueWidget)) {
    valueStr = ((QLineEdit *) m_valueWidget)->text();
  } else if (dynamic_cast<QCheckBox *>(m_valueWidget)) {
    valueBool = ((QCheckBox *) m_valueWidget)->isChecked();
  }

  if (IS_TYPE(std::filesystem::path, preference, m_preference)) {
    std::filesystem::path path;
    path = valueStr.toStdString();

    failed = prefs.set(*preference, path);
  }

  if (IS_TYPE(int, preference, m_preference)) {
    auto flag = false;
    auto valueInt = valueStr.toInt(&flag);

    if (!flag) {
      failed = true;
      msg = "The given value is not a valid integer: '" + valueStr + "'";
    } else {
      failed = !prefs.set(*preference, valueInt);
    }
  }

  if (IS_TYPE(float, preference, m_preference)) {
    auto flag = false;
    auto valueFloat = valueStr.toFloat(&flag);

    if (!flag) {
      failed = true;
      msg = "The given value is not a valid float: '" + valueStr + "'";
    } else {
      failed = !prefs.set(*preference, valueFloat);
    }
  }

  if (IS_TYPE(QString, preference, m_preference)) {
    failed = !prefs.set(*preference, valueStr);
  }

  if (IS_TYPE(bool, preference, m_preference)) {
    failed = !prefs.set(*preference, valueBool);
  }

  if (failed) {
    QMessageBox msgBox;
    msgBox.setText("Unable to set preference value.");
    msgBox.setInformativeText(msg);
    msgBox.setWindowTitle("Error which setting preference");
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.exec();
  } else {
    close();
  }
}

void PreferenceEditDialog::cancel() {
  this->close();
}
} // namespace View
} // namespace TrenchBroom
