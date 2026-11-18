/*
 Copyright (C) 2021 Kristian Duske

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
#include <kdl/result.h>
#include "Assets/EntityModel.h"
#include "Assets/Texture.h"
#include "Assets/TextureBuffer.h"
#include "IO/AssimpParser.h"
#include "IO/DiskFileSystem.h"
#include "IO/File.h"
#include "IO/ImageLoaderImpl.h"
#include "IO/ReadFreeImageTexture.h"
#include "IO/ReadDdsTexture.h"
#include "IO/Reader.h"
#include "Logger.h"
#include "Renderer/FontDescriptor.h"
#include "Renderer/FreeTypeFontFactory.h"
#include "Renderer/TextureFont.h"
#include "TestSourcePath.h"

#include <string>
#include <kdl/resource.h>

#include "Catch2.h"

namespace TrenchBroom {
namespace {
    const auto fixtureRoot = testSourceRoot() / "common/test/fixture";

    Assets::Texture loadImage(const std::filesystem::path& path) {
        const auto fs = IO::DiskFileSystem{fixtureRoot};
        const auto file = fs.openFile(path);
        REQUIRE(file.is_success());
        auto reader = file.value()->reader().buffer();
        auto result = IO::readFreeImageTexture(path.filename().string(), reader);
        REQUIRE(result.is_success());
        return std::move(result).value();
    }
} // namespace

TEST_CASE("CPM image codecs preserve dimensions and RGBA data", "[dependencies]") {
    struct ImageCase {
        const char* path;
        size_t width;
        size_t height;
    };
    const auto image = GENERATE(ImageCase{"IO/Image/5x5.png", 5, 5}, ImageCase{"IO/Image/707x710.png", 707, 710}, ImageCase{"IO/Image/jpgContentsTest.jpg", 64, 64},
                                ImageCase{"IO/Ase/fallback_to_materialname/textures/bigtile.tga", 256, 256});
    CAPTURE(image.path);
    const auto texture = loadImage(image.path);
    CHECK(texture.width() == image.width);
    CHECK(texture.height() == image.height);
    REQUIRE((texture.format() == GL_BGRA || texture.format() == GL_RGBA));
    REQUIRE(texture.buffersIfUnprepared().size() == 1);
    CHECK(texture.buffersIfUnprepared().front().size() == image.width * image.height * 4);
}

TEST_CASE("CPM build preserves the renderer's native DDS loading", "[dependencies]") {
    struct DdsCase {
        const char* name;
        GLenum format;
    };
    const auto image = GENERATE(DdsCase{"dds_rgb.dds", GL_BGR}, DdsCase{"dds_rgba.dds", GL_BGRA}, DdsCase{"dds_bc1.dds", GL_COMPRESSED_RGBA_S3TC_DXT1_EXT},
                                DdsCase{"dds_bc2.dds", GL_COMPRESSED_RGBA_S3TC_DXT3_EXT}, DdsCase{"dds_bc3.dds", GL_COMPRESSED_RGBA_S3TC_DXT5_EXT});
    CAPTURE(image.name);
    const auto fs = IO::DiskFileSystem{fixtureRoot / "IO/Dds"};
    const auto file = fs.openFile(image.name);
    REQUIRE(file.is_success());
    auto reader = file.value()->reader().buffer();
    const auto result = IO::readDdsTexture(image.name, reader);
    REQUIRE(result.is_success());
    CHECK(result.value().width() == 128);
    CHECK(result.value().height() == 128);
    CHECK(result.value().format() == image.format);
    REQUIRE_FALSE(result.value().buffersIfUnprepared().empty());
    CHECK(result.value().buffersIfUnprepared().front().size() > 0);
}

TEST_CASE("CPM image decoding preserves color order orientation and transparency", "[dependencies]") {
    const auto texture = loadImage("IO/Image/pngContentsTest.png");
    const auto* pixels = texture.buffersIfUnprepared().front().data();
    const auto red = texture.format() == GL_RGBA ? 0u : 2u;
    const auto blue = texture.format() == GL_RGBA ? 2u : 0u;
    // These fixture pixels are deliberately different at the opposite corners.
    CHECK(pixels[red] == 255);
    CHECK(pixels[1] == 0);
    CHECK(pixels[blue] == 0);
    const auto last = (texture.width() * texture.height() - 1) * 4;
    CHECK(pixels[last + red] == 0);
    CHECK(pixels[last + 1] == 255);
    CHECK(pixels[last + blue] == 0);
    CHECK(texture.type() == Assets::TextureType::Opaque);

    const auto masked = loadImage("IO/Image/alphaMaskTest.png");
    CHECK(masked.type() == Assets::TextureType::Masked);
    const auto& buffer = masked.buffersIfUnprepared().front();
    bool transparent = false;
    bool opaque = false;
    for (size_t i = 3; i < buffer.size(); i += 4) {
        transparent = transparent || buffer.data()[i] < 255;
        opaque = opaque || buffer.data()[i] == 255;
    }
    CHECK(transparent);
    CHECK(opaque);
}

TEST_CASE("CPM image decoder reports corrupt image data", "[dependencies]") {
    const auto fs = IO::DiskFileSystem{fixtureRoot / "IO/Image"};
    const auto file = fs.openFile("corruptPngTest.png");
    REQUIRE(file.is_success());
    auto reader = file.value()->reader().buffer();
    CHECK(IO::readFreeImageTexture("corrupt", reader).is_error());
}

TEST_CASE("CPM preserves the optional FreeImage codecs", "[dependencies]") {
    IO::InitFreeImage::initialize();
    CHECK(FreeImage_FIFSupportsReading(FIF_EXR));
    CHECK(FreeImage_FIFSupportsReading(FIF_RAW));
    CHECK(FreeImage_FIFSupportsReading(FIF_JXR));
    // Exercise linked codec implementations, not only plugin registration.
    const auto format = GENERATE(FIF_JXR, FIF_WEBP, FIF_TIFF, FIF_EXR);
    CAPTURE(format);
    auto image = kdl::resource{format == FIF_EXR ? FreeImage_AllocateT(FIT_RGBF, 16, 16) : FreeImage_Allocate(16, 16, 24), FreeImage_Unload};
    REQUIRE(image);
    auto memory = kdl::resource{FreeImage_OpenMemory(), FreeImage_CloseMemory};
    REQUIRE(memory);
    REQUIRE(FreeImage_SaveToMemory(format, *image, *memory));
    REQUIRE(FreeImage_SeekMemory(*memory, 0, SEEK_SET));
    auto decoded = kdl::resource{FreeImage_LoadFromMemory(format, *memory), FreeImage_Unload};
    REQUIRE(decoded);
    CHECK(FreeImage_GetWidth(*decoded) == 16);
    CHECK(FreeImage_GetHeight(*decoded) == 16);
}

TEST_CASE("CPM Assimp imports model geometry and its external texture", "[dependencies]") {
    const auto fs = IO::DiskFileSystem{fixtureRoot / "IO/assimp"};
    auto logger = NullLogger{};
    const auto* name = GENERATE("cube.dae", "cube.blend");
    CAPTURE(name);
    auto parser = IO::AssimpParser{name, fs};
    auto model = parser.initializeModel(logger);
    REQUIRE(model);
    REQUIRE(model->frameCount() >= 1);
    REQUIRE(model->surfaceCount() >= 1);
    parser.loadFrame(0, *model, logger);
    REQUIRE(model->frame(0));
    CHECK(model->frame(0)->bounds().size().x() > 0.0f);
    const auto& surface = model->surface(0);
    REQUIRE(surface.skinCount() >= 1);
    const auto* skin = surface.skin(size_t{0});
    REQUIRE(skin);
    // A fallback texture would not have the fixture's 32 x 32 dimensions.
    CHECK(skin->width() == 32);
    CHECK(skin->height() == 32);
}

TEST_CASE("CPM FreeType builds renderer glyphs at multiple sizes", "[dependencies]") {
    auto factory = Renderer::FreeTypeFontFactory{};
    const auto fontPath = testSourceRoot() / "app/resources/fonts/roboto-condensed/RobotoCondensed-Regular.ttf";
    auto small = factory.createFont(Renderer::FontDescriptor{fontPath, 12});
    auto large = factory.createFont(Renderer::FontDescriptor{fontPath, 24});
    REQUIRE(small);
    REQUIRE(large);
    const auto smallSize = small->measure(std::string{"Grid 128"});
    const auto largeSize = large->measure(std::string{"Grid 128"});
    CHECK(smallSize.x() > 0.0f);
    CHECK(smallSize.y() > 0.0f);
    CHECK(largeSize.x() > smallSize.x());
    CHECK(largeSize.y() > smallSize.y());
    CHECK_FALSE(small->quads(std::string{"XYZ"}).empty());
}
} // namespace TrenchBroom
