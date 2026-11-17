#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace TrenchBroom::View {
struct CommandHistoryEntry {
    std::string name;
    std::chrono::system_clock::time_point timestamp;
    bool applied;
};
using CommandHistory = std::vector<CommandHistoryEntry>;
} // namespace TrenchBroom::View
