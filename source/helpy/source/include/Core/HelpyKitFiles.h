#pragma once
#include "Core/ItemCart.h"
#include "Core/ConfigFiles.h"
#include <filesystem>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif
namespace PS::HelpyKitFiles {
namespace fs=std::filesystem;
inline constexpr std::size_t MaxBytes=128*1024,MaxFiles=256;
inline std::string Fold(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c+=32;return value;}
inline bool FileName(const std::string& name) {
    if(name.empty()||name.size()>80||name.front()==' '||name.back()==' ')return false;
    for(const unsigned char c:name)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c==' '||c=='_'||c=='-'))return false;
    const auto lower=Fold(name);if(lower=="con"||lower=="prn"||lower=="aux"||lower=="nul")return false;
    if(lower.size()==4&&(lower.starts_with("com")||lower.starts_with("lpt"))&&lower.back()>='1'&&lower.back()<='9')return false;
    return true;
}
inline void NoRedirect(const fs::path& target) {
    auto current=fs::absolute(target).lexically_normal();
    for(;;) {
#ifdef _WIN32
        const auto flags=GetFileAttributesW(current.c_str());
        if(flags!=INVALID_FILE_ATTRIBUTES&&(flags&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Kit path cannot pass through a link or junction");
        if(flags==INVALID_FILE_ATTRIBUTES){const auto error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)throw std::runtime_error("Kit path could not be inspected");}
#else
        std::error_code error;const auto status=fs::symlink_status(current,error);
        if(fs::is_symlink(status))throw std::runtime_error("Kit path cannot pass through a symbolic link");
        if(error&&error!=std::errc::no_such_file_or_directory)throw std::runtime_error("Kit path could not be inspected");
#endif
        const auto parent=current.parent_path();if(parent.empty()||parent==current)break;current=parent;
    }
}
inline fs::path Target(const fs::path& directory,const std::string& file) {
    const fs::path leaf(file);const auto extension=Fold(leaf.extension().string());
    if(leaf.filename()!=leaf||(extension!=".json"&&extension!=".jsonc")||!FileName(leaf.stem().string()))throw std::runtime_error("Select a .json or .jsonc kit in the kit folder");
    const auto target=fs::absolute(directory).lexically_normal()/leaf;NoRedirect(target);return target;
}
inline std::vector<std::string> List(const fs::path& directory) {
    NoRedirect(directory);if(!fs::exists(directory))return {};
    std::vector<std::string> files;std::size_t inspected=0;
    for(const auto& entry:fs::directory_iterator(directory)) {
        if(++inspected>MaxFiles)throw std::runtime_error("Kit folder exceeds 256 entries; move old kits into another folder");
        const auto leaf=entry.path().filename().string(),extension=Fold(entry.path().extension().string());
        if((extension==".json"||extension==".jsonc")&&FileName(entry.path().stem().string())) {
            NoRedirect(entry.path());if(entry.is_regular_file())files.push_back(leaf);
        }
    }
    std::ranges::sort(files);return files;
}
inline nlohmann::json Read(const fs::path& directory,const std::string& file) {
    const auto path=Target(directory,file);const auto text=ConfigFiles::Read(path,MaxBytes);
    auto doc=nlohmann::json::parse(text,nullptr,true,true);(void)ItemCart::KitItems(doc);return doc;
}
inline fs::path Write(const fs::path& directory,const std::string& name,bool jsonc,const ItemCart::Model& cart,bool replace=false,bool selectedOnly=false) {
    if(!FileName(name))throw std::runtime_error("Kit name: 1..80 letters, numbers, spaces, '-' or '_'; no extension or reserved device names");
    const auto document=ItemCart::KitDocument(cart,name,selectedOnly);const auto target=Target(directory,name+(jsonc?".jsonc":".json"));
    if(fs::exists(target)) {
        if(!replace)throw std::runtime_error("A kit with this name already exists; confirm replacement or choose a new name");
        (void)ConfigFiles::Backup(target);
    }
    const auto text=(jsonc?"// RSMK kit: importing only stages items. Deliver explicitly in RSMK.\n":"")+document.dump(2)+"\n";
    if(replace)ConfigFiles::Write(target,text);
    else {
        // Publish a complete new kit atomically without clobbering a file that
        // another writer created after our initial existence check.
        static std::atomic<std::uint64_t> serial=0;
        fs::create_directories(directory);
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        const auto staging=target.parent_path()/(".kit-write-"+std::to_string(stamp)+"-"+std::to_string(++serial));
        if(!fs::create_directory(staging))throw std::runtime_error("Cannot reserve kit staging directory");
        const auto temporary=staging/"kit.tmp";
        const auto cleanup=[&]{std::error_code error;fs::remove(temporary,error);fs::remove(staging,error);};
        try {
            ConfigFiles::Write(temporary,text);
#ifdef _WIN32
            if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Kit was not saved; its name may already be in use");
#else
            fs::create_hard_link(temporary,target);
#endif
        }catch(...){cleanup();throw;}
        cleanup();
    }
    if(Read(directory,target.filename().string())!=document)throw std::runtime_error("Kit export read-back verification failed");
    return target;
}
}
