#pragma once

#include "View/PreferenceModel.h"
#include "View/PreferencePane.h"


class QWidget;


class QTableView;


class QSortFilterProxyModel;

namespace TrenchBroom::View {
class ColorModel;


class PreferencesPreferencePane : public PreferencePane {
    Q_OBJECT
private:
    QTableView *m_table;
    PreferenceModel *m_model;
    QSortFilterProxyModel *m_proxy;
    QLabel *infoLabel;
    QPushButton *restoreDefaultButton;
    QPushButton *setValueButton;

public:
    explicit PreferencesPreferencePane(QWidget *parent = nullptr);

private:
    bool doCanResetToDefaults() override;

    void doResetToDefaults() override;

    void doUpdateControls() override;

    bool doValidate() override;

    void updateLabel();

    QHBoxLayout *getControlButtonLayout();
};
}// namespace TrenchBroom::View
