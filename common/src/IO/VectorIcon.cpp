#include "VectorIcon.h"
#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSvgRenderer>
#include <QThread>
#include "IO/SystemPaths.h"

#define IMAGES_FOLDER "images"

namespace TrenchBroom::IO {
/* ------------------------------------------------------------------------------------------- */

std::map<QString, SVGIcon *> IconCache::iconCache{};

bool IconCache::isCached(const QString &path) {
    return iconCache.count(path) > 0;
}


void IconCache::push(const QString &path, SVGIcon *icon) {
    iconCache[path] = icon;
}


SVGIcon *IconCache::pop(const QString &path) {
    return iconCache[path];
}

void IconCache::resetCache() {
    iconCache.clear();
}


/* ------------------------------------------------------------------------------------------- */

SVGIcon *SVGIcon::fromPath(const std::filesystem::path &path) {
    const auto pathStr = filePathToString(path);

    if (IconCache::isCached(pathStr)) {
        return IconCache::pop(pathStr);
    }

    const auto svgIcon = new SVGIcon{path};
    IconCache::push(pathStr, svgIcon);

    return svgIcon;
}


SVGIcon::SVGIcon(const std::filesystem::path &path) : m_path(path) {
}

bool SVGIcon::grayscale() {
    return m_grayscale;
}

bool SVGIcon::normalized() {
    return m_normalized;
}

bool SVGIcon::isVariant() const {
    return !iconPath(IconOn).isEmpty() &&
           !iconPath(IconOff).isEmpty();
}

bool SVGIcon::isSingle() const {
    return !iconPath(Single).isEmpty();
}

bool SVGIcon::isValid() const {
    return isSingle() ^ isVariant();
}

void SVGIcon::setGrayscale(bool grayscale) {
    m_grayscale = grayscale;
}

void SVGIcon::setNormalized(bool normalized) {
    m_normalized = normalized;
}

void SVGIcon::renderVariant(const IconVariant variant = Single) {
    QSvgRenderer svgRenderer(iconPath(variant));

    for (const QSize &size : m_sizes) {
        QImage image(size, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        svgRenderer.render(&painter);

        QImage normalizedImage = normalizeBrightness(image);
        m_icon.addPixmap(QPixmap::fromImage(normalizedImage));
    }
}


double SVGIcon::calculateAverageBrightness(const QImage &image) {
    int width = image.width();
    int height = image.height();
    double totalBrightness = 0.0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            QRgb pixel = image.pixel(x, y);
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);

            // Calculate brightness using luminance formula
            double brightness = 0.299 * r + 0.587 * g + 0.114 * b;
            totalBrightness += brightness;
        }
    }

    // Calculate average brightness
    return totalBrightness / (width * height);
}


QImage SVGIcon::normalizeBrightness(const QImage &image) {
    double currentBrightness = calculateAverageBrightness(image);
    double scaleFactor = (m_targetBrightness / currentBrightness) * m_adjustmentFactor;

    QImage normalizedImage = image.convertToFormat(QImage::Format_RGBA8888);
    int width = normalizedImage.width();
    int height = normalizedImage.height();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            QRgb pixel = normalizedImage.pixel(x, y);
            int r = std::min(int(qRed(pixel) * scaleFactor), 255);
            int g = std::min(int(qGreen(pixel) * scaleFactor), 255);
            int b = std::min(int(qBlue(pixel) * scaleFactor), 255);
            int a = qAlpha(pixel); // Preserve the alpha channel

            normalizedImage.setPixel(x, y, qRgba(r, g, b, a));
        }
    }

    return normalizedImage;
}


const QIcon &SVGIcon::icon() const {
    return m_icon;
}


const QList<QSize> &SVGIcon::sizes() const {
    return m_sizes;
}


void SVGIcon::setSizes(const QList<QSize> &Sizes) {
    m_sizes = Sizes;
}


double SVGIcon::targetBrightness() const {
    return m_targetBrightness;
}


void SVGIcon::setTargetBrightness(double TargetBrightness) {
    m_targetBrightness = TargetBrightness;
}


double SVGIcon::adjustmentFactor() const {
    return m_adjustmentFactor;
}


void SVGIcon::setAdjustmentFactor(double AdjustmentFactor) {
    m_adjustmentFactor = AdjustmentFactor;
}

QString SVGIcon::iconPath(const IconVariant variant = Single) const {
    if (variant == Single) {
        return filePathToString(m_path);
    }

    if (variant == IconOn) {
        return filePathToString(m_path.parent_path() / m_path.stem() += SVG_OFF_POSTFIX);
    }

    if (variant == IconOff) {
        return filePathToString(m_path.parent_path() / m_path.stem() += SVG_ON_POSTFIX);
    }

    return QString{};
}

QImage SVGIcon::convertToGrayscale(const QImage &image) {
    QImage grayscaleImage = image.convertToFormat(QImage::Format_RGBA8888);

    int width = grayscaleImage.width();
    int height = grayscaleImage.height();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            QRgb pixel = grayscaleImage.pixel(x, y);
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);
            int a = qAlpha(pixel); // Preserve the alpha channel

            // Calculate the grayscale value using luminance formula
            int gray = qGray(r, g, b);

            // Set the grayscale pixel, preserving the alpha channel
            grayscaleImage.setPixel(x, y, qRgba(gray, gray, gray, a));
        }
    }

    return grayscaleImage;
}


QString SVGIcon::filePathToString(const std::filesystem::path &imagePath) {
    const auto fullPath = imagePath.is_absolute() ? imagePath : SystemPaths::findResourceFile("images" / imagePath);
    return QString::fromStdString(fullPath);
}


/* ------------------------------------------------------------------------------------------- */
}
