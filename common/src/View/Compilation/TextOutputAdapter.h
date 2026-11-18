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
#include <QScrollBar>
#include <QTextEdit>

#include "View/Widgets/Info/SemanticTextEdit.h"

class QTextEdit;

namespace TrenchBroom {
namespace View {

namespace Ascii {
static const QChar ESC{char16_t(0x1b)}; // Escape character
static const QChar BEL{char16_t(0x07)}; // Terminal bell
static const QChar BS{char16_t(0x08)};  // Backspace
static const QChar HT{char16_t(0x09)};  // Horizontal TAB
static const QChar LF{char16_t(0x0A)};  // Linefeed (newline)
static const QChar VT{char16_t(0x0B)};  // Vertical TAB
static const QChar FF{char16_t(0x0C)};  // Form-feed (also: New page NP)
static const QChar CR{char16_t(0x0D)};  // Carriage return
static const QChar DEL{char16_t(0x7F)}; // Delete character

// start and end indicator bytes
static QChar SCREEN_CMD_START = '[';
static QChar SCREEN_CMD_END = 'm';
} // namespace Ascii

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

    template <typename T> void pushSystemMessage(const T& value, MessageStyle role = MessageStyle::System) {
        QString string;
        QTextStream stream(&string);
        stream << value;
        insertSystemMessage(string, SemanticTextEdit::messageFormat(*m_textEdit, role, true));
    }

    // Explicit colors are data, for example output supplied by an external tool.
    template <typename T> void pushSystemMessage(const T& value, const QColor& color) {
        QString string;
        QTextStream stream(&string);
        stream << value;
        auto format = SemanticTextEdit::messageFormat(*m_textEdit, MessageStyle::System, true);
        format.clearProperty(SemanticTextEdit::RoleProperty);
        format.clearProperty(SemanticTextEdit::EmphasisProperty);
        format.setForeground(color);
        insertSystemMessage(string, format);
    }

    /**
     * Appends the given value to the text widget.
     * Objects are formatted using QTextStream.
     */
    template<typename T> TextOutputAdapter &operator<<(const T &t) {
        QString string;
        QTextStream stream(&string);
        stream << t;
        appendString(string);
        return *this;
    }

  private:
    void insertSystemMessage(const QString& string, const QTextCharFormat& format);
    void appendString(const QString &string);

    QTextCharFormat &decodeVT100Command(const QString &string, QTextCharFormat &format);
};
} // namespace View
} // namespace TrenchBroom
