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

#include <filesystem>
#include <memory>
#include <string>
#include <QIcon>
#include <QPixmap>

namespace TrenchBroom {
class Logger;
}

namespace TrenchBroom::Assets {
class Texture;
}

namespace TrenchBroom::IO {
class FileSystem;

// default texture
static constexpr auto DEFAULT_TEXTURE_FILE = "textures/__TB_empty.png";

/**
 * Valid resource paths for pixmap loading.
 */
static const std::vector<std::string> ResourceSearchPaths = {
    "images",
    "cursor"
};

/**
 * Loads a default texture file from the given file system. If the default texture cannot
 * be found or opened, an empty texture is returned.
 *
 * @param fs the file system used to locate the texture file
 * @param name the name of the texture to be returned
 * @return the default texture
 */
Assets::Texture loadDefaultTexture(const FileSystem &fs, std::string name, Logger &logger);

QPixmap loadPixmapResource(const std::filesystem::path &imagePath);

QCursor loadCursorFile(const std::filesystem::path &imagePath);

/**
 * Loads a scalable SVG icon. The size parameter is a compatibility hint; actual
 * raster size and pixel density are determined when QIcon is painted. Returned
 * values share ownership and remain valid after cache eviction.
 */
QIcon loadSVGIcon(const std::filesystem::path &imagePath, int size);

} // namespace TrenchBroom::IO
