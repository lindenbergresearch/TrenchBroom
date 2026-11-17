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

#include "DiskFileSystem.h"

#include "IO/CachedFileTree.h"
#include "IO/DiskIO.h"
#include "IO/File.h"
#include "IO/PathInfo.h"
#include "IO/TraversalMode.h"

#include "kdl/path_utils.h"
#include "kdl/result.h"

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <tuple>

namespace TrenchBroom::IO {
namespace {

    // "." lexically normalizes to itself rather than to an empty path, but the cached tree
    // treats an empty path as a lookup of the root itself - so it needs to be collapsed
    // explicitly here.
    std::filesystem::path cacheLookupKey(const std::filesystem::path& normalizedPath) {
        auto path = normalizedPath;
        if (path.has_relative_path() && path.filename().empty()) path = path.parent_path();
        return kdl::path_to_lower(path == std::filesystem::path{"."} ? std::filesystem::path{} : path);
    }

} // namespace

DiskFileSystem::DiskFileSystem(const std::filesystem::path& root) : m_root{root.lexically_normal()} {
    std::ignore = reload();
}

DiskFileSystem::~DiskFileSystem() = default;

DiskFileSystem::DiskFileSystem(DiskFileSystem&& other) {
    const auto lock = std::unique_lock{other.m_mutex};
    m_root = std::move(other.m_root);
    m_cacheRoot = std::move(other.m_cacheRoot);
}

DiskFileSystem& DiskFileSystem::operator=(DiskFileSystem&& other) {
    if (this != &other) {
        const auto lock = std::scoped_lock{m_mutex, other.m_mutex};
        m_root = std::move(other.m_root);
        m_cacheRoot = std::move(other.m_cacheRoot);
    }
    return *this;
}

const std::filesystem::path& DiskFileSystem::root() const {
    return m_root;
}

Result<std::filesystem::path> DiskFileSystem::makeAbsolute(const std::filesystem::path& path) const {
    const auto canonicalPath = path.lexically_normal();
    if (!canonicalPath.empty() && kdl::path_front(canonicalPath) == "..") {
        return Error{"Failed to make absolute path of '" + path.string() + "'"};
    }
    return canonicalPath.empty() ? m_root : m_root / canonicalPath;
}

Result<void> DiskFileSystem::reload() {
    const auto lock = std::unique_lock{m_mutex};

    auto error = std::error_code{};
    const auto scanRoot = Disk::fixPath(m_root);
    auto it = std::filesystem::recursive_directory_iterator{scanRoot, std::filesystem::directory_options::follow_directory_symlink, error};
    const auto end = std::filesystem::recursive_directory_iterator{};

    if (error) {
        if (error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory) {
            m_cacheRoot = nullptr;
            return kdl::void_success;
        }
        return Error{"Failed to reload '" + m_root.string() + "': " + error.message()};
    }

    // build into a local tree and only publish it below once the scan completes fully -
    // otherwise a permission error, symlink loop, or race encountered partway through
    // would silently leave m_cacheRoot holding an incomplete tree while still reporting
    // success
    auto newCacheRoot = std::make_unique<DiskEntry>(DiskDirectoryEntry{{}, {}, {}});
    auto& rootDir = std::get<DiskDirectoryEntry>(*newCacheRoot);

    try {
        // A symlink may point at a parent directory. Follow useful aliases, but do
        // not recurse into a directory already on this branch of the scan.
        auto ancestors = std::vector<std::filesystem::path>{std::filesystem::canonical(scanRoot)};
        while (it != end) {
            ancestors.resize(size_t(it.depth()) + 1);
            const auto relativePath = it->path().lexically_relative(scanRoot);
            if (it->is_directory()) {
                findOrCreateCachedDirectory(relativePath, rootDir);
                auto target = it->is_symlink() ? std::filesystem::canonical(it->path()) : ancestors.back() / it->path().filename();
                if (std::find(ancestors.begin(), ancestors.end(), target) != ancestors.end())
                    it.disable_recursion_pending();
                else
                    ancestors.push_back(std::move(target));
            } else if (it->is_regular_file()) {
                auto& directoryEntry = findOrCreateCachedDirectory(relativePath.parent_path(), rootDir);
                addCachedEntry(directoryEntry, DiskFileEntry{relativePath.filename(), {}});
            }
            // else: neither a directory nor a regular file (e.g. a FIFO, device, socket, or
            // broken symlink) - skip it, matching Disk::pathInfo's PathInfo::Unknown for such
            // entries instead of caching it as a file
            ++it;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        // leave the previous cache (if any) in place rather than publishing a partial scan
        return Error{"Failed to reload '" + m_root.string() + "': " + e.what()};
    }

    m_cacheRoot = std::move(newCacheRoot);
    return kdl::void_success;
}

PathInfo DiskFileSystem::pathInfo(const std::filesystem::path& path) const {
    const auto normalized = path.lexically_normal();
    if (normalized.is_absolute()) {
        return Disk::pathInfo(normalized);
    }
    if (kdl::path_front(normalized) == "..") {
        return PathInfo::Unknown;
    }

    const auto lock = std::shared_lock{m_mutex};
    if (!m_cacheRoot) {
        return PathInfo::Unknown;
    }

    const auto* entry = findCachedEntry(cacheLookupKey(normalized), *m_cacheRoot);
    return entry ? isDirectoryEntry(*entry) ? PathInfo::Directory : PathInfo::File : PathInfo::Unknown;
}

Result<std::vector<std::filesystem::path>> DiskFileSystem::doFind(const std::filesystem::path& path, const TraversalMode traversalMode) const {
    const auto lock = std::shared_lock{m_mutex};

    auto result = std::vector<std::filesystem::path>{};
    // guards against a reload() racing between the base find() wrapper's pathInfo()
    // check and this call clearing the cache out from under it
    if (m_cacheRoot) {
        withCachedEntry(cacheLookupKey(path.lexically_normal()), *m_cacheRoot, {},
                        [&](const DiskEntry& entry, const std::filesystem::path& entryPath) { collectCachedEntries(entry, entryPath, 0, traversalMode, result); });
    }
    return result;
}

Result<std::shared_ptr<File>> DiskFileSystem::doOpenFile(const std::filesystem::path& path) const {
    const auto lock = std::shared_lock{m_mutex};

    // guards against a reload() racing between the base openFile() wrapper's
    // pathInfo() check and this call clearing the cache out from under it
    if (!m_cacheRoot) {
        return Result<std::shared_ptr<File>>{Error{"'" + path.string() + "' not found"}};
    }

    return withCachedEntry(
        cacheLookupKey(path.lexically_normal()), *m_cacheRoot, std::filesystem::path{},
        [&](const DiskEntry&, const std::filesystem::path& entryPath) {
            return Disk::openFile(m_root / entryPath).transform([](auto cFile) { return std::static_pointer_cast<File>(cFile); });
        },
        Result<std::shared_ptr<File>>{Error{"'" + path.string() + "' not found"}});
}

WritableDiskFileSystem::WritableDiskFileSystem(const std::filesystem::path& root) : DiskFileSystem{root} {}

Result<void> WritableDiskFileSystem::doCreateFile(const std::filesystem::path& path, const std::string& contents) {
    return reloadAfterWrite(makeAbsolute(path).and_then([&](const auto& absPath) { return Disk::withOutputStream(absPath, [&](auto& stream) { stream << contents; }); }));
}

Result<bool> WritableDiskFileSystem::doCreateDirectory(const std::filesystem::path& path) {
    return reloadAfterWrite(makeAbsolute(path).and_then(Disk::createDirectory));
}

Result<bool> WritableDiskFileSystem::doDeleteFile(const std::filesystem::path& path) {
    return reloadAfterWrite(makeAbsolute(path).and_then(Disk::deleteFile));
}

Result<void> WritableDiskFileSystem::doCopyFile(const std::filesystem::path& sourcePath, const std::filesystem::path& destPath) {
    return reloadAfterWrite(makeAbsolute(sourcePath).join(makeAbsolute(destPath)).and_then(Disk::copyFile));
}

Result<void> WritableDiskFileSystem::doMoveFile(const std::filesystem::path& sourcePath, const std::filesystem::path& destPath) {
    return reloadAfterWrite(makeAbsolute(sourcePath).join(makeAbsolute(destPath)).and_then(Disk::moveFile));
}

Result<bool> WritableDiskFileSystem::reloadAfterWrite(Result<bool> result) {
    return std::move(result).and_then([&](const bool value) { return reload().transform([&]() { return value; }); });
}

Result<void> WritableDiskFileSystem::reloadAfterWrite(Result<void> result) {
    return std::move(result).and_then([&]() { return reload(); });
}

} // namespace TrenchBroom::IO
