#pragma once
#include "NotifierConnection.h"
#include "View/EditorConfiguration.h"
#include "View/PreferencePane.h"

class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QToolBar;

namespace TrenchBroom::View {
class SearchableComboBox;
class EditorCustomizationPreferencePane : public PreferencePane {
    Q_OBJECT
    bool m_toolbarMode, m_loading = false, m_dirty = false, m_shortcutDirty = false;
    QString m_currentId;
    EditorConfiguration m_displayed;
    NotifierConnection m_connection;
    QListWidget* m_list;
    QLineEdit *m_filter, *m_label, *m_icon = nullptr, *m_tooltip = nullptr, *m_menu = nullptr;
    QLabel *m_error, *m_identity;
    QCheckBox* m_enabled;
    std::array<QCheckBox*, 4> m_targets{};
    QKeySequenceEdit* m_shortcut = nullptr;
    SearchableComboBox *m_command = nullptr, *m_available = nullptr;
    QComboBox *m_edge = nullptr, *m_alignment = nullptr, *m_arrangement = nullptr;
    QSpinBox *m_order = nullptr, *m_group = nullptr, *m_size = nullptr, *m_minimum = nullptr;
    QListWidget* m_assigned = nullptr;
    QToolBar* m_listTools;
    QWidget* m_form;
    void updateCurrentPresentation();
    void loadAssigned();
    void updateAssignedTools();
    void selectAssignedAction();
    void replaceAssignedAction();
    void createActionForm();
    void createToolbarForm();
    void reload();
    void loadCurrent();
    bool commit();
    bool save(EditorConfiguration configuration);
    void addEntry(bool duplicate);
    void removeEntry();
    void moveEntry(int delta);
    void assignAction(bool separator);
    void removeAssigned();
    void moveAssigned(int delta);
    void saveAssigned();
    void importConfiguration();
    void exportConfiguration();
    void watch(QLineEdit* edit);
    void watch(QSpinBox* spin);
    void watch(QComboBox* combo);
    void watch(QCheckBox* check);
    bool doCanResetToDefaults() override;
    void doResetToDefaults() override;
    void doUpdateControls() override;
    bool doValidate() override;

  public:
    explicit EditorCustomizationPreferencePane(bool toolbarMode, QWidget* parent = nullptr);
};
} // namespace TrenchBroom::View
