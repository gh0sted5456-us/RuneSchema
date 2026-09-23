#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace PS::ConfigFiles {
    std::string Read(const std::filesystem::path& path);
    std::string Read(const std::filesystem::path& path, size_t maximumBytes);
    std::filesystem::path Backup(const std::filesystem::path& path);
    void Write(const std::filesystem::path& path, std::string_view contents);
}
