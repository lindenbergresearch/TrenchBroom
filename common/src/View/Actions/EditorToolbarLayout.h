#pragma once
#include "View/Actions/EditorConfiguration.h"
#include <QRect>

namespace TrenchBroom::View {
struct EditorToolbarLayoutItem {
    EditorToolbarDefinition definition;
    int buttons = 0, separators = 0;
    int padding = 6, buttonPadding = 4, spacing = 4, gap = 6;
};
struct EditorToolbarPlacement {
    QString id;
    QRect geometry;
    int iconSize = 0;
    bool visible = false;
};
QSize editorToolbarSize(const EditorToolbarLayoutItem& item, int iconSize);
std::vector<EditorToolbarPlacement> layoutEditorToolbars(const QRect& area, const std::vector<EditorToolbarLayoutItem>& items, const std::array<EditorToolbarArrangement, 4>& arrangements);
} // namespace TrenchBroom::View
