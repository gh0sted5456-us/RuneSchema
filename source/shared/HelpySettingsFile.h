#pragma once
// Where Helpy's own preferences (settings.jsonc) live. AppData core/HELPY is the home; a copy still in the mod's
// Helpy/settings folder from an older install is used only until Helpy adopts it into AppData.
#include "Core/RuneSchemaDataRoot.h"
#include <filesystem>
namespace PS::HelpySettingsFile {
inline std::filesystem::path Legacy(const std::filesystem::path& helpyRoot){return helpyRoot/"settings"/"settings.jsonc";}
inline std::filesystem::path Location(const std::filesystem::path& helpyRoot){
    try{
        const auto current=RuneSchemaData::SharedDirectory("HELPY")/"settings.jsonc";std::error_code error;
        return !std::filesystem::exists(current,error)&&std::filesystem::is_regular_file(Legacy(helpyRoot),error)?Legacy(helpyRoot):current;
    }catch(...){return Legacy(helpyRoot);}
}
// Copies an older mod-side file into AppData once and returns the AppData path. Existing AppData files win.
inline std::filesystem::path Adopt(const std::filesystem::path& helpyRoot){
    const auto current=RuneSchemaData::SharedDirectory("HELPY")/"settings.jsonc";std::error_code error;
    if(!std::filesystem::exists(current,error)&&std::filesystem::is_regular_file(Legacy(helpyRoot),error)){
        std::filesystem::create_directories(current.parent_path(),error);
        std::filesystem::copy_file(Legacy(helpyRoot),current,std::filesystem::copy_options::skip_existing,error);
        if(error)return Legacy(helpyRoot);
    }
    return current;
}
}
