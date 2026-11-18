/*
 Copyright (C) 2023 Kristian Duske

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

#include "View/Tools/Brush/DrawShapeToolPage.h"
#include "PreferenceManager.h"
#include "Preferences.h"
#include "Renderer/Camera.h"
#include "View/Tools/Brush/DrawShapeToolExtensions.h"
#include "View/Document/MapDocument.h"
#include "View/App/MapFrame.h"
#include "View/MapView/MapView2D.h"
#include "View/Theme/QtUtils.h"
#include <QBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QToolButton>

namespace TrenchBroom::View {
DrawShapeToolPage::DrawShapeToolPage(std::weak_ptr<MapDocument>, DrawShapeToolExtensionManager& manager, QWidget* parent) : QWidget(parent), m_manager(manager) {
    setObjectName("ShapeToolPage");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(0, 0, 0, 0);
    auto* title = new QLabel(tr("Create"));
    title->setObjectName("ShapeToolTitle");
    toolbar->addWidget(title);
    const auto extensions = manager.extensions();
    m_group = createBitmapToggleButton("Group.svg", tr("Group the created brushes when a shape has several parts"));
    m_group->setObjectName("ShapeToolGroup");
    toolbar->addWidget(m_group);
    m_axisLabel = new QLabel(tr("Axis"));
    m_axis = new QComboBox;
    m_axis->setObjectName("ShapeToolAxis");
    m_axis->addItems({tr("View"), tr("X"), tr("Y"), tr("Z")});
    toolbar->addWidget(m_axisLabel);
    toolbar->addWidget(m_axis);
    toolbar->addStretch();
    m_apply = new QPushButton(tr("Apply to Selection"));
    m_apply->setObjectName("ShapeToolApply");
    m_apply->setToolTip(tr("Replace selected, editable brushes in the same container with this shape. Matching face attributes are preserved. Linked groups must be unlinked first."));
    toolbar->addWidget(m_apply);
    layout->addLayout(toolbar);
    m_pages = new QStackedWidget;
    m_pages->setObjectName("ShapeToolParameters");
    for (const auto* extension : extensions)
        m_pages->addWidget(new DrawShapeToolExtensionPage(extension->kind(), manager.parameters()));
    layout->addWidget(m_pages);
    connect(m_group, &QToolButton::toggled, this, [](bool value) { setPref(Preferences::GroupBrushesCreatedByShapeTool, value); });
    connect(m_axis, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) { m_manager.parameters().setAxis(index == 0 ? std::nullopt : std::optional{vm::axis::type(index - 1)}); });
    connect(m_apply, &QPushButton::clicked, this, [this] {
        auto axis = vm::axis::z;
        if (auto* frame = findMapFrame(this))
            if (auto* view = dynamic_cast<MapView2D*>(frame->currentMapViewBase())) axis = vm::find_abs_max_component(view->camera().direction());
        m_manager.applyParametersNotifier(axis);
    });
    m_connections += manager.currentExtensionDidChangeNotifier.connect([this](size_t) { refresh(); });
    m_connections += manager.parameters().parametersDidChangeNotifier.connect([this] { refresh(); });
    auto doc = manager.document();
    m_connections += doc->selectionDidChangeNotifier.connect([this](const auto&) { refresh(); });
    m_connections += doc->documentWasNewedNotifier.connect([this](auto*) { refresh(); });
    m_connections += doc->documentWasLoadedNotifier.connect([this](auto*) { refresh(); });
    m_connections += doc->nodesDidChangeNotifier.connect([this](const auto&) { refresh(); });
    m_connections += doc->nodeLockingDidChangeNotifier.connect([this](const auto&) { refresh(); });
    m_connections += doc->nodeVisibilityDidChangeNotifier.connect([this](const auto&) { refresh(); });
    m_connections += PreferenceManager::instance().preferenceDidChangeNotifier.connect([this](const auto& path) {
        if (path == Preferences::GroupBrushesCreatedByShapeTool.path()) refresh();
    });
    refresh();
}
void DrawShapeToolPage::refresh() {
    const auto& extension = m_manager.currentExtension();
    m_pages->setCurrentIndex(int(extension.kind()));
    const QSignalBlocker a(m_group), b(m_axis);
    m_group->setChecked(pref(Preferences::GroupBrushesCreatedByShapeTool));
    m_axis->setCurrentIndex(m_manager.parameters().axis() ? int(*m_manager.parameters().axis()) + 1 : 0);
    const auto kind = extension.kind();
    const bool axial = kind == DrawShapeKind::Cylinder || kind == DrawShapeKind::Cone || kind == DrawShapeKind::UVSphere || kind == DrawShapeKind::Arch;
    m_axis->setVisible(axial);
    m_axisLabel->setVisible(axial);
    m_apply->setEnabled(m_manager.canApplySelection());
}
} // namespace TrenchBroom::View
