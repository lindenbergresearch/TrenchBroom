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

#include "SelectionBoundsRenderer.h"

#include "FloatType.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/Camera.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderService.h"
#include "Renderer/TextAnchor.h"

#include <vm/bbox.h>
#include <vm/util.h>
#include <vm/vec.h>
#include <vm/vec_io.h>

#include <sstream>

namespace TrenchBroom {
namespace Renderer {

const float SelectionBoundsRenderer::AXIS_LABEL_DIM_FACTOR = 1.0f;
static const std::string AXIS_LABELS[3] = {"X", "Y", "Z"};
static const std::string AXIS_LABEL_DELIMITER = "=";


class SelectionBoundsRenderer::SizeTextAnchor2D : public TextAnchor3D {
private:
  const vm::bbox3 &m_bounds;
  const vm::axis::type m_axis;
  const Camera &m_camera;

public:
  SizeTextAnchor2D(const vm::bbox3 &bounds, const vm::axis::type axis, const Renderer::Camera &camera) : m_bounds(bounds), m_axis(axis), m_camera(camera) {
  }

private:
  vm::vec3f basePosition() const override {
    const auto half = m_bounds.size() / 2.0;
    auto pos = m_bounds.min;
    pos[m_axis] += half[m_axis];
    return vm::vec3f(pos);
  }

  TextAlignment::Type alignment() const override {
    if (m_axis == vm::axis::x) {
      return TextAlignment::Top;
    } else if (m_axis == vm::axis::y && m_camera.direction().x() != 0.0f) {
      return TextAlignment::Top;
    } else {
      return TextAlignment::Right;
    }
  }

  vm::vec2f extraOffsets(const TextAlignment::Type alignment) const override {
    vm::vec2f result;
    if (alignment & TextAlignment::Top) {
      result[1] -= 8.0f;
    }
    if (alignment & TextAlignment::Bottom) {
      result[1] += 8.0f;
    }
    if (alignment & TextAlignment::Left) {
      result[0] += 8.0f;
    }
    if (alignment & TextAlignment::Right) {
      result[0] -= 8.0f;
    }
    return result;
  }
};


class SelectionBoundsRenderer::SizeTextAnchor3D : public TextAnchor3D {
private:
  const vm::bbox3 &m_bounds;
  const vm::axis::type m_axis;
  const Camera &m_camera;

public:
  SizeTextAnchor3D(const vm::bbox3 &bounds, const vm::axis::type axis, const Renderer::Camera &camera) : m_bounds(bounds), m_axis(axis), m_camera(camera) {
  }

private:
  vm::vec3f basePosition() const override {
    const auto camPos = m_bounds.relative_position(vm::vec3(m_camera.position()));
    const auto camDir = m_camera.direction();
    vm::vec3 pos;
    const auto half = m_bounds.size() / 2.0;

    if (m_axis == vm::axis::z) {
      if ((camPos[0] == vm::bbox3::Range::less && camPos[1] == vm::bbox3::Range::less)
          || (camPos[0] == vm::bbox3::Range::less && camPos[1] == vm::bbox3::Range::within)) {
        pos[0] = m_bounds.min.x();
        pos[1] = m_bounds.max.y();
      } else if ((camPos[0] == vm::bbox3::Range::less && camPos[1] == vm::bbox3::Range::greater)
          || (camPos[0] == vm::bbox3::Range::within && camPos[1] == vm::bbox3::Range::greater)) {
        pos[0] = m_bounds.max.x();
        pos[1] = m_bounds.max.y();
      } else if ((camPos[0] == vm::bbox3::Range::greater && camPos[1] == vm::bbox3::Range::greater)
          || (camPos[0] == vm::bbox3::Range::greater && camPos[1] == vm::bbox3::Range::within)) {
        pos[0] = m_bounds.max.x();
        pos[1] = m_bounds.min.y();
      } else if ((camPos[0] == vm::bbox3::Range::within && camPos[1] == vm::bbox3::Range::less)
          || (camPos[0] == vm::bbox3::Range::greater && camPos[1] == vm::bbox3::Range::less)) {
        pos[0] = m_bounds.min.x();
        pos[1] = m_bounds.min.y();
      } else {
        // X and Y are within
        pos[0] = camDir.x() <= 0.0f ? m_bounds.min.x() : m_bounds.max.x();
        pos[1] = camDir.y() <= 0.0f ? m_bounds.min.y() : m_bounds.max.y();
      }

      pos[2] = m_bounds.min.z() + half.z();
    } else {
      if (m_axis == vm::axis::x) {
        pos[0] = m_bounds.min.x() + half.x();
        if (camPos[0] == vm::bbox3::Range::less && camPos[1] == vm::bbox3::Range::less) {
          pos[1] = camPos[2] == vm::bbox3::Range::within ? m_bounds.min.y() : m_bounds.max.y();
        } else if (camPos[0] == vm::bbox3::Range::less && camPos[1] == vm::bbox3::Range::within) {
          pos[1] = m_bounds.max.y();
        } else if (camPos[0] == vm::bbox3::Range::less && camPos[1] == vm::bbox3::Range::greater) {
          pos[1] = camPos[2] == vm::bbox3::Range::within ? m_bounds.max.y() : m_bounds.min.y();
        } else if (camPos[0] == vm::bbox3::Range::within && camPos[1] == vm::bbox3::Range::less) {
          pos[1] = camPos[2] == vm::bbox3::Range::within ? m_bounds.min.y() : m_bounds.max.y();
        } else if (camPos[0] == vm::bbox3::Range::within && camPos[1] == vm::bbox3::Range::within) {
          pos[1] = camDir.y() <= 0.0f ? m_bounds.min.y() : m_bounds.max.y();
        } else if (camPos[0] == vm::bbox3::Range::within && camPos[1] == vm::bbox3::Range::greater) {
          pos[1] = camPos[2] == vm::bbox3::Range::within ? m_bounds.max.y() : m_bounds.min.y();
        } else if (camPos[0] == vm::bbox3::Range::greater && camPos[1] == vm::bbox3::Range::less) {
          pos[1] = camPos[2] == vm::bbox3::Range::within ? m_bounds.min.y() : m_bounds.max.y();
        } else if (camPos[0] == vm::bbox3::Range::greater && camPos[1] == vm::bbox3::Range::within) {
          pos[1] = m_bounds.min.y();
        } else if (camPos[0] == vm::bbox3::Range::greater && camPos[1] == vm::bbox3::Range::greater) {
          pos[1] = camPos[2] == vm::bbox3::Range::within ? m_bounds.max.y() : m_bounds.min.y();
        }
      } else {
        pos[1] = m_bounds.min.y() + half.y();
        if (camPos[0] == vm::bbox3::Range::less && camPos[1] == vm::bbox3::Range::less) {
          pos[0] = camPos[2] == vm::bbox3::Range::within ? m_bounds.min.x() : m_bounds.max.x();
        } else if (camPos[0] == vm::bbox3::Range::less && camPos[1] == vm::bbox3::Range::within) {
          pos[0] = camPos[2] == vm::bbox3::Range::within ? m_bounds.min.x() : m_bounds.max.x();
        } else if (camPos[0] == vm::bbox3::Range::less && camPos[1] == vm::bbox3::Range::greater) {
          pos[0] = camPos[2] == vm::bbox3::Range::within ? m_bounds.min.x() : m_bounds.max.x();
        } else if (camPos[0] == vm::bbox3::Range::within && camPos[1] == vm::bbox3::Range::less) {
          pos[0] = m_bounds.min.x();
        } else if (camPos[0] == vm::bbox3::Range::within && camPos[1] == vm::bbox3::Range::within) {
          pos[0] = camDir.x() <= 0.0f ? m_bounds.min.x() : m_bounds.max.x();
        } else if (camPos[0] == vm::bbox3::Range::within && camPos[1] == vm::bbox3::Range::greater) {
          pos[0] = m_bounds.max.x();
        } else if (camPos[0] == vm::bbox3::Range::greater && camPos[1] == vm::bbox3::Range::less) {
          pos[0] = camPos[2] == vm::bbox3::Range::within ? m_bounds.max.x() : m_bounds.min.x();
        } else if (camPos[0] == vm::bbox3::Range::greater && camPos[1] == vm::bbox3::Range::within) {
          pos[0] = camPos[2] == vm::bbox3::Range::within ? m_bounds.max.x() : m_bounds.min.x();
        } else if (camPos[0] == vm::bbox3::Range::greater && camPos[1] == vm::bbox3::Range::greater) {
          pos[0] = camPos[2] == vm::bbox3::Range::within ? m_bounds.max.x() : m_bounds.min.x();
        }
      }

      if (camPos[2] == vm::bbox3::Range::less) {
        pos[2] = m_bounds.min.z();
      } else {
        pos[2] = m_bounds.max.z();
      }
    }

    return vm::vec3f(pos);
  }

  TextAlignment::Type alignment() const override {
    if (m_axis == vm::axis::z) {
      return TextAlignment::Right;
    }

    const auto camPos = m_bounds.relative_position(vm::vec3(m_camera.position()));
    if (camPos[2] == vm::bbox3::Range::less) {
      return TextAlignment::Top;
    } else {
      return TextAlignment::Bottom;
    }
  }

  vm::vec2f extraOffsets(const TextAlignment::Type alignment) const override {
    vm::vec2f result;
    if (alignment & TextAlignment::Top) {
      result[1] -= 8.0f;
    }
    if (alignment & TextAlignment::Bottom) {
      result[1] += 8.0f;
    }
    if (alignment & TextAlignment::Left) {
      result[0] += 8.0f;
    }
    if (alignment & TextAlignment::Right) {
      result[0] -= 8.0f;
    }
    return result;
  }
};


class SelectionBoundsRenderer::MinMaxTextAnchor3D : public TextAnchor3D {
private:
  const vm::bbox3 &m_bounds;
  const vm::bbox3::Corner m_minMax;
  const Renderer::Camera &m_camera;

public:
  MinMaxTextAnchor3D(const vm::bbox3 &bounds, const vm::bbox3::Corner minMax, const Renderer::Camera &camera) :
      m_bounds(bounds), m_minMax(minMax), m_camera(camera) {
  }

private:
  vm::vec3f basePosition() const override {
    if (m_minMax == vm::bbox3::Corner::min) {
      return vm::vec3f(m_bounds.min);
    } else {
      return vm::vec3f(m_bounds.max);
    }
  }

  TextAlignment::Type alignment() const override {
    const auto camPos = m_bounds.relative_position(vm::vec3(m_camera.position()));
    if (m_minMax == vm::bbox3::Corner::min) {
      if ((camPos[1] == vm::bbox3::Range::less) || (camPos[1] == vm::bbox3::Range::within && camPos[0] != vm::bbox3::Range::less)) {
        return TextAlignment::Top | TextAlignment::Right;
      } else {
        return TextAlignment::Top | TextAlignment::Left;
      }
    } else if ((camPos[1] == vm::bbox3::Range::less) || (camPos[1] == vm::bbox3::Range::within && camPos[0] != vm::bbox3::Range::less)) {
      return TextAlignment::Bottom | TextAlignment::Left;
    } else {
      return TextAlignment::Bottom | TextAlignment::Right;
    }
  }

  vm::vec2f extraOffsets(const TextAlignment::Type alignment) const override {
    vm::vec2f result;
    if (alignment & TextAlignment::Top)
      result[1] -= 8.0f;
    if (alignment & TextAlignment::Bottom)
      result[1] += 8.0f;
    if (alignment & TextAlignment::Left)
      result[0] += 8.0f;
    if (alignment & TextAlignment::Right)
      result[0] -= 8.0f;
    return result;
  }
};


SelectionBoundsRenderer::SelectionBoundsRenderer(const vm::bbox3 &bounds) : m_bounds(bounds) {
}

void SelectionBoundsRenderer::render(RenderContext &renderContext, RenderBatch &renderBatch) {
  bool showObjectBounds = pref(Preferences::ShowObjectBoundsSelectionBounds);

  if (showObjectBounds) {
    renderBounds(renderContext, renderBatch);
  }

  renderSize(renderContext, renderBatch);

  if (pref(Preferences::SelectionBoundsShowMinMax)) {
    renderMinMax(renderContext, renderBatch);
  }
}

void SelectionBoundsRenderer::renderBounds(RenderContext &renderContext, RenderBatch &renderBatch) {
  RenderService renderService(renderContext, renderBatch);
  renderService.setForegroundColor(pref(Preferences::SelectionBoundsColor));
  renderService.setLineWidth(pref(Preferences::SelectionBoundsLineWidth));
  renderService.renderBounds(vm::bbox3f(m_bounds));
}

void SelectionBoundsRenderer::renderSize(RenderContext &renderContext, RenderBatch &renderBatch) {
  if (renderContext.render2D())
    renderSize2D(renderContext, renderBatch);
  else
    renderSize3D(renderContext, renderBatch);
}

const std::string SelectionBoundsRenderer::getFormattedUnitsString(float value_units) {
  auto unitsDisplayType = (Preferences::LengthUnitDisplay) pref(Preferences::LengthUnitSystem);
  auto metricConversationFactor = pref(Preferences::MetricConversationFactor);
  auto unitsMaxDigits = pref(Preferences::UnitsMaxDigits);
  std::stringstream buffer;

  auto units_str = formatDimension(value_units, unitsMaxDigits).toStdString();
  auto metric_str = formatDimension(value_units / metricConversationFactor, unitsMaxDigits, "m").toStdString();

  switch (unitsDisplayType) {
  case Preferences::Units:buffer << units_str;
    break;
  case Preferences::Metric:buffer << metric_str;
    break;
  case Preferences::Combined:buffer << units_str << " [" << metric_str << "]";
    break;
  }

  return buffer.str();
}

void SelectionBoundsRenderer::renderSize2D(RenderContext &renderContext, RenderBatch &renderBatch) {
  Color colors[3];
  std::stringstream buffer;

  RenderService renderService(renderContext, renderBatch);
  renderService.setForegroundColor(pref(Preferences::InfoOverlayTextColor));
  renderService.setShowOccludedObjects();
  auto alpha = pref(Preferences::WeakInfoOverlayBackgroundAlpha);

  colors[0] = AXIS_LABEL_DIM_FACTOR * Color(pref(Preferences::XAxisColor), alpha);
  colors[1] = AXIS_LABEL_DIM_FACTOR * Color(pref(Preferences::YAxisColor), alpha);

  const Camera &camera = renderContext.camera();
  const vm::vec3f &direction = camera.direction();

  const vm::vec3 boundsSize = correct(m_bounds.size());
  for (size_t i = 0; i < 3; ++ i) {
    renderService.setBackgroundColor(colors[i]);
    if (direction[i] == 0.0f) {
      buffer << AXIS_LABELS[i] << AXIS_LABEL_DELIMITER << getFormattedUnitsString(float(boundsSize[i]));
      renderService.renderString(buffer.str(), SizeTextAnchor2D(m_bounds, i, camera));
      buffer.str("");
    }
  }
}

void SelectionBoundsRenderer::renderSize3D(RenderContext &renderContext, RenderBatch &renderBatch) {
  Color colors[3];
  std::stringstream buffer;

  RenderService renderService(renderContext, renderBatch);
  renderService.setForegroundColor(pref(Preferences::InfoOverlayTextColor));
  renderService.setShowOccludedObjects();
  auto alpha = pref(Preferences::WeakInfoOverlayBackgroundAlpha);

  colors[0] = AXIS_LABEL_DIM_FACTOR * Color(pref(Preferences::XAxisColor), alpha);
  colors[1] = AXIS_LABEL_DIM_FACTOR * Color(pref(Preferences::YAxisColor), alpha);
  colors[2] = AXIS_LABEL_DIM_FACTOR * Color(pref(Preferences::ZAxisColor), alpha);

  const vm::vec3 boundsSize = correct(m_bounds.size());
  for (size_t i = 0; i < 3; ++ i) {
    renderService.setBackgroundColor(colors[i]);
    buffer << AXIS_LABELS[i] << AXIS_LABEL_DELIMITER << getFormattedUnitsString(float(boundsSize[i]));
    renderService.renderString(
        buffer.str(), SizeTextAnchor3D(m_bounds, i, renderContext.camera()));
    buffer.str("");
  }
}

void SelectionBoundsRenderer::renderMinMax(RenderContext &renderContext, RenderBatch &renderBatch) {
  std::stringstream buffer;

  RenderService renderService(renderContext, renderBatch);
  renderService.setForegroundColor(pref(Preferences::InfoOverlayTextColor));
  renderService.setBackgroundColor(
      Color(
          pref(Preferences::InfoOverlayBackgroundColor), pref(Preferences::WeakInfoOverlayBackgroundAlpha)));
  renderService.setShowOccludedObjects();

  buffer << "Min: " << vm::correct(m_bounds.min);
  renderService.renderString(
      buffer.str(), MinMaxTextAnchor3D(m_bounds, vm::bbox3::Corner::min, renderContext.camera()));
  buffer.str("");

  buffer << "Max: " << vm::correct(m_bounds.max);
  renderService.renderString(
      buffer.str(), MinMaxTextAnchor3D(m_bounds, vm::bbox3::Corner::max, renderContext.camera()));
}

QString SelectionBoundsRenderer::formatDimension(const float value, const int digits, const QString &suffix) {
  auto isInt = (value - std::trunc(value) == 0);
  QString str = "";

  if (isInt) {
    str = (new QString)->sprintf("%d%s", (int) value, suffix.toStdString().c_str());
  } else {
    const char *raw_format_str = "%%.%df%%s";
    auto format_str = (new QString)->sprintf(raw_format_str, digits).toStdString();
    str = (new QString)->sprintf(format_str.c_str(), value, suffix.toStdString().c_str());
  }

  return str;
}
} // namespace Renderer
} // namespace TrenchBroom
