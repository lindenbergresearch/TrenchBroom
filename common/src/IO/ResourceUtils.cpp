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

#include "Assets/Texture.h"
#include "Ensure.h"
#include "IO/File.h"
#include "IO/FileSystem.h"
#include "IO/PathQt.h"
#include "IO/ReadFreeImageTexture.h"
#include "IO/SystemPaths.h"
#include "IO/VectorIcon.h"
#include "Logger.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QPixmapCache>
#include <QThread>

#include "kdl/set_temp.h"

namespace TrenchBroom::IO {
Assets::Texture loadDefaultTexture(const FileSystem &fs, std::string name, Logger &logger) {
    // recursion guard
    static auto executing = false;
    if (!executing) {
        const auto set_executing = kdl::set_temp{executing};
        return fs.openFile(DEFAULT_TEXTURE_FILE).and_then(
            [&](auto file) {
                auto reader = file->reader().buffer();
                return readFreeImageTexture(name, reader);
            }
        ).transform_error(
            [&](auto e) {
                logger.error() << "Could not load default texture: " << e.msg;
                return Assets::Texture{std::move(name), 32, 32};
            }
        ).value();
    }

    return Assets::Texture{std::move(name), 32, 32};
}

QPixmap loadPixmapResource(const std::filesystem::path &imagePath) {
    // only allow main thread as caller
    ensure(qApp && qApp->thread() == QThread::currentThread(), "loadPixmapResource requires the GUI thread");

    // check for valid path
    if (imagePath.empty()) return {};

    // extract directory
    const auto path = QDir::cleanPath(pathAsQString(imagePath));
    if (QFileInfo{path}.suffix().compare("svg", Qt::CaseInsensitive) == 0) {
        // Qt6's QPixmap SVG loader defaults to density 1. Use the shared vector
        // engine so even bitmap callers receive a raster at the current DPI.
        auto vectorPath = imagePath;
        if (!imagePath.is_absolute() && !path.startsWith(":/")) {
            for (const auto& directory : ResourceSearchPaths) {
                const auto resolved = SystemPaths::findResourceFile(std::filesystem::path{directory} / imagePath);
                if (!resolved.empty()) {
                    vectorPath = resolved;
                    break;
                }
            }
        }
        const auto vector = VectorIcon{vectorPath};
        return vector.icon().pixmap(vector.defaultSize());
    }
    const auto cacheKey = "tb/resource/" + path;

    QPixmap pixmap;
    if (QPixmapCache::find(cacheKey, &pixmap)) {
        return pixmap;
    }

    if (imagePath.is_absolute() || path.startsWith(":/")) {
        pixmap.load(path);
    } else {
        for (const auto &resPath : ResourceSearchPaths) {
            auto res = SystemPaths::findResourceFile(std::filesystem::path(resPath) / imagePath);
            if (!res.empty()) {
                pixmap.load(pathAsQString(res));
                break;
            }
        }
    }

    if (!pixmap.isNull()) {
        defaultQtLogger.trace() << "Add new pixmap to cache: " << cacheKey << " | " << imagePath;
        QPixmapCache::insert(cacheKey, pixmap);
    } else {
        defaultQtLogger.warn() << "Unable to load pixmap resource: " << imagePath;
    }

    return pixmap;
}

QIcon loadSVGIcon(const std::filesystem::path &imagePath, const int size) {
    // Keep the size-taking API for callers; a vector icon has no fixed raster
    // size. QIcon::paint/pixmap and the widget's iconSize specify the output.
    if (size <= 0) return {};
    return VectorIcon(imagePath).icon();
}

QCursor loadCursorFile(const std::filesystem::path &imagePath) {


    return QCursor();
}
} // namespace TrenchBroom::IO
