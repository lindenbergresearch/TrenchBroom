/*
 Copyright (C) 2021 Kristian Duske

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

#include "View/Document/MapTextEncoding.h"
#include "View/Theme/QtUtils.h"
#include "View/Compilation/TextOutputAdapter.h"
#include "View/Widgets/Info/SemanticTextEdit.h"
#include "IO/SystemPaths.h"
#include "IO/PathQt.h"

#include <QApplication>
#include <QKeySequence>
#include <QTextCursor>
#include <QTemporaryDir>

#include "Catch2.h"

namespace TrenchBroom::View {
TEST_CASE("Portable paths use the explicit executable directory and reset old overrides", "[qt6][settings]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const auto wasPortable = IO::SystemPaths::isPortable();
    const auto previousDirectory = IO::SystemPaths::userDataDirectory();
    const auto executableDirectory = IO::pathFromQString(folder.path()) / "application";
    IO::SystemPaths::setPortable(true, executableDirectory);
    const auto explicitDirectory = IO::SystemPaths::userDataDirectory();
    IO::SystemPaths::setPortable();
    const auto defaultDirectory = IO::SystemPaths::userDataDirectory();
    IO::SystemPaths::setPortable(wasPortable, wasPortable ? previousDirectory.parent_path() : std::filesystem::path{});
    CHECK(explicitDirectory == executableDirectory / "config");
    CHECK(defaultDirectory == IO::SystemPaths::appDirectory() / "config");
}

TEST_CASE("Map Latin1 encoding preserves all byte values including embedded nulls", "[qt6][encoding]") {
    auto bytes = std::string{};
    for (int i = 0; i < 256; ++i) bytes.push_back(char(i));
    const auto text = mapStringToUnicode(MapTextEncoding::Iso88591, bytes);
    REQUIRE(text.size() == 256);
    for (int i = 0; i < 256; ++i) CHECK(text[i].unicode() == i);
    CHECK(mapStringFromUnicode(MapTextEncoding::Iso88591, text) == bytes);
}

TEST_CASE("Map UTF8 encoding preserves non-BMP text and embedded nulls", "[qt6][encoding]") {
    auto text = QString::fromUtf8("Größe: äöüß, 東京, 🪵");
    text += QChar{u'\0'};
    text += QStringLiteral("targetname");
    const auto bytes = text.toUtf8().toStdString();
    CHECK(mapStringToUnicode(MapTextEncoding::Utf8, bytes) == text);
    CHECK(mapStringFromUnicode(MapTextEncoding::Utf8, text) == bytes);
    CHECK(mapStringToUnicode(MapTextEncoding::Utf8, {}).isEmpty());
    CHECK(mapStringFromUnicode(MapTextEncoding::Utf8, {}).empty());
}

TEST_CASE("Map value conversion preserves Qt5 handling of incomplete characters", "[qt6][encoding]") {
    const auto bytes = QByteArray{"target_\xE2\x82"};
    CHECK(mapStringToUnicode(MapTextEncoding::Utf8, bytes.toStdString()) == QString::fromUtf8(bytes));
    auto text = QStringLiteral("target_");
    text += QChar{char16_t(0xD800)};
    CHECK(mapStringFromUnicode(MapTextEncoding::Utf8, text) == text.toUtf8().toStdString());
}

TEST_CASE("Quake map encoding retains the native locale conversion", "[qt6][encoding]") {
    const auto text = QString::fromUtf8("map_name äöü");
    const auto bytes = text.toLocal8Bit();
    CHECK(mapStringToUnicode(MapTextEncoding::Quake, bytes.toStdString()) == QString::fromLocal8Bit(bytes));
    CHECK(mapStringFromUnicode(MapTextEncoding::Quake, text) == bytes.toStdString());
}

TEST_CASE("Shortcut preferences round-trip multi-modifier key combinations", "[qt6][shortcuts]") {
    for (const auto combination : {
        Qt::CTRL | Qt::Key_O,
        Qt::CTRL | Qt::SHIFT | Qt::Key_Plus,
        Qt::ALT | Qt::Key_Return,
        Qt::META | Qt::ALT | Qt::Key_Space}) {
        const auto sequence = QKeySequence{combination};
        const auto saved = sequence.toString(QKeySequence::PortableText);
        const auto restored = QKeySequence::fromString(saved, QKeySequence::PortableText);
        REQUIRE(restored.count() == 1);
        CHECK(restored[0] == combination);
        CHECK(restored[0].toCombined() == combination.toCombined());
    }
}

TEST_CASE("QSS message weights accept native Qt6 values and older personal styles", "[qt6][fonts]") {
    SemanticTextEdit edit;
    TextOutputAdapter output{&edit};
    edit.setStyleSheet("TrenchBroom--View--SemanticTextEdit { qproperty-systemFontWeight: 600; }");
    edit.ensurePolished();
    output.pushSystemMessage("Message", MessageStyle::System);
    const auto weight = [&] {
        QTextCursor cursor{edit.document()};
        cursor.setPosition(0);
        cursor.setPosition(1, QTextCursor::KeepAnchor);
        return cursor.charFormat().fontWeight();
    };
    CHECK(weight() == QFont::DemiBold);
    edit.setStyleSheet("TrenchBroom--View--SemanticTextEdit { qproperty-systemFontWeight: 75; }");
    for (int i = 0; i < 3; ++i) QApplication::processEvents();
    CHECK(weight() == QFont::Bold);
}
} // namespace TrenchBroom::View
