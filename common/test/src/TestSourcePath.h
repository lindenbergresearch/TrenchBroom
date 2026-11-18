#pragma once

#include <filesystem>

namespace TrenchBroom {
// Tests may move between feature folders without changing asset lookup paths.
inline std::filesystem::path testSourceRoot() {
#ifdef TB_TEST_SOURCE_ROOT
    // ccache can make __FILE__ relative to the compiler's working directory.
    // Runtime asset lookup must also work when CTest runs in another directory.
    return std::filesystem::path{TB_TEST_SOURCE_ROOT};
#else
    return std::filesystem::path(__FILE__).lexically_normal().parent_path().parent_path().parent_path().parent_path();
#endif
}
} // namespace TrenchBroom
