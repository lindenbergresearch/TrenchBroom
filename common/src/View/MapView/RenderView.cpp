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

#include "View/MapView/RenderView.h"

#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/GLVertexType.h"
#include "Renderer/ActiveShader.h"
#include "Renderer/Shaders.h"
#include "Renderer/PrimType.h"
#include "Renderer/Transformation.h"
#include "Renderer/VboManager.h"
#include "Renderer/VertexArray.h"
#include "TrenchBroomApp.h"
#include "View/MapView/GLContextManager.h"
#include "View/Input/InputEvent.h"
#include "View/Theme/QtUtils.h"
#include "Assets/Texture.h"


/*
 * - glew requires it is included before <OpenGL/gl.h>
 *
 * - Qt requires that glew is included after <qopengl.h> and <QOpenGLFunctions>
 * - QOpenGLWidget includes <qopengl.h>
 * - qopengl.h includes OpenGL/gl.h
 *
 * therefore
 * - glew wants to be included first
 * - and so does QOpenGLWidget
 *
 * Since including glew before QOpenGLWidget only generates a warning and does not seem to
 * incur any ill effects, we silence the warning here.
 *
 * Note that GCC does not let us silence this warning using diagnostic pragmas, so it is
 * disabled in the CXX_FLAGS!
 */
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

#include <QDateTime>
#include <QHideEvent>
#include <QPalette>
#include <QShowEvent>
#include <QTimer>
#include <QWidget>

#ifdef _WIN32
#endif

#include <vm/mat.h>
#include <vm/mat_ext.h>

#include <algorithm>

namespace TrenchBroom {
namespace View {

RenderView::RenderView(GLContextManager &contextManager, QWidget *parent)
    : QOpenGLWidget(parent), m_glContext(&contextManager), m_boxFilter(64) {
    auto palette = QPalette{};
    m_focusColor = palette.color(QPalette::Highlight);
    m_frameColor = palette.color(QPalette::Midlight);

    m_focusColor[3] = 0.8f;

    setUpdateBehavior(PartialUpdate);

    // screen overlay refresh
    m_timer = new QTimer(this);
    m_timer->start(250);

    m_frameClock.start();
    m_frameTimer = new QTimer(this);
    m_frameTimer->setSingleShot(true);
    m_frameTimer->setTimerType(Qt::PreciseTimer);
    connect(m_frameTimer, &QTimer::timeout, this, &RenderView::scheduleFrame);
    m_framePreferenceConnection += PreferenceManager::instance().preferenceDidChangeNotifier.connect(
        [this](const std::filesystem::path &path) {
            if (path == Preferences::RendererMaxFPS.path()) {
                m_frameTimer->stop();
                m_nextFrameDeadline = 0;
                triggerFrameSwap();
            }
        });

    m_lastFrameTime = m_timeUtil.current();

    setMouseTracking(true); // request mouse move events even when no button is held down
    setFocusPolicy(Qt::StrongFocus); // accept focus by clicking or tab

    m_statsTable.setTitle("Render Statistics");
    m_statsTable.setLabelWidth(22);

    connect(m_timer, &QTimer::timeout, this, &RenderView::timerTriggered);
    connect(this, &QOpenGLWidget::frameSwapped, this, &RenderView::frameSwappedEvent);
}


RenderView::~RenderView() {
    m_framePreferenceConnection.disconnect();
    m_frameTimer->stop();
    releaseCachedGeometry();
    if (context()) disconnect(context(), nullptr, this, nullptr);
}

void RenderView::keyPressEvent(QKeyEvent *event) {
    m_eventRecorder.recordEvent(*event);
    triggerFrameSwap();
}

void RenderView::keyReleaseEvent(QKeyEvent *event) {
    m_eventRecorder.recordEvent(*event);
    triggerFrameSwap();
}

QMouseEvent RenderView::mouseEventWithFullPrecisionLocalPos(const QWidget *widget, const QMouseEvent *event) {
    // The localPos of a Qt mouse event is only in integer coordinates, but window pos
    // and screen pos have full precision. We can't directly map the windowPos because
    // mapTo takes QPoint, so we just map the origin and subtract that.
    QPointF localPos = event->windowPos() - QPointF(widget->mapTo(widget->window(), QPoint(0, 0)));

    m_boxFilter.add(new QPointF(localPos.x(), localPos.y()));

    return QMouseEvent(
        event->type(), m_boxFilter.average(), event->windowPos(), event->screenPos(), event->button(), event->buttons(), event->modifiers(), event->source()
    );
}

void RenderView::mouseDoubleClickEvent(QMouseEvent *event) {
    m_eventRecorder.recordEvent(mouseEventWithFullPrecisionLocalPos(this, event));
    triggerFrameSwap();
}

void RenderView::mouseMoveEvent(QMouseEvent *event) {
    auto length = (size_t) pref(Preferences::CameraLookSmoothing);

    if (m_boxFilter.m_length != length) {
        m_boxFilter.reset();
        m_boxFilter.m_length = length;
    }

    m_eventRecorder.recordEvent(mouseEventWithFullPrecisionLocalPos(this, event));
    triggerFrameSwap();
}

void RenderView::mousePressEvent(QMouseEvent *event) {
    m_eventRecorder.recordEvent(mouseEventWithFullPrecisionLocalPos(this, event));
    triggerFrameSwap();
}

void RenderView::mouseReleaseEvent(QMouseEvent *event) {
    m_eventRecorder.recordEvent(mouseEventWithFullPrecisionLocalPos(this, event));
    triggerFrameSwap();
}

void RenderView::wheelEvent(QWheelEvent *event) {
    m_eventRecorder.recordEvent(*event);
    triggerFrameSwap();
}

void RenderView::paintGL() {
    if (TrenchBroom::View::isReportingCrash()) {
        return;
    }

    const auto interval = frameInterval();
    const auto now = m_frameClock.nsecsElapsed();
    // Qt and subclasses can request paints without going through triggerFrameSwap.
    // Gate the actual rendering too; PartialUpdate retains the previous framebuffer.
    if (!m_forceFrame && interval > 0 && now < m_nextFrameDeadline) {
        m_framePending = true;
        scheduleFrame();
        return;
    }

    m_frameTimer->stop();
    m_framePending = false;
    // Preserve fractional millisecond deadlines instead of accumulating timer
    // rounding error. Missed frames are dropped rather than rendered in a burst.
    m_nextFrameDeadline = interval <= 0 ? 0
        : !m_forceFrame && m_nextFrameDeadline > 0 && now - m_nextFrameDeadline < interval
            ? m_nextFrameDeadline + interval : now + interval;
    m_forceFrame = false;
    render();
}

Renderer::VboManager &RenderView::vboManager() {
    return m_glContext->vboManager();
}

Renderer::FontManager &RenderView::fontManager() {
    return m_glContext->fontManager();
}

Renderer::ShaderManager &RenderView::shaderManager() {
    return m_glContext->shaderManager();
}

int RenderView::depthBits() const {
    return format().depthBufferSize();
}

int RenderView::multisample() const {
    return format().samples();
}

float RenderView::effectiveAnisotropy() const {
    if (!pref(Preferences::EnableAnisotropicFilter)) return 1.0f;
    const auto requested = pref(Preferences::AnisotropicFilterValue);
    const auto maximum = m_glContext->maxAnisotropy();
    return requested <= 0.0f ? maximum : std::clamp(requested, 1.0f, maximum);
}

void RenderView::initializeGL() {
    doInitializeGL();
}

void RenderView::resizeGL(int w, int h) {
    m_forceFrame = true; // A resized framebuffer has no previous image to retain.
    // These are in points, not pixels
    doUpdateViewport(0, 0, w, h);
}

void RenderView::render() {
    m_totalFrames++;
    auto now = m_timeUtil.current();
    auto frameTime = now - m_lastFrameTime;
    m_fpsStats.push(frameTime);
    m_lastFrameTime = now;

    const auto t0 = m_timeUtil.current();
    processInput();
    clearBackground();
    doRender();
    renderFocusIndicator();
    const auto t = m_timeUtil.current() - t0;

    m_renderTimeStats.push(t);
}

void RenderView::processInput() {
    m_eventRecorder.processEvents(*this);
}

void RenderView::clearBackground() {
    const auto backgroundColor = getBackgroundColor();

    glAssert(glClearColor(backgroundColor.r(), backgroundColor.g(), backgroundColor.b(), backgroundColor.a()));
    glAssert(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

const Color &RenderView::getBackgroundColor() {
    return pref(Preferences::BackgroundColor);
}

void RenderView::renderFocusIndicator() {
    const auto drawFocus = hasFocus() && pref(Preferences::ShowFocusIndicator) && doShouldRenderFocusIndicator();
    const auto &frameColor = drawFocus ? m_focusColor : m_frameColor;

    const qreal r = devicePixelRatioF();
    const auto w = static_cast<float>(width() * r);
    const auto h = static_cast<float>(height() * r);
    glAssert(glViewport(0, 0, static_cast<int>(w), static_cast<int>(h)));
    glWidth = (int) w;
    glHeight = (int) h;

    const auto t = 1.25f;

    const auto projection = vm::ortho_matrix(-1.0f, 1.0f, 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h));
    Renderer::Transformation transformation(projection, vm::mat4x4f::identity());

    glAssert(glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT));
    glAssert(glDisable(GL_DEPTH_TEST));
    glAssert(glDisable(GL_CULL_FACE));
    glAssert(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));

    const auto size = QSize(glWidth, glHeight);
    if (m_focusIndicatorVertices.empty() || m_focusIndicatorSize != size) {
        using Vertex = Renderer::GLVertexTypes::P3::Vertex;
        m_focusIndicatorVertices = Renderer::VertexArray::move(
            std::vector<Vertex>(
                {
                    // top
                    Vertex(vm::vec3f(0.0f, 0.0f, 0.0f)),
                    Vertex(vm::vec3f(w, 0.0f, 0.0f)),
                    Vertex(vm::vec3f(w - t, t, 0.0f)),
                    Vertex(vm::vec3f(t, t, 0.0f)),
                    // right
                    Vertex(vm::vec3f(w, 0.0f, 0.0f)),
                    Vertex(vm::vec3f(w, h, 0.0f)),
                    Vertex(vm::vec3f(w - t, h - t, 0.0f)),
                    Vertex(vm::vec3f(w - t, t, 0.0f)),
                    // bottom
                    Vertex(vm::vec3f(w, h, 0.0f)),
                    Vertex(vm::vec3f(0.0f, h, 0.0f)),
                    Vertex(vm::vec3f(t, h - t, 0.0f)),
                    Vertex(vm::vec3f(w - t, h - t, 0.0f)),
                    // left
                    Vertex(vm::vec3f(0.0f, h, 0.0f)),
                    Vertex(vm::vec3f(0.0f, 0.0f, 0.0f)),
                    Vertex(vm::vec3f(t, t, 0.0f)),
                    Vertex(vm::vec3f(t, h - t, 0.0f))
                }
            )
        );
        m_focusIndicatorSize = size;
    }
    auto shader = Renderer::ActiveShader(shaderManager(), Renderer::Shaders::VaryingPUniformCShader);
    shader.set("Color", frameColor);
    shader.set("ApplyTinting", false);
    m_focusIndicatorVertices.prepare(vboManager());
    m_focusIndicatorVertices.render(Renderer::PrimType::Quads);
    glAssert(glPopAttrib());
}

void RenderView::releaseCachedGeometry() {
    auto *glContext = context();
    if (!glContext || !glContext->isValid()) return;
    auto *previousContext = QOpenGLContext::currentContext();
    auto *previousSurface = previousContext ? previousContext->surface() : nullptr;
    makeCurrent();
    m_focusIndicatorVertices = {};
    m_focusIndicatorSize = {};
    vboManager().clearCachedGeometry();
    if (previousContext && previousContext != glContext && previousSurface) {
        previousContext->makeCurrent(previousSurface);
    } else if (!previousContext) {
        doneCurrent();
    }
}

bool RenderView::doInitializeGL() {
    m_forceFrame = true;
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &RenderView::releaseCachedGeometry,
            Qt::ConnectionType(Qt::DirectConnection | Qt::UniqueConnection));
    return m_glContext->initialize();
}

void RenderView::doUpdateViewport(const int x, const int y, const int width, const int height) {
}


bool RenderView::autoRefresh() const {
    return m_autoRefresh;
}

void RenderView::setAutoRefresh(bool autoRefresh) {
    m_autoRefresh = autoRefresh;
    if (autoRefresh) triggerFrameSwap();
}

void RenderView::triggerFrameSwap() {
    m_framePending = true;
    scheduleFrame();
}

qint64 RenderView::frameInterval() const {
    const auto configured = pref(Preferences::RendererMaxFPS);
    const auto fps = configured < 0 ? Preferences::RendererMaxFPS.defaultValue() : std::min(configured, 1000);
    return fps == 0 ? 0 : (qint64(1000000000) + fps - 1) / fps;
}

void RenderView::scheduleFrame() {
    if (!m_framePending || !isVisible() || TrenchBroom::View::isReportingCrash()) return;
    const auto remaining = m_forceFrame || frameInterval() == 0 ? qint64(0)
        : m_nextFrameDeadline - m_frameClock.nsecsElapsed();
    if (remaining <= 0) {
        m_frameTimer->stop();
        update();
    } else if (!m_frameTimer->isActive()) {
        // Round up to avoid repeatedly waking before the deadline. No sleeping
        // or spinning on the GUI thread, and only one pending timer per view.
        m_frameTimer->start(int((remaining + 999999) / 1000000));
    }
}

void RenderView::showEvent(QShowEvent *event) {
    QOpenGLWidget::showEvent(event);
    m_forceFrame = true;
    triggerFrameSwap();
}

void RenderView::hideEvent(QHideEvent *event) {
    m_frameTimer->stop();
    QOpenGLWidget::hideEvent(event);
}

void RenderView::frameSwappedEvent() {
    onFrameSwapped();
    if (m_autoRefresh) triggerFrameSwap();
}

void RenderView::timerTriggered() {
    if (!hasFocus()) return;

    m_avgFps = m_fpsStats.average();

    m_statsTable.reset();
    m_statsTable.addPair("GL Resolution", std::to_string(glWidth) + " x " + std::to_string(glHeight));
    m_statsTable.addInt("MSAA", multisample());
    m_statsTable.addInt("Color Depth", depthBits());

    m_statsTable.addBool("Anisotropic Filter", pref(Preferences::EnableAnisotropicFilter));
    // Timer callbacks do not own a current GL context. Read the cached capability.
    m_statsTable.addPair("Anisotropic Value", std::to_string(int(effectiveAnisotropy())) + "x");

    m_statsTable.addSeparator();

    const auto averageFrameTime = m_fpsStats.average();
    m_statsTable.addDouble("FPS", averageFrameTime > 0.0 ? 1.0 / averageFrameTime : 0.0);
    m_statsTable.addTime("Render-Time", m_renderTimeStats.average(), StatsTable::MILLIS);
    m_statsTable.addTime("Frame-Time", m_fpsStats.average(), StatsTable::MILLIS);
    m_statsTable.addLong("Frames", m_totalFrames);
    m_statsTable.addSeparator();
    m_statsTable.addTime("Frame-Time min", m_fpsStats.min, StatsTable::MILLIS);
    m_statsTable.addTime("Frame-Time max", m_fpsStats.max, StatsTable::MILLIS);
    m_statsTable.addSeparator();
    m_statsTable.addTime("Render-Time min", m_renderTimeStats.min, StatsTable::MILLIS);
    m_statsTable.addTime("Render-Time max", m_renderTimeStats.max, StatsTable::MILLIS);
    m_statsTable.addSeparator();
    m_statsTable.addLong("VBOs count", (long) m_glContext->vboManager().currentVboCount());
    m_statsTable.addLong("VBOs Peak", (long) m_glContext->vboManager().peakVboCount());
    m_statsTable.addMem("VBO Memory", m_glContext->vboManager().currentVboSize());


    m_currentFPS = m_statsTable.render(true);

    m_fpsStats.softReset();
    m_renderTimeStats.softReset();
}

void RenderView::onFrameSwapped() {
}

const StatsTable &RenderView::statsTable() const {
    return m_statsTable;
}
} // namespace View
} // namespace TrenchBroom
