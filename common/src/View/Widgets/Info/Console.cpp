/*
 Copyright (C) 2010-2017 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "View/Widgets/Info/Console.h"
#include "View/Widgets/Info/SemanticTextEdit.h"

#include <QScrollBar>
#include <QTextEdit>
#include <QVBoxLayout>

#include "FileLogger.h"
#include "PreferenceManager.h"
#include "Preferences.h"

namespace TrenchBroom::View {
Console::Console(QWidget* parent) : TabBookPage(parent) {
    m_textView = new SemanticTextEdit();
    m_textView->setObjectName("ConsoleOutput");
    m_textView->setReadOnly(true);
    m_textView->setWordWrapMode(QTextOption::NoWrap);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_textView);
    setLayout(layout);
    setLogLevel(pref(Preferences::AppLogLevel));
}


Console::~Console() {}

void Console::doLog(const LogLevel level, const LogMessage* message) {
    logToConsole(level, message->format(false, false));
    // redirect to default logger
    defaultQtLogger.log(level, message);
    FileLogger::instance().log(level, message->format(true, false));
}

void Console::logToConsole(const LogLevel level, const QString& message) const {
    auto role = MessageStyle::Normal;
    switch (level) {
    case LogLevel::Trace:
        role = MessageStyle::Trace;
        break;
    case LogLevel::Debug:
        role = MessageStyle::Debug;
        break;
    case LogLevel::Info:
        role = MessageStyle::Info;
        break;
    case LogLevel::Warn:
        role = MessageStyle::Warning;
        break;
    case LogLevel::Error:
        role = MessageStyle::Error;
        break;
    case LogLevel::None:
        break;
    }
    const auto format = SemanticTextEdit::messageFormat(*m_textView, role);
    QTextCursor cursor(m_textView->document());
    cursor.movePosition(QTextCursor::MoveOperation::End);
    cursor.insertText(message, format);
    cursor.insertText("\n");

    m_textView->moveCursor(QTextCursor::MoveOperation::End);
}
}
