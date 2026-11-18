#pragma once

#include <QColor>
#include <QComboBox>

class QCompleter;

namespace TrenchBroom::View {
class ComboSearchModel;

// The combo keeps the complete source model and stable item IDs. Its one popup
// filters labels and IDs; editing a query never changes the selected item.
class SearchableComboBox : public QComboBox {
    Q_OBJECT
    Q_PROPERTY(QColor matchForeground READ matchForeground WRITE setMatchForeground)
    Q_PROPERTY(QColor matchBackground READ matchBackground WRITE setMatchBackground)
    ComboSearchModel* m_searchModel;
    QCompleter* m_searchCompleter;
    bool m_searching = false;
    QColor m_matchForeground, m_matchBackground;
    void choose(const QModelIndex& index);
    void chooseRow(int row);
    bool eventFilter(QObject* object, QEvent* event) override;

  public:
    explicit SearchableComboBox(QWidget* parent = nullptr);
    QVariant resolvedData() const;
    QString searchText() const;
    void cancelSearch();
    void showPopup() override;
    void hidePopup() override;
    QColor matchForeground() const;
    QColor matchBackground() const;
    void setMatchForeground(const QColor& color);
    void setMatchBackground(const QColor& color);

  signals:
    void searchStateChanged();
};
} // namespace TrenchBroom::View
