#pragma once
// R3v5: exclusive, verified writes. A watcher never sees a partially-written JSON.
#include "Generator/AuthoringPolicy.h"
#include "Runtime/HostServices.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>
#include <Windows.h>

namespace PS::Authoring {
inline std::filesystem::path Directory(const char* category) {
    const std::string value(category);
    if(value!="spawns" && value!="assets" && value!="raw" && value!="journal" && value!="recipes" && value!="npc")throw std::runtime_error("Invalid authoring category");
    // ModDirectory is UE4SS/Mods/RuneSchema; this is its content-mod subtree.
    return HostServices::ModDirectory()/"mods"/"runeschema"/value;
}
class StagedFile {
    std::filesystem::path temporary_, final_;
    bool committed_=false,preservePending_=false;
public:
    StagedFile(const char* category,const std::string& stem,const nlohmann::json& body) {
        if(!SafeFileStem(stem))throw std::runtime_error("Invalid authored filename");
        const auto text=body.dump(2);
        if(text.size()>1024*1024)throw std::runtime_error("Authored document exceeds 1 MiB");
        const auto folder=Directory(category);
        std::filesystem::create_directories(folder);
        final_=folder/(stem+".json");
        temporary_=folder/(stem+".pending");
        HANDLE handle=CreateFileW(temporary_.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create authored staging file: "+temporary_.string());
        DWORD written=0;
        const bool ok=WriteFile(handle,text.data(),static_cast<DWORD>(text.size()),&written,nullptr)
            && written==text.size() && FlushFileBuffers(handle);
        const bool closed=CloseHandle(handle)!=FALSE;
        try {
            if(!ok || !closed)throw std::runtime_error("Authored JSON could not be fully flushed");
            std::ifstream input(temporary_,std::ios::binary);
            if(!input || nlohmann::json::parse(input)!=body)throw std::runtime_error("Authored JSON verification failed");
        }catch(...) {std::error_code ignored;std::filesystem::remove(temporary_,ignored);throw;}
    }
    StagedFile(const StagedFile&)=delete;
    StagedFile& operator=(const StagedFile&)=delete;
    ~StagedFile() {if(!committed_&&!preservePending_){std::error_code ignored;std::filesystem::remove(temporary_,ignored);}}
    const std::filesystem::path& Path()const {return final_;}
    const std::filesystem::path& PendingPath()const {return temporary_;}
    bool Committed()const noexcept{return committed_;}
    void PreservePending() noexcept{preservePending_=true;}
    void Commit() {
        if(committed_)return;
        // No REPLACE_EXISTING: existing author-created files are never overwritten.
        if(!MoveFileExW(temporary_.c_str(),final_.c_str(),MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot install authored JSON (a file may already exist): "+final_.string());
        committed_=true;
    }
};
} // namespace PS::Authoring
