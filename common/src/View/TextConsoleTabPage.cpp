#include "TextConsoleTabPage.h"

#include <QStackedLayout>
#include <QTextEdit>
#include <QVBoxLayout>

#include "Ensure.h"
#include "Macros.h"
#include "TabBook.h"
#include "View/TabBar.h"
#include "View/ViewConstants.h"

#include <string>

namespace TrenchBroom {
namespace View {

View::TextConsoleTabPage::TextConsoleTabPage(QWidget *parent) : TabBookPage(parent) {
    m_consoleWidget = new QTextEdit();
    m_consoleWidget->setReadOnly(true);
    m_consoleWidget->setWordWrapMode(QTextOption::NoWrap);

    QVBoxLayout *sizer = new QVBoxLayout();
    sizer->setContentsMargins(0, 0, 0, 0);
    sizer->addWidget(m_consoleWidget);
    setLayout(sizer);
}
const QString &TextConsoleTabPage::getCaption() const {
    return caption;
}
void TextConsoleTabPage::setCaption(const QString &Caption) {
    caption = Caption;
}
QTextEdit *TextConsoleTabPage::getConsoleWidget() const {
    return m_consoleWidget;
}
void TextConsoleTabPage::setConsoleWidget(QTextEdit *MConsoleWidget) {
    m_consoleWidget = MConsoleWidget;
}
} // namespace View
} // namespace TrenchBroom
