#include "Renderer/GL.h"
#include "Error.h"
#include "Model/BrushBuilder.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/ActiveShader.h"
#include "Renderer/FaceRenderer.h"
#include "Renderer/GridRenderer.h"
#include "Renderer/PerspectiveCamera.h"
#include "Renderer/RenderContext.h"
#include "Renderer/Shaders.h"
#include "View/GLContextManager.h"
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <catch2/catch.hpp>
#include <kdl/result.h>
#include <vm/mat_ext.h>

namespace TrenchBroom::Renderer {
namespace {
void drawQuad(const vm::vec3f& origin, const vm::vec3f& u, const vm::vec3f& v) {
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    for (const auto& p : {origin, origin + u, origin + u + v, origin + v})
        glVertex3f(p.x(), p.y(), p.z());
    glEnd();
}

std::array<unsigned char, 4> pixel(int x, int y) {
    std::array<unsigned char, 4> rgba{};
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    return rgba;
}

void identityMatrices() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}
}

TEST_CASE("Only axis aligned cuboids coincide with their brush bounds", "[bounds]") {
    const auto worldBounds = vm::bbox3(8192.0);
    const Model::BrushBuilder builder(Model::MapFormat::Standard, worldBounds);
    auto brush = builder.createCuboid(vm::vec3(32, 64, 128), "").value();
    CHECK(brush.isAxisAlignedBox());
    REQUIRE(brush.transform(worldBounds, vm::rotation_matrix(0.0, 0.0, 0.4), false).is_success());
    CHECK_FALSE(brush.isAxisAlignedBox());
}

TEST_CASE("Grid colors and selection bounds render correctly on the GPU", "[grid-bounds][.gl]") {
    QSurfaceFormat format;
    format.setVersion(2, 1);
    QOpenGLContext gl;
    gl.setFormat(format);
    REQUIRE(gl.create());
    QOffscreenSurface surface;
    surface.setFormat(gl.format());
    surface.create();
    REQUIRE(gl.makeCurrent(&surface));
    View::GLContextManager graphics;
    REQUIRE(graphics.initialize());
    while (glGetError() != GL_NO_ERROR) {}
    QOpenGLFramebufferObject target(129, 129);
    REQUIRE(target.isValid());
    REQUIRE(target.bind());
    glViewport(0, 0, 129, 129);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0, 0, 0, 0);
    identityMatrices();

    {
        INFO("Grid colors follow lines rather than the coordinate they hold constant");
        glClear(GL_COLOR_BUFFER_BIT);
        identityMatrices();
        ActiveShader shader(graphics.shaderManager(), Shaders::Grid2DShader);
        shader.set("GridXAxisColor", Color(1.0f, 0.0f, 0.0f, 1.0f));
        shader.set("GridYAxisColor", Color(0.0f, 1.0f, 0.0f, 1.0f));
        shader.set("GridZAxisColor", Color(0.0f, 0.0f, 1.0f, 1.0f));
        shader.set("GridColor", Color(0.5f, 0.5f, 0.5f, 1.0f));
        shader.set("RenderGrid", true);
        shader.set("GridSize", 0.5f);
        shader.set("GridAlpha", 1.0f);
        shader.set("GridWidth", 2.0f);
        shader.set("MajorDivisionSize", 0.0f);
        shader.set("CameraZoom", 1000.0f);
        shader.set("Normal", vm::vec3f(0, 0, 1));
        const auto drawXY = [] { drawQuad({-1,-1,0}, {2,0,0}, {0,2,0}); };
        shader.set("GridAxisColorMix", 0.0f);
        drawXY();
        CHECK(pixel(80,64)[0] == Approx(128).margin(1));
        CHECK(pixel(80,64)[1] == Approx(128).margin(1));
        shader.set("GridAxisColorMix", 0.5f);
        drawXY();
        CHECK(pixel(80,64)[0] == Approx(191).margin(1));
        CHECK(pixel(80,64)[1] == Approx(64).margin(1));
        shader.set("GridAxisColorMix", 1.0f);
        drawXY();
        CHECK(pixel(80,64) == std::array<unsigned char, 4>{255,0,0,255});
        CHECK(pixel(64,80) == std::array<unsigned char, 4>{0,255,0,255});

        glRotatef(-90, 1, 0, 0);
        shader.set("Normal", vm::vec3f(0, 1, 0));
        drawQuad({-1,0,-1}, {2,0,0}, {0,0,2});
        CHECK(pixel(80,64) == std::array<unsigned char, 4>{255,0,0,255});
        CHECK(pixel(64,80) == std::array<unsigned char, 4>{0,0,255,255});
    }

    {
        INFO("Grid angle shading is optional");
        glClear(GL_COLOR_BUFFER_BIT);
        identityMatrices();
        PerspectiveCamera camera(90, 1, 4096, {0,0,129,129}, {2,0,1}, {-1,0,0}, {0,0,1});
        RenderContext context(RenderMode::Render3D, camera, graphics.fontManager(), graphics.shaderManager());
        identityMatrices();
        ActiveShader shader(graphics.shaderManager(), Shaders::FaceShader);
        configureFaceShader(shader, context);
        shader.set("Color", Color(0.0f, 0.0f, 0.0f, 1.0f));
        shader.set("ApplyTexture", false);
        shader.set("EnableLighting", false);
        shader.set("ShowFog", false);
        shader.set("ShowSoftMapBounds", false);
        shader.set("ShadeFaces", true);
        shader.set("ShadeLevel", 1.0f);
        shader.set("RenderGrid", true);
        shader.set("GridAxisColorMix", 0.0f);
        shader.set("GridColor", vm::vec3f(1,1,1));
        shader.set("GridSize", 0.5f);
        shader.set("GridAlpha", 1.0f);
        shader.set("GridLineWidth", 2.0f);
        shader.set("MajorDivisionSize", 0.0f);
        shader.set("Alpha", 1.0f);
        shader.set("ShadeGrid", false);
        drawQuad({-1,-1,0}, {2,0,0}, {0,2,0});
        CHECK(pixel(80,64)[0] == 255);
        shader.set("ShadeGrid", true);
        drawQuad({-1,-1,0}, {2,0,0}, {0,2,0});
        CHECK(pixel(80,64)[0] == Approx(126).margin(3));
    }

    {
        INFO("Bounds emphasize real edges and corners without showing a triangle seam");
        glClear(GL_COLOR_BUFFER_BIT);
        identityMatrices();
        ActiveShader shader(graphics.shaderManager(), Shaders::SelectionBoundsShader);
        shader.set("BoundsMin", vm::vec3f(0,0,0));
        shader.set("BoundsMax", vm::vec3f(1,1,1));
        shader.set("CameraPosition", vm::vec3f(2,0.5,1));
        shader.set("Color", Color(0.3f, 0.5f, 0.7f, 0.15f));
        shader.set("DpiScale", 1.0f);
        shader.set("ShadeLevel", 0.0f);
        glTranslatef(-0.8f, -0.8f, 0);
        glScalef(1.6f, 1.6f, 1);
        const auto drawFace = [] { drawQuad({0,0,0}, {1,0,0}, {0,1,0}); };
        drawFace();
        const auto center = pixel(64,64);
        const auto edge = pixel(13,64);
        const auto corner = pixel(13,13);
        CHECK(center[3] == Approx(38).margin(1));
        CHECK(pixel(62,65)[3] == center[3]);
        CHECK(edge[3] > center[3]);
        CHECK(corner[3] > edge[3]);
        shader.set("ShadeLevel", 1.0f);
        drawFace();
        CHECK(pixel(64,64)[0] < center[0]);
        CHECK(pixel(64,64)[3] == center[3]);
    }
    CHECK(glGetError() == GL_NO_ERROR);
}
} // namespace TrenchBroom::Renderer
