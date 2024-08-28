#pragma once

#include <QDialog>

#include "PreferenceManager.h"
#include "Preferences.h"
#include "QtUtils.h"

namespace TrenchBroom {
namespace View {

class PreferenceEditDialog : public QDialog {
  Q_OBJECT
  private:
    PreferenceBase *m_preference;
    QLabel *m_pathLabel;
    QLabel *m_typeLabel;
    QLabel *m_defaultIndicatorLabel;
    QPushButton *m_cancelButton;
    QPushButton *m_okButton;
    QWidget *m_valueWidget;
    QWidget *m_defaultValueWidget;
    int lineHeight = 0;

  public:
    PreferenceEditDialog(PreferenceBase *preference, QWidget *parent = nullptr, const Qt::WindowFlags &f = Qt::Dialog);

    ~PreferenceEditDialog() override;

    void updateValue();

    void cancel();

    template<typename T> std::tuple<T *, QHBoxLayout *> createCompound(QString labelText, const int minWidthLabel, const int minWidthWidget, const int minHeight) {
        auto labelWidget = new QLabel(labelText);
        labelWidget->setMinimumWidth(minWidthLabel);
        labelWidget->setMinimumHeight(minHeight);
        auto instance = new T;
        instance->setContentsMargins(0, 0, 0, 0);
        instance->setMinimumWidth(minWidthWidget);
        instance->setMinimumHeight(minHeight);

        auto layout = new QHBoxLayout{};

        makeMono(instance, QFont{}.pointSize());
        colorizeWidget(instance, QColor(255, 255, 255, 255), QPalette::ColorRole::Text);
        colorizeWidget(instance, QColor(255, 255, 255, 255), QPalette::ColorRole::WindowText);

        layout->addWidget(labelWidget);
        layout->addWidget(instance, 1);

        return {instance, layout};
    }

    void loadPreference(QVBoxLayout *layout);

  private:
    void createGui();
};
} // namespace View
} // namespace TrenchBroom
