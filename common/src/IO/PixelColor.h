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

#include <QColor>

namespace TrenchBroom::IO {

/**
 * Pixel Data Struct.
 */
class PixelColor {
  public:
    explicit PixelColor(const QColor &color);

    operator QColor();

    operator QRgb();

    qreal red();

    qreal green();

    qreal blue();

    qreal alpha();

    qreal hue();

    qreal saturation();

    qreal lightness();

    PixelColor &setHue(qreal value);

    PixelColor &setSaturation(qreal value);

    PixelColor &setLightness(qreal value);

    PixelColor &setRed(qreal value);

    PixelColor &setGreen(qreal value);

    PixelColor &setBlue(qreal value);

    PixelColor &setAlpha(qreal value);

    PixelColor operator*(double other);

    PixelColor operator+(PixelColor &other);

  private:
    QColor m_color; // underlying QColor
};


}
