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
#include "Renderer/Renderable.h"
#include "Renderer/VertexArray.h"

#include <vm/bbox.h>

namespace TrenchBroom::Renderer {
class RenderBatch;

class RenderContext;

class SelectionBoundsRenderer {
    static constexpr auto AXIS_LABEL_DIM_FACTOR = 0.9f;
    static constexpr std::string AXIS_LABELS[3] = {"x", "y", "z"};
    static constexpr std::string AXIS_LABEL_DELIMITER = "=";

    const vm::bbox3 m_bounds;
    const bool m_draw3DVolume;

    class SizeTextAnchor2D;

    class SizeTextAnchor3D;

    class MinMaxTextAnchor3D;

    QString formatDimension(float value, int digits = 2, const QString& suffix = "");

public:
    explicit SelectionBoundsRenderer(const vm::bbox3& bounds, bool draw3DVolume = true);

    void render(RenderContext& renderContext, RenderBatch& renderBatch);
    void renderObjectBounds(RenderContext& context, RenderBatch& batch) { renderBounds(context, batch); }

private:
    void renderBounds(RenderContext& renderContext, RenderBatch& renderBatch);

    void renderSize(RenderContext& renderContext, RenderBatch& renderBatch);

    void renderSize2D(RenderContext& renderContext, RenderBatch& renderBatch);

    void renderSize3D(RenderContext& renderContext, RenderBatch& renderBatch);

    void renderMinMax(RenderContext& renderContext, RenderBatch& renderBatch);

    const std::string getFormattedUnitsString(float value_units);
};
}
