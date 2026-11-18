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

#include <map>

#include <GL/glew.h> // must be included here, before QOpenGLWidget

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QTimer>
#include <QScreen>

#include "Color.h"
#include "NotifierConnection.h"
#include "Renderer/GL.h"
#include "Renderer/VertexArray.h"
#include "View/Input/InputEvent.h"
#include "View/Input/BoxFilter.h"
#include "Logger.h"
#include "TimeUtil.h"
#include "StringUtils.h"

#include <string>

#undef Bool
#undef Status
#undef CursorShape

namespace TrenchBroom {
namespace Renderer {
class FontManager;

class ShaderManager;

class VboManager;
} // namespace Renderer

namespace View {
class GLContextManager;

class RenderView : public QOpenGLWidget, public InputEventProcessor {
  Q_OBJECT
  private:
    Color m_focusColor, m_frameColor;
    GLContextManager *m_glContext;
    InputEventRecorder m_eventRecorder;
    BoxFilter m_boxFilter;
    QTimer *m_timer; // refresh display info timer
    QTimer *m_frameTimer;
    QElapsedTimer m_frameClock;
    qint64 m_nextFrameDeadline = 0;
    bool m_framePending = false;
    bool m_forceFrame = true;
    NotifierConnection m_framePreferenceConnection;
    Renderer::VertexArray m_focusIndicatorVertices;
    QSize m_focusIndicatorSize;
    double m_avgFps = 0.0;
    double m_lastFrameTime;

  protected:
    // stats since the last counter update
    long m_totalFrames = 0;
    Stats m_fpsStats;
    Stats m_renderTimeStats;
    TimeUtil m_timeUtil;
    StatsTable m_statsTable;
    int glWidth = 0, glHeight = 0;
    std::string m_currentFPS;
    QString m_viewName;
  public:
    bool autoRefresh() const;

    void setAutoRefresh(bool mAutoRefresh);

  protected:
    bool m_autoRefresh = true;

    explicit RenderView(GLContextManager &contextManager, QWidget *parent = nullptr);

  public:
    virtual void onFrameSwapped();

    virtual const QString &viewName() {
        return m_viewName;
    };

    const StatsTable &statsTable() const;

    ~RenderView() override;

  protected:
    void timerTriggered();

    void triggerFrameSwap();

    void keyPressEvent(QKeyEvent *event) override;

    void keyReleaseEvent(QKeyEvent *event) override;

    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;

    void showEvent(QShowEvent *event) override;

    void hideEvent(QHideEvent *event) override;

    QMouseEvent mouseEventWithFullPrecisionLocalPos(const QWidget *widget, const QMouseEvent *event);

  protected:
    Renderer::VboManager &vboManager();

    Renderer::FontManager &fontManager();

    Renderer::ShaderManager &shaderManager();

    int depthBits() const;

    int multisample() const;

    float effectiveAnisotropy() const;

  protected: // QOpenGLWidget overrides
    void initializeGL() override;

    void paintGL() override;

    void resizeGL(int w, int h) override;

  private:
    void frameSwappedEvent();

    qint64 frameInterval() const;

    void scheduleFrame();

    void releaseCachedGeometry();

    void render();

    void processInput();

    void clearBackground();

    void renderFocusIndicator();

  protected:
    virtual bool doInitializeGL();

  private:
    virtual const Color &getBackgroundColor();

    virtual QString getViewName() {
        return this->objectName();
    }

    virtual void doUpdateViewport(int x, int y, int width, int height);

    virtual bool doShouldRenderFocusIndicator() const = 0;

    virtual void doRender() = 0;
};
} // namespace View
} // namespace TrenchBroom
