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

#pragma once

#include <iostream>
#include <map>
#include <optional>
#include <mutex>

namespace TrenchBroom {

/**
 * @brief A universal thread-safe cache based on std::map.
 * 
 * Provides insertion, retrieval, removal, and checking for key existence.
 * Supports element access via `operator[]` and `at`, similar to std::map.
 * 
 * @tparam KEY Type of the keys.
 * @tparam VALUE Type of the values.
 */
template<typename KEY, typename VALUE>
class Cache {
  public:
    /**
     * @brief Inserts or updates a key-value pair in the cache.
     * 
     * @param key The key for the cache entry.
     * @param value The value to be associated with the key.
     */
    void insert(const KEY &key, const VALUE &value) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        cache_[key] = value;
    }

    /**
     * @brief Retrieves a value from the cache if the key exists.
     * 
     * @param key The key to look up.
     * @return std::optional<VALUE> The value if the key exists, or std::nullopt otherwise.
     */
    std::optional<VALUE> get(const KEY &key) const {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief Removes a key-value pair from the cache.
     * 
     * @param key The key to remove.
     * @return true if the key was found and removed, false otherwise.
     */
    bool remove(const KEY &key) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        return cache_.erase(key) > 0;
    }

    /**
     * @brief Checks whether the cache contains a specific key.
     * 
     * @param key The key to check for.
     * @return true if the key exists in the cache, false otherwise.
     */
    bool contains(const KEY &key) const {
        std::lock_guard<std::mutex> lock(cache_mutex);
        return cache_.find(key) != cache_.end();
    }

    /**
     * @brief Clears all entries in the cache.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(cache_mutex);
        cache_.clear();
    }

    /**
     * @brief Returns the number of elements in the cache.
     * 
     * @return size_t The number of key-value pairs stored in the cache.
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(cache_mutex);
        return cache_.size();
    }

    /**
     * @brief Accesses or inserts a value for a given key.
     * 
     * If the key does not exist, a default-constructed value is inserted.
     * 
     * @param key The key to access.
     * @return VALUE& A reference to the value associated with the key.
     */
    VALUE &operator[](const KEY &key) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        return cache_[key];
    }

    /**
     * @brief Accesses the value associated with the key (throws exception if key is not found).
     * 
     * @param key The key to access.
     * @return const VALUE& A reference to the value.
     * @throws std::out_of_range If the key does not exist.
     */
    const VALUE &at(const KEY &key) const {
        std::lock_guard<std::mutex> lock(cache_mutex);
        return cache_.at(key);
    }

  private:
    mutable std::mutex cache_mutex; ///< Mutex for thread-safe access.
    std::map<KEY, VALUE> cache_;    ///< The underlying map storage.
};

}
