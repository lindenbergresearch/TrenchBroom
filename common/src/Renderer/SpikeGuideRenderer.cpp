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

#include "SpikeGuideRenderer.h"

#include "Model/BrushFace.h"
#include "Model/BrushFaceHandle.h"
#include "Model/BrushNode.h"
#include "Model/Hit.h"
#include "Model/HitFilter.h"
#include "Model/PickResult.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/ActiveShader.h"
#include "Renderer/Camera.h"
#include "Renderer/PrimType.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderUtils.h"
#include "Renderer/Shaders.h"
#include "View/MapDocument.h"

#include <vm/forward.h>
#include <vm/ray.h>
#include <vm/vec.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>

namespace TrenchBroom::Renderer {

SpikeGuideRenderer::SpikeGuideRenderer() : m_valid(false) {}
void SpikeGuideRenderer::setColor(const Color& color) { m_color = color; }

void SpikeGuideRenderer::add(const vm::ray3& ray, const FloatType length, const std::shared_ptr<View::MapDocument>& document) {
    // A corner remains visible even when intersection-only mode hides its guides.
    addPoint(ray.origin);
    Model::PickResult pickResult = Model::PickResult::byDistance();
    document->pick(ray, pickResult);

    using namespace Model::HitFilters;
    const auto& hit = pickResult.first(type(Model::BrushNode::BrushHitType) && minDistance(1.0));

    if (hit.isMatch()) {
        if (hit.distance() <= length) {
            const auto& face = hit.target<Model::BrushFaceHandle>().face();
            addSurfacePoint(hit.hitPoint(), face.normal());
        }
        addSpike(ray, vm::min(length, hit.distance()));
    }
    else if (!pref(Preferences::SelectionBoundsIntersectionMode)) { addSpike(ray, length); }

    m_valid = false;
}

void SpikeGuideRenderer::clear() {
    m_spikeVertices.clear();
    m_points.clear();
    m_surfacePoints.clear();
    m_spikeArray = VertexArray();
    m_pointArray = VertexArray();
    m_surfacePointArray = VertexArray();
    m_valid = true;
}

void SpikeGuideRenderer::doPrepareVertices(VboManager& vboManager) {
    if (!m_valid) validate();
    m_pointArray.prepare(vboManager);
    m_surfacePointArray.prepare(vboManager);
    m_spikeArray.prepare(vboManager);
}

void SpikeGuideRenderer::doRender(RenderContext& renderContext) {
    const auto& viewport = renderContext.camera().viewport();
    if (viewport.width <= 0 || viewport.height <= 0) return;

    const auto scale = renderContext.dpiScale();

    ActiveShader shader(renderContext.shaderManager(), Shaders::SelectionGuideShader);
    shader.set("ViewportSize", vm::vec2f(float(viewport.width), float(viewport.height)) * scale);
    setOverlaySizeScaling(shader, renderContext);
    const auto dashSize = float(std::clamp(pref(Preferences::SelectionBoundsDashedSize), 1, 256));
    shader.set("DashSize", dashSize * scale);

    // A shared monotonic clock keeps views in sync and avoids float precision loss
    // after long editing sessions. Zero speed leaves both styles fully static.
    static const auto animationStart = std::chrono::steady_clock::now();
    const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - animationStart).count();
    const auto configuredSpeed = pref(Preferences::SelectionBoundsAnimationSpeed);
    const auto speed = std::isfinite(configuredSpeed) ? std::clamp(configuredSpeed, 0.0f, 100.0f) : 0.0f;
    shader.set("DashOffset", float(std::fmod(seconds * speed * 32.0, dashSize * 16.0)) * scale);
    const auto pulse = speed > 0.0f
        ? float(0.725 + 0.275 * std::cos(std::fmod(seconds * speed, 1.0) * 2.0 * vm::Cd::pi()))
        : 1.0f;
    shader.set("PulseAlpha", pulse);

    const auto pattern = static_cast<unsigned short>(pref(Preferences::SelectionBoundsPattern));
    for (size_t group = 0; group < 4; ++group) {
        auto bits = vm::vec4f{};
        for (size_t bit = 0; bit < 4; ++bit) bits[bit] = float((pattern >> (group * 4 + bit)) & 1);
        shader.set("DashPattern[" + std::to_string(group) + "]", bits);
    }

    glAssert(glPushAttrib(GL_ENABLE_BIT | GL_POLYGON_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT));
    glAssert(glEnable(GL_BLEND));
    glAssert(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    glAssert(glDisable(GL_CULL_FACE));
    glAssert(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
    glAssert(glEnable(GL_DEPTH_TEST));
    glAssert(glDepthFunc(GL_LEQUAL));
    glAssert(glDepthMask(GL_FALSE));
    glSetEdgeOffset(1.0);

    shader.set("MarkerMode", 0);
    shader.set("Color", m_color);
    shader.set("Width", std::max(0.0f, pref(Preferences::SelectionBoundsLineWidth)) * scale);
    shader.set("Dashed", pref(Preferences::SelectionBoundsDashedLines) && pattern != 0xffff);
    if (pref(Preferences::SelectionBoundsLineWidth) > 0.0f) m_spikeArray.render(PrimType::Triangles);

    shader.set("MarkerMode", 1);
    shader.set("Color", pref(Preferences::SelectionBoundsPointColor));
    shader.set("Width", std::max(0.0f, pref(Preferences::SelectionBoundsPointSize)) * scale);
    shader.set("Dashed", false);
    if (pref(Preferences::SelectionBoundsPointSize) > 0.0f) m_pointArray.render(PrimType::Triangles);

    // These quads lie in the hit face's plane; the fragment shader cuts out an X.
    shader.set("MarkerMode", 2);
    if (pref(Preferences::SelectionBoundsPointSize) > 0.0f) m_surfacePointArray.render(PrimType::Triangles);

    glAssert(glPopAttrib());
}

void SpikeGuideRenderer::addPoint(const vm::vec3& position) {
    const auto point = vm::vec3f(position);
    if (std::find(m_points.begin(), m_points.end(), point) == m_points.end()) m_points.push_back(point);
}

void SpikeGuideRenderer::addSurfacePoint(const vm::vec3& position, const vm::vec3& normal) {
    const auto point = SurfacePoint{vm::vec3f(position), vm::vec3f(normal)};
    const auto found = std::find_if(m_surfacePoints.begin(), m_surfacePoints.end(), [&](const auto& other) {
        return other.position == point.position && other.normal == point.normal;
    });
    if (found == m_surfacePoints.end()) m_surfacePoints.push_back(point);
}

void SpikeGuideRenderer::addSpike(const vm::ray3& ray, const FloatType length) {
    if (length <= 0.0) return;

    const auto start = vm::vec3f(ray.origin);
    const auto end = vm::vec3f(vm::point_at_distance(ray, length));
    for (const auto& corner: {
             vm::vec2f(-1, -1),
             vm::vec2f(1, -1),
             vm::vec2f(1, 1),
             vm::vec2f(-1, -1),
             vm::vec2f(1, 1),
             vm::vec2f(-1, 1)
         }) { m_spikeVertices.emplace_back(start, end, corner); }
}

// void SpikeGuideRenderer::addSpike(const vm::ray3 &ray, const FloatType length, const FloatType maxLength) {
//     m_spikeVertices.emplace_back(vm::vec3f(ray.origin), m_color);
//
//     auto stripLength = 2.0;
//     auto index = 1;
//     Color col2 = Color(0.0f, 0.0f, 1.0f, 1.0f);
//
//     while (stripLength * index++ <= length) {
//         m_spikeVertices.emplace_back(vm::vec3f(vm::point_at_distance(ray, stripLength * index)), m_color);
//     }
// }


void SpikeGuideRenderer::validate() {
    auto pointVertices = std::vector<GuideVertex>{};
    pointVertices.reserve(m_points.size() * 6);
    for (const auto& point: m_points) {
        for (const auto& corner: {
                 vm::vec2f(-1, -1),
                 vm::vec2f(1, -1),
                 vm::vec2f(1, 1),
                 vm::vec2f(-1, -1),
                 vm::vec2f(1, 1),
                 vm::vec2f(-1, 1)
             }) { pointVertices.emplace_back(point, point, corner); }
    }
    m_pointArray = VertexArray::move(std::move(pointVertices));
    auto surfaceVertices = std::vector<GuideVertex>{};
    surfaceVertices.reserve(m_surfacePoints.size() * 6);
    for (const auto& point: m_surfacePoints) {
        for (const auto& corner: {
                 vm::vec2f(-1, -1), vm::vec2f(1, -1), vm::vec2f(1, 1),
                 vm::vec2f(-1, -1), vm::vec2f(1, 1), vm::vec2f(-1, 1)
             }) { surfaceVertices.emplace_back(point.position, point.normal, corner); }
    }
    m_surfacePointArray = VertexArray::move(std::move(surfaceVertices));
    m_spikeArray = VertexArray::copy(m_spikeVertices);
    m_valid = true;
}
}
