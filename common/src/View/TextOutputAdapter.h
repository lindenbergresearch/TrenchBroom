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

#pragma once

#include <QString>
#include <QTextCursor>
#include <QTextStream>

#include <TrenchBroomApp.h>

class QTextEdit;

namespace TrenchBroom {
namespace View {

namespace Ascii {
static QChar ESC = 0x1b;   // Escape character
static QChar BEL = 0x07;   // Terminal bell
static QChar BS = 0x08;    // Backspace
static QChar HT = 0x09;    // Horizontal TAB
static QChar LF = 0x0A;    // Linefeed (newline)
static QChar VT = 0x0B;    // Vertical TAB
static QChar FF = 0x0C;    // Form-feed (also: New page NP)
static QChar CR = 0x0D;    // Carriage return
static QChar DEL = 0x7F;   // Delete character

// start and end indicator bytes
static QChar SCREEN_CMD_START = '[';
static QChar SCREEN_CMD_END = 'm';
}

/**
 * Helper for displaying the output of a command line tool in QTextEdit.
 *
 * - Interprets CR and LF control characters.
 * - Scroll bar follows output, unless it's manually raised.
 */
class TextOutputAdapter {
private:
    QTextEdit *m_textEdit;
    QTextCursor m_insertionCursor;

public:
    explicit TextOutputAdapter(QTextEdit *textEdit);

    template<typename T>
    void pushSystemMessage(const T &t) {
        QString string;
        QTextStream stream(&string);
        stream << t;

        auto format = QTextCharFormat{};
        auto font = TrenchBroomApp::instance().getUIFont();
        font.setBold(true);
        format.setFont(font);
        format.setFontPointSize(TrenchBroomApp::instance().getConsoleFont().pointSize());
        format.setForeground(QBrush{QColor{"#FFFFFF"}});
        m_insertionCursor.insertText(string, format);
    }

    /**
     * Appends the given value to the text widget.
     * Objects are formatted using QTextStream.
     */
    template<typename T>
    TextOutputAdapter &operator<<(const T &t) {
        QString string;
        QTextStream stream(&string);
        stream << t;
        appendString(string);
        return *this;
    }

private:
    void appendString(const QString &string);

    QTextCharFormat &decodeVT100Command(const QString &string, QTextCharFormat &format);
};
} // namespace View
} // namespace TrenchBroom
