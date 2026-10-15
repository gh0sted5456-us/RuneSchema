#pragma once
#include <filesystem>
namespace PS::HelpyPaths {
void Configure(const std::filesystem::path& pluginRoot);
const std::filesystem::path& PluginRoot();
std::filesystem::path Asset(const char* name);
}
