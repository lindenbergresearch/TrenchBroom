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

#include "Renderer/SkyRenderer.h"
#include "Assets/Texture.h"
#include "Error.h"
#include "Logger.h"
#include "PreferenceManager.h"
#include "IO/Skybox.h"
#include "Model/Game.h"
#include "Model/WorldNode.h"
#include "Preferences.h"
#include "Renderer/ActiveShader.h"
#include "Renderer/Camera.h"
#include "Renderer/PrimType.h"
#include "Renderer/RenderBatch.h"
#include "Renderer/RenderContext.h"
#include "Renderer/Shaders.h"
#include "View/Document/MapDocument.h"
#include <QOpenGLContext>
#include <QPointer>
#include <algorithm>
#include <cctype>
#include <map>
#include <kdl/result.h>

namespace TrenchBroom::Renderer {
namespace {
// Destruction may happen during document notifications without a GL context.
// Keep deletion in the owning share group, as with preview lightmap textures.
auto& garbage() {
    static auto* result = new std::map<QOpenGLContextGroup*, std::vector<GLuint>>;
    return *result;
}
void collectGarbage() {
    auto* group = QOpenGLContext::currentContext()->shareGroup();
    auto [it, inserted] = garbage().try_emplace(group);
    if (inserted) QObject::connect(group, &QObject::destroyed, [group] { garbage().erase(group); });
    if (!it->second.empty()) glDeleteTextures(GLsizei(it->second.size()), it->second.data());
    it->second.clear();
}
}

struct SkyRenderer::GpuSky {
    GLuint texture = 0;
    QPointer<QOpenGLContextGroup> group;
    ~GpuSky() { if (texture && group) garbage()[group].push_back(texture); }
};

bool isSkyTexture(const Assets::Texture* texture) {
    if (!texture) return false;
    if (texture->surfaceParms().contains("sky")) return true;
    const auto name = std::filesystem::path(texture->name()).filename().string();
    return name.size() >= 3 && std::tolower(static_cast<unsigned char>(name[0])) == 's'
        && std::tolower(static_cast<unsigned char>(name[1])) == 'k'
        && std::tolower(static_cast<unsigned char>(name[2])) == 'y';
}

SkyTextureBinding::SkyTextureBinding(const GLuint texture) : m_bound(texture != 0) {
    if (!m_bound) return;
    GLint active;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glActiveTexture(GL_TEXTURE2);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &m_previous);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    glActiveTexture(GLenum(active));
}
SkyTextureBinding::~SkyTextureBinding() {
    if (!m_bound) return;
    GLint active;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, GLuint(m_previous));
    glActiveTexture(GLenum(active));
}

SkyRenderer::SkyRenderer(std::weak_ptr<View::MapDocument> document)
    : m_document(std::move(document)), m_triangle(VertexArray::move(std::vector<GLVertexTypes::P2::Vertex>{
        GLVertexTypes::P2::Vertex{vm::vec2f(-1, -1)},
        GLVertexTypes::P2::Vertex{vm::vec2f(3, -1)},
        GLVertexTypes::P2::Vertex{vm::vec2f(-1, 3)}})) {}
SkyRenderer::~SkyRenderer() = default;
void SkyRenderer::invalidate() {
    m_dirty = true;
    m_gpu.reset();
}

void SkyRenderer::render(RenderContext& context, RenderBatch& batch) {
    context.setSkyTexture(0);
    if (!context.render3D()) return;
    collectGarbage();
    if (!pref(Preferences::ShowSky) || !context.showTextures() || !context.showFaces()) return;
    const auto document = m_document.lock();
    if (!document || !document->world() || !document->game()) return;
    const auto name = IO::skyboxName(document->world()->entity());
    if (m_dirty || m_name != name || (m_gpu && m_gpu->group != QOpenGLContext::currentContext()->shareGroup())) {
        m_gpu.reset();
        collectGarbage();
        m_name = name;
        m_dirty = false;
        const auto* fs = document->game()->fileSystem();
        if (!name.empty() && fs) {
            GLint limit;
            glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &limit);
            auto loaded = IO::loadSkybox(*fs, name, std::min(2048, int(limit)));
            if (loaded.is_success()) {
                auto faces = IO::skyboxCubeFaces(loaded.value());
                m_gpu = std::make_unique<GpuSky>();
                m_gpu->group = QOpenGLContext::currentContext()->shareGroup();
                glGenTextures(1, &m_gpu->texture);
                const auto binding = SkyTextureBinding(m_gpu->texture);
                GLint active;
                glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
                glActiveTexture(GL_TEXTURE2);
                glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
                glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
                for (size_t i = 0; i < faces.size(); ++i) {
                    const auto image = faces[i].convertToFormat(QImage::Format_RGBA8888);
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + GLenum(i), 0, GL_RGBA8,
                        image.width(), image.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, image.constBits());
                }
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                glPopClientAttrib();
                glActiveTexture(GLenum(active));
            } else {
                (void)loaded.if_error([&](const auto& error) { document->logger().warn(error.msg); });
            }
        }
    }
    if (m_gpu) {
        context.setSkyTexture(m_gpu->texture);
        batch.add(this);
    }
}

void SkyRenderer::doPrepareVertices(VboManager& manager) { m_triangle.prepare(manager); }
void SkyRenderer::doRender(RenderContext& context) {
    if (!m_gpu) return;
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    const auto binding = SkyTextureBinding(m_gpu->texture);
    ActiveShader shader(context.shaderManager(), Shaders::SkyboxShader);
    shader.set("SkyTexture", 2);
    const auto& camera = context.camera();
    shader.set("SkyForward", camera.direction());
    shader.set("SkyRight", camera.right() / camera.projectionMatrix()[0][0]);
    shader.set("SkyUp", camera.up() / camera.projectionMatrix()[1][1]);
    m_triangle.render(PrimType::Triangles);
    glPopAttrib();
}
} // namespace TrenchBroom::Renderer
