#pragma once

#include "IO/IconStyle.h"

#include <QDialog>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListView;
class QPushButton;
class QSortFilterProxyModel;
class QSpinBox;

namespace TrenchBroom::IO { class IconCatalog; }

namespace TrenchBroom::View {

class ColorButton;

/** Chooses a globally named SVG or font glyph. */
class IconSelectionDialog : public QDialog {
    Q_OBJECT
    const IO::IconCatalog& m_catalog;
    IO::IconStyle m_iconStyle;
    QListView* m_icons;
    QSortFilterProxyModel* m_results;
    QLineEdit* m_filter;
    QLabel *m_preview, *m_name, *m_count;
    QDialogButtonBox* m_buttons;
    ColorButton* m_color;
    QPushButton* m_defaultColor;
    QSpinBox *m_size, *m_brightness, *m_opacity;

    void filterIcons(const QString& filter);
    void updateSelection();

  public:
    IconSelectionDialog(const IO::IconCatalog& catalog, const QString& currentIcon,
        const IO::IconStyle& iconStyle = {}, QWidget* parent = nullptr);

    QString selectedIcon() const;
    const IO::IconStyle& iconStyle() const { return m_iconStyle; }
    void accept() override;
    bool eventFilter(QObject* watched, QEvent* event) override;
};

} // namespace TrenchBroom::View
