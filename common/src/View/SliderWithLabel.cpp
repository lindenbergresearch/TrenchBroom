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

#include "SliderWithLabel.h"

#include <QBoxLayout>
#include <QLabel>
#include <QSlider>

#include "View/QtUtils.h"
#include "View/ViewConstants.h"

#include <cmath> // for std::log10

namespace TrenchBroom {
namespace View {
SliderWithLabel::SliderWithLabel(const int minimum, const int maximum, const float factor, const QString &format, const int maxSliderWidth, QWidget *parent)
    : QWidget(parent), m_slider(createSlider(minimum, maximum)), m_label(new QLabel()), m_factor(factor), m_format(format) {
    // get maximum label bounding
    const auto min_size = m_label->fontMetrics().boundingRect(getValueLabel(minimum));
    const auto max_size = m_label->fontMetrics().boundingRect(getValueLabel(maximum));
    const auto max_width = std::max(min_size.width(), max_size.width()) + 1;

    m_label->setMinimumWidth(max_width);
    m_label->setAlignment(Qt::AlignRight);
    m_label->setText(getValueLabel(m_slider->value()));

    if (maxSliderWidth > 0) {
        setMinimumWidth(maxSliderWidth + LayoutConstants::MediumHMargin + max_width);
    }

    auto *layout = new QHBoxLayout();
    layout->setContentsMargins(QMargins());
    layout->setSpacing(LayoutConstants::MediumHMargin);
    layout->addWidget(m_slider, 1);
    layout->addWidget(m_label);
    setLayout(layout);

    connect(m_slider, &QSlider::valueChanged, this, &SliderWithLabel::valueChangedInternal);
}

int SliderWithLabel::value() const {
    return m_slider->value();
}

float SliderWithLabel::ratio() const {
    return getSliderRatio(m_slider);
}

void SliderWithLabel::setValue(const int value) {
    m_slider->setValue(value);
}

void SliderWithLabel::setRatio(const float ratio) {
    setSliderRatio(m_slider, ratio);
}

void SliderWithLabel::valueChangedInternal(const int value) {
    m_label->setText(getValueLabel(m_slider->value()));
    emit valueChanged(value);
}

QString SliderWithLabel::getValueLabel(const int value) {
    QString labelText;

    if (m_factor != 0) {
        // use decimal numbers if factor is set != zero
        float factorized = float(value) * m_factor;
        QString fmt = m_format.isEmpty() ? "%.2f" : m_format;
        labelText = labelText.sprintf(fmt.toStdString().c_str(), factorized);
    } else {
        // use int by default, if factor == zero
        QString fmt = m_format.isEmpty() ? "%d" : m_format;
        labelText = labelText.sprintf(fmt.toStdString().c_str(), value);;
    }

    return labelText;
}

void SliderWithLabel::setMaximumSliderWidth(const int width) {
    m_slider->setMaximumWidth(width);
    setMaximumWidth(width + LayoutConstants::MediumHMargin + m_label->minimumWidth());
}
} // namespace View
} // namespace TrenchBroom
