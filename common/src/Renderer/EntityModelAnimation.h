#pragma once

#include "Assets/EntityModel.h"

#include <algorithm>
#include <cmath>

namespace TrenchBroom::Renderer {
inline bool previewModelAnimation(const bool selected, const vm::bbox3f& bounds, const vm::vec3f& camera,
                                 const bool onSelection, const bool nearCamera, const double distance) {
    if (onSelection && selected) return true;
    if (!nearCamera || !std::isfinite(distance) || distance < 0) return false;
    double squaredDistance = 0;
    for (size_t axis = 0; axis < 3; ++axis) {
        const auto delta = std::max({double(bounds.min[axis]) - camera[axis], 0.0, double(camera[axis]) - bounds.max[axis]});
        squaredDistance += delta * delta;
    }
    return squaredDistance <= distance * distance;
}

// Per entity and camera. Rendering changes poses, never the entity's properties
// or picking geometry. Accumulated time also keeps speed edits continuous.
class EntityModelAnimationPlayback {
    bool m_playing = false;
    double m_lastTime = 0, m_position = 0;

public:
    size_t frame(const Assets::EntityModel::Animation& animation, const double now, double speed) {
        speed = std::isfinite(speed) ? std::clamp(speed, 0.0, 4.0) : 1;
        if (m_playing && std::isfinite(now)) {
            m_position += std::max(0.0, now - m_lastTime) * speed;
            if (animation.duration() > 0) m_position = std::fmod(m_position, animation.duration());
        }
        m_playing = true;
        m_lastTime = std::isfinite(now) ? now : 0;
        return animation.frameAt(m_position);
    }
};
} // namespace TrenchBroom::Renderer
