#pragma once
#include <filesystem>
#include <string_view>
namespace PS::StartupTrace {
void Begin(const std::filesystem::path& folder) noexcept;
void Mark(std::string_view stage) noexcept;
void Fatal(std::string_view stage, std::string_view reason) noexcept;
}
