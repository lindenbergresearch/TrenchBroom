#include "View/Prefabs/PrefabPreview.h"

#include "Assets/EntityDefinition.h"
#include "Assets/Texture.h"
#include "IO/File.h"
#include "IO/FileSystem.h"
#include "IO/LoadTextureCollection.h"
#include "IO/ReadFreeImageTexture.h"
#include "Model/BrushFace.h"
#include "Model/BrushNode.h"
#include "Model/EntityNode.h"
#include "Model/Game.h"
#include "Model/PatchNode.h"
#include "View/Theme/QtUtils.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace TrenchBroom::View {
namespace {
double yawAngle(double value) { return std::isfinite(value) ? std::remainder(value, 360.0) : -45.0; }
double pitchAngle(double value) { return std::isfinite(value) ? std::clamp(value, -85.0, 85.0) : 30.0; }
double previewBrightness(double value) { return std::isfinite(value) ? std::clamp(value, 0.5, 1.5) : 1.0; }

QImage texturePixels(const Assets::Texture& texture) {
    const auto& buffers = texture.buffersIfUnprepared();
    const auto width = texture.width(), height = texture.height();
    const auto format = texture.format();
    if (buffers.empty() || !width || !height || width > 16384 || height > 16384) return {};
    if (format != GL_RGB && format != GL_BGR && format != GL_RGBA && format != GL_BGRA) return {};
    const auto stride = size_t(format == GL_RGB || format == GL_BGR ? 3 : 4);
    if (buffers.front().size() < width * height * stride) return {};
    QImage image(int(width), int(height), QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) return {};
    const auto* bytes = buffers.front().data();
    const auto red = format == GL_BGR || format == GL_BGRA ? 2 : 0;
    const auto blue = red == 2 ? 0 : 2;
    const auto masked = texture.type() == Assets::TextureType::Masked && stride == 4;
    for (size_t y = 0; y < height; ++y) {
        auto* row = reinterpret_cast<QRgb*>(image.scanLine(int(y)));
        for (size_t x = 0; x < width; ++x) {
            const auto* pixel = bytes + (y * width + x) * stride;
            row[x] = qPremultiply(qRgba(pixel[red], pixel[1], pixel[blue], masked ? pixel[3] : 255));
        }
    }
    return image;
}

QImage loadTexturePixels(const Assets::Texture& texture, const Model::Game* game) {
    auto image = texturePixels(texture);
    if (!image.isNull() || !game || !game->fileSystem() || !game->textureConfig() || texture.relativePath().empty()) return image;
    // Prepared editor textures have released their CPU buffers. Decode only the
    // resources used by this preview, without reading back or changing GL state.
    image = IO::loadTexture(texture.relativePath(), *game->fileSystem(), *game->textureConfig())
        .transform([](auto decoded) { return texturePixels(decoded); }).value_or(QImage{});
    if (image.isNull() && Assets::isCompressedFormat(texture.format())) {
        // The editor uploads DDS blocks directly; FreeImage supplies CPU pixels.
        image = game->fileSystem()->openFile(texture.relativePath()).transform([&](const auto& file) {
            auto reader = file->reader().buffer();
            return IO::readFreeImageTexture(texture.name(), reader)
                .transform([](auto decoded) { return texturePixels(decoded); }).value_or(QImage{});
        }).value_or(QImage{});
    }
    return image;
}

// Bilinear, repeating sampling in premultiplied color space. Coordinates are
// normalized just like BrushFace::textureCoords and the editor's texture shader.
QRgb sampleTexture(const QImage& image, double u, double v) {
    const auto x = (u - std::floor(u)) * image.width() - 0.5;
    const auto y = (v - std::floor(v)) * image.height() - 0.5;
    const auto ix = int(std::floor(x)), iy = int(std::floor(y));
    const auto x0 = (ix + image.width()) % image.width(), x1 = (x0 + 1) % image.width();
    const auto y0 = (iy + image.height()) % image.height(), y1 = (y0 + 1) % image.height();
    const auto a = int((x - ix) * 256), b = int((y - iy) * 256);
    const auto* row0 = reinterpret_cast<const QRgb*>(image.constScanLine(y0));
    const auto* row1 = reinterpret_cast<const QRgb*>(image.constScanLine(y1));
    const auto c00 = row0[x0], c10 = row0[x1], c01 = row1[x0], c11 = row1[x1];
    const auto channel = [&](int shift) {
        const auto top = int((c00 >> shift) & 255) * (256 - a) + int((c10 >> shift) & 255) * a;
        const auto bottom = int((c01 >> shift) & 255) * (256 - a) + int((c11 >> shift) & 255) * a;
        return (top * (256 - b) + bottom * b + 32768) >> 16;
    };
    return qRgba(channel(16), channel(8), channel(0), channel(24));
}
}

QImage prefabPreviewBrightness(const QImage& image, double brightness) {
    brightness = previewBrightness(brightness);
    if (image.isNull() || brightness == 1.0) return image;
    auto result = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for (auto y = 0; y < result.height(); ++y) {
        auto* row = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (auto x = 0; x < result.width(); ++x) {
            const auto pixel = row[x];
            const auto alpha = qAlpha(pixel);
            row[x] = qRgba(std::min(alpha, qRound(qRed(pixel)*brightness)),
                std::min(alpha, qRound(qGreen(pixel)*brightness)), std::min(alpha, qRound(qBlue(pixel)*brightness)), alpha);
        }
    }
    return result;
}

PrefabPreviewGeometry::PrefabPreviewGeometry(Model::Node& root, const Model::Game* game) {
    std::unordered_map<const Assets::Texture*, std::shared_ptr<const Material>> materials;
    const auto materialFor = [&](const Assets::Texture* texture) -> std::shared_ptr<const Material> {
        if (!texture) return {};
        if (const auto it = materials.find(texture); it != materials.end()) return it->second;
        auto material = std::make_shared<Material>();
        material->masked = texture->type() == Assets::TextureType::Masked;
        auto image = loadTexturePixels(*texture, game);
        if (!image.isNull()) {
            if (image.width() > 1024 || image.height() > 1024)
                image = image.scaled(1024, 1024, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            material->mipmaps.push_back(std::move(image));
            while (material->mipmaps.back().width() > 1 || material->mipmaps.back().height() > 1) {
                const auto& previous = material->mipmaps.back();
                material->mipmaps.push_back(previous.scaled(std::max(1, previous.width()/2), std::max(1, previous.height()/2), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            }
        }
        materials.emplace(texture, material);
        return material;
    };
    const auto addBox = [&](const vm::bbox3& b, const QColor& color) {
        const auto x0 = b.min.x(), x1 = b.max.x(), y0 = b.min.y(), y1 = b.max.y(), z0 = b.min.z(), z1 = b.max.z();
        m_faces.push_back({{{x1,y0,z0},{x1,y1,z0},{x1,y1,z1},{x1,y0,z1}}, {1,0,0}, color, false, {}, {}});
        m_faces.push_back({{{x0,y1,z0},{x0,y0,z0},{x0,y0,z1},{x0,y1,z1}}, {-1,0,0}, color, false, {}, {}});
        m_faces.push_back({{{x1,y1,z0},{x0,y1,z0},{x0,y1,z1},{x1,y1,z1}}, {0,1,0}, color, false, {}, {}});
        m_faces.push_back({{{x0,y0,z0},{x1,y0,z0},{x1,y0,z1},{x0,y0,z1}}, {0,-1,0}, color, false, {}, {}});
        m_faces.push_back({{{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}}, {0,0,1}, color, false, {}, {}});
        m_faces.push_back({{{x0,y1,z0},{x1,y1,z0},{x1,y0,z0},{x0,y0,z0}}, {0,0,-1}, color, false, {}, {}});
    };
    const auto visit = [&](auto&& self, Model::Node* node) -> void {
        if (const auto* brush = dynamic_cast<Model::BrushNode*>(node)) {
            for (const auto& face : brush->brush().faces()) {
                auto preview = Face{face.vertexPositions(), face.normal(), face.texture() ? toQColor(face.texture()->averageColor()) : QColor(155,180,205), false, {}, {}};
                for (const auto& point : preview.points) preview.texCoords.push_back(face.textureCoords(point));
                preview.material = materialFor(face.texture());
                m_faces.push_back(std::move(preview));
            }
        } else if (const auto* patch = dynamic_cast<Model::PatchNode*>(node)) {
            const auto& grid = patch->grid();
            const auto* texture = patch->patch().texture();
            const auto material = materialFor(texture);
            const auto color = texture ? toQColor(texture->averageColor()) : QColor(155,180,205);
            for (size_t r = 0; r < grid.quadRowCount(); ++r) for (size_t c = 0; c < grid.quadColumnCount(); ++c) {
                // Split non-planar patch quads before projecting. Whole projected
                // quads can fold over themselves as the camera moves.
                const auto points = std::array{grid.point(r,c), grid.point(r+1,c), grid.point(r+1,c+1), grid.point(r,c+1)};
                for (const auto indices : {std::array<size_t,3>{0,1,2}, std::array<size_t,3>{0,2,3}}) {
                    const auto normal = vm::cross(points[indices[1]].position - points[0].position, points[indices[2]].position - points[0].position);
                    if (vm::squared_length(normal) < 1e-12) continue;
                    auto preview = Face{{}, vm::normalize(normal), color, true, {}, material};
                    for (const auto i : indices) { preview.points.push_back(points[i].position); preview.texCoords.push_back(vm::vec2f(points[i].texCoords)); }
                    m_faces.push_back(std::move(preview));
                }
            }
        } else if (const auto* entity = dynamic_cast<Model::EntityNode*>(node); entity && !entity->hasChildren()) {
            addBox(entity->physicalBounds(), entity->entity().definition() ? toQColor(entity->entity().definition()->color()) : QColor(230,180,95));
        }
        for (auto* child : node->children()) self(self, child);
    };
    visit(visit, &root);
    const auto center = root.physicalBounds().center();
    for (auto& face : m_faces) for (auto& point : face.points) point = point - center;
}

QImage PrefabPreviewGeometry::render(const QSize& size, double yaw, double pitch, double brightness) const {
    if (size.isEmpty()) return {};
    // Supersampling smooths silhouettes and alpha-tested texture edges. Large
    // high-DPI previews already provide sufficient samples at their native size.
    const auto samples = qint64(size.width()) * size.height() <= 1024 * 768 ? 2 : 1;
    const auto renderSize = size * samples;
    QImage image(renderSize, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) return {};
    image.fill(Qt::transparent);
    const auto width = image.width(), height = image.height();
    auto depth = std::vector<double>(size_t(width) * size_t(height), -std::numeric_limits<double>::infinity());
    constexpr double radians = 3.14159265358979323846 / 180.0;
    const auto azimuth = yawAngle(yaw) * radians, elevation = pitchAngle(pitch) * radians;
    const auto camera = vm::vec3{std::cos(elevation)*std::cos(azimuth), std::cos(elevation)*std::sin(azimuth), std::sin(elevation)};
    const auto right = vm::vec3{-std::sin(azimuth), std::cos(azimuth), 0};
    const auto up = vm::cross(camera, right);
    const auto project = [&](const vm::vec3& point) { return QPointF(vm::dot(point, right), -vm::dot(point, up)); };
    QRectF extent;
    bool first = true;
    for (const auto& face : m_faces) for (const auto& point : face.points) {
        const auto p = project(point);
        if (first) { extent = QRectF(p, QSizeF(0,0)); first = false; }
        else {
            extent.setLeft(std::min(extent.left(), p.x())); extent.setRight(std::max(extent.right(), p.x()));
            extent.setTop(std::min(extent.top(), p.y())); extent.setBottom(std::max(extent.bottom(), p.y()));
        }
    }
    const auto scale = std::min(width*0.93 / std::max(1.0, extent.width()), height*0.92 / std::max(1.0, extent.height()));
    const auto light = vm::normalize(vm::vec3{0.3,-0.5,1});
    struct Vertex { double x, y, z, u, v; };
    for (const auto& face : m_faces) {
        if (face.points.size() < 3) continue;
        const auto facing = vm::dot(face.normal, camera);
        if (!face.twoSided && facing <= 0) continue;
        const auto normal = facing < 0 ? -face.normal : face.normal;
        const auto lighting = 0.45 + 0.55 * std::max(0.0, vm::dot(normal, light));
        const auto textured = face.material && !face.material->mipmaps.empty() && face.texCoords.size() == face.points.size()
            && std::all_of(face.texCoords.begin(), face.texCoords.end(), [](const auto& uv) { return std::isfinite(uv.x()) && std::isfinite(uv.y()); });
        auto vertices = std::vector<Vertex>{};
        vertices.reserve(face.points.size());
        for (size_t i = 0; i < face.points.size(); ++i) {
            const auto p = (project(face.points[i]) - extent.center()) * scale + QPointF(width/2.0, height/2.0);
            const auto uv = textured ? face.texCoords[i] : vm::vec2f{0,0};
            vertices.push_back({p.x(), p.y(), vm::dot(face.points[i], camera), uv.x(), uv.y()});
        }
        const auto flatColor = qRgb(int(face.color.red()*lighting), int(face.color.green()*lighting), int(face.color.blue()*lighting));
        for (size_t triangle = 1; triangle + 1 < vertices.size(); ++triangle) {
            const auto a = vertices[0], b = vertices[triangle], c = vertices[triangle+1];
            const auto area = (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x);
            if (std::abs(area) < 1e-10) continue;
            const auto bx = (c.y-a.y)/area, by = (a.x-c.x)/area;
            const auto cx = (a.y-b.y)/area, cy = (b.x-a.x)/area;
            const QImage* texture = nullptr;
            if (textured) {
                const auto& mipmaps = face.material->mipmaps;
                const auto dux = (b.u-a.u)*bx + (c.u-a.u)*cx, duy = (b.u-a.u)*by + (c.u-a.u)*cy;
                const auto dvx = (b.v-a.v)*bx + (c.v-a.v)*cx, dvy = (b.v-a.v)*by + (c.v-a.v)*cy;
                const auto footprint = std::max(std::hypot(dux*mipmaps[0].width(), dvx*mipmaps[0].height()), std::hypot(duy*mipmaps[0].width(), dvy*mipmaps[0].height()));
                const auto level = std::clamp(int(std::floor(std::log2(std::max(1.0, footprint)))), 0, int(mipmaps.size())-1);
                texture = &mipmaps[size_t(level)];
            }
            const auto x0 = std::clamp(int(std::floor(std::min({a.x,b.x,c.x}))), 0, width-1);
            const auto x1 = std::clamp(int(std::ceil(std::max({a.x,b.x,c.x}))), 0, width-1);
            const auto y0 = std::clamp(int(std::floor(std::min({a.y,b.y,c.y}))), 0, height-1);
            const auto y1 = std::clamp(int(std::ceil(std::max({a.y,b.y,c.y}))), 0, height-1);
            for (auto y = y0; y <= y1; ++y) {
                auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
                auto* depths = depth.data() + size_t(y)*size_t(width);
                auto wb = (x0+0.5-a.x)*bx + (y+0.5-a.y)*by;
                auto wc = (x0+0.5-a.x)*cx + (y+0.5-a.y)*cy;
                for (auto x = x0; x <= x1; ++x, wb += bx, wc += cx) {
                    if (wb < -1e-9 || wc < -1e-9 || wb+wc > 1+1e-9) continue;
                    const auto z = a.z + wb*(b.z-a.z) + wc*(c.z-a.z);
                    if (z <= depths[x]) continue;
                    auto color = flatColor;
                    if (texture) {
                        color = sampleTexture(*texture, a.u + wb*(b.u-a.u) + wc*(c.u-a.u), a.v + wb*(b.v-a.v) + wc*(c.v-a.v));
                        const auto alpha = qAlpha(color);
                        if (face.material->masked && alpha < 128) continue;
                        const auto shade = lighting * 255 / std::max(1, alpha);
                        color = qRgb(std::min(255, int(qRed(color)*shade)), std::min(255, int(qGreen(color)*shade)), std::min(255, int(qBlue(color)*shade)));
                    }
                    row[x] = color;
                    depths[x] = z;
                }
            }
        }
    }
    return prefabPreviewBrightness(samples == 1 ? image : image.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation), brightness);
}

PrefabPreview::PrefabPreview(std::shared_ptr<const PrefabPreviewGeometry> geometry, QWidget* parent)
    : QFrame(parent), m_geometry(std::move(geometry)) {
    setObjectName("PrefabPreview");
    setFrameShape(QFrame::StyledPanel); setBackgroundRole(QPalette::Base); setAutoFillBackground(true);
    setMinimumSize(240, 180); setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::OpenHandCursor);
    setAccessibleName(tr("Prefab preview"));
    setToolTip(tr("Drag with the left mouse button to rotate. Double-click to reset the view."));
    m_refresh.setSingleShot(true); m_refresh.setInterval(16);
    connect(&m_refresh, &QTimer::timeout, this, qOverload<>(&QWidget::update));
}

void PrefabPreview::setOrientation(double yaw, double pitch) {
    const auto nextYaw = yawAngle(yaw), nextPitch = pitchAngle(pitch);
    if (nextYaw == m_yaw && nextPitch == m_pitch) return;
    m_yaw = nextYaw; m_pitch = nextPitch; m_image = {};
    if (!m_refresh.isActive()) m_refresh.start();
}
double PrefabPreview::yaw() const { return m_yaw; }
double PrefabPreview::pitch() const { return m_pitch; }
void PrefabPreview::setBrightness(double brightness) {
    const auto value = previewBrightness(brightness);
    if (m_brightness == value) return;
    m_brightness = value; m_image = {};
    if (!m_refresh.isActive()) m_refresh.start();
}
double PrefabPreview::brightness() const { return m_brightness; }
QImage PrefabPreview::image() const { return m_geometry ? m_geometry->render({512,384}, m_yaw, m_pitch, m_brightness) : QImage{}; }
QSize PrefabPreview::sizeHint() const { return {320,240}; }

void PrefabPreview::paintEvent(QPaintEvent* event) {
    QFrame::paintEvent(event);
    if (!m_geometry) return;
    const auto available = contentsRect().adjusted(8,8,-8,-8);
    const auto fitted = QSize(4,3).scaled(available.size(), Qt::KeepAspectRatio);
    const auto target = QRect(available.center() - QPoint(fitted.width()/2, fitted.height()/2), fitted);
    const auto size = target.size() * devicePixelRatioF();
    if (m_image.isNull() || m_renderSize != size) { m_image = m_geometry->render(size, m_yaw, m_pitch, m_brightness); m_renderSize = size; }
    QPainter painter(this); painter.setRenderHint(QPainter::SmoothPixmapTransform); painter.drawImage(target, m_image);
}

void PrefabPreview::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) { QFrame::mousePressEvent(event); return; }
    m_dragging = true; m_dragStart = event->pos(); m_startAngles = {m_yaw, m_pitch};
    setCursor(Qt::ClosedHandCursor); event->accept();
}
void PrefabPreview::mouseMoveEvent(QMouseEvent* event) {
    if (!m_dragging) { QFrame::mouseMoveEvent(event); return; }
    const auto delta = event->pos() - m_dragStart;
    setOrientation(m_startAngles.x()-delta.x()*0.5, m_startAngles.y()+delta.y()*0.5); event->accept();
}
void PrefabPreview::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton || !m_dragging) { QFrame::mouseReleaseEvent(event); return; }
    mouseMoveEvent(event); m_dragging = false; setCursor(Qt::OpenHandCursor); event->accept();
}
void PrefabPreview::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) { QFrame::mouseDoubleClickEvent(event); return; }
    m_dragging = false; setCursor(Qt::OpenHandCursor); setOrientation(-45,30); event->accept();
}
}
