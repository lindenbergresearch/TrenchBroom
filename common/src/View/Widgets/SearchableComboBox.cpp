#include "View/Widgets/SearchableComboBox.h"

#include "View/Preferences/FontUtils.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTextCharFormat>
#include <QTextLayout>
#include <QTimer>

namespace TrenchBroom::View {
class ComboSearchModel : public QSortFilterProxyModel {
    QString m_query;
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        const auto index = sourceModel()->index(row, 0, parent);
        return index.data().toString().contains(m_query, Qt::CaseInsensitive) || index.data(Qt::UserRole).toString().contains(m_query, Qt::CaseInsensitive);
    }

  public:
    explicit ComboSearchModel(QObject* parent) : QSortFilterProxyModel{parent} {}
    void setQuery(const QString& query) {
        if (m_query == query) return;
        m_query = query;
        invalidateFilter();
    }
};

namespace {
    class SearchLineEdit : public QLineEdit {
      public:
        using QLineEdit::QLineEdit;
        void mousePressEvent(QMouseEvent* event) override {
            QLineEdit::mousePressEvent(event);
            selectAll();
        }
        void focusInEvent(QFocusEvent* event) override {
            QLineEdit::focusInEvent(event);
            selectAll();
        }
    };

    class ComboSearchDelegate : public QStyledItemDelegate {
        SearchableComboBox& m_combo;
        void drawText(QPainter& painter, const QRect& rect, const QString& text, const QFont& font, const QColor& color) const {
            const auto elided = QFontMetrics{font}.elidedText(text, Qt::ElideMiddle, rect.width());
            QTextLayout layout{elided, font};
            QVector<QTextLayout::FormatRange> formats;
            const auto query = m_combo.searchText();
            if (!query.isEmpty()) {
                for (auto offset = elided.indexOf(query, 0, Qt::CaseInsensitive); offset >= 0; offset = elided.indexOf(query, offset + query.size(), Qt::CaseInsensitive)) {
                    QTextCharFormat match;
                    match.setFontWeight(QFont::Bold);
                    if (m_combo.matchForeground().isValid()) match.setForeground(m_combo.matchForeground());
                    if (m_combo.matchBackground().isValid()) match.setBackground(m_combo.matchBackground());
                    formats.push_back({int(offset), int(query.size()), match});
                }
            }
            layout.setFormats(formats);
            layout.beginLayout();
            auto line = layout.createLine();
            if (line.isValid()) line.setLineWidth(rect.width());
            layout.endLayout();
            painter.save();
            painter.setClipRect(rect);
            painter.setPen(color);
            layout.draw(&painter, rect.topLeft());
            painter.restore();
        }

      public:
        explicit ComboSearchDelegate(SearchableComboBox& combo) : QStyledItemDelegate{&combo}, m_combo{combo} {}
        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override {
            return {360, option.fontMetrics.height() + QFontMetrics{detailFont(option.font)}.height() + 10};
        }
        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
            auto opt = option;
            initStyleOption(&opt, index);
            const auto text = opt.text;
            const auto icon = opt.icon;
            opt.text.clear();
            opt.icon = {};
            const auto* style = opt.widget ? opt.widget->style() : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
            const auto selected = opt.state.testFlag(QStyle::State_Selected);
            const auto color = opt.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
            const auto iconSize = qMax(16, opt.decorationSize.width());
            const auto left = opt.rect.left() + 6;
            icon.paint(painter, QRect{left, opt.rect.center().y() - iconSize / 2, iconSize, iconSize});
            auto textRect = opt.rect.adjusted(iconSize + 12, 4, -6, -4);
            textRect.setHeight(opt.fontMetrics.height());
            drawText(*painter, textRect, text, opt.font, color);
            textRect.moveTop(textRect.bottom() + 1);
            const auto font = detailFont(opt.font);
            textRect.setHeight(QFontMetrics{font}.height());
            drawText(*painter, textRect, index.data(Qt::UserRole).toString(), font, color);
        }
    };
} // namespace

SearchableComboBox::SearchableComboBox(QWidget* parent) : QComboBox{parent}, m_searchModel{new ComboSearchModel{this}}, m_searchCompleter{new QCompleter{m_searchModel, this}} {
    setProperty("searchable", true);
    setEditable(true);
    setInsertPolicy(QComboBox::NoInsert);
    setMaxVisibleItems(12);
    setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    setMinimumContentsLength(24);
    setLineEdit(new SearchLineEdit);
    // QComboBox's default completer can update the text before an activated
    // signal resolves its source row. Own the popup and resolve its item ID first.
    setCompleter(nullptr);
    m_searchModel->setSourceModel(model());
    m_searchCompleter->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    m_searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_searchCompleter->setMaxVisibleItems(maxVisibleItems());
    lineEdit()->setCompleter(m_searchCompleter);
    auto* popup = m_searchCompleter->popup();
    popup->setObjectName("SearchableComboPopup");
    popup->setItemDelegate(new ComboSearchDelegate{*this});
    popup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    popup->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    lineEdit()->installEventFilter(this);
    connect(lineEdit(), &QLineEdit::textEdited, this, [this](const QString& text) {
        m_searching = true;
        m_searchModel->setQuery(text.trimmed());
        emit searchStateChanged();
        showPopup();
    });
    connect(lineEdit(), &QLineEdit::editingFinished, this, [this] {
        QTimer::singleShot(0, this, [this] {
            if (!lineEdit()->hasFocus() && !m_searchCompleter->popup()->isVisible()) cancelSearch();
        });
    });
    connect(m_searchCompleter, qOverload<const QModelIndex&>(&QCompleter::activated), this, &SearchableComboBox::choose);
    connect(this, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        m_searching = false;
        m_searchModel->setQuery({});
    });
}

QVariant SearchableComboBox::resolvedData() const {
    if (m_searching) return {};
    if (currentIndex() >= 0 && currentText() == itemText(currentIndex())) return currentData();
    const auto row = findData(currentText().trimmed());
    return row >= 0 ? itemData(row) : QVariant{};
}
QString SearchableComboBox::searchText() const {
    return m_searching ? lineEdit()->text().trimmed() : QString{};
}

void SearchableComboBox::choose(const QModelIndex& index) {
    const auto id = index.data(Qt::UserRole);
    const auto row = id.isValid() ? findData(id) : -1;
    chooseRow(row);
}
void SearchableComboBox::chooseRow(int row) {
    if (row < 0) return;
    m_searchCompleter->popup()->hide();
    m_searching = false;
    m_searchModel->setQuery({});
    setCurrentIndex(row);
    setEditText(itemText(row));
    lineEdit()->setCursorPosition(0);
    emit searchStateChanged();
    emit activated(row);
    emit textActivated(itemText(row));
}
void SearchableComboBox::cancelSearch() {
    m_searchCompleter->popup()->hide();
    m_searching = false;
    m_searchModel->setQuery({});
    setEditText(currentIndex() >= 0 ? itemText(currentIndex()) : QString{});
    lineEdit()->setCursorPosition(0);
    emit searchStateChanged();
}
void SearchableComboBox::showPopup() {
    if (!m_searching) m_searchModel->setQuery({});
    m_searchCompleter->setCompletionPrefix({});
    if (!m_searchModel->rowCount()) {
        m_searchCompleter->popup()->hide();
        return;
    }
    m_searchCompleter->complete(QRect{-lineEdit()->x(), -lineEdit()->y(), width(), height()});
}
void SearchableComboBox::hidePopup() {
    cancelSearch();
}

bool SearchableComboBox::eventFilter(QObject* object, QEvent* event) {
    if (object == lineEdit() && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape && m_searching) {
            cancelSearch();
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            const auto selected = m_searchCompleter->popup()->currentIndex();
            if (m_searching && m_searchModel->rowCount())
                choose(selected.isValid() ? selected : m_searchCompleter->completionModel()->index(0, 0));
            else if (resolvedData().isValid())
                chooseRow(findData(resolvedData()));
            else
                cancelSearch();
            return true;
        }
        if (key->key() == Qt::Key_Down && !m_searchCompleter->popup()->isVisible()) {
            showPopup();
            return true;
        }
    }
    return QComboBox::eventFilter(object, event);
}

QColor SearchableComboBox::matchForeground() const {
    return m_matchForeground;
}
QColor SearchableComboBox::matchBackground() const {
    return m_matchBackground;
}
void SearchableComboBox::setMatchForeground(const QColor& color) {
    m_matchForeground = color;
}
void SearchableComboBox::setMatchBackground(const QColor& color) {
    m_matchBackground = color;
}
} // namespace TrenchBroom::View
