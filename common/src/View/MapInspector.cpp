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

#include "MapInspector.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QToolBar>

#include "Model/EntityProperties.h"
#include "Model/WorldNode.h"
#include "View/TitledPanel.h"
#include "View/MapDocument.h"
#include "View/ModEditor.h"
#include "View/QtUtils.h"
#include "View/TitleBar.h"
#include "View/ViewConstants.h"

#include <kdl/memory_utils.h>

#include <vm/vec_io.h>

#include <optional>
#include <utility>

namespace TrenchBroom {
namespace View {
// MapInspector

MapInspector::MapInspector(std::weak_ptr<MapDocument> document, QWidget *parent)
    : QWidget(parent) {
    createGui(document);
    setObjectName("MapInspector_Widget");
}

std::vector<InspectorToolSection> MapInspector::toolSections() const {
    return {
        {QStringLiteral("map-properties"), tr("Map Properties"), m_mapPropertiesEditorPanel,
         m_mapPropertiesEditorPanel ? m_mapPropertiesEditorPanel->getTitleBar() : nullptr, 0},
        {QStringLiteral("mods"), tr("Mods"), m_modEditorPanel,
         m_modEditorPanel ? m_modEditorPanel->getTitleBar() : nullptr, 1},
    };
}

void MapInspector::createGui(std::weak_ptr<MapDocument> document) {
    m_mapPropertiesEditorPanel = createMapPropertiesEditor(document);
    m_modEditorPanel = createModEditor(document);
}

TitledPanel *MapInspector::createMapPropertiesEditor(std::weak_ptr<MapDocument> document) {
    TitledPanel *titledPanel = new TitledPanel(tr("Map Properties"), this);
    titledPanel->setObjectName("MapInspector_MapPropertiesPanel");

    auto *editor = new MapPropertiesEditor(document);

    auto *sizer = new QVBoxLayout();
    sizer->setContentsMargins(0, 0, 0, 0);
    sizer->addWidget(editor, 1);
    titledPanel->getPanel()->setLayout(sizer);

    return titledPanel;
}

TitledPanel *MapInspector::createModEditor(std::weak_ptr<MapDocument> document) {
    TitledPanel *titledPanel = new TitledPanel(tr("Mods"), this);
    titledPanel->setObjectName("MapInspector_ModsPanel");

    ModEditor *modEditor = new ModEditor(document);

    auto *sizer = new QVBoxLayout();
    sizer->setContentsMargins(0, 0, 0, 0);
    sizer->addWidget(modEditor, 1);
    titledPanel->getPanel()->setLayout(sizer);

    return titledPanel;
}

// MapPropertiesEditor

MapPropertiesEditor::MapPropertiesEditor(std::weak_ptr<MapDocument> document, QWidget *parent)
    : QWidget(parent), m_document(document), m_updatingGui(false), m_softBoundsFromGameMinLabel(nullptr), m_softBoundsFromGameMaxLabel(nullptr), m_softBoundsFromMapMinEdit(nullptr), m_softBoundsFromMapMaxEdit(nullptr) {
    createGui();
    connectObservers();
}

static std::optional<vm::vec3> parseVec(const QString &qString) {
    const std::string string = qString.toStdString();
    if (const auto vec = vm::parse<double, 3u>(string)) {
        return *vec;
    } else if (const auto val = vm::parse<double, 1u>(string)) {
        return vm::vec3::fill(val->x());
    } else {
        return std::nullopt;
    }
}

std::optional<vm::bbox3> MapPropertiesEditor::parseLineEdits() {
    const auto min = parseVec(m_softBoundsFromMapMinEdit->text());
    const auto max = parseVec(m_softBoundsFromMapMaxEdit->text());

    if (!min.has_value() || !max.has_value()) {
        return std::nullopt;
    }

    for (size_t i = 0; i < 3; ++i) {
        if ((*min)[i] >= (*max)[i]) {
            return std::nullopt;
        }
    }

    return vm::bbox3(*min, *max);
}

void MapPropertiesEditor::createGui() {
    setObjectName("MapPropertiesEditor");
    setAttribute(Qt::WA_StyledBackground);

    auto *toolbar = createInspectorToolBar();
    auto *caption = new QLabel(tr("Soft bounds"));
    m_boundsMode = new QComboBox();
    caption->setBuddy(m_boundsMode);
    m_boundsMode->setAccessibleName(caption->text());
    m_boundsMode->addItems({tr("Disabled"), tr("Game default"), tr("Custom")});
    toolbar->addWidget(caption);
    toolbar->addWidget(m_boundsMode);

    m_softBoundsFromGameMinLabel = new QLabel();
    m_softBoundsFromGameMaxLabel = new QLabel();
    makeInfo(m_softBoundsFromGameMinLabel);
    makeInfo(m_softBoundsFromGameMaxLabel);
    m_softBoundsFromMapMinEdit = new QLineEdit();
    m_softBoundsFromMapMaxEdit = new QLineEdit();

    auto *fields = new QFormLayout();
    fields->setContentsMargins(LayoutConstants::MediumHMargin, LayoutConstants::MediumVMargin,
                               LayoutConstants::MediumHMargin, LayoutConstants::MediumVMargin);
    fields->addRow(tr("Game minimum"), m_softBoundsFromGameMinLabel);
    fields->addRow(tr("Game maximum"), m_softBoundsFromGameMaxLabel);
    fields->addRow(tr("Custom minimum"), m_softBoundsFromMapMinEdit);
    fields->addRow(tr("Custom maximum"), m_softBoundsFromMapMaxEdit);

    auto *box = new QVBoxLayout(this);
    box->setContentsMargins(0, 0, 0, 0);
    box->addWidget(toolbar);
    box->addLayout(fields);

    connect(m_boundsMode, qOverload<int>(&QComboBox::activated), this, [this](int mode) {
        auto document = kdl::mem_lock(m_document);
        if (!document) return;
        if (mode == Disabled) {
            document->setSoftMapBounds({Model::Game::SoftMapBoundsType::Map, std::nullopt});
        } else if (mode == GameDefault) {
            document->setSoftMapBounds({Model::Game::SoftMapBoundsType::Game, std::nullopt});
        } else {
            m_softBoundsFromMapMinEdit->setEnabled(true);
            m_softBoundsFromMapMaxEdit->setEnabled(true);
            // Commit custom bounds once both fields contain a valid box.
            if (const auto parsed = parseLineEdits())
                document->setSoftMapBounds({Model::Game::SoftMapBoundsType::Map, *parsed});
        }
    });

    const auto textEditingFinished = [this]() {
        // Disabling the fields can also emit editingFinished.
        if (m_updatingGui || m_boundsMode->currentIndex() != Custom) {
            return;
        }
        auto document = kdl::mem_lock(m_document);

        const std::optional<vm::bbox3> parsed = parseLineEdits();
        if (parsed.has_value()) {
            document->setSoftMapBounds({Model::Game::SoftMapBoundsType::Map, parsed.value()});
        }
    };
    connect(m_softBoundsFromMapMinEdit, &QLineEdit::editingFinished, this, textEditingFinished);
    connect(m_softBoundsFromMapMaxEdit, &QLineEdit::editingFinished, this, textEditingFinished);

    updateGui();
}

void MapPropertiesEditor::connectObservers() {
    auto document = kdl::mem_lock(m_document);
    m_notifierConnection += document->documentWasNewedNotifier.connect(this, &MapPropertiesEditor::documentWasNewed);
    m_notifierConnection += document->documentWasLoadedNotifier.connect(this, &MapPropertiesEditor::documentWasLoaded);
    m_notifierConnection += document->nodesDidChangeNotifier.connect(this, &MapPropertiesEditor::nodesDidChange);
}

void MapPropertiesEditor::documentWasNewed(MapDocument *) {
    updateGui();
}

void MapPropertiesEditor::documentWasLoaded(MapDocument *) {
    updateGui();
}

void MapPropertiesEditor::nodesDidChange(const std::vector<Model::Node *> &nodes) {
    auto document = kdl::mem_lock(m_document);
    if (!document) {
        return;
    }

    for (Model::Node *node : nodes) {
        if (node == document->world()) {
            updateGui();
            return;
        }
    }
}

static QString formatVec(const std::optional<vm::bbox3> &bbox, const bool max) {
    if (!bbox.has_value()) {
        return QObject::tr("None");
    }
    const vm::vec3 &vec = max ? bbox->max : bbox->min;
    std::stringstream str;
    if (vec.x() == vec.y() && vec.y() == vec.z()) {
        // Just print the first component to save space
        str << vec.x();
    } else {
        str << vec;
    }
    return QString::fromStdString(str.str());
}

/**
 * Refresh the UI from the model
 */
void MapPropertiesEditor::updateGui() {
    const kdl::set_temp flagChange(m_updatingGui, true);

    auto document = kdl::mem_lock(m_document);
    if (!document) {
        return;
    }

    auto game = document->game();
    if (game == nullptr) {
        return;
    }

    const std::optional<vm::bbox3> gameBounds = game->softMapBounds();
    m_softBoundsFromGameMinLabel->setText(formatVec(gameBounds, false));
    m_softBoundsFromGameMaxLabel->setText(formatVec(gameBounds, true));

    const auto bounds = document->softMapBounds();

    if (bounds.source == Model::Game::SoftMapBoundsType::Map && !bounds.bounds.has_value()) {
        m_boundsMode->setCurrentIndex(Disabled);

        m_softBoundsFromMapMinEdit->setEnabled(false);
        m_softBoundsFromMapMaxEdit->setEnabled(false);
    } else if (bounds.source == Model::Game::SoftMapBoundsType::Map) {
        m_boundsMode->setCurrentIndex(Custom);

        m_softBoundsFromMapMinEdit->setEnabled(true);
        m_softBoundsFromMapMaxEdit->setEnabled(true);

        m_softBoundsFromMapMinEdit->setText(formatVec(bounds.bounds, false));
        m_softBoundsFromMapMaxEdit->setText(formatVec(bounds.bounds, true));
    } else {
        m_boundsMode->setCurrentIndex(GameDefault);

        m_softBoundsFromMapMinEdit->setEnabled(false);
        m_softBoundsFromMapMaxEdit->setEnabled(false);
    }
}
} // namespace View
} // namespace TrenchBroom
