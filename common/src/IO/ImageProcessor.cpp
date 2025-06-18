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

#include "ImageProcessor.h"
#include <QtMath>

namespace TrenchBroom::IO {

void ImagePixelProcessor::process() {
    preProcess();

    // Convert to target format if specified
    if (m_targetFormat != QImage::Format_Invalid && m_sourceImage.format() != m_targetFormat) {
        m_targetImage = m_sourceImage.convertToFormat(m_targetFormat);
    } else {
        m_targetImage = m_sourceImage;
    }


    // Iterate over each pixel
    for (int y = 0; y < m_targetImage.height(); ++y) {
        for (int x = 0; x < m_targetImage.width(); ++x) {
            QPoint location{x, y};
            PixelColor pixelColor{m_targetImage.pixelColor(location)};

            // Apply custom pixel processing
            m_targetImage.setPixelColor(location, processPixel(location, pixelColor));
        }
    }

    postProcess();
    m_processed = true;
}

QImage &ImagePixelProcessor::process(QImage &image) {
    m_sourceImage = image;
    process();

    return m_targetImage;
}

ImagePixelProcessor::ImagePixelProcessor(const QImage &image, const QImage::Format targetFormat) :
    m_sourceImage(image), m_targetFormat(targetFormat == QImage::Format_Invalid ? image.format() : targetFormat), m_processed(false) {
}

QImage &ImagePixelProcessor::run() {
    process();

    return m_targetImage;
}

/* ------------------------------------------------------------------------------------------- */


QColor GrayScaleProcessor::processPixel(const QPoint &location, PixelColor &pixelColor) {
    return pixelColor.setSaturation(0);
}

GrayScaleProcessor::GrayScaleProcessor(const QImage &image, const QImage::Format targetFormat) : ImagePixelProcessor(image, targetFormat) {}

void GrayScaleProcessor::preProcess() {

}

void GrayScaleProcessor::postProcess() {

}

/* ------------------------------------------------------------------------------------------- */

ResizeSharpenProcessor::ResizeSharpenProcessor(const QImage &image, QImage::Format targetFormat, float sharpenStrength) :
    ImagePixelProcessor(image, targetFormat), m_sharpenStrength(sharpenStrength) {
    kernel = {
        {0, -1, 0},
        {-1, 5, -1},
        {0, -1, 0}
    };
}

QColor ResizeSharpenProcessor::processPixel(const QPoint &location, PixelColor &pixelData) {
    int x = location.x();
    int y = location.y();

    double r = 0, g = 0, b = 0;

    for (int ky = -1; ky <= 1; ++ky) {
        for (int kx = -1; kx <= 1; ++kx) {
            int nx = x + kx;
            int ny = y + ky;

            if (nx >= 0 && ny >= 0 && nx < width() && ny < height()) {
                QColor neighborColor = image().pixelColor(nx, ny);
                auto weight = kernel[ky + 1][kx + 1];

                r += neighborColor.redF() * weight;
                g += neighborColor.greenF() * weight;
                b += neighborColor.blueF() * weight;
            }
        }
    }

    r = qBound(0.0, r * m_sharpenStrength, 1.0);
    g = qBound(0.0, g * m_sharpenStrength, 1.0);
    b = qBound(0.0, b * m_sharpenStrength, 1.0);

    return QColor::fromRgbF(r, g, b, pixelData.alpha());
}

double ResizeSharpenProcessor::sharpenStrength() const {
    return m_sharpenStrength;
}

void ResizeSharpenProcessor::setSharpenStrength(double sharpenStrength) {
    m_sharpenStrength = sharpenStrength;
}

void ResizeSharpenProcessor::preProcess() {}

void ResizeSharpenProcessor::postProcess() {}

/* ------------------------------------------------------------------------------------------- */

ImageStatisticsAnalyser::ImageStatisticsAnalyser(const QImage &image, QImage::Format targetFormat) :
    ImagePixelProcessor(image, targetFormat) {}

QColor ImageStatisticsAnalyser::processPixel(const QPoint &location, PixelColor &pixelData) {
    double lum = 0.299 * pixelData.red() + 0.587 * pixelData.green() + 0.114 * pixelData.blue();
    double light = pixelData.lightness();
    double bright = (pixelData.red() + pixelData.green() + pixelData.blue()) / 3.;

    m_avgLuminance += lum * pixelData.alpha();
    m_avgLightness += light * pixelData.alpha();
    m_avgBrightness += bright * pixelData.alpha();

    m_weightedSum += pixelData.alpha();

    if (!m_colors.contains(pixelData)) {
        m_colors[pixelData] = 1;
    } else {
        m_colors[pixelData]++;
    }

    rgbColor.rgb[0] = static_cast<uint8_t>(pixelData.red());
    rgbColor.rgb[1] = static_cast<uint8_t>(pixelData.green());
    rgbColor.rgb[2] = static_cast<uint8_t>(pixelData.blue());

    auto colorCode = rgbColor.color;

    if (!m_rgbColors.contains(colorCode)) {
        m_rgbColors[colorCode] = 1;
    } else {
        m_rgbColors[colorCode]++;
    }

    return pixelData;
}

void ImageStatisticsAnalyser::preProcess() {
    m_avgBrightness = 0;
    m_avgLightness = 0;
    m_avgLuminance = 0;
    m_weightedSum = 0;
}

void ImageStatisticsAnalyser::postProcess() {
    m_avgLuminance = m_avgLuminance / m_weightedSum;
    m_avgLightness = m_avgLightness / m_weightedSum;
    m_avgBrightness = m_avgBrightness / m_weightedSum;
}

double ImageStatisticsAnalyser::avgBrightness() const {
    return m_avgBrightness;
}

double ImageStatisticsAnalyser::avgLightness() const {
    return m_avgLightness;
}

double ImageStatisticsAnalyser::avgLuminance() const {
    return m_avgLuminance;
}

int ImageStatisticsAnalyser::colorCount() {
    return m_colors.size();
}

int ImageStatisticsAnalyser::rgbColorCount() {
    return m_rgbColors.size();
}

/* ------------------------------------------------------------------------------------------- */


QColor BrightnessNormalizer::processPixel(const QPoint &location, PixelColor &pixelData) {
    return pixelData * m_correctionFactor;
}

void BrightnessNormalizer::preProcess() {
    m_correctionFactor = m_targetBrightness / m_averageBrightness;
}

void BrightnessNormalizer::postProcess() {}

BrightnessNormalizer::BrightnessNormalizer(const QImage &image, const QImage::Format targetFormat, double averageBrightness, double targetBrightness) :
    ImagePixelProcessor(image, targetFormat), m_targetBrightness(targetBrightness), m_averageBrightness(averageBrightness) {}

BrightnessNormalizer::BrightnessNormalizer(double targetBrightness, double averageBrightness) :
    ImagePixelProcessor(), m_targetBrightness(targetBrightness), m_averageBrightness(averageBrightness) {}

/* ------------------------------------------------------------------------------------------- */

QColor RecolorImageProcessor::processPixel(const QPoint &location, PixelColor &pixelData) {
    auto target = PixelColor{m_color} * m_intensity;
    auto sourceColor = pixelData * (1.0 - m_intensity);

    return target + sourceColor;
}

void RecolorImageProcessor::preProcess() {}

void RecolorImageProcessor::postProcess() {}

RecolorImageProcessor::RecolorImageProcessor(const QImage &image, const QImage::Format targetFormat, const QColor &color, double intensity)
    : ImagePixelProcessor(image, targetFormat), m_color(color), m_intensity(intensity) {}

RecolorImageProcessor::RecolorImageProcessor(const QColor &color, double intensity)
    : ImagePixelProcessor(), m_color(color), m_intensity(intensity) {}
}
