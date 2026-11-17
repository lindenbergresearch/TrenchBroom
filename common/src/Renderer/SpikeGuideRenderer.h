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

#include <vm/forward.h>

#include <memory>
#include <vector>

namespace TrenchBroom {namespace Model {
    class Picker;
    }

    namespace View {
    // FIXME: Renderer should not depend on View
    class MapDocument;
    } // namespace View

    namespace Renderer {
    class SpikeGuideRenderer : public DirectRenderable {
    private:
        Color m_color;

        // Position and normal carry the two world-space endpoints; UV selects a
        // corner of the rectangle expanded by the guide shader.
        using GuideVertex = GLVertexTypes::P3NT2::Vertex;

        std::vector<GuideVertex> m_spikeVertices;
        std::vector<vm::vec3f> m_points;

        struct SurfacePoint {
            vm::vec3f position;
            vm::vec3f normal;
        };

        std::vector<SurfacePoint> m_surfacePoints;

        VertexArray m_spikeArray;
        VertexArray m_pointArray;
        VertexArray m_surfacePointArray;

        bool m_valid;

    public:
        SpikeGuideRenderer();

        void setColor(const Color& color);

        void add(const vm::ray3& ray, FloatType length, const std::shared_ptr<View::MapDocument>& document);

        void clear();

    private:
        void doPrepareVertices(VboManager& vboManager) override;

        void doRender(RenderContext& renderContext) override;

        void addPoint(const vm::vec3& position);

        void addSurfacePoint(const vm::vec3& position, const vm::vec3& normal);

        void addSpike(const vm::ray3& ray, FloatType length);

        void validate();
    };
    } // namespace Renderer
} // namespace TrenchBroom
