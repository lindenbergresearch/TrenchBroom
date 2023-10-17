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

#include <QWidget>
#include <QSlider>
#include <QStyleOptionSlider>
#include <QPainter>

class QLabel;

class QSlider;

namespace TrenchBroom {
namespace View {

class SliderWithLabel : public QWidget {
Q_OBJECT
private:
    QSlider *m_slider;
    QLabel *m_label;
    float m_factor;
    QString m_format;

public:
    SliderWithLabel(int minimum, int maximum, const float factor = 0.0, const QString &format = "%d", const int maxSliderWidth = 0, const int minLabelWidth = 0, QWidget *parent = nullptr);

    int value() const;

    float ratio() const;

    void setMaximumSliderWidth(const int width);

public slots:

    void setValue(int value);

    void setRatio(float ratio);

private slots:

    void valueChangedInternal(int value);

     QString getValueLabel(int value);

signals:

    void valueChanged(const int value);
};
} // namespace View
} // namespace TrenchBroom
