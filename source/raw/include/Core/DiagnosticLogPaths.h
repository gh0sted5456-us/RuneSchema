#pragma once
#include <filesystem>
namespace PS::DiagnosticLogPaths {
// Frozen at process startup; changing the saved setting takes effect next launch.
inline bool UseUE4SS=false;
inline std::filesystem::path UE4SS(const std::filesystem::path& mod){return mod.parent_path().parent_path();}
inline std::filesystem::path Directory(const std::filesystem::path& mod){return UseUE4SS?UE4SS(mod):mod;}
}
