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

#include <QImage>
#include <QColor>
#include <QPoint>
#include <QMap>

#include "PixelColor.h"

namespace TrenchBroom::IO {

/**
 * @class ImagePixelProcessor
 * @brief Provides a framework for pixel-by-pixel image processing.
 *
 * This class allows custom image processing by overriding the `processPixel` method
 * to define specific behavior for individual pixels.
 */
class ImagePixelProcessor {
  public:
    /**
     * Constructor to initialize the processor with an image and an optional target format.
     *
     * @param image The input QImage to process.
     * @param targetFormat The format to which the image should be converted before processing.
     */
    explicit ImagePixelProcessor(const QImage &image = QImage{}, const QImage::Format targetFormat = QImage::Format::Format_Invalid);

    /**
     * Implicit conversation to a QImage.
     * If not yet processed it runs the pixel processor for easy cascading .
     * @return
     */
    operator QImage &() {
        if (!m_processed) process();
        return m_targetImage;
    }


    QImage operator^(ImagePixelProcessor &other) {
        m_sourceImage = other;
        if (!m_processed) process();
        return m_targetImage;
    }

    QImage operator^(ImagePixelProcessor *other) {
        m_sourceImage = other->m_sourceImage;
        if (!m_processed) process();
        return m_targetImage;
    }

    /**
     * Processes the stored image pixel by pixel.
     *
     * @return A new QImage with the processed pixels.
     */
    void process();

    /**
     * Processes a new image by replacing the stored image.
     *
     * @param image The new QImage to process.
     * @return A new QImage with the processed pixels.
     */
    QImage &process(QImage &image);

    QImage &run();

    /**
     * Shorthand operator to add the ability to use the image processor like a function call.
     *
     * @param image The image to process.
     * @return The processed image.
     */
    QImage operator()(QImage &image) {
        return process(image);
    }


  protected:
    /**
     * Virtual method to define custom pixel processing logic.
     *
     * @param location The pixel's coordinates in the image.
     * @param pixelData The detailed data of the current pixel.
     * @return The processed QColor for the pixel.
     */
    virtual QColor processPixel(const QPoint &location, PixelColor &pixelData) = 0;

    /**
     * Gets called before any processing is done.
     */
    virtual void preProcess() = 0;

    /**
     * Gets called after processing.
     */
    virtual void postProcess() = 0;

  public:
    int width() {
        return m_sourceImage.width();
    }

    int height() {
        return m_sourceImage.height();
    }

    size_t size() {
        return size_t(width() * height());
    }

    QImage &image() {
        return m_sourceImage;
    }

  private:
    QImage m_sourceImage;          // The input image to be processed
    QImage m_targetImage;          // The processed image
    QImage::Format m_targetFormat; // Optional target format for the image
    bool m_processed;              // Mark if the images has been processed
};

/* ------------------------------------------------------------------------------------------- */

using IPList = QList<ImagePixelProcessor *>;

class ImageProcessors {
  public:
    ImageProcessors(const QImage &sourceImage) :
        m_sourceImage(sourceImage), m_processed(false) {}

    operator QImage &() {
        if (!m_processed) process();
        return m_targetImage;
    }

    void add(ImagePixelProcessor *pixelProcessor) {
        m_ipList.push_back(pixelProcessor);
    }

    void process() {
        m_targetImage = m_sourceImage;
        for (auto *processor : m_ipList) {
            m_targetImage = processor->process(m_targetImage);
        }

        m_processed = true;
    }

    QImage &run(const QImage sourceImage) {
        m_sourceImage = sourceImage;

        process();

        return m_targetImage;
    }

    bool processed() const {
        return m_processed;
    }

    void setProcessed(bool processed) {
        m_processed = processed;
    }

    const QImage &sourceImage() const {
        return m_sourceImage;
    }

    void setSourceImage(const QImage &sourceImage) {
        m_sourceImage = sourceImage;
    }

    const QImage &targetImage() const {
        return m_targetImage;
    }

    void setTargetImage(const QImage &targetImage) {
        m_targetImage = targetImage;
    }

  private:
    IPList m_ipList;
    QImage m_targetImage;
    QImage m_sourceImage;
    bool m_processed;

};


/* ------------------------------------------------------------------------------------------- */


class GrayScaleProcessor : public ImagePixelProcessor {
  public:
    GrayScaleProcessor(const QImage &image = QImage{}, QImage::Format targetFormat = QImage::Format_Invalid);

  protected:
    QColor processPixel(const QPoint &location, PixelColor &pixelColor) override;

    void preProcess() override;

    void postProcess() override;
};

/* ------------------------------------------------------------------------------------------- */

class ResizeSharpenProcessor : public ImagePixelProcessor {
  public:
    ResizeSharpenProcessor(const QImage &image = QImage{}, QImage::Format targetFormat = QImage::Format_Invalid, float sharpenStrength = 1.2f);

    double sharpenStrength() const;

    void setSharpenStrength(double sharpenStrength);

  protected:
    QColor processPixel(const QPoint &location, PixelColor &pixelData) override;

    void preProcess() override;

    void postProcess() override;

  private:
    QVector<QVector<float>> kernel;
    double m_sharpenStrength;
};

/* ------------------------------------------------------------------------------------------- */

class ImageStatisticsAnalyser : public ImagePixelProcessor {
  public:
    ImageStatisticsAnalyser(const QImage &image = QImage{}, const QImage::Format targetFormat = QImage::Format_Invalid);

    double avgBrightness() const;

    double avgLightness() const;

    double avgLuminance() const;

    int colorCount();

    int rgbColorCount();

  private:

    union rgb {
      uint32_t color;
      uint8_t rgb[3];
    } rgbColor;

    double m_weightedSum;
    double m_avgBrightness;
    double m_avgLightness;
    double m_avgLuminance;
    QMap<QRgb, int> m_colors{};
    QMap<QRgb, uint32_t> m_rgbColors{};

  protected:
    QColor processPixel(const QPoint &location, PixelColor &pixelData) override;

    void preProcess() override;

    void postProcess() override;
};

/* ------------------------------------------------------------------------------------------- */

class BrightnessNormalizer : public ImagePixelProcessor {
  public:
    BrightnessNormalizer(const QImage &image = QImage{}, QImage::Format targetFormat = QImage::Format_Invalid, double averageBrightness = 0.0, double targetBrightness = 1.0);

    BrightnessNormalizer(double targetBrightness, double averageBrightness);

  protected:
    QColor processPixel(const QPoint &location, PixelColor &pixelData) override;

    void preProcess() override;

    void postProcess() override;

  private:
    double m_targetBrightness;
    double m_averageBrightness;
    double m_correctionFactor;
};

/* ------------------------------------------------------------------------------------------- */

class RecolorImageProcessor : public ImagePixelProcessor {
  public:
    RecolorImageProcessor(const QImage &image = QImage{}, QImage::Format targetFormat = QImage::Format_Invalid, const QColor &color = QColorConstants::White, double intensity = 0.0);

    RecolorImageProcessor(const QColor &color, double intensity);

  protected:
    QColor processPixel(const QPoint &location, PixelColor &pixelData) override;

    void preProcess() override;

    void postProcess() override;

  private:
    QColor m_color;
    double m_intensity;
};

}
