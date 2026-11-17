#include "ScreenLabelLayout.h"

#include <QHash>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <queue>
#include <set>

namespace TrenchBroom::Renderer {
ScreenLabelLayout::ScreenLabelLayout(QRectF viewport) : m_viewport{viewport.adjusted(4, 4, -4, -4)} {}
size_t ScreenLabelLayout::add(Request request) {
    m_requests.push_back(std::move(request));
    m_valid = false;
    return m_requests.size() - 1;
}
const ScreenLabelLayout::Placement& ScreenLabelLayout::placement(size_t id) {
    if (!m_valid) layout();
    return m_placements.at(id);
}
void ScreenLabelLayout::layout() {
    m_valid = true;
    m_placements.assign(m_requests.size(), {});
    std::vector<size_t> order(m_requests.size());
    std::iota(order.begin(), order.end(), size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) {
        if (m_requests[a].priority != m_requests[b].priority) return m_requests[a].priority > m_requests[b].priority;
        return m_requests[a].distance < m_requests[b].distance;
    });
    // Screen cells keep dense maps from requiring an all-pairs overlap scan.
    QHash<quint64, std::vector<QRectF>> cells;
    const auto key = [](int x, int y) { return (quint64(quint32(x)) << 32) | quint32(y); };
    const auto visitCells = [&](const QRectF& rect, const auto& visit) {
        const auto clipped = rect.intersected(m_viewport);
        if (clipped.isEmpty()) return;
        for (auto y = int(std::floor(clipped.top() / 64)); y <= int(std::floor(clipped.bottom() / 64)); ++y)
            for (auto x = int(std::floor(clipped.left() / 64)); x <= int(std::floor(clipped.right() / 64)); ++x)
                visit(key(x, y));
    };
    for (auto id : order) {
        const auto& request = m_requests[id];
        const auto& desired = request.bounds;
        if (!std::isfinite(desired.x()) || !std::isfinite(desired.y()) || !std::isfinite(desired.width()) || !std::isfinite(desired.height()) || desired.isEmpty()) continue;
        auto placed = desired;
        if (request.movable) {
            if (desired.width() > m_viewport.width() || desired.height() > m_viewport.height()) continue;
            placed.moveLeft(std::clamp(desired.left(), m_viewport.left(), m_viewport.right() - desired.width()));
            placed.moveTop(std::clamp(desired.top(), m_viewport.top(), m_viewport.bottom() - desired.height()));
            // Selection dimensions remain available when most of a large
            // selection lies outside the view; their search starts at its edge.
            const auto reference = request.priority >= 30 ? placed.center() : desired.center();
            struct Candidate {
                QRectF bounds;
                double cost;
                size_t order;
            };
            const auto later = [](const Candidate& a, const Candidate& b) { return a.cost == b.cost ? a.order > b.order : a.cost > b.cost; };
            std::priority_queue<Candidate, std::vector<Candidate>, decltype(later)> candidates{later};
            std::set<std::pair<qreal, qreal>> visited;
            const auto limit = request.priority > 0 ? 256.0 : 96.0;
            size_t serial = 0;
            const auto enqueue = [&](const QRectF& rect) {
                const auto delta = rect.center() - reference;
                const auto cost = delta.x() * delta.x() + delta.y() * delta.y();
                if (serial >= 384 || !m_viewport.contains(rect) || cost > limit * limit || !visited.emplace(rect.x(), rect.y()).second) return;
                candidates.push({rect, cost, serial++});
            };
            enqueue(placed);
            bool found = false;
            for (size_t attempt = 0; !candidates.empty() && attempt < 96; ++attempt) {
                const auto candidate = candidates.top().bounds;
                candidates.pop();
                std::vector<QRectF> blockers;
                visitCells(candidate.adjusted(-4, -4, 4, 4), [&](auto cell) {
                    const auto it = cells.constFind(cell);
                    if (it == cells.cend()) return;
                    for (const auto& rect : *it)
                        if (candidate.adjusted(-4, -4, 4, 4).intersects(rect) && std::find(blockers.begin(), blockers.end(), rect) == blockers.end()) blockers.push_back(rect);
                });
                if (blockers.empty()) {
                    placed = candidate;
                    found = true;
                    break;
                }
                for (const auto& rect : blockers) {
                    enqueue({rect.left() - 4 - desired.width(), candidate.y(), desired.width(), desired.height()});
                    enqueue({rect.right() + 4, candidate.y(), desired.width(), desired.height()});
                    enqueue({candidate.x(), rect.top() - 4 - desired.height(), desired.width(), desired.height()});
                    enqueue({candidate.x(), rect.bottom() + 4, desired.width(), desired.height()});
                }
            }
            if (!found) continue; // Suppress excess labels rather than obscure important ones.
        }
        m_placements[id] = {placed, true};
        visitCells(placed, [&](auto cell) { cells[cell].push_back(placed); });
    }
}
} // namespace TrenchBroom::Renderer
