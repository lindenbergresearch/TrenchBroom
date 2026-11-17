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

#pragma once

#include "Result.h"
#include <QImage>
#include <array>
#include <string>

namespace TrenchBroom::Model { class Entity; }
namespace TrenchBroom::IO {
class FileSystem;

// Quake image order: right, back, left, front, up, down. Images use top-first rows.
struct Skybox {
    std::array<QImage, 6> faces;
};

std::string skyboxName(const Model::Entity& world);
Result<Skybox> loadSkybox(const FileSystem& fs, const std::string& name, int maxFaceSize = 2048);
// OpenGL face order +X,-X,+Y,-Y,+Z,-Z, for a lookup direction (world.x,world.z,world.y).
std::array<QImage, 6> skyboxCubeFaces(const Skybox& skybox);
} // namespace TrenchBroom::IO
