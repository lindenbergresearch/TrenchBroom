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

#include "Shaders.h"

namespace TrenchBroom::Renderer::Shaders {
const auto Grid2DShader = ShaderConfig{
    "2D Grid",
    {"Grid2D.vertsh"},
    {"Grid.fragsh", "Grid2D.fragsh"},
};

const auto VaryingPCShader = ShaderConfig{
    "Varying Position / Color",
    {"VaryingPC.vertsh"},
    {"VaryingPC.fragsh"},
};

const auto VaryingPUniformCShader = ShaderConfig{
    "Varying Position / Uniform Color",
    {"VaryingPUniformC.vertsh"},
    {"VaryingPC.fragsh"},
};

const auto SelectionGuideShader = ShaderConfig{
    "Selection Guides",
    {"OverlaySize.vertsh", "SelectionGuide.vertsh"},
    {"SelectionGuide.fragsh"},
};

const auto MiniMapEdgeShader = ShaderConfig{
    "MiniMap Edges",
    {"MiniMapEdge.vertsh"},
    {"MiniMapEdge.fragsh"},
};

const auto EntityModelShader = ShaderConfig{
    "Entity Model",
    {"EntityModel.vertsh"},
    {"Lighting.fragsh", "MapBounds.fragsh", "EntityModel.fragsh"},
};

const auto FaceShader = ShaderConfig{
    "Face",
    {"Face.vertsh"},
    {"Lighting.fragsh", "Grid.fragsh", "MapBounds.fragsh", "Face.fragsh"},
};

const auto PatchShader = ShaderConfig{
    "Patch",
    {"Face.vertsh"},
    {"Lighting.fragsh", "Grid.fragsh", "MapBounds.fragsh", "Face.fragsh"},
};

const auto EdgeShader = ShaderConfig{
    "Edge",
    {"OverlaySize.vertsh", "Edge.vertsh"},
    {"MapBounds.fragsh", "Edge.fragsh"},
};

const auto ColoredTextShader = ShaderConfig{
    "Colored Text",
    {"ColoredText.vertsh"},
    {"Text.fragsh"},
};

const auto TextShader = ShaderConfig{
    "Text",
    {"Text.vertsh"},
    {"Text.fragsh"},
};

const auto TextBackgroundShader = ShaderConfig{
    "Text Background",
    {"TextBackground.vertsh"},
    {"TextBackground.fragsh"},
};

const auto TextureBrowserShader = ShaderConfig{
    "Texture Browser",
    {"TextureBrowser.vertsh"},
    {"TextureBrowser.fragsh"},
};

const auto TextureBrowserBorderShader = ShaderConfig{
    "Texture Browser Border",
    {"TextureBrowserBorder.vertsh"},
    {"TextureBrowserBorder.fragsh"},
};

const auto HandleShader = ShaderConfig{
    "Handle",
    {"Handle.vertsh"},
    {"Handle.fragsh"},
};

const auto ColoredHandleShader = ShaderConfig{
    "Colored Handle",
    {"ColoredHandle.vertsh"},
    {"Handle.fragsh"},
};

const auto CompassShader = ShaderConfig{
    "Compass",
    {"Compass.vertsh"},
    {"Compass.fragsh"},
};

const auto CompassOutlineShader = ShaderConfig{
    "Compass Outline",
    {"CompassOutline.vertsh"},
    {"Compass.fragsh"},
};

const auto CompassBackgroundShader = ShaderConfig{
    "Compass Background",
    {"VaryingPUniformC.vertsh"},
    {"VaryingPC.fragsh"},
};

const auto LinkLineShader = ShaderConfig{
    "Link Line",
    {"LinkLine.vertsh"},
    {"LinkLine.fragsh"},
};

const auto LinkArrowShader = ShaderConfig{
    "Link Arrow",
    {"LinkArrow.vertsh"},
    {"LinkArrow.fragsh"},
};

const auto TriangleShader = ShaderConfig{
    "Shaded Triangles",
    {"Triangle.vertsh"},
    {"Triangle.fragsh"},
};

const auto UVViewShader = ShaderConfig{
    "UV View",
    {"UVView.vertsh"},
    {"UVView.fragsh"},
};
} // namespace TrenchBroom::Renderer::Shaders
