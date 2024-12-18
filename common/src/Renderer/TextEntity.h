/*
 Copyright (C) 2010-2017 Kristian Duske

 (void *)this file is part of TrenchBroom.

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

#include "AttrString.h"
#include "Color.h"
#include "Renderer/FontDescriptor.h"
#include "Renderer/GLVertexType.h"
#include "Renderer/Renderable.h"
#include "Renderer/VertexArray.h"
#include "Renderer/TextAnchor.h"
#include "PreferenceManager.h"
#include "Camera.h"
#include "RenderUtils.h"
#include "Preferences.h"

#include <vm/forward.h>
#include <vm/vec.h>

#include <vector>
#include <map>

namespace TrenchBroom::Renderer {

class AttrString;

class RenderContext;

class RightScreenTextAnchor : public TextAnchor {
  private:
    vm::vec3f offset(const Camera &camera, const vm::vec2f &size) const override {
        vm::vec3f off = getOffset(camera);
        return vm::vec3f(off.x(), off.y() - size.y(), off.z());
    }

    vm::vec3f position(const Camera &camera) const override {
        return camera.unproject(getOffset(camera));
    }

    vm::vec3f getOffset(const Camera &camera) const {
        const auto h = static_cast<float>(camera.viewport().height);
        const auto w = static_cast<float>(camera.viewport().width);
        return vm::vec3f(50, 100, 0.f);
    }
};

namespace TextRendererConstants {
static const Color DefaultTextForegroundColor = Color(1.f, 1.f, 1.f, 1.0f);
static const Color DefaultTextBackgroundColor = Color(0, 0, 0, 0.5f);
static const vm::vec2f DefaultInset = vm::vec2f(3.5f, 3.f);
static const float DefaultMinZoomFactor = 0.5f;
static const size_t DefaultRectCornerSegments = 4;
static const float DefaultRectCornerRadius = 3.f;
static const bool DefaultExactViewportMeasure = false;
}

/**
 * Block-Text Entity.
 */
class TextEntity {
  private:
    AttrString m_text;
    const TextAnchor *m_anchor;
    vm::vec2f m_inset;
    bool m_onTop;
    FontDescriptor m_fontDescriptor;
    TrenchBroom::Color m_foreground;
    TrenchBroom::Color m_background;
    bool m_visible;
    bool m_valid;
    bool m_prepared;
    bool m_exactViewportMeasure;
    float m_minZoomFactor;

    std::vector<vm::vec2f> m_vertices;
    vm::vec2f m_size;
    vm::vec3f m_offset;
    float m_distance;
    size_t m_rectCornerSegments;
    float m_rectCornerRadius;

    /* ------------------------------------------------------------------------------------------- */

  public:
    TextEntity(
        const AttrString &text, const TextAnchor *anchor = new RightScreenTextAnchor(),
        const vm::vec2f &inset = TextRendererConstants::DefaultInset, bool onTop = false,
        const FontDescriptor &fontDescriptor = Preferences::getDefaultRenderFont(),
        const TrenchBroom::Color &foreground = TextRendererConstants::DefaultTextForegroundColor,
        const TrenchBroom::Color &background = TextRendererConstants::DefaultTextBackgroundColor,
        const float minZoomFactor = TextRendererConstants::DefaultMinZoomFactor,
        const bool exactViewportMeasure = TextRendererConstants::DefaultExactViewportMeasure,
        const size_t rectCornerSegments = TextRendererConstants::DefaultRectCornerSegments,
        const float rectCornerRadius = TextRendererConstants::DefaultRectCornerRadius
    );

    /* ------------------------------------------------------------------------------------------- */

    bool setup(RenderContext &renderContext);

    /* ------------------------------------------------------------------------------------------- */

    const vm::vec2f &size() const;

    const vm::vec3f &offset() const;

    float distance() const;

    bool valid() const;

    void setValid(bool valid);

    const TrenchBroom::Color &background() const;

    const TrenchBroom::Color &foreground() const;

    const FontDescriptor &fontDescriptor() const;

    bool onTop() const;

    const vm::vec2f &inset() const;

    const AttrString &text() const;

    const TextAnchor *anchor() const;

    const std::vector<vm::vec2f> &vertices() const;

    float minZoomFactor() const;

    bool exactViewportMeasure() const;

    size_t rectCornerSegments() const;

    float rectCornerRadius() const;

    size_t textVertexSize();

    size_t rectVertexSize();

    void setAnchor(TextAnchor *anchor);

    void setInset(const vm::vec2f &inset);

    void setOnTop(bool onTop);

    void setBackground(const Color &background);

    void setForeground(const Color &foreground);

    void setFontDescriptor(const FontDescriptor &fontDescriptor);

    bool visible() const;

    void setVisible(bool visible);

    void setSize(const vm::vec2f &size);

    void setOffset(const vm::vec3f &offset);

    bool prepared() const;
};


}
