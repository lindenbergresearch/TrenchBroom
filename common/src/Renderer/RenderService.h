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
#include "Macros.h"

#include <vm/constants.h>
#include <vm/forward.h>
#include <vm/util.h>
#include "Renderer/FontDescriptor.h"
#include "Renderer/TextEntityRenderer.h"
#include "Renderer/EntityRenderer.h"

#include <memory>
#include <vector>

namespace TrenchBroom {
namespace Renderer {
class AttrString;

class PointHandleRenderer;

class PrimitiveRenderer;

enum class PrimitiveRendererCullingPolicy;
enum class PrimitiveRendererOcclusionPolicy;

class RenderBatch;

class RenderContext;

class TextAnchor;

class TextRenderer;

enum ScreenCorner {
  TOP_LEFT,
  TOP_RIGHT,
  BOTTOM_LEFT,
  BOTTOM_RIGHT
};

class LeftTopScreenTextAnchor : public TextAnchor {
    vm::vec2f m_extra;

  public:
    LeftTopScreenTextAnchor(const vm::vec2f &extra = vm::vec2f{}) : m_extra(extra) {}

  private:
    vm::vec3f offset(const Camera &camera, const vm::vec2f &size) const override {
        vm::vec3f off = getOffset(camera);
        return vm::vec3f(off.x(), off.y() - size.y(), off.z());
    }

    vm::vec3f position(const Camera &camera) const override {
        return camera.unproject(getOffset(camera));
    }

    vm::vec3f getOffset(const Camera &camera) const {
        const auto h = static_cast<float>(camera.viewport().height);
        return vm::vec3f(m_extra.x(), h - m_extra.y(), 0.f);
    }
};

class RightTopScreenTextAnchor : public TextAnchor {
    vm::vec2f m_extra;

  public:
    RightTopScreenTextAnchor(const vm::vec2f &extra = vm::vec2f{}) : m_extra(extra) {}

  private:
    vm::vec3f offset(const Camera &camera, const vm::vec2f &size) const override {
        vm::vec3f off = getOffset(camera);
        return vm::vec3f(off.x()-size.x(), off.y() - size.y(), off.z());
    }

    vm::vec3f position(const Camera &camera) const override {
        return camera.unproject(getOffset(camera));
    }

    vm::vec3f getOffset(const Camera &camera) const {
        const auto h = static_cast<float>(camera.viewport().height);
        const auto w = static_cast<float>(camera.viewport().width);
        return vm::vec3f(w- m_extra.x(), h - m_extra.y(), 0.f);
    }
};

class RightBottomScreenTextAnchor : public TextAnchor {
    vm::vec2f m_extra;

  public:
    RightBottomScreenTextAnchor(const vm::vec2f &extra = vm::vec2f{}) : m_extra(extra) {}

  private:
    vm::vec3f offset(const Camera &camera, const vm::vec2f &size) const override {
        vm::vec3f off = getOffset(camera);
        return vm::vec3f(off.x()-size.x(), off.y(), off.z());
    }

    vm::vec3f position(const Camera &camera) const override {
        return camera.unproject(getOffset(camera));
    }

    vm::vec3f getOffset(const Camera &camera) const {
        const auto h = static_cast<float>(camera.viewport().height);
        const auto w = static_cast<float>(camera.viewport().width);
        return vm::vec3f(w- m_extra.x(), m_extra.y(), 0.f);
    }
};

class LeftBottomScreenTextAnchor : public TextAnchor {
    vm::vec2f m_extra;

  public:
    LeftBottomScreenTextAnchor(const vm::vec2f &extra = vm::vec2f{}) : m_extra(extra) {}

  private:
    vm::vec3f offset(const Camera &camera, const vm::vec2f &size) const override {
        vm::vec3f off = getOffset(camera);
        return vm::vec3f(off.x(), off.y(), off.z());
    }

    vm::vec3f position(const Camera &camera) const override {
        return camera.unproject(getOffset(camera));
    }

    vm::vec3f getOffset(const Camera &camera) const {
        return vm::vec3f(m_extra.x(), m_extra.y(), 0.f);
    }
};


class HeadsUpTextAnchor : public TextAnchor {
  private:
    vm::vec3f offset(const Camera &camera, const vm::vec2f &size) const override {
        vm::vec3f off = getOffset(camera);
        return vm::vec3f(off.x() - size.x() / 2.0f, off.y() - size.y(), off.z());
    }

    vm::vec3f position(const Camera &camera) const override {
        return camera.unproject(getOffset(camera));
    }

    vm::vec3f getOffset(const Camera &camera) const {
        const auto w = static_cast<float>(camera.viewport().width);
        const auto h = static_cast<float>(camera.viewport().height);
        return vm::vec3f(w / 2.0f, h - 20.0f, 0.f);
    }
};


class RenderService {
  public:
    using OcclusionPolicy = PrimitiveRendererOcclusionPolicy;
    using CullingPolicy = PrimitiveRendererCullingPolicy;

    RenderContext &m_renderContext;
    RenderBatch &m_renderBatch;
    //  std::unique_ptr<TextRenderer> m_textRenderer;
    std::unique_ptr<TextEntityRenderer> m_textEntityRenderer;
    std::unique_ptr<PointHandleRenderer> m_pointHandleRenderer;
    std::unique_ptr<PrimitiveRenderer> m_primitiveRenderer;

    Color m_foregroundColor;
    Color m_backgroundColor;
    float m_lineWidth;
    OcclusionPolicy m_occlusionPolicy;
    CullingPolicy m_cullingPolicy;

  public:
    RenderService(RenderContext &renderContext, RenderBatch &renderBatch);

    ~RenderService();

  deleteCopyAndMove(RenderService);

    void setForegroundColor(const Color &foregroundColor);

    void setBackgroundColor(const Color &backgroundColor);

    void setLineWidth(float lineWidth);

    void setShowOccludedObjects();

    void setShowOccludedObjectsTransparent();

    void setHideOccludedObjects();

    void setShowBackfaces();

    void setCullBackfaces();

    void renderString(const AttrString &string, const vm::vec3f &position);

    void renderCornerScreen(const std::string &string, ScreenCorner corner, vm::vec2f offset, const FontDescriptor &fontDescriptor);

    void renderString(const AttrString &string, const TextAnchor &position);

    void renderHeadsUp(const AttrString &string);

    void renderLeftScreen(const AttrString &string);

    void renderString(const std::string &string, const vm::vec3f &position);

    void renderString(const std::string &string, const TextAnchor &position);

    void renderString(const std::string &string, const TextAnchor &position, FontDescriptor fontDescriptor);

    void renderHeadsUp(const std::string &string);

    void renderText(TextEntity &entity);

    void renderLeftScreen(const std::string &string);

    void renderHandles(const std::vector<vm::vec3f> &positions);

    void renderHandle(const vm::vec3f &position);

    void renderHandleHighlight(const vm::vec3f &position);

    void renderHandles(const std::vector<vm::segment3f> &positions);

    void renderHandle(const vm::segment3f &position);

    void renderHandleHighlight(const vm::segment3f &position);

    void renderHandles(const std::vector<vm::polygon3f> &positions);

    void renderHandle(const vm::polygon3f &position);

    void renderHandleHighlight(const vm::polygon3f &position);

    void renderLine(const vm::vec3f &start, const vm::vec3f &end);

    void renderLines(const std::vector<vm::vec3f> &positions);

    void renderDashedLines(const std::vector<vm::vec3f> &positions, int factor = 4, unsigned short pattern = 0x3333);

    void renderLineStrip(const std::vector<vm::vec3f> &positions);

    void renderCoordinateSystem(const vm::bbox3f &bounds);

    void renderPolygonOutline(const std::vector<vm::vec3f> &positions);

    void renderFilledPolygon(const std::vector<vm::vec3f> &positions);

    void renderBounds(const vm::bbox3f &bounds);

    void renderCircle(const vm::vec3f &position, vm::axis::type normal, size_t segments, float radius, const vm::vec3f &startAxis, const vm::vec3f &endAxis);

    void renderCircle(const vm::vec3f &position, vm::axis::type normal, size_t segments, float radius, float startAngle = 0.0f, float angleLength = vm::Cf::two_pi());

    void renderFilledCircle(const vm::vec3f &position, vm::axis::type normal, size_t segments, float radius, const vm::vec3f &startAxis, const vm::vec3f &endAxis);

    void renderFilledCircle(const vm::vec3f &position, vm::axis::type normal, size_t segments, float radius, float startAngle = 0.0f, float angleLength = vm::Cf::two_pi());

    // static helper

    /* ------------------------------------------------------------------------------------------- */

    static float distanceToEntity(const Camera &camera, const Model::EntityNode *entityNode);


  private:
    void flush();
};
} // namespace Renderer
} // namespace TrenchBroom
