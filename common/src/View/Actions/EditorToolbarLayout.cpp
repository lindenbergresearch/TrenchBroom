#include "View/Actions/EditorToolbarLayout.h"
#include <algorithm>
#include <cmath>

namespace TrenchBroom::View {
namespace {
    bool vertical(EditorToolbarEdge edge) { return edge == EditorToolbarEdge::Left || edge == EditorToolbarEdge::Right; }
    struct Side {
        std::vector<const EditorToolbarLayoutItem*> items;
        std::vector<QRect> rects;
        std::vector<int> icons;
        EditorToolbarArrangement arrangement;
        double factor = 1, minimum = 0;
        int thickness = 0, length = 0;
        bool visible = true;

        bool evaluate(double scale) {
            rects.clear();
            icons.clear();
            thickness = 0;
            if (items.empty()) return true;
            const auto gap = (*std::max_element(items.begin(), items.end(), [](auto* a, auto* b) { return a->gap < b->gap; }))->gap;
            std::array<int, 3> lengths{}, counts{};
            std::vector<QSize> sizes;
            for (const auto* item : items) {
                const auto icon = std::max(item->definition.minimumIconSize, int(std::floor(item->definition.iconSize * scale + .000001)));
                icons.push_back(icon);
                auto size = editorToolbarSize(*item, icon);
                if (!vertical(item->definition.edge)) size.transpose(); // x = cross, y = along edge
                sizes.push_back(size);
                const auto group = size_t(item->definition.alignment);
                lengths[group] += size.height() + (counts[group]++ ? gap : 0);
            }
            std::array<int, 3> positions{0, (length - lengths[1]) / 2, length - lengths[2]};
            auto cross = 0;
            for (size_t i = 0; i < items.size(); ++i) {
                const auto group = size_t(items[i]->definition.alignment);
                const auto main =
                    arrangement == EditorToolbarArrangement::Parallel ? (group == 0 ? 0 : group == 1 ? (length - sizes[i].height()) / 2 : length - sizes[i].height()) : positions[group];
                if (main < 0 || main + sizes[i].height() > length) return false;
                rects.emplace_back(arrangement == EditorToolbarArrangement::Parallel ? cross : 0, main, sizes[i].width(), sizes[i].height());
                if (arrangement == EditorToolbarArrangement::Parallel) {
                    cross += sizes[i].width() + gap;
                    thickness = cross - gap;
                } else {
                    thickness = std::max(thickness, sizes[i].width());
                    positions[group] += sizes[i].height() + gap;
                }
            }
            for (size_t i = 0; i < rects.size(); ++i)
                for (size_t j = i + 1; j < rects.size(); ++j) {
                    if (rects[i].intersects(rects[j])) return false;
                    if (arrangement == EditorToolbarArrangement::Sequential) {
                        const auto separation = std::max(rects[i].top(), rects[j].top()) - std::min(rects[i].bottom(), rects[j].bottom()) - 1;
                        if (separation < gap) return false;
                    }
                }
            return true;
        }
        void fit(int availableLength) {
            length = availableLength;
            if (items.empty()) {
                thickness = 0;
                return;
            }
            for (const auto* item : items)
                minimum = std::max(minimum, double(item->definition.minimumIconSize) / item->definition.iconSize);
            if (!evaluate(minimum)) {
                visible = false;
                thickness = 0;
                return;
            }
            if (evaluate(1)) {
                factor = 1;
                return;
            }
            auto low = minimum, high = 1.0;
            for (auto i = 0; i < 24; ++i) {
                const auto mid = (low + high) / 2;
                if (evaluate(mid))
                    low = mid;
                else
                    high = mid;
            }
            factor = low;
            evaluate(factor);
        }
    };

    void fitOppositeSides(Side& a, Side& b, int available) {
        const auto fits = [&](double scale) {
            if (a.visible) a.evaluate(a.factor * scale);
            if (b.visible) b.evaluate(b.factor * scale);
            return a.thickness + b.thickness <= available;
        };
        if (fits(1)) return;
        const auto minimum = std::max(a.visible ? a.minimum / a.factor : 0, b.visible ? b.minimum / b.factor : 0);
        if (!fits(minimum)) {
            a.visible = b.visible = false;
            a.thickness = b.thickness = 0;
            return;
        }
        auto low = minimum, high = 1.0;
        for (auto i = 0; i < 24; ++i) {
            const auto mid = (low + high) / 2;
            if (fits(mid))
                low = mid;
            else
                high = mid;
        }
        a.factor *= low;
        b.factor *= low;
        fits(1);
    }
} // namespace

QSize editorToolbarSize(const EditorToolbarLayoutItem& item, const int iconSize) {
    const auto button = iconSize + 2 * item.buttonPadding;
    const auto count = item.buttons + item.separators;
    const auto length = item.buttons * button + item.separators * (item.spacing * 2 + 1) + std::max(0, count - 1) * item.spacing + 2 * item.padding;
    const auto cross = button + 2 * item.padding;
    return vertical(item.definition.edge) ? QSize{cross, length} : QSize{length, cross};
}

std::vector<EditorToolbarPlacement> layoutEditorToolbars(const QRect& area, const std::vector<EditorToolbarLayoutItem>& items, const std::array<EditorToolbarArrangement, 4>& arrangements) {
    std::array<Side, 4> sides;
    std::vector<EditorToolbarPlacement> result;
    for (size_t i = 0; i < 4; ++i)
        sides[i].arrangement = arrangements[i];
    for (const auto& item : items) {
        result.push_back({item.definition.id, {}, 0, false});
        if (item.definition.enabled && item.buttons > 0) sides[size_t(item.definition.edge)].items.push_back(&item);
    }
    if (area.isEmpty()) return result;
    auto& left = sides[0];
    auto& top = sides[1];
    auto& right = sides[2];
    auto& bottom = sides[3];
    left.fit(area.height());
    right.fit(area.height());
    fitOppositeSides(left, right, area.width());
    const auto sideGap = [](const Side& side) { return side.visible && !side.items.empty() ? side.items.front()->gap : 0; };
    const auto horizontal = area.adjusted(left.thickness + sideGap(left), 0, -right.thickness - sideGap(right), 0);
    top.fit(horizontal.width());
    bottom.fit(horizontal.width());
    fitOppositeSides(top, bottom, area.height());
    for (size_t edge = 0; edge < 4; ++edge) {
        const auto& side = sides[edge];
        if (!side.visible) continue;
        for (size_t i = 0; i < side.items.size(); ++i) {
            auto rect = side.rects[i];
            if (edge == 0)
                rect.translate(area.topLeft());
            else if (edge == 2)
                rect = {area.right() + 1 - rect.x() - rect.width(), area.y() + rect.y(), rect.width(), rect.height()};
            else if (edge == 1)
                rect = {horizontal.x() + rect.y(), area.y() + rect.x(), rect.height(), rect.width()};
            else
                rect = {horizontal.x() + rect.y(), area.bottom() + 1 - rect.x() - rect.width(), rect.height(), rect.width()};
            const auto found = std::find_if(result.begin(), result.end(), [&](const auto& placement) { return placement.id == side.items[i]->definition.id; });
            *found = {found->id, rect, side.icons[i], true};
        }
    }
    return result;
}
} // namespace TrenchBroom::View
