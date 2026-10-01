#include "PreviewLightmap.h"

#include "Assets/Texture.h"
#include "Renderer/ActiveShader.h"
#include "Renderer/PrimType.h"
#include "Renderer/RenderUtils.h"
#include "Renderer/VertexArray.h"

#include <QDebug>
// As in RenderView.cpp: GLEW must come first. Only Qt's context/share-group
// metadata is used here, not QOpenGLFunctions.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcpp"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#endif
#include <QOpenGLContext>
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <map>
#include <limits>

namespace TrenchBroom::Renderer {
namespace {
constexpr int AtlasSize = 512;
constexpr int MaxPages = 32;
using LitVertex = GLVertex<GLVertexAttributeTypes::P3, GLVertexAttributeTypes::N,
    GLVertexAttributeTypes::T02, GLVertexAttributeTypes::T12>;
struct Page {
    int height = 0;
    std::vector<float> pixels;
};
struct Draw {
    size_t surface, page;
    GLint first;
    GLsizei count;
};
struct Result {
    std::vector<LitVertex> vertices;
    std::vector<Page> pages;
    std::vector<Draw> draws;
};
struct Work {
    std::atomic<bool> cancelled{false};
    std::atomic<bool> done{false};
    std::unique_ptr<Result> result;
};

// Keep background work bounded even while several views or selection renderers
// request new lightmaps. Obsolete work checks cancellation at every sample row.
QThreadPool& lightmapPool() {
    static QThreadPool pool;
    static const auto initialized = [] { pool.setMaxThreadCount(2); return true; }();
    (void)initialized;
    return pool;
}

// Map/selection edits may destroy renderers without a current context. Defer
// texture deletion until this share group is current; never delete in a worker.
auto& textureGarbage() {
    static auto* garbage = new std::map<QOpenGLContextGroup*, std::vector<GLuint>>;
    return *garbage;
}
void collectTextures() {
    auto* group = QOpenGLContext::currentContext()->shareGroup();
    auto [it, inserted] = textureGarbage().try_emplace(group);
    if (inserted) QObject::connect(group, &QObject::destroyed, [group] { textureGarbage().erase(group); });
    auto& ids = it->second;
    if (!ids.empty()) glAssert(glDeleteTextures(static_cast<GLsizei>(ids.size()), ids.data()));
    ids.clear();
}
struct GpuResult {
    QPointer<QOpenGLContextGroup> group;
    VertexArray vertices;
    std::vector<GLuint> textures;
    std::vector<Draw> draws;
    bool grouped = false;
    ~GpuResult() {
        if (group) {
            auto& garbage = textureGarbage()[group];
            garbage.insert(garbage.end(), textures.begin(), textures.end());
        }
    }
};

std::unique_ptr<Result> bake(const std::vector<PreviewLightmap::Vertex>& vertices,
    const std::vector<PreviewLightmap::Surface>& surfaces, const PreviewLighting& lighting,
    const Work& work) {
    auto result = std::make_unique<Result>();
    result->vertices.reserve(vertices.size());
    // Use roughly Quake-sized luxels, increasing spacing for large scenes to
    // bound memory. Each individual chart has a one-texel replicated border.
    double estimated = 0;
    for (size_t i = 0; i + 2 < vertices.size(); i += 3) {
        const auto& a = getVertexComponent<0>(vertices[i]);
        estimated += (vm::length(getVertexComponent<0>(vertices[i + 1]) - a) / 16.0 + 4.0)
            * (vm::length(getVertexComponent<0>(vertices[i + 2]) - a) / 16.0 + 4.0);
    }
    const auto spacing = 16.0f * std::max(1.0f, static_cast<float>(std::sqrt(estimated / 1048576.0)));
    int x = 0, y = 0, rowHeight = 0;
    result->pages.emplace_back();
    for (size_t surfaceIndex = 0; surfaceIndex < surfaces.size(); ++surfaceIndex) {
        const auto& surface = surfaces[surfaceIndex];
        for (auto i = surface.first; i + 2 < surface.first + surface.count; i += 3) {
            if (work.cancelled.load(std::memory_order_relaxed)) return nullptr;
            const auto& a = getVertexComponent<0>(vertices[i]);
            const auto ab = getVertexComponent<0>(vertices[i + 1]) - a;
            const auto ac = getVertexComponent<0>(vertices[i + 2]) - a;
            const auto nu = surface.sky ? 1 : std::clamp(static_cast<int>(std::ceil(vm::length(ab) / spacing)), 1, 64);
            const auto nv = surface.sky ? 1 : std::clamp(static_cast<int>(std::ceil(vm::length(ac) / spacing)), 1, 64);
            const auto width = nu + 3, height = nv + 3;
            if (x + width > AtlasSize) { x = 0; y += rowHeight; rowHeight = 0; }
            if (y + height > AtlasSize) {
                if (result->pages.size() >= MaxPages) return nullptr;
                result->pages.emplace_back();
                x = y = rowHeight = 0;
            }
            auto& page = result->pages.back();
            page.height = std::max(page.height, y + height);
            page.pixels.resize(static_cast<size_t>(page.height * AtlasSize * 3), 0.0f);
            auto bounds = vm::bbox3f::builder{};
            bounds.add(a); bounds.add(a + ab); bounds.add(a + ac);
            const auto candidates = previewLightCandidates(lighting, bounds.bounds());
            for (int v = 0; v <= nv; ++v) {
                if (work.cancelled.load(std::memory_order_relaxed)) return nullptr;
                for (int u = 0; u <= nu; ++u) {
                    auto s = static_cast<float>(u) / nu;
                    auto t = static_cast<float>(v) / nv;
                    // Clamp samples outside the triangular chart to its edge,
                    // then nudge inwards to avoid sampling inside an adjacent wall.
                    if (s + t > 1.0f) { const auto sum = s + t; s /= sum; t /= sum; }
                    const auto inset = std::min(0.001f, 0.05f / std::max({vm::length(ab), vm::length(ac), 1.0f}));
                    s = s * (1.0f - 3.0f * inset) + inset;
                    t = t * (1.0f - 3.0f * inset) + inset;
                    const auto normal = getVertexComponent<1>(vertices[i]) * (1.0f - s - t)
                        + getVertexComponent<1>(vertices[i + 1]) * s + getVertexComponent<1>(vertices[i + 2]) * t;
                    // Store unexposed light in 0..255 units / 255. Gamma and editor
                    // exposure remain live shader controls and never trigger baking.
                    const auto color = surface.sky ? vm::vec3f::fill(1.0f)
                        : samplePreviewLighting(lighting, candidates, a + ab * s + ac * t, normal) / 255.0f;
                    const auto pixel = static_cast<size_t>(((y + v + 1) * AtlasSize + x + u + 1) * 3);
                    for (size_t channel = 0; channel < 3; ++channel)
                        page.pixels[pixel + channel] = std::clamp(color[channel], 0.0f, 65000.0f);
                }
            }
            // Replicate the chart border, including corners; filtering cannot
            // mix adjacent atlas charts or read uninitialized pixels.
            for (int v = 0; v < height; ++v) {
                for (int u = 0; u < width; ++u) {
                    if (u > 0 && u < width - 1 && v > 0 && v < height - 1) continue;
                    const auto src = static_cast<size_t>(((y + std::clamp(v, 1, height - 2)) * AtlasSize + x + std::clamp(u, 1, width - 2)) * 3);
                    const auto dst = static_cast<size_t>(((y + v) * AtlasSize + x + u) * 3);
                    for (size_t channel = 0; channel < 3; ++channel) page.pixels[dst + channel] = page.pixels[src + channel];
                }
            }
            const auto uv = vm::vec2f{(x + 1.5f) / AtlasSize, (y + 1.5f) / AtlasSize};
            const auto lightUVs = std::array{uv, uv + vm::vec2f{static_cast<float>(nu) / AtlasSize, 0},
                uv + vm::vec2f{0, static_cast<float>(nv) / AtlasSize}};
            for (size_t j = 0; j < 3; ++j) result->vertices.emplace_back(
                getVertexComponent<0>(vertices[i + j]), getVertexComponent<1>(vertices[i + j]),
                getVertexComponent<2>(vertices[i + j]), lightUVs[j]);
            const auto pageIndex = result->pages.size() - 1;
            if (result->draws.empty() || result->draws.back().surface != surfaceIndex || result->draws.back().page != pageIndex)
                result->draws.push_back({surfaceIndex, pageIndex, static_cast<GLint>(i), 0});
            result->draws.back().count += 3;
            x += width;
            rowHeight = std::max(rowHeight, height);
        }
    }
    return result;
}
}

struct PreviewLightmap::Impl {
    std::shared_ptr<const std::vector<Vertex>> vertices;
    std::shared_ptr<const std::vector<Surface>> surfaces;
    PreviewLighting lighting;
    std::chrono::steady_clock::time_point changed;
    uint64_t revision = 0;
    std::shared_ptr<Work> work;
    std::unique_ptr<Result> pending;
    std::unique_ptr<GpuResult> uploading, active;
    bool started = false;
    ~Impl() { if (work) work->cancelled = true; }
};

PreviewLightmap::PreviewLightmap(std::vector<Vertex> vertices, std::vector<Surface> surfaces)
    : m_impl{std::make_unique<Impl>()} {
    m_impl->vertices = std::make_shared<const std::vector<Vertex>>(std::move(vertices));
    m_impl->surfaces = std::make_shared<const std::vector<Surface>>(std::move(surfaces));
}
PreviewLightmap::~PreviewLightmap() = default;

void PreviewLightmap::update(const PreviewLighting& lighting) {
    auto& state = *m_impl;
    if (state.revision != lighting.revision) {
        const auto rebake = state.revision == 0 || state.lighting.lights != lighting.lights
            || state.lighting.minLight != lighting.minLight || state.lighting.occlusion != lighting.occlusion;
        state.revision = lighting.revision;
        state.lighting = lighting;
        if (rebake) {
            if (state.work) state.work->cancelled = true;
            state.work.reset();
            state.pending.reset();
            state.uploading.reset();
            state.changed = std::chrono::steady_clock::now();
            state.started = false;
        }
    }
    if (state.started || std::chrono::steady_clock::now() - state.changed < std::chrono::milliseconds(200)) return;
    state.started = true;
    state.work = std::make_shared<Work>();
    lightmapPool().start(QRunnable::create([work = state.work, vertices = state.vertices,
        surfaces = state.surfaces, snapshot = state.lighting] {
        try {
            work->result = bake(*vertices, *surfaces, snapshot, *work);
            if (!work->result && !work->cancelled) qWarning() << "Preview lightmap exceeds memory budget; using direct lighting";
        } catch (const std::exception& error) {
            qWarning() << "Preview lightmap failed:" << error.what();
        }
        work->done.store(true, std::memory_order_release);
    }));
}

void PreviewLightmap::prepare(VboManager& manager) {
    collectTextures();
    auto& state = *m_impl;
    if (state.work && state.work->done.load(std::memory_order_acquire)) {
        state.pending = std::move(state.work->result);
        state.work.reset();
        if (state.pending && !(GLEW_VERSION_3_0 || GLEW_ARB_texture_float)) {
            state.pending.reset();
            qWarning() << "Float textures unavailable; using direct preview lighting";
        }
        if (!state.pending) state.active.reset();
        if (state.pending) {
            state.uploading = std::make_unique<GpuResult>();
            state.uploading->group = QOpenGLContext::currentContext()->shareGroup();
            state.uploading->vertices = VertexArray::move(std::move(state.pending->vertices));
            state.uploading->vertices.prepare(manager);
            state.uploading->draws = std::move(state.pending->draws);
        }
    }
    if (!state.pending) return;
    // Upload one page per frame to avoid a long transfer on the UI thread.
    const auto pageIndex = state.uploading->textures.size();
    if (pageIndex < state.pending->pages.size()) {
        auto& page = state.pending->pages[pageIndex];
        GLuint texture = 0;
        glAssert(glGenTextures(1, &texture));
        glAssert(glActiveTexture(GL_TEXTURE1));
        glAssert(glBindTexture(GL_TEXTURE_2D, texture));
        glAssert(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        glAssert(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        glAssert(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        glAssert(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        // Fixed atlas dimensions preserve UVs; upload only rows actually used.
        glAssert(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F_ARB, AtlasSize, AtlasSize, 0, GL_RGB, GL_FLOAT, nullptr));
        glAssert(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, AtlasSize, page.height, GL_RGB, GL_FLOAT, page.pixels.data()));
        glAssert(glBindTexture(GL_TEXTURE_2D, 0));
        glAssert(glActiveTexture(GL_TEXTURE0));
        state.uploading->textures.push_back(texture);
        std::vector<float>().swap(page.pixels);
    }
    if (state.uploading->textures.size() == state.pending->pages.size()) {
        state.active = std::move(state.uploading);
        state.pending.reset();
    }
}

bool PreviewLightmap::render(ActiveShader& shader, TextureRenderFunc& textures,
    const std::vector<const Assets::Texture*>& materials) {
    auto* active = m_impl->active.get();
    if (!active || !active->vertices.setup()) return false;
    if (!active->grouped) {
        auto grouped = std::vector<Draw>{};
        for (const auto& draw : active->draws) {
            if (!grouped.empty() && grouped.back().page == draw.page
                && grouped.back().first + grouped.back().count == draw.first
                && materials[grouped.back().surface] == materials[draw.surface]
                && (*m_impl->surfaces)[grouped.back().surface].sky == (*m_impl->surfaces)[draw.surface].sky) {
                grouped.back().count += draw.count;
            } else grouped.push_back(draw);
        }
        active->draws = std::move(grouped);
        active->grouped = true;
    }
    shader.set("UsePreviewLightmap", true);
    shader.set("PreviewLightmap", 1);
    size_t lastSurface = std::numeric_limits<size_t>::max();
    for (const auto& draw : active->draws) {
        if (lastSurface != draw.surface) {
            if (lastSurface != std::numeric_limits<size_t>::max()) textures.after(materials[lastSurface]);
            lastSurface = draw.surface;
            const auto* texture = materials[lastSurface];
            shader.set("GridColor", gridColorForTexture(texture));
            shader.set("EnableMasked", texture && texture->masked());
            shader.set("PreviewFullbright", (*m_impl->surfaces)[lastSurface].sky);
            textures.before(texture);
        }
        glAssert(glActiveTexture(GL_TEXTURE1));
        glAssert(glBindTexture(GL_TEXTURE_2D, active->textures[draw.page]));
        glAssert(glActiveTexture(GL_TEXTURE0));
        active->vertices.render(PrimType::Triangles, draw.first, draw.count);
    }
    if (lastSurface != std::numeric_limits<size_t>::max()) textures.after(materials[lastSurface]);
    glAssert(glActiveTexture(GL_TEXTURE1));
    glAssert(glBindTexture(GL_TEXTURE_2D, 0));
    glAssert(glActiveTexture(GL_TEXTURE0));
    active->vertices.cleanup();
    shader.set("UsePreviewLightmap", false);
    shader.set("PreviewFullbright", false);
    return true;
}
}
