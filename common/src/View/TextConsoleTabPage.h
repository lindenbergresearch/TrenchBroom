#pragma once

#include "Logger.h"
#include "View/TabBook.h"

#include <string>


class QTextEdit;
class QString;
class QWidget;

namespace TrenchBroom {
namespace View {
class TextConsoleTabPage : public TabBookPage {
private:
   QTextEdit *m_consoleWidget;
   QString caption;

public:
    TextConsoleTabPage(QWidget *parent = nullptr);

    const QString &getCaption() const;
    void setCaption(const QString &Caption);

    QTextEdit *getConsoleWidget() const;
    void setConsoleWidget(QTextEdit *MConsoleWidget);

private:

};
}// namespace View
}// namespace TrenchBroom
