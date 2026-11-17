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

#include "ImageFileSystem.h"
#include "IO/CachedFileTree.h"

#include "Ensure.h"
#include "Error.h"
#include "IO/DiskFileSystem.h"
#include "IO/File.h"
#include "IO/PathInfo.h"
#include "IO/TraversalMode.h"

#include "kdl/overload.h"
#include "kdl/path_utils.h"
#include "kdl/result.h"

#include <cassert>
#include <memory>

namespace TrenchBroom::IO {

ImageFileSystemBase::ImageFileSystemBase() : m_root{ImageDirectoryEntry{{}, {}, {}}} {
}

ImageFileSystemBase::~ImageFileSystemBase() = default;

Result<std::filesystem::path> ImageFileSystemBase::makeAbsolute(const std::filesystem::path &path) const {
    return Result<std::filesystem::path>{"/" / path};
}

Result<void> ImageFileSystemBase::reload() {
    m_root = ImageDirectoryEntry{{}, {}, {}};
    return doReadDirectory();
}

void ImageFileSystemBase::addFile(const std::filesystem::path &path, GetImageFile getFile) {
    auto& directoryEntry = findOrCreateCachedDirectory(path.parent_path(), std::get<ImageDirectoryEntry>(m_root));
    addCachedEntry(directoryEntry, ImageFileEntry{path.filename(), std::move(getFile)});
}

PathInfo ImageFileSystemBase::pathInfo(const std::filesystem::path &path) const {
    const auto *entry = findCachedEntry(kdl::path_to_lower(path), m_root);
    return entry ? isDirectoryEntry(*entry) ? PathInfo::Directory : PathInfo::File : PathInfo::Unknown;
}

Result<std::vector<std::filesystem::path>> ImageFileSystemBase::doFind(const std::filesystem::path &path, const TraversalMode traversalMode) const {
    auto result = std::vector<std::filesystem::path>{};
    withCachedEntry(kdl::path_to_lower(path), m_root, {}, [&](const ImageEntry &entry, const std::filesystem::path &entryPath) {
        collectCachedEntries(entry, entryPath, 0, traversalMode, result);
    });
    return result;
}

Result<std::shared_ptr<File>> ImageFileSystemBase::doOpenFile(const std::filesystem::path &path) const {
    return withCachedEntry(kdl::path_to_lower(path), m_root, std::filesystem::path{}, [&](const ImageEntry &entry, const std::filesystem::path &) {
        return std::visit(kdl::overload([&](const ImageDirectoryEntry &) {
            return Result<std::shared_ptr<File>>{
                Error{"Cannot open directory entry at '" + path.string() + "'"}};
        }, [](const ImageFileEntry &fileEntry) { return fileEntry.payload(); }), entry);
    }, Result<std::shared_ptr<File>>{Error{"'" + path.string() + "' not found"}});
}
} // namespace TrenchBroom::IO
