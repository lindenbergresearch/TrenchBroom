/*
Copyright (C) 2023 Kristian Duske

This file is part of TrenchBroom.

   TrenchBroom is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
     the Free Software Foundation, either version 3 of the License, or
                                    (at your option) any later version.

                                    TrenchBroom is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
*/

#include "SmartWadEditor.h"

#include <QBrush>
#include <QFileDialog>
#include <QListWidget>
#include <QToolButton>

#include <Preferences.h>
#include <PreferenceManager.h>
#include "IO/DiskIO.h"
#include "IO/PathInfo.h"
#include "IO/PathQt.h"
#include "IO/SystemPaths.h"
#include "Model/EntityNodeBase.h"
#include "View/BorderLine.h"
#include "View/ChoosePathTypeDialog.h"
#include "View/MapDocument.h"
#include "View/QtUtils.h"
#include "View/TitleBar.h"
#include "View/ViewConstants.h"

#include "kdl/string_utils.h"
#include "kdl/vector_utils.h"

#include <filesystem>

namespace TrenchBroom::View {

namespace {

std::vector<std::filesystem::path> getWadPaths(const std::vector<Model::EntityNodeBase *> &nodes, const std::string &propertyKey) {
    if (nodes.size() == 1) {
        if (const auto *wadPathsStr = nodes.front()->entity().property(propertyKey)) {
            return kdl::vec_transform(kdl::str_split(*wadPathsStr, ";"), [](const auto &s) {
                return std::filesystem::path{s};
            });
        }
    }
    return {};
}

std::string getWadPathStr(const std::vector<std::filesystem::path> &wadPaths) {
    return kdl::str_join(kdl::vec_transform(wadPaths, [](const auto &path) { return path.string(); }), ";");
}

} // namespace

SmartWadEditor::SmartWadEditor(std::weak_ptr<MapDocument> document, QWidget *parent) : SmartPropertyEditor{
    std::move(document), parent} {
    auto *header = new TitleBar{"Wad Files"};

    m_wadPaths = new QListWidget{};
    m_wadPaths->setSelectionMode(QAbstractItemView::ExtendedSelection);

    m_addWadsButton = createBitmapButton("Add.svg", "Add texture collections from the file system");
    m_removeWadsButton = createBitmapButton("Remove.svg", "Remove the selected texture collections");
    m_moveWadUpButton = createBitmapButton("Up.svg", "Move the selected texture collection up");
    m_moveWadDownButton = createBitmapButton("Down.svg", "Move the selected texture collection down");
    m_reloadWadsButton = createBitmapButton("Refresh.svg", "Reload all texture collections");

    auto *toolBar = createMiniToolBarLayout(m_addWadsButton, m_removeWadsButton, LayoutConstants::WideHMargin, m_moveWadUpButton, m_moveWadDownButton, LayoutConstants::WideHMargin, m_reloadWadsButton);

    auto *layout = new QVBoxLayout{};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);
    layout->addWidget(m_wadPaths, 1);
    layout->addWidget(new BorderLine{}, 0);
    layout->addLayout(toolBar, 0);

    setLayout(layout);

    connect(m_wadPaths, &QListWidget::itemSelectionChanged, this, &SmartWadEditor::updateButtons);

    connect(m_addWadsButton, &QAbstractButton::clicked, this, &SmartWadEditor::addWads);
    connect(m_removeWadsButton, &QAbstractButton::clicked, this, &SmartWadEditor::removeSelectedWads);
    connect(m_moveWadUpButton, &QAbstractButton::clicked, this, &SmartWadEditor::moveSelectedWadsUp);
    connect(m_moveWadDownButton, &QAbstractButton::clicked, this, &SmartWadEditor::moveSelectedWadsDown);
    connect(m_reloadWadsButton, &QAbstractButton::clicked, this, &SmartWadEditor::reloadWads);

    setAcceptDrops(true);
}

void SmartWadEditor::addWads() {
    const auto paths = QFileDialog::getOpenFileNames(window(), tr("Load Wad Files"), fileDialogDefaultDirectory(FileDialogDir::TextureCollection), tr("Wad files (*.wad);;All files (*.*)"));
    if (paths.isEmpty()) {
        return;
    }

    updateFileDialogDefaultDirectoryWithFilename(FileDialogDir::TextureCollection, paths.front());
    const auto doc = document();
    auto pathDialog = ChoosePathTypeDialog{
        window(), IO::pathFromQString(paths.front()), doc->path(), doc->game()->gamePath()
    };
    if (paths.size() > 1) {
        pathDialog.setWindowTitle(tr("Path Type for %1 WAD Files").arg(paths.size()));
    }
    if (pathDialog.exec() != QDialog::Accepted) {
        return;
    }

    auto wadPaths = getWadPaths(nodes(), propertyKey());
    const auto firstAddedRow = int(wadPaths.size());
    for (const auto &path : paths) {
        const auto absolutePath = IO::pathFromQString(path);
        const auto convertedPath = convertToPathType(pathDialog.pathType(), absolutePath, doc->path(), doc->game()->gamePath());
        // A relative path may be unavailable for a file on another drive.
        wadPaths.push_back(convertedPath.empty() ? absolutePath : convertedPath);
    }

    // Update the property once so the entire batch is a single undoable change.
    if (doc->setProperty(propertyKey(), getWadPathStr(wadPaths))) {
        m_wadPaths->clearSelection();
        for (int row = firstAddedRow; row < m_wadPaths->count(); ++row) {
            m_wadPaths->setCurrentRow(row, QItemSelectionModel::Select);
        }
    }
}

void SmartWadEditor::removeSelectedWads() {
    if (!canRemoveWads()) {
        return;
    }

    const auto indicesToRemove = kdl::vec_sort(
        kdl::vec_transform(m_wadPaths->selectedItems(),
                           [&](const auto &selectedItem) {
                               return size_t(m_wadPaths->row(selectedItem));
                           }
        ), std::greater<size_t>{}
    );

    auto wadPaths = getWadPaths(nodes(), propertyKey());
    for (const auto index : indicesToRemove) {
        wadPaths = kdl::vec_erase_at(std::move(wadPaths), index);
    }

    document()->setProperty(propertyKey(), getWadPathStr(wadPaths));
    m_wadPaths->setCurrentRow(
        std::min(int(indicesToRemove.back()),
                 m_wadPaths->count() - 1), QItemSelectionModel::ClearAndSelect
    );
}

void SmartWadEditor::moveSelectedWadsUp() {
    if (!canMoveWadsUp()) {
        return;
    }

    const auto index = size_t(m_wadPaths->currentRow());
    if (index > 0) {
        auto wadPaths = getWadPaths(nodes(), propertyKey());

        using std::swap;
        swap(wadPaths[index], wadPaths[index - 1]);

        document()->setProperty(propertyKey(), getWadPathStr(wadPaths));
        m_wadPaths->setCurrentRow(int(index) - 1, QItemSelectionModel::ClearAndSelect);
    }
}

void SmartWadEditor::moveSelectedWadsDown() {
    if (!canMoveWadsDown()) {
        return;
    }

    auto wadPaths = getWadPaths(nodes(), propertyKey());
    const auto index = size_t(m_wadPaths->currentRow());
    if (index < wadPaths.size() - 1) {
        using std::swap;
        swap(wadPaths[index], wadPaths[index + 1]);

        document()->setProperty(propertyKey(), getWadPathStr(wadPaths));
        m_wadPaths->setCurrentRow(int(index) + 1, QItemSelectionModel::ClearAndSelect);
    }
}

void SmartWadEditor::reloadWads() {
    document()->reloadTextureCollections();
    updateWadFileStatus();
}

bool SmartWadEditor::canRemoveWads() const {
    auto selections = std::vector<int>{};
    for (const auto *item : m_wadPaths->selectedItems()) {
        selections.push_back(m_wadPaths->row(item));
    }

    const auto wadPaths = getWadPaths(nodes(), propertyKey());
    return !selections.empty() && std::all_of(selections.begin(), selections.end(), [&](const auto s) {
        return size_t(s) < wadPaths.size();
    });

    return true;
}

bool SmartWadEditor::canMoveWadsUp() const {
    return m_wadPaths->selectedItems().size() == 1 && size_t(m_wadPaths->currentRow()) > 0;
}

bool SmartWadEditor::canMoveWadsDown() const {
    return m_wadPaths->selectedItems().size() == 1 &&
           size_t(m_wadPaths->currentRow()) < getWadPaths(nodes(), propertyKey()).size() - 1;
}

bool SmartWadEditor::canReloadWads() const {
    return m_wadPaths->count() > 0;
}

void SmartWadEditor::doUpdateVisual(const std::vector<Model::EntityNodeBase *> &nodes) {
    const auto selectedRows = kdl::vec_transform(m_wadPaths->selectedItems(), [&](const auto &selectedItem) {
        return std::tuple{m_wadPaths->row(selectedItem), selectedItem->text()};
    });

    m_wadPaths->clear();

    for (const auto &path : getWadPaths(nodes, propertyKey())) {
        m_wadPaths->addItem(IO::pathAsQString(path));
    }
    updateWadFileStatus();

    for (const auto &[index, text] : selectedRows) {
        if (index < m_wadPaths->count() && m_wadPaths->item(index)->text() == text) {
            m_wadPaths->setCurrentRow(index, QItemSelectionModel::Select);
        } else {
            m_wadPaths->clearSelection();
            break;
        }
    }
    updateButtons();
}

void SmartWadEditor::updateWadFileStatus() {
    if (m_wadPaths->count() == 0) {
        return;
    }

    const auto doc = document();
    // Keep the lookup order in sync with GameImpl::doReloadWads.
    const auto searchPaths = std::vector{
        doc->path().parent_path(), doc->game()->gamePath(), IO::SystemPaths::appDirectory()
    };

    for (int row = 0; row < m_wadPaths->count(); ++row) {
        auto *item = m_wadPaths->item(row);
        const auto path = IO::pathFromQString(item->text());
        const auto resolvedPath = IO::Disk::resolvePath(searchPaths, path);
        const auto exists = !resolvedPath.empty() && IO::Disk::pathInfo(resolvedPath) == IO::PathInfo::File;

        item->setForeground(exists ? QBrush{} : QBrush{pref(Preferences::UIErrorColor)});
        item->setToolTip(exists ? IO::pathAsQString(resolvedPath) : tr("WAD file not found: %1").arg(item->text()));
    }
}

void SmartWadEditor::updateButtons() {
    m_removeWadsButton->setEnabled(canRemoveWads());
    m_moveWadUpButton->setEnabled(canMoveWadsUp());
    m_moveWadDownButton->setEnabled(canMoveWadsDown());
    m_reloadWadsButton->setEnabled(canReloadWads());
}

} // namespace TrenchBroom::View
