#pragma once
#include <filesystem>
#include <algorithm>
#include <string>
#include <vector>
#include <stdexcept>
#include <cctype>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace PS::MappingCompatibility {
namespace fs=std::filesystem;
inline std::string Lower(std::string value){for(auto& c:value)c=char(std::tolower(static_cast<unsigned char>(c)));return value;}
inline bool Input(const fs::path& file,bool definitions){const auto ext=Lower(file.extension().string());return ext==".pak"||ext==".utoc"||ext==".ucas"||(definitions&&(ext==".json"||ext==".jsonc"));}
inline void Append(std::string& result,const std::string& value){
    if(result.size()+value.size()+32>8*1024*1024)throw std::runtime_error("Mapping compatibility identity exceeds its bound");
    result+=std::to_string(value.size())+":"+value;
}
inline void File(std::string& result,const fs::path& file,const std::string& label){
    Append(result,label);
    if(!fs::exists(file)){Append(result,"missing");return;}
    if(!fs::is_regular_file(file))throw std::runtime_error("Mapping compatibility input is not a regular file");
    Append(result,std::to_string(fs::file_size(file)));Append(result,std::to_string(fs::last_write_time(file).time_since_epoch().count()));
}
inline void Tree(std::string& result,const fs::path& root,const std::string& label,bool definitions){
    Append(result,label);if(!fs::exists(root)){Append(result,"0");return;}
    std::vector<fs::path> files;size_t visited=0;
    for(fs::recursive_directory_iterator it(root),end;it!=end;++it){
        if(++visited>200000||it.depth()>24)throw std::runtime_error("Mapping compatibility scan exceeds its bound");
#ifdef _WIN32
        const auto attributes=GetFileAttributesW(it->path().c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES)throw std::runtime_error("Mapping compatibility input became inaccessible");
        if(attributes&FILE_ATTRIBUTE_REPARSE_POINT){if(attributes&FILE_ATTRIBUTE_DIRECTORY)it.disable_recursion_pending();continue;}
#else
        if(it->is_symlink()){it.disable_recursion_pending();continue;}
#endif
        if(it->is_directory()){
            const auto name=Lower(it->path().filename().string());
            if(it->is_symlink()||name=="cache"||name=="runtime"||name==".git"||name=="mod-imports")it.disable_recursion_pending();
        }else if(it->is_regular_file()&&Input(it->path(),definitions)){
            if(files.size()>=32768)throw std::runtime_error("Mapping compatibility input count exceeds its bound");
            files.push_back(it->path());
        }
    }
    std::sort(files.begin(),files.end());Append(result,std::to_string(files.size()));
    for(const auto& file:files)File(result,file,file.lexically_relative(root).generic_string());
}
inline std::string Identity(const fs::path& binary,const fs::path& host,const fs::path&,const std::string& version){
    std::string result;Append(result,"RuneSchemaMappingCompatibility.v3");Append(result,version);
    // Runtime/mod updates do not change the game's native save/reflection contract.
    File(result,binary,"game executable");
    const auto game=host.parent_path().parent_path().parent_path();
    Tree(result,game/"Content/Paks","game containers",false);Tree(result,game/"Plugins","game plugin containers",false);
    return result;
}
}
