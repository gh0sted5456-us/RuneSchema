#pragma once
#include <filesystem>
namespace PS::HelpyPaths {
void Configure(const std::filesystem::path& pluginRoot);
const std::filesystem::path& PluginRoot();
// Managed UI artwork. Lives in AppData core/HELPY/ui-assets; the mod folder is only an old-install fallback.
std::filesystem::path Asset(const char* name);
// Helpy-owned folders in AppData core/HELPY. Created on demand.
std::filesystem::path BackgroundsFolder();
std::filesystem::path FontsFolder();
// Writes a script built into Helpy.exe to core/HELPY/tools and returns its path.
std::filesystem::path BuiltInScript(int resource,const char* name);
// Keeps a copy of the mod's licence bundle in AppData core/licenses. Best effort.
void CacheLicenses();
}
