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

#include <mach/mach_init.h>
#include <mach/thread_act.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <QOpenGLContext>
#include <QDateTime>
#include <QPalette>
#include <QTimer>
#include <QWidget>
#include <vm/mat.h>
#include <vm/mat_ext.h>
#include <iostream>
#include <GL/glew.h> // must be included here, before QOpenGLWidget
#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <string>
#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/GLVertexType.h"
#include "Renderer/PrimType.h"
#include "Renderer/Transformation.h"
#include "Renderer/VboManager.h"
#include "Renderer/VertexArray.h"
#include "TrenchBroomApp.h"
#include "View/GLContextManager.h"
#include "View/InputEvent.h"
#include "View/QtUtils.h"
#include "Assets/Texture.h"
#include "Color.h"
#include "Renderer/GL.h"

namespace TrenchBroom::View {
struct BoxFilter {
  std::vector<QPointF *> samples;
  size_t m_size, m_length;
  size_t m_index;

  explicit BoxFilter(size_t size, size_t length = 0) : m_size(size) {
      samples.resize(m_size);
      m_index = 0;

      m_length = length <= 0 ? size : length;
      reset();
  }

  QPointF average();

  void reset();

  void add(QPointF *point);
};
}
