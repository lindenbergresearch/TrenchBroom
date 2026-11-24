#pragma once

#include <QDialog>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListView;
class QSortFilterProxyModel;

namespace TrenchBroom::IO { class IconCatalog; }

namespace TrenchBroom::View {

/** Chooses a globally named SVG or font glyph. */
class IconSelectionDialog : public QDialog {
    Q_OBJECT
    const IO::IconCatalog& m_catalog;
    QListView* m_icons;
    QSortFilterProxyModel* m_results;
    QLineEdit* m_filter;
    QLabel *m_preview, *m_name, *m_count;
    QDialogButtonBox* m_buttons;

    void filterIcons(const QString& filter);
    void updateSelection();

  public:
    IconSelectionDialog(const IO::IconCatalog& catalog, const QString& currentIcon, QWidget* parent = nullptr);

    QString selectedIcon() const;
    void accept() override;
    bool eventFilter(QObject* watched, QEvent* event) override;
};

} // namespace TrenchBroom::View
