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

#include "View/Inspector/Texture/UVEditor.h"

#include <QLabel>
#include <QSpinBox>
#include <QToolBar>
#include <QToolButton>
#include <QtGlobal>

#include "Model/BrushFaceHandle.h"
#include "Model/ChangeBrushFaceAttributesRequest.h"
#include "Model/Game.h"
#include "View/Document/MapDocument.h"
#include "View/Theme/QtUtils.h"
#include "View/Tools/UV/UVView.h"
#include "View/Theme/ViewConstants.h"

#include <kdl/memory_utils.h>

namespace TrenchBroom {
namespace View {
UVEditor::UVEditor(std::weak_ptr<MapDocument> document, GLContextManager &contextManager, QWidget *parent) : QWidget{parent}, m_document{std::move(document)} {
    createGui(contextManager);
    connectObservers();
}

bool UVEditor::cancelMouseDrag() {
    return m_uvView->cancelDrag();
}

void UVEditor::updateButtons() {
    auto document = kdl::mem_lock(m_document);
    const bool enabled = !document->allSelectedBrushFaces().empty();

    m_resetTextureButton->setEnabled(enabled);
    m_resetTextureToWorldButton->setEnabled(enabled);
    m_flipTextureHButton->setEnabled(enabled);
    m_flipTextureVButton->setEnabled(enabled);
    m_rotateTextureCCWButton->setEnabled(enabled);
    m_rotateTextureCWButton->setEnabled(enabled);
}

void UVEditor::createGui(GLContextManager &contextManager) {
    m_uvView = new UVView{m_document, contextManager};

    m_resetTextureButton = createBitmapButton("ResetTexture.svg", tr("Reset texture alignment"), this);
    m_resetTextureToWorldButton = createBitmapButton("ResetTextureToWorld.svg", tr("Reset texture alignment to world aligned"), this);
    m_flipTextureHButton = createBitmapButton("FlipTextureH.svg", tr("Flip texture X axis"), this);
    m_flipTextureVButton = createBitmapButton("FlipTextureV.svg", tr("Flip texture Y axis"), this);
    m_rotateTextureCCWButton = createBitmapButton("RotateTextureCCW.svg", tr("Rotate texture 90° counter-clockwise"), this);
    m_rotateTextureCWButton = createBitmapButton("RotateTextureCW.svg", tr("Rotate texture 90° clockwise"), this);

    connect(m_resetTextureButton, &QAbstractButton::clicked, this, &UVEditor::resetTextureClicked);
    connect(m_resetTextureToWorldButton, &QAbstractButton::clicked, this, &UVEditor::resetTextureToWorldClicked);
    connect(m_flipTextureHButton, &QAbstractButton::clicked, this, &UVEditor::flipTextureHClicked);
    connect(m_flipTextureVButton, &QAbstractButton::clicked, this, &UVEditor::flipTextureVClicked);
    connect(m_rotateTextureCCWButton, &QAbstractButton::clicked, this, &UVEditor::rotateTextureCCWClicked);
    connect(m_rotateTextureCWButton, &QAbstractButton::clicked, this, &UVEditor::rotateTextureCWClicked);

    m_xSubDivisionEditor = new QSpinBox();
    m_xSubDivisionEditor->setRange(1, 16);
    m_xSubDivisionEditor->setValue(1);
    m_xSubDivisionEditor->setToolTip(tr("Horizontal grid subdivisions"));

    m_ySubDivisionEditor = new QSpinBox();
    m_ySubDivisionEditor->setRange(1, 16);
    m_ySubDivisionEditor->setValue(1);
    m_ySubDivisionEditor->setToolTip(tr("Vertical grid subdivisions"));

    connect(m_xSubDivisionEditor, QOverload<int>::of(&QSpinBox::valueChanged), this, &UVEditor::subDivisionChanged);
    connect(m_ySubDivisionEditor, QOverload<int>::of(&QSpinBox::valueChanged), this, &UVEditor::subDivisionChanged);

    auto* toolbar = createInspectorToolBar(this);
    toolbar->addWidget(m_resetTextureButton);
    toolbar->addWidget(m_resetTextureToWorldButton);
    toolbar->addWidget(m_flipTextureHButton);
    toolbar->addWidget(m_flipTextureVButton);
    toolbar->addWidget(m_rotateTextureCCWButton);
    toolbar->addWidget(m_rotateTextureCWButton);
    auto* gridXLabel = new QLabel(tr("Grid X:"));
    gridXLabel->setBuddy(m_xSubDivisionEditor);
    toolbar->addWidget(gridXLabel);
    toolbar->addWidget(m_xSubDivisionEditor);
    auto* gridYLabel = new QLabel(tr("Y:"));
    gridYLabel->setBuddy(m_ySubDivisionEditor);
    toolbar->addWidget(gridYLabel);
    toolbar->addWidget(m_ySubDivisionEditor);

    auto *outerLayout = new QVBoxLayout();
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(toolbar);
    outerLayout->addWidget(m_uvView, 1);
    setLayout(outerLayout);

    updateButtons();
}

void UVEditor::selectionDidChange(const Selection &) {
    updateButtons();
}

void UVEditor::connectObservers() {
    auto document = kdl::mem_lock(m_document);
    m_notifierConnection += document->selectionDidChangeNotifier.connect(this, &UVEditor::selectionDidChange);
}

void UVEditor::resetTextureClicked() {
    Model::ChangeBrushFaceAttributesRequest request;

    auto document = kdl::mem_lock(m_document);
    request.resetAll(document->game()->defaultFaceAttribs());
    document->setFaceAttributes(request);
}

void UVEditor::resetTextureToWorldClicked() {
    Model::ChangeBrushFaceAttributesRequest request;

    auto document = kdl::mem_lock(m_document);
    request.resetAllToParaxial(document->game()->defaultFaceAttribs());
    document->setFaceAttributes(request);
}

void UVEditor::flipTextureHClicked() {
    Model::ChangeBrushFaceAttributesRequest request;
    request.mulXScale(-1.0f);

    auto document = kdl::mem_lock(m_document);
    document->setFaceAttributes(request);
}

void UVEditor::flipTextureVClicked() {
    Model::ChangeBrushFaceAttributesRequest request;
    request.mulYScale(-1.0f);

    auto document = kdl::mem_lock(m_document);
    document->setFaceAttributes(request);
}

void UVEditor::rotateTextureCCWClicked() {
    Model::ChangeBrushFaceAttributesRequest request;
    request.addRotation(90.0f);

    auto document = kdl::mem_lock(m_document);
    document->setFaceAttributes(request);
}

void UVEditor::rotateTextureCWClicked() {
    Model::ChangeBrushFaceAttributesRequest request;
    request.addRotation(-90.0f);

    auto document = kdl::mem_lock(m_document);
    document->setFaceAttributes(request);
}

void UVEditor::subDivisionChanged() {
    const int x = m_xSubDivisionEditor->value();
    const int y = m_ySubDivisionEditor->value();
    m_uvView->setSubDivisions(vm::vec2i(x, y));
}
} // namespace View
} // namespace TrenchBroom
