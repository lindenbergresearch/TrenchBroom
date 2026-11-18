#pragma once

#include <filesystem>

namespace TrenchBroom {
// Tests may move between feature folders without changing asset lookup paths.
inline std::filesystem::path testSourceRoot() {
    return std::filesystem::path(__FILE__).lexically_normal().parent_path().parent_path().parent_path().parent_path();
}
} // namespace TrenchBroom
