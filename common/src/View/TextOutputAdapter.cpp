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

#include "TextOutputAdapter.h"

#include <QScrollBar>
#include <QString>
#include <QTextEdit>

#include "Ensure.h"

#include <string>

#define match_foreground(STR, COLOR) \
    if (string == STR) { \
        format.setForeground(QBrush{QColor{COLOR}}); return format; \
    } \

#define match_background(STR, COLOR)  \
    if (string == STR) { \
        format.setBackground(QBrush{QColor{COLOR}}); return format; \
    } \


namespace TrenchBroom {
namespace View {
TextOutputAdapter::TextOutputAdapter(QTextEdit *textEdit) {
    ensure(textEdit != nullptr, "textEdit is null");
    m_textEdit = textEdit;

    // Create our own private cursor, separate from the UI cursor
    // so user selections don't interfere with our text insertions
    m_insertionCursor = QTextCursor(m_textEdit->document());
    m_insertionCursor.movePosition(QTextCursor::End);
}

void TextOutputAdapter::appendString(const QString &string) {
    QScrollBar *scrollBar = m_textEdit->verticalScrollBar();
    const bool wasAtBottom = (scrollBar->value() >= scrollBar->maximum());
    auto format = QTextCharFormat{};

    const int size = string.size();
    for (int i = 0; i < size; ++i) {
        const QChar c = string[i];
        const QChar n = (i + 1) < size ? string[i + 1] : static_cast<QChar>(0);

        // Handle CRLF by advancing to the LF, which is handled below
        if (c == Ascii::CR && n == Ascii::LF) {
            continue;
        }

        // Handle LF
        if (c == Ascii::LF) {
            m_insertionCursor.movePosition(QTextCursor::End);
            m_insertionCursor.insertBlock();
            continue;
        }

        // Handle CR, next character not LF
        if (c == Ascii::CR) {
            m_insertionCursor.movePosition(QTextCursor::StartOfLine);
            continue;
        }

        // handle ESC basic sequences
        if (c == Ascii::ESC && n == Ascii::SCREEN_CMD_START) {
            auto j = 2;
            QString cmd = "";
            while (string[i + j] != Ascii::SCREEN_CMD_END && i + j < string.length()) {
                cmd.append(string[i + j]);
                j++;
            }

            i += j;

            if (string[i] != Ascii::SCREEN_CMD_END) {
                // no valid or supported ESC sequence
                printf("!!! %s < %d %d\n", cmd.toStdString().c_str(), i, j);
                continue;
            }

            format = decodeVT100Command(cmd, format);
            continue;
        }

        // Insert characters from index i, up to but excluding the next
        // CR or LF, as a literal string
        int lastToInsert = i;
        for (int j = i; j < size; ++j) {
            const QChar charJ = string[j];
            if (charJ == Ascii::CR || charJ == Ascii::LF || charJ == Ascii::ESC) {
                break;
            }
            lastToInsert = j;
        }
        const int insertionSize = lastToInsert - i + 1;
        const QString substring = string.mid(i, insertionSize);
        if (!m_insertionCursor.atEnd()) {
            // This means a CR was previously used. We need to select
            // the same number of characters as we're inserting, so the
            // text is overwritten.
            m_insertionCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, insertionSize);
        }
        m_insertionCursor.insertText(substring, format);
        i = lastToInsert;
    }

    if (wasAtBottom) {
        m_textEdit->verticalScrollBar()->setValue(m_textEdit->verticalScrollBar()->maximum());
    }
}

QTextCharFormat &TextOutputAdapter::decodeVT100Command(const QString &string, QTextCharFormat &format) {
    // reset
    if (string == "0") {
        format = QTextCharFormat{};
        return format;
    }

    if (string == "1") {
        format.font().setBold(true);
        return format;
    }

    match_foreground("30", "#000000");
    match_foreground("31", "#AA0000");
    match_foreground("32", "#00AA00");
    match_foreground("33", "#AAAA00");
    match_foreground("34", "#0000AA");
    match_foreground("35", "#8800AA");
    match_foreground("36", "#00AAAA");
    match_foreground("37", "#AAAAAA");

    match_background("40", "#000000");
    match_background("41", "#AA0000");
    match_background("42", "#00AA00");
    match_background("43", "#AAAA00");
    match_background("44", "#0000AA");
    match_background("45", "#8800AA");
    match_background("46", "#00AAAA");
    match_background("47", "#AAAAAA");

    match_foreground("90", "#666666");
    match_foreground("91", "#FF0000");
    match_foreground("92", "#00FF00");
    match_foreground("93", "#FFFF00");
    match_foreground("94", "#0000FF");
    match_foreground("95", "#AA00FF");
    match_foreground("96", "#00FFFF");
    match_foreground("97", "#FFFFFF");

    match_background("100", "#666666");
    match_background("101", "#FF0000");
    match_background("102", "#00FF00");
    match_background("103", "#FFFF00");
    match_background("104", "#0000FF");
    match_background("105", "#AA00FF");
    match_background("106", "#00FFFF");
    match_background("107", "#FFFFFF");

    printf("unknown esc: '%s'", string.toStdString().c_str());

    return format;
}

} // namespace View
} // namespace TrenchBroom
