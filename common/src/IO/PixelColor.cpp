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
#include "PixelColor.h"

namespace TrenchBroom::IO {

PixelColor::PixelColor(const QColor &color) : m_color(color) {}

PixelColor::operator QColor() {
    return m_color;
}

PixelColor::operator QRgb() {
    return m_color.rgb();
}

qreal PixelColor::red() { return m_color.redF(); }

qreal PixelColor::green() { return m_color.greenF(); }

qreal PixelColor::blue() { return m_color.blueF(); }

qreal PixelColor::alpha() { return m_color.alphaF(); }

qreal PixelColor::hue() { return m_color.hueF(); }

qreal PixelColor::saturation() { return m_color.saturationF(); }

qreal PixelColor::lightness() { return m_color.lightnessF(); }

PixelColor &PixelColor::setHue(qreal value) {
    m_color.setHslF(value, m_color.saturationF(), m_color.lightnessF(), m_color.alphaF());
    return *this;
}

PixelColor &PixelColor::setSaturation(qreal value) {
    m_color.setHslF(m_color.hueF(), value, m_color.lightnessF(), m_color.alphaF());
    return *this;
}

PixelColor &PixelColor::setLightness(qreal value) {
    m_color.setHslF(m_color.hueF(), m_color.saturationF(), value, m_color.alphaF());
    return *this;
}

PixelColor &PixelColor::setRed(qreal value) {
    m_color.setRedF(value);
    return *this;
}

PixelColor &PixelColor::setGreen(qreal value) {
    m_color.setGreenF(value);
    return *this;
}

PixelColor &PixelColor::setBlue(qreal value) {
    m_color.setBlueF(value);
    return *this;
}

PixelColor &PixelColor::setAlpha(qreal value) {
    m_color.setAlphaF(value);
    return *this;
}

PixelColor PixelColor::operator*(double other) {
    QColor tmp_color = m_color;

    tmp_color.setRedF(red() * other);
    tmp_color.setGreenF(green() * other);
    tmp_color.setBlueF(blue() * other);

    return PixelColor(tmp_color);
}

PixelColor PixelColor::operator+(PixelColor &other) {
    return PixelColor{
        QColor::fromRgbF(
            red() + other.red(),
            green() + other.green(),
            blue() + other.blue(),
            other.alpha()
        )
    };
}
}
