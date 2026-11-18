#pragma once

#include <QDialog>
#include <QStringList>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListWidget;

namespace TrenchBroom::View {

/** Chooses an SVG from the editor's icon resources without storing absolute paths. */
class IconSelectionDialog : public QDialog {
    Q_OBJECT
    QListWidget* m_icons;
    QLineEdit* m_filter;
    QLabel *m_preview, *m_name, *m_count;
    QDialogButtonBox* m_buttons;

    void filterIcons(const QString& filter);
    void updateSelection();

  public:
    explicit IconSelectionDialog(const QString& currentIcon, QWidget* parent = nullptr);

    static QStringList availableIcons();
    QString selectedIcon() const;
    void accept() override;
    bool eventFilter(QObject* watched, QEvent* event) override;
};

} // namespace TrenchBroom::View
