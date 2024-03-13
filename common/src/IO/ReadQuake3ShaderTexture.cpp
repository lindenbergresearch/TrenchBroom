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

#include "ReadQuake3ShaderTexture.h"

#include "Assets/Quake3Shader.h"
#include "Assets/Texture.h"
#include "Error.h"
#include "IO/File.h"
#include "IO/FileSystem.h"
#include "IO/PathInfo.h"
#include "IO/ReadFreeImageTexture.h"
#include "IO/TraversalMode.h"

#include "kdl/functional.h"
#include "kdl/result.h"
#include "kdl/result_fold.h"
#include "kdl/string_format.h"
#include "kdl/vector_utils.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace TrenchBroom::IO
{

namespace
{

std::optional<std::filesystem::path> findImage(
  const std::filesystem::path& texturePath, const FileSystem& fs)
{
  static const auto imageExtensions =
    std::vector<std::string>{".tga", ".png", ".jpg", ".jpeg"};

  if (!texturePath.empty())
  {
    if (
      kdl::vec_contains(
        imageExtensions, kdl::str_to_lower(texturePath.extension().string()))
      && fs.pathInfo(texturePath) == PathInfo::File)
    {
      return texturePath;
    }

    const auto directoryPath = texturePath.parent_path();
    const auto basename = texturePath.stem().string();
    return fs
      .find(
        texturePath.parent_path(),
        TraversalMode::Flat,
        kdl::lift_and(
          makeFilenamePathMatcher(basename + ".*"),
          makeExtensionPathMatcher(imageExtensions)))
      .transform([](auto candidates) -> std::optional<std::filesystem::path> {
        if (!candidates.empty())
        {
          return candidates.front();
        }
        return std::nullopt;
      })
      .value_or(std::nullopt);
  }

  // texture path is empty OR (the extension is not empty AND the file does not exist)
  return std::nullopt;
}

std::optional<std::filesystem::path> findImagePath(
  const Assets::Quake3Shader& shader, const FileSystem& fs)
{
  if (const auto path = findImage(shader.editorImage, fs))
  {
    return path;
  }
  if (const auto path = findImage(shader.shaderPath, fs))
  {
    return path;
  }
  if (const auto path = findImage(shader.lightImage, fs))
  {
    return path;
  }
  for (const auto& stage : shader.stages)
  {
    if (const auto path = findImage(stage.map, fs))
    {
      return path;
    }
  }

  return std::nullopt;
}

Result<Assets::Texture, ReadTextureError> loadTextureImage(
  std::string shaderName, const std::filesystem::path& imagePath, const FileSystem& fs)
{
  auto imageName = imagePath.filename();
  if (fs.pathInfo(imagePath) != PathInfo::File)
  {
    return ReadTextureError{
      std::move(shaderName), "Image file '" + imagePath.string() + "' does not exist"};
  }

  return fs.openFile(imagePath)
    .and_then([&](auto file) {
      auto reader = file->reader().buffer();
      return readFreeImageTexture(shaderName, reader);
    })
    .or_else([&](auto e) {
      return Result<Assets::Texture, ReadTextureError>{
        ReadTextureError{shaderName, e.msg}};
    });
}

} // namespace

Result<Assets::Texture, ReadTextureError> readQuake3ShaderTexture(
  std::string shaderName, const File& file, const FileSystem& fs)
{
  const auto* shaderFile = dynamic_cast<const ObjectFile<Assets::Quake3Shader>*>(&file);
  if (!shaderFile)
  {
    return ReadTextureError{std::move(shaderName), "Shader not found: " + shaderName};
  }

  const auto& shader = shaderFile->object();
  const auto imagePath = findImagePath(shader, fs);
  if (!imagePath)
  {
    return ReadTextureError{
      std::move(shaderName),
      "Could not find texture path for shader '" + shader.shaderPath.string() + "'"};
  }

  return loadTextureImage(std::move(shaderName), *imagePath, fs)
    .transform([&](auto texture) {
      texture.setSurfaceParms(shader.surfaceParms);
      texture.setOpaque();

      // Note that Quake 3 has a different understanding of front and back, so we need to
      // invert them.
      switch (shader.culling)
      {
      case Assets::Quake3Shader::Culling::Front:
        texture.setCulling(Assets::TextureCulling::Back);
        break;
      case Assets::Quake3Shader::Culling::Back:
        texture.setCulling(Assets::TextureCulling::Front);
        break;
      case Assets::Quake3Shader::Culling::None:
        texture.setCulling(Assets::TextureCulling::None);
        break;
      }

      if (!shader.stages.empty())
      {
        const auto& stage = shader.stages.front();
        if (stage.blendFunc.enable())
        {
          texture.setBlendFunc(
            glGetEnum(stage.blendFunc.srcFactor), glGetEnum(stage.blendFunc.destFactor));
        }
        else
        {
          texture.disableBlend();
        }
      }

      return texture;
    });
}

} // namespace TrenchBroom::IO
