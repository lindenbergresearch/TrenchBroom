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

#include <QProxyStyle>

namespace TrenchBroom::View {
// Keep these radii in sync with the stylesheet through the QSS replacements.
class TrenchBroomProxyStyle : public QProxyStyle {
public:
    static constexpr int MenuCornerRadius = 12;
    static constexpr int ToolTipCornerRadius = 8;

    explicit TrenchBroomProxyStyle(const QString& key);
    explicit TrenchBroomProxyStyle(QStyle* style = nullptr);

    using QProxyStyle::polish;
    using QProxyStyle::unpolish;
    void polish(QWidget* widget) override;
    void unpolish(QWidget* widget) override;
    int styleHint(StyleHint hint, const QStyleOption* option = nullptr,
        const QWidget* widget = nullptr, QStyleHintReturn* returnData = nullptr) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
        const QWidget* widget = nullptr) const override;
    QPixmap generatedIconPixmap(QIcon::Mode mode, const QPixmap& pixmap, const QStyleOption* option) const override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};
} // namespace TrenchBroom::View
