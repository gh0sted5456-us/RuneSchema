#pragma once
#include "Core/CharacterSaveEditor.h"
#include <filesystem>
namespace PS::XboxCharacterSaves {
namespace fs=std::filesystem;
inline bool Candidate(const fs::path& root,const fs::path& file){
    std::error_code ec;const auto base=fs::canonical(root,ec);if(ec)return false;
    const auto path=fs::canonical(file,ec);if(ec)return false;
    const auto relative=path.lexically_relative(base);size_t depth=0;
    for(const auto& part:relative){if(part==".."||part==".")return false;++depth;}
    if(depth!=3||path.has_extension()||CharacterSaveEditor::Guid(path.filename().string()).empty())return false;
    for(auto current=file;!current.empty();current=current.parent_path()){
        if(fs::is_symlink(fs::symlink_status(current,ec))||ec)return false;
        if(current==root)break;if(current==current.parent_path())return false;
    }
    return fs::is_regular_file(path,ec)&&!ec;
}
inline bool Character(const nlohmann::json& value){
    return !CharacterSaveEditor::Identity(value).empty()&&value.contains("GameProgress")
        &&value["GameProgress"].is_object()&&value["GameProgress"].contains("Inventory")
        &&value["GameProgress"]["Inventory"].is_object();
}
inline std::vector<fs::path> Candidates(const fs::path& root){
    std::vector<fs::path> files;std::error_code ec;if(root.empty()||!fs::is_directory(root,ec))return files;
    fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end;size_t examined=0;
    for(;!ec&&it!=end&&++examined<=8192;it.increment(ec)){
        const auto& entry=*it;
        if(it.depth()>=2||fs::is_symlink(entry.symlink_status(ec)))it.disable_recursion_pending();
        if(it.depth()==2&&Candidate(root,entry.path()))files.push_back(entry.path());
    }
    return files;
}
}
