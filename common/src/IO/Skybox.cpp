/*
 Copyright (C) 2026 TrenchBroom contributors

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

#include "IO/Skybox.h"
#include "Assets/Texture.h"
#include "Error.h"
#include "IO/File.h"
#include "IO/FileSystem.h"
#include "IO/PathInfo.h"
#include "IO/ReadFreeImageTexture.h"
#include "IO/Reader.h"
#include "Model/Entity.h"
#include <QTransform>
#include <algorithm>
#include <filesystem>
#include <kdl/result.h>

namespace TrenchBroom::IO {
std::string skyboxName(const Model::Entity& world) {
    std::string result;
    for (const auto& property : world.properties()) {
        auto key = property.key();
        if (!key.empty() && key.front() == '_') key.erase(0, 1);
        while (!key.empty() && key.back() == ' ') key.pop_back();
        if (key == "sky" || key == "skyname" || key == "qlsky") result = property.value();
    }
    return result;
}

Result<Skybox> loadSkybox(const FileSystem& fs, const std::string& name, const int maxFaceSize) {
    const auto relative = std::filesystem::path(name);
    if (name.empty() || relative.is_absolute() || maxFaceSize < 1
        || std::any_of(relative.begin(), relative.end(), [](const auto& part) { return part == ".."; }))
        return Error{"Invalid skybox name: " + name};

    constexpr std::array suffixes{"rt", "bk", "lf", "ft", "up", "dn"};
    Skybox result;
    int size = 1;
    for (size_t face = 0; face < suffixes.size(); ++face) {
        QImage image;
        for (const auto* folder : {"gfx/env", "env"}) {
            for (const auto* separator : {"", "_"}) {
                for (const auto* extension : {".tga", ".png", ".jpg", ".jpeg", ".pcx"}) {
                    if (!image.isNull()) break;

                    const auto path = std::filesystem::path(folder) / (name + separator + suffixes[face] + extension);
                    if (fs.pathInfo(path) != PathInfo::File) continue;
                    auto file = fs.openFile(path);

                    if (!file.is_success()) continue;

                    auto reader = file.value()->reader().buffer();
                    auto decoded = readFreeImageTexture(path.string(), reader);

                    if (!decoded.is_success()) continue;

                    auto texture = std::move(decoded).value();
                    const auto& data = texture.buffersIfUnprepared().front();
                    const auto red = texture.format() == GL_BGRA ? 2 : 0;
                    const auto blue = texture.format() == GL_BGRA ? 0 : 2;

                    image = QImage(int(texture.width()), int(texture.height()), QImage::Format_RGBA8888);
                    if (image.isNull()) continue;
                    for (int y = 0; y < image.height(); ++y) {
                        auto* row = image.scanLine(y);
                        const auto* source = data.data() + size_t(y) * texture.width() * 4;
                        for (int x = 0; x < image.width(); ++x) {
                            row[4*x] = source[4*x + red];
                            row[4*x + 1] = source[4*x + 1];
                            row[4*x + 2] = source[4*x + blue];
                            row[4*x + 3] = 255;
                        }
                    }
                }

                if (!image.isNull()) break;
            }

            if (!image.isNull()) break;
        }

        if (image.isNull()) return Error{"Skybox '" + name + "': missing or invalid face '" + suffixes[face] + "'"};
        size = std::max(size, std::max(image.width(), image.height()));
        result.faces[face] = std::move(image);
    }

    size = std::min(size, maxFaceSize);
    for (auto& face : result.faces) {
        if (face.size() != QSize(size, size)) face = face.scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    return result;
}

std::array<QImage, 6> skyboxCubeFaces(const Skybox& skybox) {
    // Quake's upper/lower face axes differ from OpenGL's cube convention.
    return {skybox.faces[0], skybox.faces[2], skybox.faces[4].transformed(QTransform().rotate(-90)),
        skybox.faces[5].transformed(QTransform().rotate(90)), skybox.faces[1], skybox.faces[3]};
}
} // namespace TrenchBroom::IO
