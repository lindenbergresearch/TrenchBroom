#include "View/Widgets/Lists/StyledItemDelegate.h"

#include <QStyleOptionViewItem>

namespace TrenchBroom::View {
StyledItemDelegate::StyledItemDelegate(QString context, QObject* parent) : QStyledItemDelegate(parent), m_context(std::move(context)) {}

StyledItemDelegate::~StyledItemDelegate() {
    for (auto style : m_styles)
        delete style.data();
}

QWidget* StyledItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    auto* editor = QStyledItemDelegate::createEditor(parent, option, index);
    if (editor) {
        editor->setObjectName("StyledCellEditor");
        editor->setProperty("cellContext", m_context);
        editor->setProperty("cellRole", index.data(TextStyleRole));
        editor->setProperty("cellStates", index.data(TextStatesRole));
    }
    return editor;
}

void StyledItemDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const {
    QStyledItemDelegate::initStyleOption(option, index);
    const auto role = index.data(TextStyleRole).toString();
    if (role.isEmpty()) return;
    const auto states = index.data(TextStatesRole).toStringList();
    const auto key = role + QChar(0x1f) + states.join(QChar(0x1f));
    auto& proxy = m_styles[key];
    if (!proxy) {
        proxy = new ItemCellStyle(const_cast<QWidget*>(option->widget));
        proxy->setObjectName("ItemCellStyle");
        proxy->setProperty("cellContext", m_context);
        proxy->setProperty("cellRole", role);
        proxy->setProperty("cellStates", states);
        proxy->setAttribute(Qt::WA_DontShowOnScreen);
        proxy->hide();
    }
    proxy->ensurePolished();
    option->font = proxy->font();
    option->fontMetrics = QFontMetrics(option->font);
    if (proxy->overrideTextColor) {
        option->palette.setColor(QPalette::Text, proxy->palette().color(QPalette::WindowText));
        option->palette.setColor(QPalette::WindowText, proxy->palette().color(QPalette::WindowText));
    }
}
} // namespace TrenchBroom::View
