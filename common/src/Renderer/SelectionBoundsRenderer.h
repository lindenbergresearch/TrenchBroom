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

#include "Color.h"
#include "FloatType.h"
#include "Renderer/GLVertexType.h"
#include "Renderer/Renderable.h"
#include "Renderer/VertexArray.h"

#include <vm/bbox.h>

namespace TrenchBroom {
namespace Renderer {
class RenderBatch;

class RenderContext;

class SelectionBoundsRenderer {
    static const float AXIS_LABEL_DIM_FACTOR;

  private:
    const vm::bbox3 m_bounds;

    class SizeTextAnchor2D;

    class SizeTextAnchor3D;

    class MinMaxTextAnchor3D;

    QString formatDimension(const float value, const int digits = 2, const QString &suffix = "");

  public:
    SelectionBoundsRenderer(const vm::bbox3 &bounds);

    void render(RenderContext &renderContext, RenderBatch &renderBatch);

  private:
    void renderBounds(RenderContext &renderContext, RenderBatch &renderBatch);

    void renderSize(RenderContext &renderContext, RenderBatch &renderBatch);

    void renderSize2D(RenderContext &renderContext, RenderBatch &renderBatch);

    void renderSize3D(RenderContext &renderContext, RenderBatch &renderBatch);

    void renderMinMax(RenderContext &renderContext, RenderBatch &renderBatch);

    const std::string getFormattedUnitsString(float value_units);
};
} // namespace Renderer
} // namespace TrenchBroom
