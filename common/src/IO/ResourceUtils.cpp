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

#include "ResourceUtils.h"

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

#include "Assets/Texture.h"
#include "Ensure.h"
#include "Error.h"
#include "IO/File.h"
#include "IO/FileSystem.h"
#include "IO/PathQt.h"
#include "IO/ReadFreeImageTexture.h"
#include "IO/SystemPaths.h"
#include "Logger.h"

#include "kdl/result.h"
#include "kdl/set_temp.h"

#include <map>
#include <string>

namespace TrenchBroom::IO {

Assets::Texture loadDefaultTexture(const FileSystem &fs, std::string name, Logger &logger) {
    // recursion guard
    static auto executing = false;
    if (!executing) {
        const auto set_executing = kdl::set_temp{executing};

        return fs.openFile("textures/__TB_empty.png").and_then([&](auto file) {
            auto reader = file->reader().buffer();
            return readFreeImageTexture(name, reader);
        }).transform_error([&](auto e) {
            logger.error() << "Could not load default texture: " << e.msg;
            return Assets::Texture{std::move(name), 32, 32};
        }).value();
    } else {
        logger.error() << "Could not load default texture";
    }
    return Assets::Texture{std::move(name), 32, 32};
}


/* ------------------------------------------------------------------------------------------- */


static QString imagePathToString(const std::filesystem::path &imagePath) {
    const auto fullPath =
        imagePath.is_absolute() ?
        imagePath :
        SystemPaths::findResourceFile("images" / imagePath);

    return pathAsQString(fullPath);
}


QPixmap loadPixmapResource(const std::filesystem::path &imagePath) {
    return QPixmap{imagePathToString(imagePath)};
}


static QImage recolorSvgIcon(const QImage &image, float boost = 1.0f, float alphaFact = 1.f) {
    auto disabledImage = image.convertToFormat(QImage::Format_ARGB32);

    const auto w = disabledImage.width();
    const auto h = disabledImage.height();

    for (int y = 0; y < h; ++y) {
        auto *row = reinterpret_cast<QRgb *>(disabledImage.scanLine(y));

        for (int x = 0; x < w; ++x) {
            const auto pixel = row[x];

            const auto red = qRed(pixel);
            const auto blue = qGreen(pixel);
            const auto green = qBlue(pixel);
            const int alpha = qAlpha(pixel) > 0 ? qRound(alphaFact * 255.f) : 0;

            row[x] = qRgba(std::min(qRound((float) red * boost), 255), std::min(qRound((float) blue * boost), 255), std::min(qRound((float) green * boost), 255), alpha);
        }
    }

    return disabledImage;
}


static float analyseBrightness(const QImage &image) {
    auto qimage = image.convertToFormat(QImage::Format_ARGB32);

    const auto w = qimage.width();
    const auto h = qimage.height();

    float delta = 0;

    for (int y = 0; y < h; ++y) {
        auto *row = reinterpret_cast<QRgb *>(qimage.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const auto pixel = row[x];

            float uniq = float(qRed(pixel) + qGreen(pixel) + qBlue(pixel)) / 3.f;
            const float alpha = float(qAlpha(pixel)) / 255.f;

            if (uniq >= 0) {
                delta += uniq * alpha;
            }
        }
    }

    return (delta == 0) ? 0 : delta / float(w * h);
}


static QImage normaliseBrightness(const QImage &image, float absBrightness, float alpha = 1.f) {
    const auto theta = 1.f + (analyseBrightness(image) / 255.f);
    return recolorSvgIcon(image, theta * absBrightness, alpha);
}


static QImage makeGrayscale(const QImage &image, float alpha = 1.f) {
    auto disabledImage = image.convertToFormat(QImage::Format_ARGB32);

    const auto w = disabledImage.width();
    const auto h = disabledImage.height();

    for (int y = 0; y < h; ++y) {
        auto *row = reinterpret_cast<QRgb *>(disabledImage.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const auto oldPixel = row[x];
            const auto grey = (qRed(oldPixel) + qGreen(oldPixel) + qBlue(oldPixel)) / 3;
            row[x] = qRgba(grey, grey, grey, qRound(alpha * (float) qAlpha(oldPixel)));
        }
    }

    return disabledImage;
}


static QImage createDisabledState(const QImage &image) {
    return makeGrayscale(image, 0.25f);
}


static void renderSvgToIcon(QSvgRenderer &svgSource, QIcon &icon, const QIcon::State state, const bool invert, const qreal devicePixelRatio, int size, float opacity = DefaultIconAlpha) {
    if (!svgSource.isValid()) {
        return;
    }

    auto image = normaliseBrightness(QImage{
        int(size * devicePixelRatio * OverSampleFactor), int(size * devicePixelRatio * OverSampleFactor), QImage::Format_ARGB32
    }, 1.f, opacity);

    image.fill(Qt::transparent);

    {
        auto paint = QPainter{&image};
        paint.setOpacity(opacity);
        svgSource.render(&paint);
    }

    image.setDevicePixelRatio(devicePixelRatio);

    if (invert && image.isGrayscale()) {
        image.invertPixels();
    }

    icon.addPixmap(QPixmap::fromImage(image).scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation), QIcon::Normal, state);
    icon.addPixmap(QPixmap::fromImage(createDisabledState(image)).scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation), QIcon::Disabled, state);
}


QIcon loadSVGIcon(const std::filesystem::path &imagePath, int size) {
    // Simple caching layer.
    // Without it, the .svg files would be read from disk and decoded each time this is
    // called, which is slow. We never evict from the cache which is assumed to be OK
    // because this is just used for icons and there's a relatively small set of them.
    ensure(qApp->thread() == QThread::currentThread(), "loadIconResourceQt can only be used on the main thread");

    static auto cache = std::map<std::filesystem::path, QIcon>{};
    if (const auto it = cache.find(imagePath); it != cache.end()) {
        return it->second;
    }

    const auto invert =
#ifdef DISABLE_RECOLORING_ICON
        false;
#else
    const auto palette = QPalette{};
  const auto windowColor = palette.color(QPalette::Active, QPalette::Window);
  windowColor.lightness() <= 127;
#endif

    // Cache miss, load the icon
    auto result = QIcon{};

    if (!imagePath.empty()) {
        const auto onPath = imagePathToString(imagePath.parent_path() / imagePath.stem() += "_on.svg");
        const auto offPath = imagePathToString(imagePath.parent_path() / imagePath.stem() += "_off.svg");
        const auto imagePathString = imagePathToString(imagePath);

        if (!onPath.isEmpty() && !offPath.isEmpty()) {
            auto onRenderer = QSvgRenderer{onPath};
            if (!onRenderer.isValid()) {
                qWarning() << "Failed to load SVG " << onPath;
            }

            auto offRenderer = QSvgRenderer{offPath};
            if (!offRenderer.isValid()) {
                qWarning() << "Failed to load SVG " << offPath;
            }

            renderSvgToIcon(onRenderer, result, QIcon::On, invert, 1.0, size);
            renderSvgToIcon(onRenderer, result, QIcon::On, invert, 2.0, size);
            renderSvgToIcon(offRenderer, result, QIcon::Off, invert, 1.0, size);
            renderSvgToIcon(offRenderer, result, QIcon::Off, invert, 2.0, size);
        } else if (!imagePathString.isEmpty()) {
            auto renderer = QSvgRenderer{imagePathString};
            if (!renderer.isValid()) {
                qWarning() << "Failed to load SVG " << imagePathString;
            }

            renderSvgToIcon(renderer, result, QIcon::Off, invert, 1.0, size);
            renderSvgToIcon(renderer, result, QIcon::Off, invert, 2.0, size);
        } else {
            qWarning() << "Couldn't find image for path: " << pathAsQString(imagePath);
        }
    }

    cache[imagePath] = result;
    return result;
}

} // namespace TrenchBroom::IO
