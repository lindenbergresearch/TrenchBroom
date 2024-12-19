/*
 * Copyright (C) 2010-2017 Kristian Duske
 *
 * This file is part of TrenchBroom.
 *
 * TrenchBroom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * TrenchBroom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file PerspectiveCamera.h
 * @brief Defines the PerspectiveCamera class for 3D rendering.
 *
 * This file contains the declaration of the PerspectiveCamera class, which is a
 * specialized Camera with perspective projection capabilities.
 */

#pragma once

#include "Renderer/Camera.h"

#include <vm/forward.h>

namespace TrenchBroom {

namespace Renderer {
/**
 * @class PerspectiveCamera
 * @brief Extends the Camera class to support perspective projection.
 *
 * This class adds functionality for handling perspective projections, including
 * field of view (FOV) and related frustum calculations.
 */
class PerspectiveCamera : public Camera {
  private:
    float m_fov; ///< The field of view angle in degrees.

  public:
    /**
     * @brief Default constructor. Initializes with default FOV.
     */
    PerspectiveCamera();

    /**
     * @brief Parameterized constructor.
     * @param fov The field of view angle in degrees.
     * @param nearPlane The near clipping plane.
     * @param farPlane The far clipping plane.
     * @param viewport The viewport dimensions.
     * @param position The position of the camera.
     * @param direction The direction the camera is facing.
     * @param up The up vector of the camera.
     */
    PerspectiveCamera(float fov, float nearPlane, float farPlane, const Viewport &viewport,
                      const vm::vec3f &position, const vm::vec3f &direction, const vm::vec3f &up);

    /**
     * @brief Gets the field of view angle.
     * @return The field of view angle in degrees.
     */
    /**
     * @brief Retrieves the current field of view (FOV).
     * @return The field of view in degrees.
     */
    float fov() const;

    /**
     * @brief Computes the field of view adjusted for the current zoom level.
     * @return The adjusted field of view angle.
     */
    float zoomedFov() const;

    /**
     * @brief Sets a new field of view angle.
     * @param fov The new field of view angle in degrees.
     */
    /**
     * @brief Sets a new field of view (FOV).
     * @param fov The desired field of view in degrees.
     */
    void setFov(float fov);

  private:
    /**
     * @brief Computes the zoom-adjusted FOV based on the given zoom and base FOV.
     * @param zoom The zoom factor.
     * @param fov The base field of view angle.
     * @return The adjusted field of view angle.
     */
    static float computeZoomedFov(float zoom, float fov);

    /**
     * @brief Gets the projection type as perspective.
     * @return The projection type.
     */
    ProjectionType doGetProjectionType() const override;

    /**
     * @brief Validates and updates the projection and view matrices.
     * @param projectionMatrix The projection matrix to update.
     * @param viewMatrix The view matrix to update.
     */
    void doValidateMatrices(vm::mat4x4f &projectionMatrix, vm::mat4x4f &viewMatrix) const override;

    /**
     * @brief Gets a picking ray based on a screen point.
     * @param point The point on the screen.
     * @return The corresponding picking ray in world space.
     */
    vm::ray3f doGetPickRay(const vm::vec3f &point) const override;

    /**
     * @brief Computes the frustum planes for the current view.
     * @param topPlane The top plane of the frustum.
     * @param rightPlane The right plane of the frustum.
     * @param bottomPlane The bottom plane of the frustum.
     * @param leftPlane The left plane of the frustum.
     */
    void doComputeFrustumPlanes(vm::plane3f &topPlane, vm::plane3f &rightPlane,
                                vm::plane3f &bottomPlane, vm::plane3f &leftPlane) const override;

    /**
     * @brief Renders the camera's frustum.
     * @param renderContext The rendering context.
     * @param vboManager The VBO manager.
     * @param size The size of the frustum.
     * @param color The color of the frustum.
     */
    void doRenderFrustum(RenderContext &renderContext, VboManager &vboManager,
                         float size, const Color &color) const override;

    /**
     * @brief Determines the distance to the frustum when picking.
     * @param size The size of the frustum.
     * @param ray The picking ray.
     * @return The distance to the frustum.
     */
    float doPickFrustum(float size, const vm::ray3f &ray) const override;

    /**
     * @brief Retrieves the vertices defining the frustum.
     * @param size The size of the frustum.
     * @param verts An array to store the vertices.
     */
    void getFrustumVertices(float size, vm::vec3f (&verts)[4]) const;

    /**
     * @brief Computes the frustum dimensions.
     * @return A 2D vector representing the frustum width and height.
     */
    vm::vec2f getFrustum() const;

    /**
     * @brief Computes the scaling factor for perspective transformations.
     * @param position The position in world space.
     * @return The scaling factor.
     */
    float doGetPerspectiveScalingFactor(const vm::vec3f &position) const override;

    /**
     * @brief Computes the distance to the viewport's frustum.
     * @return The distance to the frustum.
     */
    float viewportFrustumDistance() const;

    /**
     * @brief Checks if the given zoom level is valid.
     * @param zoom The zoom level to validate.
     * @return True if the zoom is valid, false otherwise.
     */
    bool isValidZoom(float zoom) const override;

    /**
     * @brief Updates the zoom level. (No-op in this implementation.)
     */
    void doUpdateZoom() override;
};

} // namespace Renderer
} // namespace TrenchBroom
