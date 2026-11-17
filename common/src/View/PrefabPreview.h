#pragma once

#include "FloatType.h"

#include <QColor>
#include <QFrame>
#include <QImage>
#include <QPointF>
#include <QTimer>

#include <vm/vec.h>
#include <memory>
#include <vector>

namespace TrenchBroom::Model { class Node; class Game; }
namespace TrenchBroom::View {
QImage prefabPreviewBrightness(const QImage& image, double brightness);
// Geometry and texture pixels are copied once. Rotating a preview never reads or changes
// the editor document, its selection, camera or OpenGL context.
class PrefabPreviewGeometry {
    struct Material {
        std::vector<QImage> mipmaps;
        bool masked = false;
    };
    struct Face {
        std::vector<vm::vec3> points;
        vm::vec3 normal;
        QColor color;
        bool twoSided = false;
        std::vector<vm::vec2f> texCoords;
        std::shared_ptr<const Material> material;
    };
    std::vector<Face> m_faces;

public:
    explicit PrefabPreviewGeometry(Model::Node& root, const Model::Game* game = nullptr);
    QImage render(const QSize& size = {512, 384}, double yaw = -45, double pitch = 30, double brightness = 1.0) const;
};

class PrefabPreview : public QFrame {
    Q_OBJECT
    std::shared_ptr<const PrefabPreviewGeometry> m_geometry;
    double m_yaw = -45;
    double m_pitch = 30;
    double m_brightness = 1.0;
    bool m_dragging = false;
    QPoint m_dragStart;
    QPointF m_startAngles;
    QTimer m_refresh;
    QImage m_image;
    QSize m_renderSize;

public:
    explicit PrefabPreview(std::shared_ptr<const PrefabPreviewGeometry> geometry, QWidget* parent = nullptr);
    void setOrientation(double yaw, double pitch);
    double yaw() const;
    double pitch() const;
    void setBrightness(double brightness);
    double brightness() const;
    QImage image() const;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
};
}
