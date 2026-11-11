#pragma once

#include <filesystem>
#include <vector>

namespace PS::SisterContent {
struct Root {
    std::filesystem::path::string_type Name;
    std::filesystem::path Content;
};

std::vector<Root> Discover(const std::filesystem::path& runeMods);
}
