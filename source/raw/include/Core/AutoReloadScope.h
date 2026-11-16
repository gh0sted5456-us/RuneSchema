#pragma once
#include <filesystem>
#include <string_view>
namespace PS::AutoReloadScope {
// These loaders reread the whole containing folder, even for a single file event.
inline std::filesystem::path Key(const std::filesystem::path& file,std::string_view loader){
    return loader=="players"||loader=="nameplates"?file.parent_path():file;
}
}
