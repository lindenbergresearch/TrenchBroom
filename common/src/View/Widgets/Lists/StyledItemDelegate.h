#pragma once

#include <QHash>
#include <QPointer>
#include <QStyledItemDelegate>
#include <QWidget>

namespace TrenchBroom::View {
class ItemCellStyle : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool overrideTextColor MEMBER overrideTextColor)
  public:
    using QWidget::QWidget;
    bool overrideTextColor = false;
};

// One hidden style proxy per semantic role, shared by all rows of a view.
// Models expose meaning; QSS owns fonts and optional foreground overrides.
class StyledItemDelegate : public QStyledItemDelegate {
  public:
    static constexpr int TextStyleRole = Qt::UserRole + 16000;
    static constexpr int TextStatesRole = TextStyleRole + 1;
    explicit StyledItemDelegate(QString context, QObject* parent = nullptr);
    ~StyledItemDelegate() override;
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

  protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;

  private:
    QString m_context;
    mutable QHash<QString, QPointer<ItemCellStyle>> m_styles;
};
} // namespace TrenchBroom::View
