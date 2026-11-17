#pragma once

#include <QRectF>
#include <vector>

namespace TrenchBroom::Renderer {
// One layout per camera/frame, shared by all text render services and font batches.
class ScreenLabelLayout {
  public:
    struct Request {
        QRectF bounds;
        int priority = 0;
        float distance = 0;
        bool movable = true;
    };
    struct Placement {
        QRectF bounds;
        bool visible = false;
    };
    explicit ScreenLabelLayout(QRectF viewport);
    size_t add(Request request);
    const Placement& placement(size_t id);

  private:
    QRectF m_viewport;
    std::vector<Request> m_requests;
    std::vector<Placement> m_placements;
    bool m_valid = false;
    void layout();
};
} // namespace TrenchBroom::Renderer
