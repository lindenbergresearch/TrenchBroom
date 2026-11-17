/*
 Copyright (C) 2026 TrenchBroom contributors

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

#include "Renderer/GL.h"
#include "Error.h"
#include "IO/GameConfigParser.h"
#include "Logger.h"
#include "Model/GameImpl.h"
#include "Model/WorldNode.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/ActiveShader.h"
#include "Renderer/FaceRenderer.h"
#include "Renderer/PerspectiveCamera.h"
#include "Renderer/RenderBatch.h"
#include "Renderer/RenderContext.h"
#include "Renderer/Shaders.h"
#include "Renderer/SkyRenderer.h"
#include "View/GLContextManager.h"
#include "View/MapDocumentCommandFacade.h"
#include "View/ViewEditor.h"
#include <QCheckBox>
#include <QDir>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QTemporaryDir>
#include <catch2/catch.hpp>
#include <kdl/result.h>

namespace TrenchBroom::Renderer {
namespace {
struct SkyScene {
    QTemporaryDir directory;
    NullLogger logger;
    Model::GameConfig config = IO::GameConfigParser(R"({
        "version": 8, "name": "SkyTest",
        "fileformats": [{"format": "Standard"}],
        "filesystem": {"searchpath": ".", "packageformat": {"extension": ".pak", "format": "idpak"} },
        "textures": {"root": "textures", "format": {"extensions": [".png"], "format": "image"} },
        "entities": {"definitions": ["Generic.fgd"], "defaultcolor": "0.6 0.6 0.6 1.0"}
    })", std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path()
        / "app/resources/games/Generic/GameConfig.cfg").parse();
    std::shared_ptr<Model::GameImpl> game;
    std::shared_ptr<View::MapDocument> document = View::MapDocumentCommandFacade::newMapDocument();
    std::unique_ptr<View::ViewEditor> editor;
    const std::array<QColor, 6> colors{Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::yellow, Qt::magenta};
    SkyScene() {
        REQUIRE(directory.isValid());
        REQUIRE(QDir().mkpath(directory.path() + "/gfx/env"));
        constexpr std::array suffixes{"rt", "bk", "lf", "ft", "up", "dn"};
        for (size_t i = 0; i < 6; ++i) {
            QImage image(16, 16, QImage::Format_RGB32);
            image.fill(colors[i]);
            REQUIRE(image.save(directory.path() + "/gfx/env/smoke" + suffixes[i] + ".png"));
        }
        game = std::make_shared<Model::GameImpl>(config, directory.path().toStdString(), logger);
        editor = std::make_unique<View::ViewEditor>(document, nullptr);
        REQUIRE(document->newDocument(Model::MapFormat::Standard, vm::bbox3(8192.0), game).is_success());
        document->deselectAll();
        REQUIRE(document->setProperty("sky", "smoke"));
    }
};
}

TEST_CASE("Skybox View Details checkbox updates the registered boolean setting", "[sky]") {
    setPref(Preferences::ShowSky, false);
    SkyScene scene;
    auto* toggle = scene.editor->findChild<QCheckBox*>("showMapSkybox");
    REQUIRE(toggle);
    CHECK_FALSE(toggle->isChecked());
    CHECK(Preferences::staticPreferencesMap().at("Map view/Show Sky") == &Preferences::ShowSky);
    toggle->click();
    CHECK(pref(Preferences::ShowSky));
    toggle->click();
    CHECK_FALSE(pref(Preferences::ShowSky));
}

// Explicit opt-in: requires a native OpenGL compatibility context (not Qt's offscreen platform).
TEST_CASE("Skybox renders on the GPU and preserves material rendering", "[sky][.gl]") {
    QSurfaceFormat format;
    format.setVersion(2, 1);
    format.setDepthBufferSize(24);
    QOpenGLContext gl;
    gl.setFormat(format);
    REQUIRE(gl.create());
    QOffscreenSurface surface;
    surface.setFormat(gl.format());
    surface.create();
    REQUIRE(gl.makeCurrent(&surface));
    View::GLContextManager graphics;
    REQUIRE(graphics.initialize()); // Compiles ALL application shaders, including Face and Patch.
    while (glGetError() != GL_NO_ERROR) {}
    QOpenGLFramebufferObject target(65, 65, QOpenGLFramebufferObject::CombinedDepthStencil);
    REQUIRE(target.isValid());
    REQUIRE(target.bind());
    glViewport(0, 0, 65, 65);
    SkyScene scene;
    SkyRenderer sky(scene.document);
    PerspectiveCamera camera(90, 1, 4096, {0, 0, 65, 65}, {0, 0, 0}, {1, 0, 0}, {0, 0, 1});
    setPref(Preferences::ShowSky, true);
    GLuint cached = 0;
    const std::array directions{vm::vec3f{1,0,0}, vm::vec3f{0,1,0}, vm::vec3f{-1,0,0},
        vm::vec3f{0,-1,0}, vm::vec3f{0,0,1}, vm::vec3f{0,0,-1}};
    for (size_t i = 0; i < directions.size(); ++i) {
        camera.setDirection(directions[i], i < 4 ? vm::vec3f{0,0,1} : vm::vec3f{0,1,0});
        RenderContext context(RenderMode::Render3D, camera, graphics.fontManager(), graphics.shaderManager());
        RenderBatch batch(graphics.vboManager());
        sky.render(context, batch);
        REQUIRE(context.skyTexture() != 0);
        if (cached) CHECK(context.skyTexture() == cached);
        cached = context.skyTexture();
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        batch.render(context);
        CHECK(target.toImage().pixelColor(32, 32) == scene.colors[i]);
        CHECK(glIsEnabled(GL_DEPTH_TEST));
        GLboolean writes;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &writes);
        CHECK(writes == GL_TRUE);
        CHECK(glGetError() == GL_NO_ERROR);
        camera.moveTo({100, 200, 300}); // Translation must not change the sky direction.
    }
    camera.moveTo({0, 0, -100});
    RenderContext context(RenderMode::Render3D, camera, graphics.fontManager(), graphics.shaderManager());
    context.setSkyTexture(cached);
    {
        SkyTextureBinding binding(cached);
        ActiveShader shader(graphics.shaderManager(), Shaders::FaceShader);
        configureFaceShader(shader, context);
        shader.set("RenderSky", true);
        shader.set("EnableLighting", true);
        shader.set("ShowFog", true);
        shader.set("Brightness", 10.0f);
        shader.set("ApplyTinting", false);
        shader.set("GrayScale", false);
        shader.set("Alpha", 1.0f);
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        auto quad = [] {
            glBegin(GL_QUADS);
            glVertex3f(-1,-1,0); glVertex3f(1,-1,0); glVertex3f(1,1,0); glVertex3f(-1,1,0);
            glEnd();
        };
        quad();
        CHECK(target.toImage().pixelColor(32, 32) == QColor(Qt::yellow));
        shader.set("RenderSky", false);
        shader.set("EnableLighting", false);
        shader.set("ShowFog", false);
        shader.set("RenderGrid", false);
        shader.set("ShadeFaces", false);
        shader.set("Brightness", 1.0f);
        shader.set("ApplyTexture", false);
        shader.set("Color", Color{0.0f, 1.0f, 0.0f, 1.0f});
        quad();
        CHECK(target.toImage().pixelColor(32, 32) == QColor(Qt::green));
        CHECK(glGetError() == GL_NO_ERROR);
    }
    {
        // A disabled sky must also leave the ordinary Face shader usable with no cube bound.
        ActiveShader shader(graphics.shaderManager(), Shaders::FaceShader);
        configureFaceShader(shader, context);
        shader.set("ApplyTexture", false);
        shader.set("RenderGrid", false);
        shader.set("ShadeFaces", false);
        shader.set("EnableLighting", false);
        shader.set("Brightness", 1.0f);
        shader.set("Color", Color{0.0f, 0.0f, 1.0f, 1.0f});
        glBegin(GL_TRIANGLES);
        glVertex3f(-1,-1,0); glVertex3f(3,-1,0); glVertex3f(-1,3,0);
        glEnd();
        CHECK(target.toImage().pixelColor(32, 32) == QColor(Qt::blue));
        CHECK(glGetError() == GL_NO_ERROR);
    }
    {
        RenderContext flat(RenderMode::Render2D, camera, graphics.fontManager(), graphics.shaderManager());
        RenderBatch batch(graphics.vboManager());
        sky.render(flat, batch);
        CHECK(flat.skyTexture() == 0);
    }
    {
        setPref(Preferences::ShowSky, false);
        RenderBatch batch(graphics.vboManager());
        sky.render(context, batch);
        CHECK(context.skyTexture() == 0);
    }
    setPref(Preferences::ShowSky, true);
    REQUIRE(scene.document->setProperty("sky", "missing"));
    {
        RenderBatch batch(graphics.vboManager());
        sky.render(context, batch);
        CHECK(context.skyTexture() == 0);
    }
    REQUIRE(scene.document->setProperty("sky", "smoke"));
    {
        RenderBatch batch(graphics.vboManager());
        sky.render(context, batch);
        CHECK(context.skyTexture() != 0);
    }
    setPref(Preferences::ShowSky, false);
}
} // namespace TrenchBroom::Renderer
