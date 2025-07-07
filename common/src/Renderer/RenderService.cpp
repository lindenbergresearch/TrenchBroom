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

#include "RenderService.h"

#include "AttrString.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/Camera.h"
#include "Renderer/FontDescriptor.h"
#include "Renderer/IndexRangeMapBuilder.h"
#include "Renderer/IndexRangeRenderer.h"
#include "Renderer/PointHandleRenderer.h"
#include "Renderer/PrimitiveRenderer.h"
#include "Renderer/RenderBatch.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderUtils.h"
#include "Renderer/TextAnchor.h"
#include "Renderer/TextRenderer.h"
#include "View/QtUtils.h"

#include <vm/forward.h>
#include <vm/polygon.h>
#include <vm/segment.h>
#include <vm/vec.h>
#include <vm/vec_ext.h>

namespace TrenchBroom {
namespace Renderer {


RenderService::RenderService(RenderContext &renderContext, RenderBatch &renderBatch) :
    m_renderContext(renderContext), m_renderBatch(renderBatch), m_textEntityRenderer(std::make_unique<TextEntityRenderer>()), m_pointHandleRenderer(std::make_unique<PointHandleRenderer>()), m_primitiveRenderer(std::make_unique<PrimitiveRenderer>()), m_foregroundColor(1.0f, 1.0f, 1.0f, 1.0f)
    , m_backgroundColor(0.0f, 0.0f, 0.0f, 1.0f), m_lineWidth(1.0f), m_occlusionPolicy(PrimitiveRendererOcclusionPolicy::Transparent), m_cullingPolicy(PrimitiveRendererCullingPolicy::CullBackfaces) {
}

RenderService::~RenderService() {
    flush();
}

void RenderService::setForegroundColor(const Color &foregroundColor) {
    m_foregroundColor = foregroundColor;
}

void RenderService::setBackgroundColor(const Color &backgroundColor) {
    m_backgroundColor = backgroundColor;
}

void RenderService::setLineWidth(const float lineWidth) {
    m_lineWidth = lineWidth;
}

void RenderService::setShowOccludedObjects() {
    m_occlusionPolicy = PrimitiveRendererOcclusionPolicy::Show;
}

void RenderService::setShowOccludedObjectsTransparent() {
    m_occlusionPolicy = PrimitiveRendererOcclusionPolicy::Transparent;
}

void RenderService::setHideOccludedObjects() {
    m_occlusionPolicy = PrimitiveRendererOcclusionPolicy::Hide;
}

void RenderService::setShowBackfaces() {
    m_cullingPolicy = PrimitiveRendererCullingPolicy::ShowBackfaces;
}

void RenderService::setCullBackfaces() {
    m_cullingPolicy = PrimitiveRendererCullingPolicy::CullBackfaces;
}

void RenderService::renderString(const AttrString &string, const vm::vec3f &position) {
    renderString(string, SimpleTextAnchor(position, TextAlignment::Bottom, vm::vec2f(0.0f, 16.0f)));
}

void RenderService::renderCornerScreen(const std::string &string, ScreenCorner corner, vm::vec2f offset, const FontDescriptor &fontDescriptor) {
    TextAnchor *anchor;

    switch (corner) {
        case TOP_LEFT:anchor = new LeftTopScreenTextAnchor{offset};
            break;
        case TOP_RIGHT:anchor = new RightTopScreenTextAnchor{offset};
            break;
        case BOTTOM_LEFT:anchor = new LeftBottomScreenTextAnchor{offset};
            break;
        case BOTTOM_RIGHT:anchor = new RightBottomScreenTextAnchor{offset};
            break;
    }

    TextEntity textEntity{
        AttrString(string),
        anchor,
        vm::vec2f(3.5, 3),
        true,
        fontDescriptor,
        m_foregroundColor,
        m_backgroundColor,
        0.5f,
        false,
        6, 3
    };

    renderText(textEntity);
}

void RenderService::renderString(const std::string &string, const TextAnchor &position, FontDescriptor fontDescriptor) {
    TextEntity textEntity{
        AttrString(string),
        &position
    };

    textEntity.setForeground(m_foregroundColor);
    textEntity.setBackground(m_backgroundColor);

    if (m_occlusionPolicy != PrimitiveRendererOcclusionPolicy::Hide) {
        textEntity.setOnTop(true);
    }

    renderText(textEntity);
}

void RenderService::renderString(const AttrString &string, const TextAnchor &position) {
    TextEntity textEntity{
        string,
        &position
    };

    textEntity.setForeground(m_foregroundColor);
    textEntity.setBackground(m_backgroundColor);

    if (m_occlusionPolicy != PrimitiveRendererOcclusionPolicy::Hide) {
        textEntity.setOnTop(true);
    }

    renderText(textEntity);
}

void RenderService::renderLeftScreen(const AttrString &string) {
    TextEntity textEntity{
        string,
        new LeftTopScreenTextAnchor(vm::vec2f{20, 20}),
        vm::vec2f(12.f, 12.f),
        true,
        FontDescriptor{"fonts/JetBrainsMono-Bold.ttf", 12, 4},
        Color(0.9f, 0.9f, 1.f, 0.85f),
        Color(0.f, 0.f, 0.2f, 0.3f),
        0.5f,
        false,
        12, 10.f
    };

    renderText(textEntity);
}

void RenderService::renderHeadsUp(const AttrString &string) {
    TextEntity textEntity{
        string,
        new HeadsUpTextAnchor()
    };

    textEntity.setForeground(m_foregroundColor);
    textEntity.setBackground(m_backgroundColor);
    textEntity.setOnTop(true);

    renderText(textEntity);
}

void RenderService::renderString(const std::string &string, const vm::vec3f &position) {
    renderString(AttrString(string), position);
}

void RenderService::renderString(const std::string &string, const TextAnchor &position) {
    renderString(AttrString(string), position);
}

void RenderService::renderText(TextEntity &entity) {
    m_textEntityRenderer->addTextEntity(entity, m_renderContext);
}

void RenderService::renderHeadsUp(const std::string &string) {
    renderHeadsUp(AttrString(string));
}

void RenderService::renderLeftScreen(const std::string &string) {
    auto str = AttrString(string);

    renderLeftScreen(str);
}

void RenderService::renderHandles(const std::vector<vm::vec3f> &positions) {
    for (const vm::vec3f &position : positions)
        renderHandle(position);
}

void RenderService::renderHandle(const vm::vec3f &position) {
    m_pointHandleRenderer->addPoint(m_foregroundColor, position);
}

void RenderService::renderHandleHighlight(const vm::vec3f &position) {
    m_pointHandleRenderer->addHighlight(m_foregroundColor, position);
}

void RenderService::renderHandles(const std::vector<vm::segment3f> &positions) {
    for (const vm::segment3f &position : positions)
        renderHandle(position);
}

void RenderService::renderHandle(const vm::segment3f &position) {
    m_primitiveRenderer->renderLine(m_foregroundColor, m_lineWidth, m_occlusionPolicy, position.start(), position.end());
    renderHandle(position.center());
}

void RenderService::renderHandleHighlight(const vm::segment3f &position) {
    m_primitiveRenderer->renderLine(m_foregroundColor, 2.0f * m_lineWidth, m_occlusionPolicy, position.start(), position.end());
    renderHandleHighlight(position.center());
}

void RenderService::renderHandles(const std::vector<vm::polygon3f> &positions) {
    for (const vm::polygon3f &position : positions)
        renderHandle(position);
}

void RenderService::renderHandle(const vm::polygon3f &position) {
    setShowBackfaces();
    m_primitiveRenderer->renderFilledPolygon(mixAlpha(m_foregroundColor, 0.07f), m_occlusionPolicy, m_cullingPolicy, position.vertices());
    renderHandle(position.center());
    setCullBackfaces();
}

void RenderService::renderHandleHighlight(const vm::polygon3f &position) {
    m_primitiveRenderer->renderPolygon(m_foregroundColor, 2.0f * m_lineWidth, m_occlusionPolicy, position.vertices());
    renderHandleHighlight(position.center());
}

void RenderService::renderLine(const vm::vec3f &start, const vm::vec3f &end) {
    m_primitiveRenderer->renderLine(m_foregroundColor, m_lineWidth, m_occlusionPolicy, start, end);
}

void RenderService::renderLines(const std::vector<vm::vec3f> &positions) {
    m_primitiveRenderer->renderLines(m_foregroundColor, m_lineWidth, m_occlusionPolicy, positions);
}

void RenderService::renderDashedLines(const std::vector<vm::vec3f> &positions, int factor, unsigned short pattern) {
    m_primitiveRenderer->renderDashedLines(m_foregroundColor, m_lineWidth, m_occlusionPolicy, positions, factor, pattern);
}

void RenderService::renderLineStrip(const std::vector<vm::vec3f> &positions) {
    m_primitiveRenderer->renderLineStrip(m_foregroundColor, m_lineWidth, m_occlusionPolicy, positions);
}

void RenderService::renderCoordinateSystem(const vm::bbox3f &bounds) {
    const Color &x = modifyAlpha(pref(Preferences::XAxisColor), 0.5f);
    const Color &y = modifyAlpha(pref(Preferences::YAxisColor), 0.5f);
    const Color &z = modifyAlpha(pref(Preferences::ZAxisColor), 0.5f);

    m_lineWidth = 1.5f;

    if (m_renderContext.render2D()) {
        const Camera &camera = m_renderContext.camera();
        switch (vm::find_abs_max_component(camera.direction())) {
            case vm::axis::x:m_primitiveRenderer->renderCoordinateSystemYZ(y, z, m_lineWidth, m_occlusionPolicy, bounds);
                break;
            case vm::axis::y:m_primitiveRenderer->renderCoordinateSystemXZ(x, z, m_lineWidth, m_occlusionPolicy, bounds);
                break;
            default:m_primitiveRenderer->renderCoordinateSystemXY(x, y, m_lineWidth, m_occlusionPolicy, bounds);
                break;
        }
    } else {
        m_primitiveRenderer->renderCoordinateSystem3D(x, y, z, m_lineWidth, m_occlusionPolicy, bounds);
    }
}

void RenderService::renderPolygonOutline(const std::vector<vm::vec3f> &positions) {
    m_primitiveRenderer->renderPolygon(m_foregroundColor, m_lineWidth, m_occlusionPolicy, positions);
}

void RenderService::renderFilledPolygon(const std::vector<vm::vec3f> &positions) {
    m_primitiveRenderer->renderFilledPolygon(m_foregroundColor, m_occlusionPolicy, m_cullingPolicy, positions);
}

void RenderService::renderBounds(const vm::bbox3f &bounds) {
    const vm::vec3f p1(bounds.min.x(), bounds.min.y(), bounds.min.z());
    const vm::vec3f p2(bounds.min.x(), bounds.min.y(), bounds.max.z());
    const vm::vec3f p3(bounds.min.x(), bounds.max.y(), bounds.min.z());
    const vm::vec3f p4(bounds.min.x(), bounds.max.y(), bounds.max.z());
    const vm::vec3f p5(bounds.max.x(), bounds.min.y(), bounds.min.z());
    const vm::vec3f p6(bounds.max.x(), bounds.min.y(), bounds.max.z());
    const vm::vec3f p7(bounds.max.x(), bounds.max.y(), bounds.min.z());
    const vm::vec3f p8(bounds.max.x(), bounds.max.y(), bounds.max.z());

    std::vector<vm::vec3f> positions;
    positions.reserve(12 * 2);
    positions.push_back(p1);
    positions.push_back(p2);
    positions.push_back(p1);
    positions.push_back(p3);
    positions.push_back(p1);
    positions.push_back(p5);
    positions.push_back(p2);
    positions.push_back(p4);
    positions.push_back(p2);
    positions.push_back(p6);
    positions.push_back(p3);
    positions.push_back(p4);
    positions.push_back(p3);
    positions.push_back(p7);
    positions.push_back(p4);
    positions.push_back(p8);
    positions.push_back(p5);
    positions.push_back(p6);
    positions.push_back(p5);
    positions.push_back(p7);
    positions.push_back(p6);
    positions.push_back(p8);
    positions.push_back(p7);
    positions.push_back(p8);

    if (pref(Preferences::SelectionBoundsDashedLines)) {
        renderDashedLines(positions, pref(Preferences::SelectionBoundsDashedSize), (GLushort) pref(Preferences::SelectionBoundsPattern));
    } else {
        renderLines(positions);
    }
}

void RenderService::renderCircle(const vm::vec3f &position, const vm::axis::type normal, const size_t segments, const float radius, const vm::vec3f &startAxis, const vm::vec3f &endAxis) {
    const std::pair<float, float> angles = startAngleAndLength(normal, startAxis, endAxis);
    renderCircle(position, normal, segments, radius, angles.first, angles.second);
}

void RenderService::renderCircle(const vm::vec3f &position, const vm::axis::type normal, const size_t segments, const float radius, const float startAngle, const float angleLength) {
    const std::vector<vm::vec3f> positions = circle2D(radius, normal, startAngle, angleLength, segments) + position;
    m_primitiveRenderer->renderLineStrip(m_foregroundColor, m_lineWidth, m_occlusionPolicy, positions);
}

void RenderService::renderFilledCircle(const vm::vec3f &position, const vm::axis::type normal, const size_t segments, const float radius, const vm::vec3f &startAxis, const vm::vec3f &endAxis) {
    const std::pair<float, float> angles = startAngleAndLength(normal, startAxis, endAxis);
    renderFilledCircle(position, normal, segments, radius, angles.first, angles.second);
}

void RenderService::renderFilledCircle(const vm::vec3f &position, const vm::axis::type normal, const size_t segments, const float radius, const float startAngle, const float angleLength) {
    const std::vector<vm::vec3f> positions = circle2D(radius, normal, startAngle, angleLength, segments) + position;
    m_primitiveRenderer->renderFilledPolygon(m_foregroundColor, m_occlusionPolicy, m_cullingPolicy, positions);
}

void RenderService::flush() {
    m_renderBatch.addOneShot(m_primitiveRenderer.release());
    m_renderBatch.addOneShot(m_pointHandleRenderer.release());
    m_renderBatch.addOneShot(m_textEntityRenderer.release());
}

float RenderService::distanceToEntity(const Camera &camera, const Model::EntityNode *entityNode) {
    auto center = entityNode->logicalBounds().center();
    auto position = vm::vec3f(center.x(), center.y(), center.z());
    return camera.perpendicularDistanceTo(position);
}


} // namespace Renderer
} // namespace TrenchBroom
