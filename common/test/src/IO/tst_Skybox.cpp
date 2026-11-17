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

#include "Error.h"
#include "IO/DiskFileSystem.h"
#include "IO/Skybox.h"
#include "IO/VirtualFileSystem.h"
#include "Model/Entity.h"
#include "Model/GameConfig.h"
#include "Assets/Texture.h"
#include "Renderer/SkyRenderer.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <catch2/catch.hpp>
#include <kdl/result.h>

namespace TrenchBroom::IO {
namespace {
constexpr std::array skySuffixes{"rt", "bk", "lf", "ft", "up", "dn"};
QImage skyTestImage(int face) {
    QImage image(8, 8, QImage::Format_RGBA8888);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            image.setPixelColor(x, y, QColor(20 + face * 30, x * 30, y * 30, 128));
    return image;
}
void writeSky(const QString& root, const QString& prefix) {
    REQUIRE(QDir().mkpath(root + "/" + QFileInfo(prefix).path()));
    for (size_t i = 0; i < skySuffixes.size(); ++i)
        REQUIRE(skyTestImage(int(i)).save(root + "/" + prefix + skySuffixes[i] + ".png"));
}
}

TEST_CASE("Skybox reads Quake worldspawn aliases in property order", "[sky]") {
    CHECK(skyboxName(Model::Entity{}).empty());
    for (const auto* key : {"sky", "_sky", "skyname", "_skyname", "qlsky", "sky   "})
        CHECK(skyboxName(Model::Entity({}, {{key, "sunset"}})) == "sunset");
    CHECK(skyboxName(Model::Entity({}, {{"sky", "first"}, {"_sky", "last"}})) == "last");
    CHECK(skyboxName(Model::Entity({}, {{"sky", "first"}, {"skyname", ""}})).empty());
    CHECK(skyboxName(Model::Entity({}, {{"_sunlight", "300"}, {"skyfog", "0.2"}})).empty());
}

TEST_CASE("Skybox loads six faces with exact colors and top-first orientation", "[sky]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    writeSky(dir.path(), "gfx/env/test");
    const DiskFileSystem fs{dir.path().toStdString()};
    const auto result = loadSkybox(fs, "test");
    REQUIRE(result.is_success());
    for (size_t face = 0; face < 6; ++face) {
        CHECK(result.value().faces[face].size() == QSize(8, 8));
        for (const auto& corner : {QPoint(0, 0), QPoint(7, 0), QPoint(0, 7), QPoint(7, 7)}) {
            auto expected = skyTestImage(int(face)).pixelColor(corner);
            expected.setAlpha(255);
            CHECK(result.value().faces[face].pixelColor(corner) == expected);
        }
    }
    const auto small = loadSkybox(fs, "test", 4);
    REQUIRE(small.is_success());
    for (const auto& image : small.value().faces) CHECK(image.size() == QSize(4, 4));
}

TEST_CASE("Skybox honors mounted resource overrides and handles absent or broken faces", "[sky]") {
    QTemporaryDir base, mod;
    REQUIRE(base.isValid());
    REQUIRE(mod.isValid());
    writeSky(base.path(), "env/test_");
    REQUIRE(QDir().mkpath(mod.path() + "/env"));
    auto overrideImage = QImage(16, 16, QImage::Format_RGB32);
    overrideImage.fill(Qt::red);
    REQUIRE(overrideImage.save(mod.path() + "/env/test_rt.png"));
    VirtualFileSystem fs;
    fs.mount({}, std::make_unique<DiskFileSystem>(base.path().toStdString()));
    fs.mount({}, std::make_unique<DiskFileSystem>(mod.path().toStdString()));
    auto result = loadSkybox(fs, "test");
    REQUIRE(result.is_success());
    CHECK(result.value().faces[0].pixelColor(0, 0) == QColor(Qt::red));
    for (const auto& image : result.value().faces) CHECK(image.size() == QSize(16, 16));
    for (const auto* invalid : {"", "/test", "../test", "sub/../../test", "absent"})
        CHECK(loadSkybox(fs, invalid).is_error());
    CHECK(loadSkybox(fs, "test", 0).is_error());
    REQUIRE(QFile::remove(base.path() + "/env/test_dn.png"));
    CHECK(loadSkybox(fs, "test").is_error());
    QFile broken(base.path() + "/env/test_dn.png");
    REQUIRE(broken.open(QIODevice::WriteOnly));
    broken.write("invalid image");
    broken.close();
    CHECK(loadSkybox(fs, "test").is_error());
}

TEST_CASE("Skybox cube faces preserve Quake cardinal directions and polar orientation", "[sky]") {
    Skybox sky;
    for (size_t i = 0; i < 6; ++i) sky.faces[i] = skyTestImage(int(i));
    const auto cube = skyboxCubeFaces(sky);
    CHECK(cube[0] == sky.faces[0]); // +X: right
    CHECK(cube[1] == sky.faces[2]); // -X: left
    CHECK(cube[4] == sky.faces[1]); // world +Y: back (shader maps world xyz to xzy)
    CHECK(cube[5] == sky.faces[3]); // world -Y: front
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            CHECK(cube[2].pixel(x, y) == sky.faces[4].pixel(7-y, x));
            CHECK(cube[3].pixel(x, y) == sky.faces[5].pixel(y, 7-x));
        }
    }
}

TEST_CASE("Skybox recognizes sky materials without replacing ordinary textures", "[sky]") {
    CHECK_FALSE(Renderer::isSkyTexture(nullptr));
    for (const auto* name : {"sky1", "SKY", "textures/sky_night"}) {
        const auto texture = Assets::Texture{name, 1, 1};
        CHECK(Renderer::isSkyTexture(&texture));
    }
    const auto texture = Assets::Texture{"walls/bluesky_wall", 1, 1};
    CHECK_FALSE(Renderer::isSkyTexture(&texture));
}
} // namespace TrenchBroom::IO
